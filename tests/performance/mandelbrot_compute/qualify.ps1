param(
    [Parameter(Mandatory)][string]$ArtifactDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$VmHost = '',
    [int]$Repetitions = 9,
    [int]$VmRepetitions = 3,
    [int]$TimeoutSeconds = 180,
    [int]$LogicalProcessor = -1,
    [double]$MaxRelativeRange = 0.15,
    [double]$MaxRelativeDrift = 0.10,
    [switch]$NativeOnly
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../Measurement.psm1') -Force
if ($Repetitions -lt 3 -or $VmRepetitions -lt 3 -or $TimeoutSeconds -lt 1 -or
    ![double]::IsFinite($MaxRelativeRange) -or ![double]::IsFinite($MaxRelativeDrift) -or
    $MaxRelativeRange -le 0 -or $MaxRelativeDrift -le 0) {
    throw 'Require >=3 samples per backend, a positive timeout and positive stability limits.'
}
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$ArtifactDirectory = [IO.Path]::GetFullPath($ArtifactDirectory)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Choose a fresh evidence directory.' }
if (!$VmHost) { $VmHost = Join-Path $taskRoot 'build/vm/Release/wio_mandelbrot_vm_benchmark.exe' }
$VmHost = [IO.Path]::GetFullPath($VmHost)
$taskCases = [Collections.Generic.List[object]]::new()
$taskPaths = [Collections.Generic.List[string]]::new()
foreach ($taskKind in @('compute', 'full')) {
    $taskDirectory = Join-Path $ArtifactDirectory $taskKind
    if (!(Test-Path -LiteralPath $taskDirectory)) {
        if ($taskKind -eq 'compute') { throw 'Compute artifacts are required.' }
        continue
    }
    $taskNative = Join-Path $taskDirectory 'native.exe'
    $taskCases.Add(@{ Name="$taskKind-native"; Program=$taskNative; Arguments=@()
        Directory=$taskDirectory; Kind=$taskKind; Count=$Repetitions })
    if ($taskKind -eq 'compute') {
        # A/A control is the EXACT SAME executable, not another compilation.
        $taskCases.Add(@{ Name='compute-native-control'; Program=$taskNative; Arguments=@()
            Directory=$taskDirectory; Kind=$taskKind; Count=$Repetitions })
    }
    $taskPrevious = Join-Path $taskDirectory 'previous.exe'
    if ($taskKind -eq 'full' -and (Test-Path -LiteralPath $taskPrevious)) {
        $taskCases.Add(@{ Name='full-previous'; Program=$taskPrevious; Arguments=@()
            Directory=$taskDirectory; Kind=$taskKind; Count=$Repetitions })
    }
    if (!$NativeOnly) {
        $taskCases.Add(@{ Name="$taskKind-vm"; Program=$VmHost
            Arguments=@((Join-Path $taskDirectory 'main.wiob'), (Join-Path $taskDirectory 'bridge.dll'))
            Directory=$taskDirectory; Kind=$taskKind; Count=$VmRepetitions })
    }
    foreach ($taskFile in Get-ChildItem -LiteralPath $taskDirectory -File) {
        if ($taskFile.Extension -in @('.exe', '.dll', '.wiob', '.wio', '.cpp', '.h')) { $taskPaths.Add($taskFile.FullName) }
    }
}
foreach ($taskCase in $taskCases) {
    $taskPaths.Add($taskCase.Program)
    foreach ($taskArgument in $taskCase.Arguments) { $taskPaths.Add($taskArgument) }
}
# Preserve the exact controller/module identity as well as the workload identity.
# Editing a runner during a long measurement invalidates that run too.
$taskPaths.Add($PSCommandPath)
$taskPaths.Add([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Measurement.psm1')))
foreach ($taskProvenance in @('build.json', 'results.json')) {
    $taskProvenancePath = Join-Path $ArtifactDirectory $taskProvenance
    if (Test-Path -LiteralPath $taskProvenancePath) { $taskPaths.Add($taskProvenancePath) }
}
$taskArtifactsBefore = @(Get-MeasurementArtifacts $taskPaths.ToArray())
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$taskRevision = (& git -C $taskRoot rev-parse HEAD).Trim()
$taskWorktree = (@(& git -C $taskRoot status --short) -join "`n")
$taskSamples = [Collections.Generic.List[object]]::new()
$taskSummary = @(); $taskFailure = $null; $taskContext = $null
$taskEnvironmentBefore = Get-MeasurementEnvironment
$taskEnvironmentAfter = $null; $taskArtifactsAfter = @(); $taskUnchanged = $false
$taskAccepted = $false; $taskControl = $null; $taskPowerUnchanged = $false
function Save-Evidence {
    [pscustomobject]@{ Schema=2; Utc=[DateTime]::UtcNow.ToString('o')
        Revision=$taskRevision; Worktree=$taskWorktree
        ArtifactDirectory=$ArtifactDirectory; Rebuilt=$false
        Repetitions=$Repetitions; VmRepetitions=$VmRepetitions; NativeOnly=[bool]$NativeOnly
        TimeoutSeconds=$TimeoutSeconds; VmHost=$VmHost
        BuildConfiguration='See frozen input provenance; standalone qualification does not infer optimization flags from binary names.'
        Policy='All raw samples retained; one excluded warm-up per case; rotating/reversing order; no trimming/retry-until-pass.'
        Limits=@{ RelativeRange=$MaxRelativeRange; RelativeDrift=$MaxRelativeDrift; ControlMedianDifference=$MaxRelativeDrift }
        LogicalProcessor=if ($taskContext) { $taskContext.LogicalProcessor } else { $null }
        AffinityMask=if ($taskContext) { $taskContext.Mask } else { $null }
        AffinityPolicy='Controller pinned before launch; Windows child inherits; original controller mask restored in finally.'
        EnvironmentBefore=$taskEnvironmentBefore; EnvironmentAfter=$taskEnvironmentAfter
        ArtifactsBefore=$taskArtifactsBefore; ArtifactsAfter=$taskArtifactsAfter; ArtifactsUnchanged=$taskUnchanged
        Samples=$taskSamples.ToArray(); Summary=$taskSummary; SameBinaryControl=$taskControl
        PowerSchemeUnchanged=$taskPowerUnchanged; Accepted=$taskAccepted; Failure=$taskFailure
    } | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'results.json') -Encoding utf8
}
try {
    $taskContext = Enter-MeasurementAffinity $LogicalProcessor
    Save-Evidence
    foreach ($taskRound in 0..([Math]::Max($Repetitions, $(if ($NativeOnly) { 0 } else { $VmRepetitions })))) {
        foreach ($taskCase in @(Get-MeasurementOrder $taskCases.ToArray() $taskRound)) {
            if ($taskRound -gt $taskCase.Count) { continue }
            $taskInfo = [Diagnostics.ProcessStartInfo]::new()
            $taskInfo.FileName = $taskCase.Program
            $taskInfo.WorkingDirectory = $taskCase.Directory
            $taskInfo.UseShellExecute = $false; $taskInfo.CreateNoWindow = $true
            $taskInfo.RedirectStandardOutput = $true; $taskInfo.RedirectStandardError = $true
            foreach ($taskArgument in $taskCase.Arguments) { $taskInfo.ArgumentList.Add($taskArgument) }
            $taskOutputPath = Join-Path $taskCase.Directory 'mandelbrot.ppm'
            $taskPriorStamp = if (Test-Path -LiteralPath $taskOutputPath) { (Get-Item $taskOutputPath).LastWriteTimeUtc } else { $null }
            $taskSystemBefore = Get-MeasurementSystemTimes
            $taskWatch = [Diagnostics.Stopwatch]::StartNew()
            $taskProcess = $null
            try {
                $taskProcess = [Diagnostics.Process]::Start($taskInfo)
                $taskStdout = $taskProcess.StandardOutput.ReadToEndAsync()
                $taskStderr = $taskProcess.StandardError.ReadToEndAsync()
                $taskTimedOut = $false; $taskNextNotice = 30
                while (!$taskProcess.WaitForExit(250)) {
                    if ($taskWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
                        $taskProcess.Kill($true); $taskProcess.WaitForExit(); $taskTimedOut = $true; break
                    }
                    if ($taskWatch.Elapsed.TotalSeconds -ge $taskNextNotice) {
                        Write-Host "$($taskCase.Name) [$taskRound]: running ($taskNextNotice s)"
                        $taskNextNotice += 30
                    }
                }
                $taskWatch.Stop()
                $taskSystemAfter = Get-MeasurementSystemTimes
                $taskCpu = $taskProcess.TotalProcessorTime.TotalSeconds
                $taskRow = [pscustomobject]@{ Name=$taskCase.Name; Round=$taskRound; Warmup=($taskRound -eq 0)
                    Utc=[DateTime]::UtcNow.ToString('o'); Status='passed'; ExitCode=$taskProcess.ExitCode
                    WallSeconds=$taskWatch.Elapsed.TotalSeconds; CpuSeconds=$taskCpu
                    CpuToWall=($taskCpu / $taskWatch.Elapsed.TotalSeconds)
                    SystemBusyFraction=if ($taskSystemAfter.Total -gt $taskSystemBefore.Total) {
                        1 - ($taskSystemAfter.Idle - $taskSystemBefore.Idle) / ($taskSystemAfter.Total - $taskSystemBefore.Total)
                    } else { $null }
                    KernelSeconds=$null; Output=$taskStdout.Result.Trim(); Error=$taskStderr.Result.Trim()
                    Bytes=$null; Hash=$null }
                if ($taskTimedOut) { $taskRow.Status = 'timeout' }
                elseif ($taskProcess.ExitCode -ne 0) { $taskRow.Status = 'failed' }
                elseif ($taskCase.Kind -eq 'compute') {
                    $taskMatch = [regex]::Match($taskRow.Output,
                        '^seconds=(\d+\.\d+) iterations=11613930 weighted=3721766287256 interior=94440$')
                    if (!$taskMatch.Success) { $taskRow.Status = 'checksum-mismatch' }
                    else { $taskRow.KernelSeconds = [double]::Parse($taskMatch.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture) }
                } else {
                    if (!(Test-Path -LiteralPath $taskOutputPath) -or
                        ($taskPriorStamp -and (Get-Item $taskOutputPath).LastWriteTimeUtc -eq $taskPriorStamp)) {
                        $taskRow.Status = 'stale-output'
                    } else {
                        $taskRow.Bytes = (Get-Item $taskOutputPath).Length
                        $taskRow.Hash = (Get-FileHash -LiteralPath $taskOutputPath).Hash
                        if ($taskRow.Bytes -ne 5156528 -or $taskRow.Hash -ne
                            '1984A4F3A3575BD438EBD79F6EC3D0EB571BD141E5A9CBD29DFABA37941FF733') {
                            $taskRow.Status = 'checksum-mismatch'
                        }
                    }
                }
                $taskSamples.Add($taskRow)
                Save-Evidence
                Write-Host "$($taskRow.Name) [$taskRound] $($taskRow.Status): kernel=$($taskRow.KernelSeconds) wall=$($taskRow.WallSeconds)"
                if ($taskRow.Status -ne 'passed') { throw "Sample rejected: $($taskRow.Name): $($taskRow.Status)" }
            } finally {
                if ($taskProcess) {
                    if (!$taskProcess.HasExited) { $taskProcess.Kill($true); $taskProcess.WaitForExit() }
                    $taskProcess.Dispose()
                }
            }
        }
    }
    $taskSummary = @(foreach ($taskCase in $taskCases) {
        $taskMetric = if ($taskCase.Kind -eq 'compute') { 'KernelSeconds' } else { 'WallSeconds' }
        $taskRows = @($taskSamples | Where-Object { $_.Name -eq $taskCase.Name -and !$_.Warmup -and $_.Status -eq 'passed' })
        $taskStability = Get-MeasurementStability @($taskRows | ForEach-Object $taskMetric) $taskCase.Count $MaxRelativeRange $MaxRelativeDrift
        [pscustomobject]@{ Name=$taskCase.Name; Metric=$taskMetric; Stability=$taskStability }
    })
    $taskA = ($taskSummary | Where-Object Name -EQ 'compute-native').Stability.Median
    $taskB = ($taskSummary | Where-Object Name -EQ 'compute-native-control').Stability.Median
    $taskDifference = [Math]::Abs($taskA - $taskB) / (($taskA + $taskB) / 2)
    $taskControl = [pscustomobject]@{ RelativeMedianDifference=$taskDifference
        Accepted=($taskDifference -le $MaxRelativeDrift); SameExecutable=$true }
} catch { $taskFailure = $_.Exception.Message }
finally {
    if ($taskContext) {
        try { Exit-MeasurementAffinity $taskContext }
        catch { $taskFailure = "Affinity restoration: $($_.Exception.Message)" }
    }
    $taskEnvironmentAfter = Get-MeasurementEnvironment
    try {
        $taskArtifactsAfter = @(Get-MeasurementArtifacts $taskPaths.ToArray())
        $taskUnchanged = Test-MeasurementArtifacts $taskArtifactsBefore $taskArtifactsAfter
    } catch { $taskFailure = "Artifact verification: $($_.Exception.Message)" }
    $taskPowerUnchanged = $taskEnvironmentBefore.PowerQueryExit -eq 0 -and $taskEnvironmentAfter.PowerQueryExit -eq 0 -and
        $taskEnvironmentBefore.PowerScheme -eq $taskEnvironmentAfter.PowerScheme
    $taskAccepted = !$taskFailure -and $taskUnchanged -and $taskPowerUnchanged -and $taskControl -and $taskControl.Accepted -and
        @($taskSummary | Where-Object { !$_.Stability.Accepted }).Count -eq 0
    Save-Evidence
}
$taskSummary | Format-List
if (!$taskAccepted) { throw "Measurement NOT qualified; raw evidence retained: $OutputDirectory/results.json; $taskFailure" }
Write-Host 'Measurement qualified within the declared range/drift limits; not proof of cross-session equivalence.'

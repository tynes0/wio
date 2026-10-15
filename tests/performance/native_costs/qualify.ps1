param(
    [Parameter(Mandatory)][string]$ArtifactDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [int]$Repetitions = 15,
    [int]$TimeoutSeconds = 120,
    [int]$LogicalProcessor = -1,
    [double]$MaxRelativeRange = 0.15,
    [double]$MaxRelativeDrift = 0.10,
    [switch]$LegacyInstrumentedCore
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../Measurement.psm1') -Force
if ($Repetitions -lt 3 -or $TimeoutSeconds -lt 1 -or
    ![double]::IsFinite($MaxRelativeRange) -or ![double]::IsFinite($MaxRelativeDrift) -or
    $MaxRelativeRange -le 0 -or $MaxRelativeDrift -le 0) {
    throw 'Require >=3 samples, a positive timeout and finite positive stability limits.'
}
Initialize-MeasurementWindows
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$ArtifactDirectory = [IO.Path]::GetFullPath($ArtifactDirectory)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Choose a fresh evidence directory.' }
$taskProvenance = Join-Path $ArtifactDirectory 'build.json'
if (!(Test-Path -LiteralPath $taskProvenance)) { $taskProvenance = Join-Path $ArtifactDirectory 'results.json' }
$taskBuild = Get-Content -LiteralPath $taskProvenance -Raw | ConvertFrom-Json
$taskSuite = $taskBuild.Suite; $taskCounters = $taskBuild.CountersEnabled
if ($LegacyInstrumentedCore) {
    if ($taskSuite -or $null -ne $taskCounters -or $taskBuild.Scope -notlike 'instrumented ordinary new/new*' -or
        @($taskBuild.Flags | Where-Object { $_ -match 'WIO_COST_NO_COUNTERS' }).Count) {
        throw 'Legacy override only accepts the original explicitly instrumented core provenance.'
    }
    $taskSuite = 'core'; $taskCounters = $true
}
if ($taskSuite -notin @('core', 'extended') -or $taskCounters -isnot [bool] -or
    ($taskBuild.Count -isnot [long] -and $taskBuild.Count -isnot [int]) -or
    $taskBuild.Count -lt 1 -or $taskBuild.Count -gt 100000) {
    throw 'Input provenance must declare a valid suite, boolean counter mode and count in [1,100000].'
}
$taskCount = [int]$taskBuild.Count
$taskKinds = @('concat', 'builder', 'format', 'bytes', 'array', 'component', 'object', 'native_call', 'result')
if ($taskSuite -eq 'extended') { $taskKinds = @('unicode', 'dictionary', 'result_error', 'weak', 'closure', 'async_ready') }
$taskGoldens = @{}
if ($taskSuite -eq 'extended') {
    $taskGoldenTable = @{
        1=@('17445855498045112857','99','145247952907163005','1','1','1')
        17=@('16810242653416937817','10007309852365595643','2535633178212605909','3335769950788531865','3335769950788531865','3335769950788531865')
        2000=@('11864642420176368741','16071255685101081899','7352865453926614568','11505999919603546472','11505999919603546472','11505999919603546472')
    }
    if ($taskGoldenTable.ContainsKey($taskCount)) {
        for ($taskIndex = 0; $taskIndex -lt $taskKinds.Count; ++$taskIndex) {
            $taskGoldens[$taskKinds[$taskIndex]] = $taskGoldenTable[$taskCount][$taskIndex]
        }
    }
}
$taskRecordedNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($taskRecorded in $taskBuild.Artifacts) {
    if (!$taskRecorded.Path -or $taskRecorded.Path -ne [IO.Path]::GetFileName($taskRecorded.Path) -or
        $taskRecorded.Path -in @('.', '..') -or !$taskRecordedNames.Add($taskRecorded.Path)) {
        throw 'Build provenance must contain unique, local artifact filenames.'
    }
    if ((Get-FileHash -LiteralPath (Join-Path $ArtifactDirectory $taskRecorded.Path)).Hash -ne $taskRecorded.Hash) {
        throw "Frozen artifact does not match build provenance: $($taskRecorded.Path)"
    }
}
if (!$taskRecordedNames.Contains('cpp.exe') -or !$taskRecordedNames.Contains('wio.exe')) {
    throw 'Build provenance must include both cpp.exe and wio.exe.'
}
$taskPaths = @($PSCommandPath, (Join-Path $PSScriptRoot '../Measurement.psm1'), $taskProvenance)
$taskPaths += @(Get-ChildItem -LiteralPath $ArtifactDirectory -File | Where-Object {
    $_.Extension -in @('.exe', '.h', '.cpp', '.wio')
} | Select-Object -ExpandProperty FullName)
$taskBefore = @(Get-MeasurementArtifacts $taskPaths)
$taskCases = @(foreach ($taskKind in $taskKinds) {
    foreach ($taskVariant in @('cpp', 'wio')) {
        [pscustomobject]@{ Case=$taskKind; Variant=$taskVariant; Program=(Join-Path $ArtifactDirectory "$taskVariant.exe") }
    }
})
$taskCases += [pscustomobject]@{ Case=$taskKinds[0]; Variant='cpp-control'; Program=(Join-Path $ArtifactDirectory 'cpp.exe') }
$taskSamples = [Collections.Generic.List[object]]::new()
$taskExpected = @{}; $taskSummary = @(); $taskAfter = @(); $taskContext = $null
$taskFailure = $null; $taskCorrect = $false; $taskTiming = $false; $taskImmutable = $false
$taskPowerUnchanged = $false; $taskControl = $null; $taskEnvironmentAfter = $null
$taskRevision = (& git -C $taskRoot rev-parse HEAD).Trim()
$taskWorktree = (@(& git -C $taskRoot status --short) -join "`n")
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$taskEnvironmentBefore = Get-MeasurementEnvironment
function Save-Evidence {
    [pscustomobject]@{ Schema=2; Revision=$taskRevision; Worktree=$taskWorktree; Utc=[DateTime]::UtcNow.ToString('o')
        ArtifactDirectory=$ArtifactDirectory; Rebuilt=$false; Provenance=$taskProvenance
        Suite=$taskSuite; Count=$taskCount; CountersEnabled=$taskCounters; LegacyInstrumentedCore=[bool]$LegacyInstrumentedCore
        Repetitions=$Repetitions; TimeoutSeconds=$TimeoutSeconds
        Limits=@{ RelativeRange=$MaxRelativeRange; RelativeDrift=$MaxRelativeDrift; ControlMedianDifference=$MaxRelativeDrift }
        Policy='One excluded warm-up per lane; rotating/reversing order; exact-binary A/A; all samples retained, no trimming/retry-until-pass.'
        ChecksumOracle=if ($taskGoldens.Count) { 'Independent extended-suite goldens plus excluded C++ warm-up parity' }
            else { 'Excluded C++ warm-up parity; NOT an independent algorithm oracle' }
        CounterScope=if ($taskCounters) { 'Instrumented ordinary new/new[] requests; not malloc/aligned/live/peak/copy/RC attribution' }
            else { 'Standard allocation operators; allocation fields are unmeasured/null, never zero claims' }
        LogicalProcessor=if ($taskContext) { $taskContext.LogicalProcessor } else { $null }
        AffinityMask=if ($taskContext) { $taskContext.Mask } else { $null }
        AffinityPolicy='Controller pinned BEFORE launch; Windows children inherit; original mask restored in finally.'
        EnvironmentBefore=$taskEnvironmentBefore; EnvironmentAfter=$taskEnvironmentAfter
        ArtifactsBefore=$taskBefore; ArtifactsAfter=$taskAfter; ArtifactsUnchanged=$taskImmutable
        PowerSchemeUnchanged=$taskPowerUnchanged; Samples=$taskSamples.ToArray(); Summary=$taskSummary
        SameBinaryControl=$taskControl; OutputsCorrect=$taskCorrect; TimingQualified=$taskTiming
        Accepted=($taskCorrect -and $taskTiming); Failure=$taskFailure
    } | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'results.json') -Encoding utf8
}
try {
    $taskContext = Enter-MeasurementAffinity $LogicalProcessor
    Save-Evidence
    foreach ($taskRound in 0..$Repetitions) {
        foreach ($taskCase in @(Get-MeasurementOrder $taskCases $taskRound)) {
            $taskProcess = Invoke-MeasurementProcess -Program $taskCase.Program -Arguments @($taskCase.Case, "$taskCount") `
                -WorkingDirectory $ArtifactDirectory -TimeoutSeconds $TimeoutSeconds -Label "$($taskCase.Case)/$($taskCase.Variant) [$taskRound]"
            $taskParsed = ConvertFrom-MeasurementCostOutput $taskProcess.Output
            $taskSeconds = $taskParsed.Seconds; $taskChecksum = $taskParsed.Checksum
            $taskStatus = $taskProcess.Status
            if ($taskStatus -eq 'passed') {
                if ($taskParsed.Status -ne 'passed') { $taskStatus = $taskParsed.Status }
                else {
                    if (!$taskCounters -and ($taskParsed.Allocations -ne 0 -or $taskParsed.RequestedBytes -ne 0)) {
                        $taskStatus = 'counter-mode-mismatch'
                    }
                    elseif ($taskGoldens.ContainsKey($taskCase.Case) -and $taskChecksum -ne $taskGoldens[$taskCase.Case]) {
                        $taskStatus = 'independent-checksum-mismatch'
                    } else {
                        if ($taskRound -eq 0 -and $taskCase.Variant -eq 'cpp') { $taskExpected[$taskCase.Case] = $taskChecksum }
                        if ($taskChecksum -ne $taskExpected[$taskCase.Case]) { $taskStatus = 'checksum-mismatch' }
                    }
                }
            }
            $taskRow = [pscustomobject]@{ Case=$taskCase.Case; Variant=$taskCase.Variant; Round=$taskRound
                Warmup=($taskRound -eq 0); Status=$taskStatus; Seconds=$taskSeconds; Checksum=$taskChecksum
                Allocations=if ($taskCounters) { $taskParsed.Allocations } else { $null }
                RequestedBytes=if ($taskCounters) { $taskParsed.RequestedBytes } else { $null }
                WallSeconds=$taskProcess.WallSeconds; CpuSeconds=$taskProcess.CpuSeconds; CpuToWall=$taskProcess.CpuToWall
                SystemBusyFraction=$taskProcess.SystemBusyFraction; Utc=$taskProcess.Utc
                ExitCode=$taskProcess.ExitCode; Error=$taskProcess.Error; Output=$taskProcess.Output }
            $taskSamples.Add($taskRow)
            Save-Evidence
            if ($taskStatus -ne 'passed') { throw "$($taskCase.Case)/$($taskCase.Variant): $taskStatus" }
        }
        Write-Host "Round $taskRound complete: $($taskCases.Count) checked executions (warm-up=$($taskRound -eq 0))"
    }
    $taskSummary = @(foreach ($taskCase in $taskCases) {
        $taskRows = @($taskSamples | Where-Object { $_.Case -eq $taskCase.Case -and $_.Variant -eq $taskCase.Variant -and !$_.Warmup })
        $taskStability = Get-MeasurementStability @($taskRows.Seconds) $Repetitions $MaxRelativeRange $MaxRelativeDrift
        [pscustomobject]@{ Case=$taskCase.Case; Variant=$taskCase.Variant; Timing=$taskStability
            Allocations=if ($taskCounters) { @($taskRows.Allocations | Sort-Object -Unique) } else { $null }
            RequestedBytes=if ($taskCounters) { @($taskRows.RequestedBytes | Sort-Object -Unique) } else { $null } }
    })
    $taskA = ($taskSummary | Where-Object { $_.Case -eq $taskKinds[0] -and $_.Variant -eq 'cpp' }).Timing.Median
    $taskB = ($taskSummary | Where-Object Variant -EQ 'cpp-control').Timing.Median
    $taskDifference = [Math]::Abs($taskA - $taskB) / (($taskA + $taskB) / 2)
    $taskControl = [pscustomobject]@{ SameExecutable=$true; RelativeMedianDifference=$taskDifference
        Accepted=($taskDifference -le $MaxRelativeDrift) }
} catch { $taskFailure = $_.Exception.Message }
finally {
    if ($taskContext) {
        try { Exit-MeasurementAffinity $taskContext }
        catch { $taskFailure = "Affinity restoration: $($_.Exception.Message)" }
    }
    $taskEnvironmentAfter = Get-MeasurementEnvironment
    try {
        $taskAfter = @(Get-MeasurementArtifacts $taskPaths)
        $taskImmutable = Test-MeasurementArtifacts $taskBefore $taskAfter
    } catch { $taskFailure = "Artifact verification: $($_.Exception.Message)" }
    $taskPowerUnchanged = $taskEnvironmentBefore.PowerQueryExit -eq 0 -and $taskEnvironmentAfter.PowerQueryExit -eq 0 -and
        $taskEnvironmentBefore.PowerScheme -eq $taskEnvironmentAfter.PowerScheme
    $taskCorrect = !$taskFailure -and $taskImmutable -and $taskSamples.Count -eq $taskCases.Count * ($Repetitions + 1) -and
        @($taskSamples | Where-Object Status -NE 'passed').Count -eq 0
    $taskTiming = $taskCorrect -and $taskPowerUnchanged -and $taskControl -and $taskControl.Accepted -and
        @($taskSummary | Where-Object { !$_.Timing.Accepted }).Count -eq 0
    Save-Evidence
    $taskSummary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'summary.json') -Encoding utf8
}
$taskSummary | Select-Object Case, Variant, @{Name='MedianMs'; Expression={1000*$_.Timing.Median}},
    @{Name='Stable'; Expression={$_.Timing.Accepted}} | Format-Table -AutoSize
if (!$taskTiming) { throw "Timing NOT qualified (OutputsCorrect=$taskCorrect). All evidence retained: $OutputDirectory/results.json; $taskFailure" }
Write-Host 'Timing qualified within coarse declared limits; not a precise cross-session performance guarantee.'

param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [int]$Repetitions = 15,
    [int]$Count = 2000,
    [int]$TimeoutSeconds = 120,
    [string]$CppCompiler = 'g++',
    [ValidateSet('core', 'extended')][string]$Suite = 'core',
    [switch]$DisableAllocationCounters
)

$ErrorActionPreference = 'Stop'
if ($Repetitions -lt 3 -or $Count -lt 1 -or $Count -gt 100000 -or $TimeoutSeconds -lt 1) {
    throw 'Require >=3 samples, count in [1, 100000], and a positive timeout.'
}
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Choose a fresh output directory.' }
$taskCpp = (Get-Command $CppCompiler).Source
$taskWio = Join-Path $taskRoot 'build/app/Debug/wio.exe'
$taskRuntime = Join-Path $taskRoot 'build/runtime/backend/libwio_runtime.a'
$taskFlags = @('-std=c++20', '-O2', '-ffp-contract=off', '-static', '-Wa,-mbig-obj')
if ($DisableAllocationCounters) { $taskFlags += '-DWIO_COST_NO_COUNTERS' }
$taskKinds = @('concat', 'builder', 'format', 'bytes', 'array', 'component', 'object', 'native_call', 'result')
$taskFixtureRoot = $PSScriptRoot
$taskFixtureFiles = @('costs.wio', 'reference.cpp')
if ($Suite -eq 'extended') {
    $taskFixtureRoot = Join-Path $PSScriptRoot 'extended'
    $taskFixtureFiles += 'workloads.h'
    $taskKinds = @('unicode', 'dictionary', 'result_error', 'weak', 'closure', 'async_ready')
}
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'probe.h') -Destination $OutputDirectory
foreach ($taskFile in $taskFixtureFiles) {
    Copy-Item -LiteralPath (Join-Path $taskFixtureRoot $taskFile) -Destination $OutputDirectory
}
function Build([string]$program, [string[]]$arguments) {
    & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $program ($LASTEXITCODE)" }
}
$taskPriorRoot = $env:WIO_ROOT
try {
    $env:WIO_ROOT = $taskRoot
    Build $taskWio @((Join-Path $OutputDirectory 'costs.wio'), '--emit-cpp', '--include-dir', $OutputDirectory)
} finally { $env:WIO_ROOT = $taskPriorRoot }
foreach ($taskVariant in @('cpp', 'wio')) {
    $taskSource = if ($taskVariant -eq 'cpp') { 'reference.cpp' } else { 'costs.wio.cpp' }
    $taskArguments = $taskFlags + @((Join-Path $OutputDirectory $taskSource), "-I$OutputDirectory",
        "-I$taskRoot/runtime/include", "-I$taskRoot/sdk/include")
    if ($taskVariant -eq 'wio' -or $Suite -eq 'extended') {
        $taskArguments += @($taskRuntime, '-lws2_32', '-lshell32')
    }
    Build $taskCpp ($taskArguments + @('-o', (Join-Path $OutputDirectory "$taskVariant.exe")))
}

$taskSamples = [Collections.Generic.List[object]]::new()
$taskExpected = @{}
function SaveEvidence {
    $taskFiles = $taskFixtureFiles + @('probe.h', 'costs.wio.cpp', 'cpp.exe', 'wio.exe')
    $taskArtifacts = foreach ($taskFile in $taskFiles) {
        $taskItem = Get-Item -LiteralPath (Join-Path $OutputDirectory $taskFile)
        [pscustomobject]@{Path=$taskFile; Bytes=$taskItem.Length; Hash=(Get-FileHash $taskItem.FullName).Hash}
    }
    $taskSourcePaths = @('runtime/include/ref.h', 'runtime/include/integer_format.h',
        'runtime/include/text.h', 'runtime/include/std_unicode.h', 'runtime/include/std_async.h',
        'runtime/include/wir_async.h',
        'compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp',
        'compiler/src/wio_compiler/codegen/wir_cpp_storage_plan.cpp')
    $taskSourcePaths += @(Get-ChildItem -LiteralPath (Join-Path $taskRoot 'std') -File -Filter '*.wio' |
        Sort-Object Name | ForEach-Object { 'std/' + $_.Name })
    $taskSources = foreach ($taskFile in $taskSourcePaths) {
        [pscustomobject]@{Path=$taskFile; Hash=(Get-FileHash (Join-Path $taskRoot $taskFile)).Hash}
    }
    [pscustomobject]@{
        Revision=(& git -C $taskRoot rev-parse HEAD).Trim(); Utc=[DateTime]::UtcNow.ToString('o')
        OS=[Environment]::OSVersion.VersionString; Processor=$env:PROCESSOR_IDENTIFIER
        LogicalProcessors=$env:NUMBER_OF_PROCESSORS; Affinity='not pinned'
        Worktree=(@(& git -C $taskRoot status --short) -join "`n")
        Compiler=(& $taskCpp --version | Select-Object -First 1); Flags=$taskFlags
        CompilerHash=(Get-FileHash $taskWio).Hash; RuntimeHash=(Get-FileHash $taskRuntime).Hash
        RunnerHash=(Get-FileHash $PSCommandPath).Hash; Suite=$Suite
        CountersEnabled=(!$DisableAllocationCounters)
        Count=$Count; Repetitions=$Repetitions; TimeoutSeconds=$TimeoutSeconds
        Scope=if ($DisableAllocationCounters) { 'counter-free timing; standard allocation operators; allocation metrics unmeasured' }
            else { 'instrumented ordinary new/new[] calls and requested bytes; not malloc/aligned/live bytes/RSS' }
        ChecksumOracle='C++ excluded warm-up; all Wio/C++ samples compared; not an independent algorithm oracle'
        Artifacts=@($taskArtifacts); Sources=@($taskSources); Samples=$taskSamples.ToArray()
    } | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'results.json') -Encoding utf8
}

foreach ($taskKind in $taskKinds) {
    foreach ($taskRound in 0..$Repetitions) {
        # C++ goes first only for the checksum-oracle warm-up; measured pairs alternate.
        $taskOrder = if ($taskRound % 2 -eq 0) { @('cpp', 'wio') } else { @('wio', 'cpp') }
        foreach ($taskVariant in $taskOrder) {
            $taskInfo = [Diagnostics.ProcessStartInfo]::new()
            $taskInfo.FileName = Join-Path $OutputDirectory "$taskVariant.exe"
            $taskInfo.WorkingDirectory = $OutputDirectory
            $taskInfo.UseShellExecute = $false
            $taskInfo.CreateNoWindow = $true
            $taskInfo.RedirectStandardOutput = $true
            $taskInfo.RedirectStandardError = $true
            $taskInfo.ArgumentList.Add($taskKind)
            $taskInfo.ArgumentList.Add("$Count")
            $taskWatch = [Diagnostics.Stopwatch]::StartNew()
            $taskProcess = [Diagnostics.Process]::Start($taskInfo)
            $taskOutput = $taskProcess.StandardOutput.ReadToEndAsync()
            $taskError = $taskProcess.StandardError.ReadToEndAsync()
            $taskPassed = $taskProcess.WaitForExit($TimeoutSeconds * 1000)
            if (!$taskPassed) {
                $taskProcess.Kill($true)
                $taskProcess.WaitForExit()
            }
            $taskWatch.Stop()
            $taskMatch = [regex]::Match($taskOutput.Result.Trim(),
                '^seconds=(\d+\.\d+) checksum=(\d+) allocations=(\d+) requested_bytes=(\d+)$')
            $taskStatus = if (!$taskPassed) { 'timeout' } elseif ($taskProcess.ExitCode -ne 0 -or !$taskMatch.Success) {
                'failed'
            } else { 'passed' }
            if ($taskStatus -eq 'passed') {
                $taskChecksum = $taskMatch.Groups[2].Value
                if ($taskRound -eq 0 -and $taskVariant -eq 'cpp') { $taskExpected[$taskKind] = $taskChecksum }
                if ($taskChecksum -ne $taskExpected[$taskKind]) { $taskStatus = 'checksum-mismatch' }
            }
            $taskRow = [pscustomobject]@{
                Case=$taskKind; Variant=$taskVariant; Round=$taskRound; Warmup=($taskRound -eq 0); Status=$taskStatus
                WallSeconds=$taskWatch.Elapsed.TotalSeconds
                Seconds=if ($taskStatus -eq 'passed') {
                    [double]::Parse($taskMatch.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
                } else { $null }
                Checksum=if ($taskMatch.Success) { $taskMatch.Groups[2].Value } else { $null }
                Allocations=if ($taskMatch.Success -and !$DisableAllocationCounters) { [long]$taskMatch.Groups[3].Value } else { $null }
                RequestedBytes=if ($taskMatch.Success -and !$DisableAllocationCounters) { [long]$taskMatch.Groups[4].Value } else { $null }
                Error=$taskError.Result; Output=$taskOutput.Result
            }
            $taskSamples.Add($taskRow)
            $taskProcess.Dispose()
            if ($taskStatus -ne 'passed') {
                SaveEvidence
                throw "$taskKind/$taskVariant failed: $taskStatus; $($taskRow.Error)"
            }
        }
    }
    Write-Host "Qualified $taskKind ($Repetitions measured pairs + excluded warm-up)"
}
SaveEvidence
$taskSummary = foreach ($taskKind in $taskKinds) {
    foreach ($taskVariant in @('cpp', 'wio')) {
        $taskRows = @($taskSamples | Where-Object { $_.Case -eq $taskKind -and $_.Variant -eq $taskVariant -and !$_.Warmup })
        $taskTimes = @($taskRows.Seconds | Sort-Object)
        $taskMiddle = [int][Math]::Floor($taskTimes.Count / 2)
        $taskMedian = if ($taskTimes.Count % 2) { $taskTimes[$taskMiddle] } else {
            ($taskTimes[$taskMiddle - 1] + $taskTimes[$taskMiddle]) / 2
        }
        [pscustomobject]@{Case=$taskKind; Variant=$taskVariant; MedianMs=1000*$taskMedian
            MinMs=1000*$taskTimes[0]; MaxMs=1000*$taskTimes[-1]
            Allocations=if ($DisableAllocationCounters) { 'not-measured' } else { ($taskRows.Allocations | Sort-Object -Unique) -join ',' }
            RequestedBytes=if ($DisableAllocationCounters) { 'not-measured' } else { ($taskRows.RequestedBytes | Sort-Object -Unique) -join ',' }}
    }
}
$taskSummary | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'summary.json') -Encoding utf8
$taskSummary | Format-Table -AutoSize

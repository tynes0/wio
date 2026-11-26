param(
    [string]$RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..')),
    [string]$BuildDirectory = '',
    [string]$CppCompiler = 'g++',
    [string]$DotNet = 'dotnet',
    [string]$Python = 'python',
    [int]$Repetitions = 5,
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
if ($Repetitions -lt 1 -or $TimeoutSeconds -lt 1) { throw 'Repetitions and timeout must be positive.' }
if ($BuildDirectory -eq '') { $BuildDirectory = Join-Path $RepositoryRoot 'build/mandelbrot-compute-audit' }
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
New-Item -ItemType Directory -Path $BuildDirectory -Force | Out-Null
$taskCpp = (Get-Command $CppCompiler -ErrorAction Stop).Source
$taskDotNet = (Get-Command $DotNet -ErrorAction Stop).Source
$taskPython = (Get-Command $Python -ErrorAction Stop).Source
$taskWio = Join-Path $RepositoryRoot 'build/app/Debug/wio.exe'
$taskRuntime = Join-Path $RepositoryRoot 'build/runtime/backend/libwio_runtime.a'
if (-not (Test-Path $taskWio) -or -not (Test-Path $taskRuntime)) {
    throw 'Build the Debug compiler and backend runtime first.'
}

function Invoke-BuildCommand {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $Program (exit $LASTEXITCODE)" }
}

# Emit in the scratch directory, never beside the versioned fixture.
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'compute.wio') -Destination (Join-Path $BuildDirectory 'compute.wio') -Force
$taskPreviousRoot = $env:WIO_ROOT
try {
    $env:WIO_ROOT = $RepositoryRoot
    Invoke-BuildCommand $taskWio @(
        (Join-Path $BuildDirectory 'compute.wio'), '--no-builtin', '--emit-cpp',
        '--include-dir', $PSScriptRoot, '--output', (Join-Path $BuildDirectory 'compute.exe')
    )
} finally {
    $env:WIO_ROOT = $taskPreviousRoot
}

foreach ($taskOptimization in @('0', '2')) {
    Invoke-BuildCommand $taskCpp @(
        '-std=c++20', "-O$taskOptimization", '-ffp-contract=off',
        (Join-Path $PSScriptRoot 'compute.cpp'), '-o', (Join-Path $BuildDirectory "cpp-o$taskOptimization.exe")
    )
    Invoke-BuildCommand $taskCpp @(
        '-std=c++20', "-O$taskOptimization", '-ffp-contract=off', '-static', '-Wa,-mbig-obj',
        (Join-Path $BuildDirectory 'compute.wio.cpp'), "-I$PSScriptRoot",
        "-I$(Join-Path $RepositoryRoot 'runtime/include')", "-I$(Join-Path $RepositoryRoot 'sdk/include')",
        $taskRuntime, '-lws2_32', '-lshell32', '-o', (Join-Path $BuildDirectory "wio-o$taskOptimization.exe")
    )
}
Invoke-BuildCommand $taskDotNet @(
    'build', (Join-Path $PSScriptRoot 'csharp/compute.csproj'), '-c', 'Release', '--nologo',
    '--ignore-failed-sources', '-p:NuGetAudit=false',
    "-p:BaseIntermediateOutputPath=$(Join-Path $BuildDirectory 'csharp-obj/')",
    '--output', (Join-Path $BuildDirectory 'csharp')
)

$taskCases = @(
    @{ Name = 'cpp-o0'; Program = Join-Path $BuildDirectory 'cpp-o0.exe'; Arguments = @() },
    @{ Name = 'cpp-o2'; Program = Join-Path $BuildDirectory 'cpp-o2.exe'; Arguments = @() },
    @{ Name = 'wio-o0'; Program = Join-Path $BuildDirectory 'wio-o0.exe'; Arguments = @() },
    @{ Name = 'wio-o2'; Program = Join-Path $BuildDirectory 'wio-o2.exe'; Arguments = @() },
    @{ Name = 'csharp-release'; Program = $taskDotNet; Arguments = @((Join-Path $BuildDirectory 'csharp/compute.dll')) },
    @{ Name = 'python'; Program = $taskPython; Arguments = @((Join-Path $PSScriptRoot 'compute.py')) }
)
$taskRecords = [Collections.Generic.List[object]]::new()
$taskExpected = '11613930/3721766287256/94440'
foreach ($taskRepetition in 1..$Repetitions) {
    foreach ($taskCase in $taskCases) {
        $taskInfo = [Diagnostics.ProcessStartInfo]::new()
        $taskInfo.FileName = $taskCase.Program
        $taskInfo.WorkingDirectory = $BuildDirectory
        $taskInfo.UseShellExecute = $false
        $taskInfo.CreateNoWindow = $true
        $taskInfo.RedirectStandardOutput = $true
        $taskInfo.RedirectStandardError = $true
        foreach ($taskArgument in $taskCase.Arguments) { $taskInfo.ArgumentList.Add($taskArgument) }
        $taskClock = [Diagnostics.Stopwatch]::StartNew()
        $taskProcess = [Diagnostics.Process]::Start($taskInfo)
        $taskStdout = $taskProcess.StandardOutput.ReadToEndAsync()
        $taskStderr = $taskProcess.StandardError.ReadToEndAsync()
        $taskNextNotice = 30
        while (-not $taskProcess.WaitForExit(1000)) {
            if ($taskClock.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
                $taskProcess.Kill($true)
                $taskProcess.WaitForExit()
                throw "$($taskCase.Name) exceeded $TimeoutSeconds seconds; no exact duration measured."
            }
            if ($taskClock.Elapsed.TotalSeconds -ge $taskNextNotice) {
                Write-Host "$($taskCase.Name) repetition ${taskRepetition}: still running ($taskNextNotice s)"
                $taskNextNotice += 30
            }
        }
        $taskClock.Stop()
        if ($taskProcess.ExitCode -ne 0) {
            throw "$($taskCase.Name) failed: $($taskStdout.Result) $($taskStderr.Result)"
        }
        $taskMatch = [regex]::Match($taskStdout.Result,
            'seconds=(\d+\.\d+) iterations=(\d+) weighted=(\d+) interior=(\d+)')
        if (-not $taskMatch.Success) { throw "Invalid benchmark output: $($taskStdout.Result)" }
        $taskFingerprint = "$($taskMatch.Groups[2].Value)/$($taskMatch.Groups[3].Value)/$($taskMatch.Groups[4].Value)"
        if ($taskFingerprint -ne $taskExpected) { throw "Checksum mismatch: $($taskCase.Name): $taskFingerprint != $taskExpected" }
        $taskRecord = [pscustomobject]@{
            Name = $taskCase.Name
            Repetition = $taskRepetition
            KernelSeconds = [double]::Parse($taskMatch.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
            WallSeconds = $taskClock.Elapsed.TotalSeconds
            Iterations = [uint64]$taskMatch.Groups[2].Value
            Weighted = [uint64]$taskMatch.Groups[3].Value
            Interior = [uint64]$taskMatch.Groups[4].Value
        }
        $taskRecords.Add($taskRecord)
        Write-Host "$($taskCase.Name) [$taskRepetition]: $($taskStdout.Result.Trim()) wall=$($taskRecord.WallSeconds)"
        $taskProcess.Dispose()
    }
}
$taskRecords | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $BuildDirectory 'measurements.json') -Encoding utf8

function Get-Median {
    param([double[]]$Values)
    $taskOrdered = @($Values | Sort-Object)
    $taskMiddle = [int][Math]::Floor($taskOrdered.Count / 2)
    if ($taskOrdered.Count % 2 -eq 1) { return $taskOrdered[$taskMiddle] }
    return ($taskOrdered[$taskMiddle - 1] + $taskOrdered[$taskMiddle]) / 2
}

$taskSummary = foreach ($taskCase in $taskCases) {
    $taskRows = @($taskRecords | Where-Object Name -EQ $taskCase.Name)
    $taskKernel = Get-Median @($taskRows.KernelSeconds)
    [pscustomobject]@{
        Name = $taskCase.Name
        KernelMedian = $taskKernel
        KernelMin = ($taskRows.KernelSeconds | Measure-Object -Minimum).Minimum
        KernelMax = ($taskRows.KernelSeconds | Measure-Object -Maximum).Maximum
        WallMedian = Get-Median @($taskRows.WallSeconds)
        NanosecondsPerStep = $taskKernel * 1e9 / $taskRows[0].Iterations
    }
}
$taskSummary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $BuildDirectory 'summary.json') -Encoding utf8
$taskSummary | Format-Table -AutoSize
Write-Host "All checksums match: $taskExpected. Results: $BuildDirectory"

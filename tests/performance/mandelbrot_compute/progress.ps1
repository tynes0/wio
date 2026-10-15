param(
    [string]$RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..')),
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$VmHost = '',
    [string]$FullSourceDirectory = '',
    [int]$Repetitions = 5,
    [int]$VmRepetitions = 1,
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
if ($Repetitions -lt 3 -or $VmRepetitions -lt 1 -or $TimeoutSeconds -lt 1) {
    throw 'Require at least three native samples, one VM sample and a positive timeout.'
}
$taskRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Choose a fresh output directory.' }
if (!$VmHost) { $VmHost = Join-Path $taskRoot 'build/vm/Release/wio_mandelbrot_vm_benchmark.exe' }
$VmHost = [IO.Path]::GetFullPath($VmHost)
if (!(Test-Path -LiteralPath $VmHost)) { throw 'Build the Release wio_mandelbrot_vm_benchmark target first.' }
$taskCompiler = Join-Path $taskRoot 'build/app/Debug/wio.exe'
$taskCpp = (Get-Command g++).Source
$taskRuntime = Join-Path $taskRoot 'build/runtime/backend/libwio_runtime.a'
$taskFlags = @('-std=c++20', '-O2', '-ffp-contract=off', '-static', '-Wa,-mbig-obj')
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
function Build([string]$program, [string[]]$arguments) {
    & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $program ($LASTEXITCODE)" }
}
$taskCases = [Collections.Generic.List[object]]::new()
$taskArtifacts = [Collections.Generic.List[object]]::new()
foreach ($taskKind in @('compute', 'full')) {
    if ($taskKind -eq 'full' -and !$FullSourceDirectory) { continue }
    $taskDirectory = Join-Path $OutputDirectory $taskKind
    New-Item -ItemType Directory -Path $taskDirectory | Out-Null
    if ($taskKind -eq 'compute') {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'compute.wio') -Destination (Join-Path $taskDirectory 'main.wio')
        $taskExtra = @('--no-builtin', '--include-dir', $PSScriptRoot)
    } else {
        foreach ($taskSource in @('main.wio', 'mandelbrot.wio')) {
            Copy-Item -LiteralPath (Join-Path $FullSourceDirectory $taskSource) -Destination $taskDirectory
        }
        $taskExtra = @()
    }
    $taskPriorRoot = $env:WIO_ROOT
    try {
        $env:WIO_ROOT = $taskRoot
        Build $taskCompiler (@((Join-Path $taskDirectory 'main.wio'), '--emit-cpp') + $taskExtra)
        Build $taskCompiler (@((Join-Path $taskDirectory 'main.wio'), '--target', 'shared', '--native-bridge-only',
            '--output', (Join-Path $taskDirectory 'bridge.dll'), '--bytecode-sidecar',
            (Join-Path $taskDirectory 'main.wiob')) + $taskExtra)
    } finally { $env:WIO_ROOT = $taskPriorRoot }
    $taskNative = Join-Path $taskDirectory 'native.exe'
    $taskCompile = $taskFlags + @((Join-Path $taskDirectory 'main.wio.cpp'), "-I$taskRoot/runtime/include",
        "-I$taskRoot/sdk/include", "-I$PSScriptRoot", $taskRuntime, '-lws2_32', '-lshell32')
    Build $taskCpp ($taskCompile + @('-o', $taskNative))
    $taskCases.Add(@{ Name="$taskKind-native"; Program=$taskNative; Arguments=@(); Directory=$taskDirectory
        Kind=$taskKind; Count=$Repetitions; Warmup=$true })
    if ($taskKind -eq 'full' -and (Test-Path -LiteralPath (Join-Path $FullSourceDirectory 'main.wio.cpp'))) {
        Copy-Item -LiteralPath (Join-Path $FullSourceDirectory 'main.wio.cpp') `
            -Destination (Join-Path $taskDirectory 'previous.wio.cpp')
        $taskPrevious = Join-Path $taskDirectory 'previous.exe'
        Build $taskCpp ($taskFlags + @((Join-Path $taskDirectory 'previous.wio.cpp'), "-I$taskRoot/runtime/include",
            "-I$taskRoot/sdk/include", $taskRuntime, '-lws2_32', '-lshell32', '-o', $taskPrevious))
        $taskCases.Add(@{ Name='full-previous'; Program=$taskPrevious; Arguments=@(); Directory=$taskDirectory
            Kind=$taskKind; Count=$Repetitions; Warmup=$true })
    }
    $taskCases.Add(@{ Name="$taskKind-vm"; Program=$VmHost;
        Arguments=@((Join-Path $taskDirectory 'main.wiob'), (Join-Path $taskDirectory 'bridge.dll'))
        Directory=$taskDirectory; Kind=$taskKind; Count=$VmRepetitions; Warmup=$false })
    foreach ($taskFile in Get-ChildItem -LiteralPath $taskDirectory -File) {
        $taskArtifacts.Add([pscustomobject]@{Path="$taskKind/$($taskFile.Name)"; Hash=(Get-FileHash $taskFile.FullName).Hash})
    }
}
$taskSamples = [Collections.Generic.List[object]]::new()
foreach ($taskRound in 0..([Math]::Max($Repetitions, $VmRepetitions))) {
    foreach ($taskCase in $taskCases) {
        if (($taskRound -eq 0 -and !$taskCase.Warmup) -or $taskRound -gt $taskCase.Count) { continue }
        $taskInfo = [Diagnostics.ProcessStartInfo]::new()
        $taskInfo.FileName = $taskCase.Program
        $taskInfo.WorkingDirectory = $taskCase.Directory
        $taskInfo.UseShellExecute = $false
        $taskInfo.CreateNoWindow = $true
        $taskInfo.RedirectStandardOutput = $true
        $taskInfo.RedirectStandardError = $true
        foreach ($taskArgument in $taskCase.Arguments) { $taskInfo.ArgumentList.Add($taskArgument) }
        # Never validate an old output left by another variant or repetition.
        $taskOutputPath = Join-Path $taskCase.Directory 'mandelbrot.ppm'
        $taskPriorStamp = if (Test-Path -LiteralPath $taskOutputPath) { (Get-Item $taskOutputPath).LastWriteTimeUtc } else { $null }
        $taskWatch = [Diagnostics.Stopwatch]::StartNew()
        $taskProcess = [Diagnostics.Process]::Start($taskInfo)
        $taskStdout = $taskProcess.StandardOutput.ReadToEndAsync()
        $taskStderr = $taskProcess.StandardError.ReadToEndAsync()
        $taskTimeout = $false
        $taskNextNotice = 30
        while (!$taskProcess.WaitForExit(1000)) {
            if ($taskWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
                $taskProcess.Kill($true)
                $taskProcess.WaitForExit()
                $taskTimeout = $true
                break
            }
            if ($taskWatch.Elapsed.TotalSeconds -ge $taskNextNotice) {
                Write-Host "$($taskCase.Name) round ${taskRound}: running ($taskNextNotice s)"
                $taskNextNotice += 30
            }
        }
        $taskWatch.Stop()
        $taskRow = [ordered]@{ Name=$taskCase.Name; Round=$taskRound; Warmup=($taskRound -eq 0)
            Status='passed'; WallSeconds=$taskWatch.Elapsed.TotalSeconds; KernelSeconds=$null
            Output=$taskStdout.Result.Trim(); Error=$taskStderr.Result.Trim() }
        if ($taskTimeout) { $taskRow.Status='timeout'; $taskRow.TimeoutSeconds=$TimeoutSeconds }
        elseif ($taskProcess.ExitCode -ne 0) { throw "Benchmark failed: $($taskCase.Name): $($taskRow.Error) $($taskRow.Output)" }
        elseif ($taskCase.Kind -eq 'compute') {
            $taskMatch = [regex]::Match($taskRow.Output,
                'seconds=(\d+\.\d+) iterations=11613930 weighted=3721766287256 interior=94440')
            if (!$taskMatch.Success) { throw "Compute checksum failed: $($taskCase.Name): $($taskRow.Output)" }
            $taskRow.KernelSeconds = [double]::Parse($taskMatch.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
        } else {
            if (!(Test-Path -LiteralPath $taskOutputPath) -or
                ($taskPriorStamp -and (Get-Item $taskOutputPath).LastWriteTimeUtc -eq $taskPriorStamp)) {
                throw "Full render did not produce a fresh PPM: $($taskCase.Name)"
            }
            $taskRow.Bytes = (Get-Item $taskOutputPath).Length
            $taskRow.Hash = (Get-FileHash $taskOutputPath).Hash
            if ($taskRow.Bytes -ne 5156528 -or
                $taskRow.Hash -ne '1984A4F3A3575BD438EBD79F6EC3D0EB571BD141E5A9CBD29DFABA37941FF733') {
                throw "Full render checksum failed: $($taskCase.Name)"
            }
        }
        $taskSamples.Add([pscustomobject]$taskRow)
        Write-Host "$($taskCase.Name) [$taskRound] $($taskRow.Status): kernel=$($taskRow.KernelSeconds) wall=$($taskRow.WallSeconds)"
        $taskProcess.Dispose()
    }
}
$taskSummary = foreach ($taskCase in $taskCases) {
    $taskRows = @($taskSamples | Where-Object { $_.Name -eq $taskCase.Name -and !$_.Warmup })
    $taskPassed = @($taskRows | Where-Object Status -EQ 'passed')
    $taskMetric = if ($taskCase.Kind -eq 'compute') { 'KernelSeconds' } else { 'WallSeconds' }
    $taskNumbers = @($taskPassed | ForEach-Object $taskMetric | Sort-Object)
    [pscustomobject]@{ Name=$taskCase.Name; Samples=$taskRows.Count; Passed=$taskPassed.Count; Metric=$taskMetric
        Median=if ($taskNumbers.Count) {
            $taskMiddle = [int][Math]::Floor($taskNumbers.Count/2)
            if ($taskNumbers.Count % 2) { $taskNumbers[$taskMiddle] }
            else { ($taskNumbers[$taskMiddle - 1] + $taskNumbers[$taskMiddle]) / 2 }
        } else { $null }
        Min=if ($taskNumbers.Count) { $taskNumbers[0] } else { $null }
        Max=if ($taskNumbers.Count) { $taskNumbers[-1] } else { $null } }
}
$taskSources = foreach ($taskSource in @('tests/performance/mandelbrot_compute/compute.wio',
    'tests/performance/mandelbrot_compute/benchmark_clock.h', 'tests/performance/mandelbrot_compute/vm_host.cpp',
    'runtime/include/integer_format.h', 'compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp',
    'compiler/src/wio_compiler/codegen/wir_cpp_storage_plan.cpp',
    'compiler/include/wio/codegen/wir_cpp_storage_plan.h')) {
    [pscustomobject]@{ Path=$taskSource; Hash=(Get-FileHash (Join-Path $taskRoot $taskSource)).Hash }
}
[pscustomobject]@{
    Revision=(& git -C $taskRoot rev-parse HEAD).Trim(); Compiler=(& $taskCpp --version | Select-Object -First 1)
    VmConfiguration='MSVC Release, non-JIT, 10 billion instruction benchmark-only budget'
    CompilerHash=(Get-FileHash $taskCompiler).Hash; VmHostHash=(Get-FileHash $VmHost).Hash
    RuntimeHash=(Get-FileHash $taskRuntime).Hash; Flags=$taskFlags; TimeoutSeconds=$TimeoutSeconds
    Repetitions=$Repetitions; VmRepetitions=$VmRepetitions; Sources=@($taskSources)
    Artifacts=$taskArtifacts.ToArray(); Samples=$taskSamples.ToArray(); Summary=@($taskSummary)
} | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'results.json') -Encoding utf8
$taskSummary | Format-List

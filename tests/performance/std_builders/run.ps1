param(
    [string]$RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..')),
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][string]$BaselineDirectory,
    [string]$PreviousDirectory = '',
    [string]$CppCompiler = 'g++',
    [int]$Repetitions = 15
)

$ErrorActionPreference = 'Stop'
if ($Repetitions -lt 3) { throw 'At least three repetitions are required.' }
$taskRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Choose a fresh measurement directory.' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$taskCompiler = (Get-Command $CppCompiler).Source
$taskWio = Join-Path $taskRoot 'build/app/Debug/wio.exe'
$taskRuntime = Join-Path $taskRoot 'build/runtime/backend/libwio_runtime.a'
function Build([string]$program, [string[]]$arguments) {
    & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $program ($LASTEXITCODE)" }
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'append.wio') -Destination $OutputDirectory
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'append.cpp') -Destination $OutputDirectory
Copy-Item -LiteralPath (Join-Path ([IO.Path]::GetFullPath($BaselineDirectory)) 'append.wio.cpp') `
    -Destination (Join-Path $OutputDirectory 'baseline.wio.cpp')
$taskPriorRoot = $env:WIO_ROOT
try {
    $env:WIO_ROOT = $taskRoot
    Build $taskWio @((Join-Path $OutputDirectory 'append.wio'), '--emit-cpp',
        '--include-dir', (Join-Path $taskRoot 'tests/native'), '--output', (Join-Path $OutputDirectory 'current.exe'))
} finally {
    $env:WIO_ROOT = $taskPriorRoot
}
$taskFlags = @('-std=c++20', '-O2', '-ffp-contract=off', '-static')
$taskCases = @(
    @{Name='baseline'; Source='baseline.wio.cpp'}
)
if ($PreviousDirectory) {
    Copy-Item -LiteralPath (Join-Path ([IO.Path]::GetFullPath($PreviousDirectory)) 'append.wio.cpp') `
        -Destination (Join-Path $OutputDirectory 'previous.wio.cpp')
    $taskCases += @{Name='previous'; Source='previous.wio.cpp'}
}
$taskCases += @(
    @{Name='current'; Source='append.wio.cpp'},
    @{Name='cpp'; Source='append.cpp'}
)
foreach ($taskCase in $taskCases) {
    $taskCase.Path = Join-Path $OutputDirectory "$($taskCase.Name).exe"
    $taskArguments = $taskFlags + @('-Wa,-mbig-obj', (Join-Path $OutputDirectory $taskCase.Source),
        "-I$taskRoot/tests/native", "-I$taskRoot/runtime/include", "-I$taskRoot/sdk/include")
    if ($taskCase.Name -ne 'cpp') { $taskArguments += @($taskRuntime, '-lws2_32', '-lshell32') }
    Build $taskCompiler ($taskArguments + @('-o', $taskCase.Path))
}
$taskSamples = [Collections.Generic.List[object]]::new()
foreach ($taskRound in 0..$Repetitions) {
    foreach ($taskCase in $taskCases) {
        $taskWatch = [Diagnostics.Stopwatch]::StartNew()
        $taskOutput = & $taskCase.Path
        $taskWatch.Stop()
        if ($LASTEXITCODE -ne 0) { throw "Execution failed: $($taskCase.Name)" }
        $taskMatch = [regex]::Match(($taskOutput -join "`n"),
            'seconds=(\d+\.\d+) bytes=(\d+) hash=(\d+) allocations=(\d+)')
        if (!$taskMatch.Success -or $taskMatch.Groups[2].Value -ne '960000' -or
            $taskMatch.Groups[3].Value -ne '2058931307218008101') { throw "Checksum mismatch: $taskOutput" }
        $taskSamples.Add([pscustomobject]@{
            Name=$taskCase.Name; Round=$taskRound; Warmup=($taskRound -eq 0)
            Seconds=[double]::Parse($taskMatch.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
            WallSeconds=$taskWatch.Elapsed.TotalSeconds; Allocations=[long]$taskMatch.Groups[4].Value
        })
    }
}
$taskSummary = foreach ($taskCase in $taskCases) {
    $taskValues = @($taskSamples | Where-Object { $_.Name -eq $taskCase.Name -and !$_.Warmup } |
        ForEach-Object Seconds | Sort-Object)
    $taskMiddle = [int][Math]::Floor($taskValues.Count / 2)
    $taskMedian = if ($taskValues.Count % 2) { $taskValues[$taskMiddle] } else {
        ($taskValues[$taskMiddle - 1] + $taskValues[$taskMiddle]) / 2
    }
    [pscustomobject]@{Name=$taskCase.Name; Median=$taskMedian; Min=$taskValues[0]; Max=$taskValues[-1]}
}
$taskSources = foreach ($taskSource in @('std/builders.wio', 'std/binary.wio', 'runtime/include/std_builders.h',
    'runtime/include/wir_inline_place.h', 'compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp',
    'compiler/src/wio_compiler/codegen/wir_cpp_storage_plan.cpp',
    'vm/src/string_intrinsics.cpp', 'vm/src/value.cpp', 'tests/native/builder_benchmark.h',
    'tests/performance/std_builders/append.wio', 'tests/performance/std_builders/append.cpp')) {
    [pscustomobject]@{Path=$taskSource; Hash=(Get-FileHash -LiteralPath (Join-Path $taskRoot $taskSource)).Hash}
}
$taskArtifacts = foreach ($taskCase in $taskCases) {
    [pscustomobject]@{
        Name=$taskCase.Name
        GeneratedHash=(Get-FileHash -LiteralPath (Join-Path $OutputDirectory $taskCase.Source)).Hash
        BinaryHash=(Get-FileHash -LiteralPath $taskCase.Path).Hash
    }
}
[pscustomobject]@{
    Revision=(& git -C $taskRoot rev-parse HEAD).Trim()
    Compiler=(& $taskCompiler --version | Select-Object -First 1); Flags=$taskFlags; Repetitions=$Repetitions
    CompilerHash=(Get-FileHash -LiteralPath $taskWio).Hash; RuntimeHash=(Get-FileHash -LiteralPath $taskRuntime).Hash
    Sources=@($taskSources); Artifacts=@($taskArtifacts); Samples=$taskSamples.ToArray(); Summary=@($taskSummary)
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'results.json') -Encoding utf8
$taskSummary | Format-List
Write-Host "Checksums passed: $OutputDirectory"

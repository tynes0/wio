param(
    [string]$RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..')),
    [string]$OutputDirectory = '',
    [string]$BaselineDirectory = '',
    [string]$PreviousDirectory = '',
    [string]$CppCompiler = 'g++',
    [int]$Repetitions = 9
)

$ErrorActionPreference = 'Stop'
if ($Repetitions -lt 3) { throw 'At least three repetitions are required.' }
if (!$OutputDirectory) { throw 'Choose a fresh OutputDirectory to preserve previous measurements.' }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'OutputDirectory must be fresh.' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$taskRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$taskFixture = Join-Path $taskRoot 'tests/performance/mandelbrot_compute'
$taskCompiler = (Get-Command $CppCompiler -ErrorAction Stop).Source
$taskWio = Join-Path $taskRoot 'build/app/Debug/wio.exe'
$taskRuntime = Join-Path $taskRoot 'build/runtime/backend/libwio_runtime.a'
$taskVersion = (& $taskCompiler --version | Select-Object -First 1)
$taskRevision = (& git -C $taskRoot rev-parse HEAD).Trim()
Copy-Item -LiteralPath (Join-Path $taskFixture 'compute.wio') -Destination $OutputDirectory
Copy-Item -LiteralPath (Join-Path $taskFixture 'compute.cpp') -Destination $OutputDirectory
Copy-Item -LiteralPath (Join-Path $taskFixture 'benchmark_clock.h') -Destination $OutputDirectory
function Build([string]$program, [string[]]$arguments) {
    & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $program ($LASTEXITCODE)" }
}
$taskPriorRoot = $env:WIO_ROOT
try {
    $env:WIO_ROOT = $taskRoot
    Build $taskWio @((Join-Path $OutputDirectory 'compute.wio'), '--no-builtin', '--emit-cpp',
        '--include-dir', $OutputDirectory, '--output', (Join-Path $OutputDirectory 'wio.exe'))
} finally {
    $env:WIO_ROOT = $taskPriorRoot
}
$taskFlags = @('-std=c++20', '-O2', '-ffp-contract=off', '-static')
Build $taskCompiler ($taskFlags + @((Join-Path $OutputDirectory 'compute.cpp'), '-o', (Join-Path $OutputDirectory 'cpp.exe')))
Build $taskCompiler ($taskFlags + @('-Wa,-mbig-obj', (Join-Path $OutputDirectory 'compute.wio.cpp'),
    "-I$OutputDirectory", "-I$taskRoot/runtime/include", "-I$taskRoot/sdk/include", $taskRuntime,
    '-lws2_32', '-lshell32', '-o', (Join-Path $OutputDirectory 'wio.exe')))
$taskCases = @(
    @{Name='cpp'; Path=(Join-Path $OutputDirectory 'cpp.exe')},
    @{Name='wio'; Path=(Join-Path $OutputDirectory 'wio.exe')}
)
foreach ($taskSnapshot in @(
    @{ Name = 'baseline'; Directory = $BaselineDirectory },
    @{ Name = 'previous'; Directory = $PreviousDirectory }
)) {
    if (!$taskSnapshot.Directory) { continue }
    $taskSnapshotCpp = Join-Path ([IO.Path]::GetFullPath($taskSnapshot.Directory)) 'compute.wio.cpp'
    # Recompile the saved pre-change generated code against the exact same
    # toolchain, headers, flags and runtime as the current generated output.
    $taskSnapshotSource = Join-Path $OutputDirectory "$($taskSnapshot.Name).wio.cpp"
    $taskSnapshotBinary = Join-Path $OutputDirectory "$($taskSnapshot.Name).exe"
    Copy-Item -LiteralPath $taskSnapshotCpp -Destination $taskSnapshotSource
    Build $taskCompiler ($taskFlags + @('-Wa,-mbig-obj', $taskSnapshotSource,
        "-I$OutputDirectory", "-I$taskRoot/runtime/include", "-I$taskRoot/sdk/include", $taskRuntime,
        '-lws2_32', '-lshell32', '-o', $taskSnapshotBinary))
    $taskCases += @{Name=$taskSnapshot.Name; Path=$taskSnapshotBinary}
}
$taskSamples = [Collections.Generic.List[object]]::new()
foreach ($taskRound in 0..$Repetitions) {
    # Round zero warms code/data paths and is retained but excluded from medians.
    foreach ($taskCase in $taskCases) {
        $taskWatch = [Diagnostics.Stopwatch]::StartNew()
        $taskText = & $taskCase.Path
        $taskWatch.Stop()
        if ($LASTEXITCODE -ne 0) { throw "Benchmark failed: $($taskCase.Path)" }
        $taskMatch = [regex]::Match(($taskText -join "`n"),
            'seconds=(\d+\.\d+) iterations=(\d+) weighted=(\d+) interior=(\d+)')
        if (!$taskMatch.Success -or $taskMatch.Groups[2].Value -ne '11613930' -or
            $taskMatch.Groups[3].Value -ne '3721766287256' -or $taskMatch.Groups[4].Value -ne '94440') {
            throw "Checksum/report mismatch: $taskText"
        }
        $taskSamples.Add([pscustomobject]@{
            Name=$taskCase.Name; Round=$taskRound; Warmup=($taskRound -eq 0)
            Seconds=[double]::Parse($taskMatch.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
            WallSeconds=$taskWatch.Elapsed.TotalSeconds
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
$taskGenerated = Join-Path $OutputDirectory 'compute.wio.cpp'
$taskSources = foreach ($taskSource in @(
    'compiler/src/wio_compiler/codegen/wir_cpp_backend.cpp',
    'compiler/src/wio_compiler/codegen/wir_cpp_storage_plan.cpp',
    'compiler/include/wio/codegen/wir_cpp_storage_plan.h'
)) {
    [pscustomobject]@{
        Path=$taskSource
        Hash=(Get-FileHash -LiteralPath (Join-Path $taskRoot $taskSource) -Algorithm SHA256).Hash
    }
}
$taskArtifacts = foreach ($taskCase in $taskCases) {
    $taskSourceName = switch ($taskCase.Name) {
        'cpp' { 'compute.cpp' }
        'wio' { 'compute.wio.cpp' }
        default { "$($taskCase.Name).wio.cpp" }
    }
    [pscustomobject]@{
        Name=$taskCase.Name
        GeneratedHash=(Get-FileHash -LiteralPath (Join-Path $OutputDirectory $taskSourceName) -Algorithm SHA256).Hash
        BinaryHash=(Get-FileHash -LiteralPath $taskCase.Path -Algorithm SHA256).Hash
    }
}
[pscustomobject]@{
    Revision=$taskRevision; Compiler=$taskVersion; Flags=$taskFlags; Repetitions=$Repetitions
    CompilerHash=(Get-FileHash -LiteralPath $taskWio -Algorithm SHA256).Hash
    RuntimeHash=(Get-FileHash -LiteralPath $taskRuntime -Algorithm SHA256).Hash
    FixtureHash=(Get-FileHash -LiteralPath (Join-Path $taskFixture 'compute.wio') -Algorithm SHA256).Hash
    GeneratedBytes=(Get-Item -LiteralPath $taskGenerated).Length
    GeneratedLines=([IO.File]::ReadLines($taskGenerated) | Measure-Object).Count
    BinaryBytes=(Get-Item -LiteralPath (Join-Path $OutputDirectory 'wio.exe')).Length
    Sources=@($taskSources); Artifacts=@($taskArtifacts)
    Samples=$taskSamples.ToArray(); Summary=@($taskSummary)
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'results.json') -Encoding utf8
$taskSummary | Format-Table -AutoSize
Write-Host "Checksums passed. Measurements: $OutputDirectory"

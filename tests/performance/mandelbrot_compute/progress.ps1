param(
    [string]$RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..')),
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$VmHost = '',
    [string]$FullSourceDirectory = '',
    [int]$Repetitions = 9,
    [int]$VmRepetitions = 3,
    [int]$TimeoutSeconds = 180,
    [int]$LogicalProcessor = -1
)

$ErrorActionPreference = 'Stop'
if ($Repetitions -lt 3 -or $VmRepetitions -lt 3 -or $TimeoutSeconds -lt 1) {
    throw 'Require at least three samples per backend and a positive timeout.'
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
    if ($taskKind -eq 'full' -and (Test-Path -LiteralPath (Join-Path $FullSourceDirectory 'main.wio.cpp'))) {
        Copy-Item -LiteralPath (Join-Path $FullSourceDirectory 'main.wio.cpp') `
            -Destination (Join-Path $taskDirectory 'previous.wio.cpp')
        $taskPrevious = Join-Path $taskDirectory 'previous.exe'
        Build $taskCpp ($taskFlags + @((Join-Path $taskDirectory 'previous.wio.cpp'), "-I$taskRoot/runtime/include",
            "-I$taskRoot/sdk/include", $taskRuntime, '-lws2_32', '-lshell32', '-o', $taskPrevious))
    }
    foreach ($taskFile in Get-ChildItem -LiteralPath $taskDirectory -File) {
        $taskArtifacts.Add([pscustomobject]@{Path="$taskKind/$($taskFile.Name)"; Hash=(Get-FileHash $taskFile.FullName).Hash})
    }
}
$taskBuildMetadata = [pscustomobject]@{
    Revision=(& git -C $taskRoot rev-parse HEAD).Trim()
    Compiler=(& $taskCpp --version | Select-Object -First 1)
    VmConfiguration='MSVC Release, non-JIT, 10 billion instruction benchmark-only budget'
    CompilerHash=(Get-FileHash $taskCompiler).Hash; VmHostHash=(Get-FileHash $VmHost).Hash
    RuntimeHash=(Get-FileHash $taskRuntime).Hash; Flags=$taskFlags
    Artifacts=$taskArtifacts.ToArray()
}
$taskBuildMetadata | ConvertTo-Json -Depth 7 |
    Set-Content -LiteralPath (Join-Path $OutputDirectory 'build.json') -Encoding utf8
# All builds finish before the pinned qualification phase. Reuse the same controller
# as standalone same-binary diagnostics; no legacy unqualified timing path remains.
& (Join-Path $PSScriptRoot 'qualify.ps1') -ArtifactDirectory $OutputDirectory `
    -OutputDirectory (Join-Path $OutputDirectory 'qualification') -VmHost $VmHost `
    -Repetitions $Repetitions -VmRepetitions $VmRepetitions `
    -TimeoutSeconds $TimeoutSeconds -LogicalProcessor $LogicalProcessor

param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [switch]$Configure
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path $PSScriptRoot -Parent
$buildScript = Join-Path $PSScriptRoot "Build-Wio.ps1"
$invokeScript = Join-Path $PSScriptRoot "Invoke-WithSanitizedPath.ps1"
$exampleDir = Join-Path $repoRoot "examples\hybrid_arena"

$buildArgs = @(
    "-ExecutionPolicy", "Bypass",
    "-File", $buildScript,
    "-BuildDir", $BuildDir,
    "-Config", $Config
)

$defaultExe = Join-Path $repoRoot "$BuildDir\app\$Config\wio_app.exe"
$fallbackExe = Join-Path $repoRoot "$BuildDir\app\wio_app.exe"
$shouldConfigure = $Configure -or (
    -not (Test-Path -LiteralPath $defaultExe) -and
    -not (Test-Path -LiteralPath $fallbackExe)
)

if ($shouldConfigure) {
    $buildArgs += "-Configure"
}

& powershell @buildArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$wioExe = $defaultExe
if (-not (Test-Path -LiteralPath $wioExe)) {
    $wioExe = $fallbackExe
}

if (-not (Test-Path -LiteralPath $wioExe)) {
    throw "Compiled wio_app executable was not found under '$BuildDir'."
}

$interopDir = Join-Path $repoRoot "$BuildDir\interop"
New-Item -ItemType Directory -Force -Path $interopDir | Out-Null

$moduleSource = Join-Path $exampleDir "arena_module.wio"
$nativeSource = Join-Path $exampleDir "hybrid_arena_native.cpp"
$hostSource = Join-Path $exampleDir "hybrid_arena_host.cpp"
$moduleA = Join-Path $interopDir "hybrid_arena_demo.a.dll"
$moduleB = Join-Path $interopDir "hybrid_arena_demo.b.dll"
$hostExe = Join-Path $interopDir "hybrid_arena_demo.host.exe"

$commonCompilerArgs = @(
    "--include-dir", $exampleDir,
    "--backend-arg", $nativeSource
)

$moduleACompileArgs = @($wioExe, $moduleSource, "--target", "shared", "--output", $moduleA) + $commonCompilerArgs
& $invokeScript @moduleACompileArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$moduleBCompileArgs = @($wioExe, $moduleSource, "--target", "shared", "--output", $moduleB) + $commonCompilerArgs
& $invokeScript @moduleBCompileArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$hostCompileArgs = @(
    "g++",
    "-std=c++20",
    "-I", (Join-Path $repoRoot "sdk\include"),
    $hostSource,
    "-o", $hostExe
)
& $invokeScript @hostCompileArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Module A:" $moduleA
Write-Host "Module B:" $moduleB
Write-Host "Host EXE:" $hostExe

$hostRunArgs = @($hostExe, $moduleA, $moduleB)
& $invokeScript @hostRunArgs
exit $LASTEXITCODE

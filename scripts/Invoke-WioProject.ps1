param(
    [string]$Project = ".",
    [string]$Config,
    [string]$BuildDir,
    [switch]$Configure,
    [switch]$NoRun,
    [switch]$Describe
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path $PSScriptRoot -Parent
$buildScript = Join-Path $PSScriptRoot "Build-Wio.ps1"

$toolchainBuildDir = if ([string]::IsNullOrWhiteSpace($BuildDir)) { "build" } else { $BuildDir }
$toolchainConfig = if ([string]::IsNullOrWhiteSpace($Config)) { "Debug" } else { $Config }

$buildArgs = @(
    "-ExecutionPolicy", "Bypass",
    "-File", $buildScript,
    "-BuildDir", $toolchainBuildDir,
    "-Config", $toolchainConfig
)

$defaultExe = Join-Path $repoRoot "$toolchainBuildDir\\app\\$toolchainConfig\\wio.exe"
$fallbackExe = Join-Path $repoRoot "$toolchainBuildDir\\app\\wio.exe"
$legacyDefaultExe = Join-Path $repoRoot "$toolchainBuildDir\\app\\$toolchainConfig\\wio_app.exe"
$legacyFallbackExe = Join-Path $repoRoot "$toolchainBuildDir\\app\\wio_app.exe"
$shouldConfigure = $Configure -or (
    -not (Test-Path -LiteralPath $defaultExe) -and
    -not (Test-Path -LiteralPath $fallbackExe) -and
    -not (Test-Path -LiteralPath $legacyDefaultExe) -and
    -not (Test-Path -LiteralPath $legacyFallbackExe)
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
    $wioExe = $legacyDefaultExe
}
if (-not (Test-Path -LiteralPath $wioExe)) {
    $wioExe = $legacyFallbackExe
}

if (-not (Test-Path -LiteralPath $wioExe)) {
    throw "Compiled wio executable was not found under '$toolchainBuildDir'."
}

$subcommand = if ($Describe) {
    "describe"
}
elseif ($NoRun) {
    "build"
}
else {
    "run"
}

$toolArgs = @("project", $subcommand, "--project", $Project)

if (-not [string]::IsNullOrWhiteSpace($Config)) {
    $toolArgs += @("--config", $Config)
}

if (-not [string]::IsNullOrWhiteSpace($BuildDir)) {
    $toolArgs += @("--build-dir", $BuildDir)
}

if ($Configure) {
    $toolArgs += "--configure"
}

& $wioExe @toolArgs
exit $LASTEXITCODE

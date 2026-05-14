param(
    [Parameter(Mandatory = $true)]
    [string]$WioFile,
    [Parameter(Mandatory = $true)]
    [string]$HostSource,
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [ValidateSet("static", "shared")]
    [string]$Target = "shared",
    [string]$OutputName,
    [switch]$Configure,
    [switch]$NoRun,
    [string[]]$LibraryArgs = @(),
    [string[]]$HostArgs = @(),
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ExtraCompilerArgs
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path $PSScriptRoot -Parent
$buildScript = Join-Path $PSScriptRoot "Build-Wio.ps1"

$buildArgs = @(
    "-ExecutionPolicy", "Bypass",
    "-File", $buildScript,
    "-BuildDir", $BuildDir,
    "-Config", $Config
)

$defaultExe = Join-Path $repoRoot "$BuildDir\\app\\$Config\\wio.exe"
$fallbackExe = Join-Path $repoRoot "$BuildDir\\app\\wio.exe"
$legacyDefaultExe = Join-Path $repoRoot "$BuildDir\\app\\$Config\\wio_app.exe"
$legacyFallbackExe = Join-Path $repoRoot "$BuildDir\\app\\wio_app.exe"
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
    throw "Compiled wio executable was not found under '$BuildDir'."
}

$toolScript = Join-Path $repoRoot "scripts\\wio\\run_host_interop.wio"
$toolArgs = @(
    "file", "run", $toolScript, "--",
    "--wio-file", $WioFile,
    "--host-source", $HostSource,
    "--build-dir", $BuildDir,
    "--config", $Config,
    "--target", $Target
)

if (-not [string]::IsNullOrWhiteSpace($OutputName)) {
    $toolArgs += @("--output-name", $OutputName)
}

if ($Configure) {
    $toolArgs += "--configure"
}

if ($NoRun) {
    $toolArgs += "--no-run"
}

foreach ($value in $LibraryArgs) {
    $toolArgs += @("--library-arg", $value)
}

foreach ($value in $HostArgs) {
    $toolArgs += @("--host-arg", $value)
}

foreach ($value in $ExtraCompilerArgs) {
    $toolArgs += @("--wio-arg", $value)
}

& $wioExe @toolArgs
exit $LASTEXITCODE

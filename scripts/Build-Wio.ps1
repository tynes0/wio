param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [switch]$Configure,
    [switch]$Test
)

$ErrorActionPreference = "Stop"

$invokeScript = Join-Path $PSScriptRoot "Invoke-WithSanitizedPath.ps1"
$repoRoot = Split-Path $PSScriptRoot -Parent

if ($Configure) {
    & $invokeScript -Command @("cmake", "-S", $repoRoot, "-B", (Join-Path $repoRoot $BuildDir))
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

& $invokeScript -Command @("cmake", "--build", (Join-Path $repoRoot $BuildDir), "--config", $Config)
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if ($Test) {
    & $invokeScript -Command @("ctest", "--test-dir", (Join-Path $repoRoot $BuildDir), "-C", $Config, "--output-on-failure")
    exit $LASTEXITCODE
}

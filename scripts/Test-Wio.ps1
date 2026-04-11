param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [switch]$List,
    [switch]$Test
)

$ErrorActionPreference = "Stop"

$buildScript = Join-Path $PSScriptRoot "Build-Wio.ps1"
if (-not $List -and -not $Test) {
    $Test = $true
}

& powershell -ExecutionPolicy Bypass -File $buildScript -BuildDir $BuildDir -Config $Config -Configure
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$invokeScript = Join-Path $PSScriptRoot "Invoke-WithSanitizedPath.ps1"
$repoRoot = Split-Path $PSScriptRoot -Parent
$buildPath = Join-Path $repoRoot $BuildDir

if ($List) {
    & $invokeScript -Command @("ctest", "--test-dir", $buildPath, "-C", $Config, "-N")
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if ($Test) {
    & $invokeScript -Command @("ctest", "--test-dir", $buildPath, "-C", $Config, "--output-on-failure")
    exit $LASTEXITCODE
}

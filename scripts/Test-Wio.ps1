param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

$buildScript = Join-Path $PSScriptRoot "Build-Wio.ps1"

& powershell -ExecutionPolicy Bypass -File $buildScript -BuildDir $BuildDir -Config $Config -Configure -Test
exit $LASTEXITCODE

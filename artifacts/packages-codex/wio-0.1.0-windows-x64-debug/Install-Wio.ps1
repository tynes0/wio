param(
    [switch]$SetUserEnvironment,
    [switch]$NoPrompt
)

$ErrorActionPreference = "Stop"

$packageRoot = Split-Path $MyInvocation.MyCommand.Path -Parent
$binDir = Join-Path $packageRoot "bin"

if (-not (Test-Path -LiteralPath $binDir)) {
    throw "The packaged Wio bin directory was not found under '$packageRoot'."
}

$shouldSetEnvironment = $SetUserEnvironment
if (-not $shouldSetEnvironment -and -not $NoPrompt) {
    $choice = Read-Host "Set WIO_ROOT and WIO_HOME for the current user? [y/N]"
    if ($choice -match '^(y|yes)$') {
        $shouldSetEnvironment = $true
    }
}

if ($shouldSetEnvironment) {
    [Environment]::SetEnvironmentVariable("WIO_ROOT", $packageRoot, "User")
    [Environment]::SetEnvironmentVariable("WIO_HOME", $packageRoot, "User")
    $env:WIO_ROOT = $packageRoot
    $env:WIO_HOME = $packageRoot
    Write-Host "Set user environment variables:"
    Write-Host "  WIO_ROOT=$packageRoot"
    Write-Host "  WIO_HOME=$packageRoot"
} else {
    Write-Host "Skipped persistent WIO_ROOT/WIO_HOME configuration."
}

Write-Host ""
Write-Host "Wio package root: $packageRoot"
Write-Host "Binary directory : $binDir"
Write-Host ""
Write-Host "Recommended next steps:"
Write-Host "  1. Run '$binDir\wio.exe --help' (or 'wio --help' if already on PATH)."
Write-Host "  2. Use the packaged scripts only as compatibility helpers while the Wio CLI grows."
Write-Host "  3. Point CMake projects at WIO_ROOT='$packageRoot' when using WioProject.cmake."
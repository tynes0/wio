param(
    [string]$PackageZipPath,
    [string]$InstallRoot,
    [switch]$AllUsers,
    [switch]$NoPrompt,
    [switch]$Force,
    [switch]$SkipEnvironmentSetup,
    [switch]$SkipPath
)

$ErrorActionPreference = "Stop"
$scriptRoot = Split-Path $MyInvocation.MyCommand.Path -Parent
if ([string]::IsNullOrWhiteSpace($PackageZipPath)) {
    $PackageZipPath = Join-Path $scriptRoot "__PACKAGE_NAME__.zip"
}
if (-not (Test-Path -LiteralPath $PackageZipPath)) {
    throw "The package zip was not found at '$PackageZipPath'."
}

$stagingRoot = Join-Path $env:TEMP ("wio-installer-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $stagingRoot | Out-Null

try {
    Expand-Archive -LiteralPath $PackageZipPath -DestinationPath $stagingRoot -Force
    $packageRoot = Join-Path $stagingRoot "__PACKAGE_NAME__"
    $installScript = Join-Path $packageRoot "Install-Wio.ps1"
    if (-not (Test-Path -LiteralPath $installScript)) {
        throw "The extracted package does not contain Install-Wio.ps1."
    }

    $installArgs = @()
    if (-not [string]::IsNullOrWhiteSpace($InstallRoot)) { $installArgs += @('-InstallRoot', $InstallRoot) }
    if ($AllUsers) { $installArgs += '-AllUsers' }
    if ($NoPrompt) { $installArgs += '-NoPrompt' }
    if ($Force) { $installArgs += '-Force' }
    if ($SkipEnvironmentSetup) { $installArgs += '-SkipEnvironmentSetup' }
    if ($SkipPath) { $installArgs += '-SkipPath' }

    & $installScript @installArgs
    exit $LASTEXITCODE
}
finally {
    if (Test-Path -LiteralPath $stagingRoot) {
        Remove-Item -LiteralPath $stagingRoot -Recurse -Force
    }
}

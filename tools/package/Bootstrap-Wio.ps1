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
    $tarCommand = Get-Command tar.exe -ErrorAction SilentlyContinue
    if ($null -ne $tarCommand) {
        & $tarCommand.Source -xf $PackageZipPath -C $stagingRoot
        if ($LASTEXITCODE -ne 0) {
            throw "tar.exe failed to extract the package (exit code $LASTEXITCODE)."
        }
    } else {
        Expand-Archive -LiteralPath $PackageZipPath -DestinationPath $stagingRoot -Force
    }
    $packageRoot = Join-Path $stagingRoot "__PACKAGE_NAME__"
    $installScript = Join-Path $packageRoot "Install-Wio.ps1"
    if (-not (Test-Path -LiteralPath $installScript)) {
        throw "The extracted package does not contain Install-Wio.ps1."
    }

    $installArgs = @{}
    if (-not [string]::IsNullOrWhiteSpace($InstallRoot)) { $installArgs.InstallRoot = $InstallRoot }
    if ($AllUsers) { $installArgs.AllUsers = $true }
    if ($NoPrompt) { $installArgs.NoPrompt = $true }
    if ($Force) { $installArgs.Force = $true }
    if ($SkipEnvironmentSetup) { $installArgs.SkipEnvironmentSetup = $true }
    if ($SkipPath) { $installArgs.SkipPath = $true }

    & $installScript @installArgs
    exit $LASTEXITCODE
}
finally {
    if (Test-Path -LiteralPath $stagingRoot) {
        Remove-Item -LiteralPath $stagingRoot -Recurse -Force
    }
}

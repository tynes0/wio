param(
    [string]$InstallRoot,
    [switch]$AllUsers,
    [switch]$NoPrompt,
    [switch]$Force,
    [switch]$SkipEnvironmentSetup,
    [switch]$SkipPath
)

$ErrorActionPreference = "Stop"

$packageRoot = Split-Path $MyInvocation.MyCommand.Path -Parent
$packageWioExe = Join-Path $packageRoot "bin\wio.exe"

if (-not (Test-Path -LiteralPath $packageWioExe)) {
    throw "The packaged wio executable was not found under '$packageRoot\bin'."
}

if ([string]::IsNullOrWhiteSpace($InstallRoot)) {
    if ($AllUsers) {
        $InstallRoot = Join-Path $env:ProgramFiles "Wio"
    } elseif (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        $InstallRoot = Join-Path $env:LOCALAPPDATA "Programs\Wio"
    } else {
        $InstallRoot = Join-Path $HOME "Wio"
    }
}

$packageRoot = [System.IO.Path]::GetFullPath($packageRoot)
$InstallRoot = [System.IO.Path]::GetFullPath($InstallRoot)

if ((-not $Force) -and (-not $NoPrompt) -and (Test-Path -LiteralPath $InstallRoot) -and ($packageRoot -ne $InstallRoot)) {
    $response = Read-Host "Wio is already installed at '$InstallRoot'. Overwrite it? [y/N]"
    if ($response -notmatch '^(?i:y|yes)$') {
        Write-Host "Installation cancelled."
        exit 1
    }
}

if ($packageRoot -ne $InstallRoot) {
    $installParent = Split-Path -Parent $InstallRoot
    if (-not [string]::IsNullOrWhiteSpace($installParent)) {
        New-Item -ItemType Directory -Force -Path $installParent | Out-Null
    }

    if (Test-Path -LiteralPath $InstallRoot) {
        Remove-Item -LiteralPath $InstallRoot -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $InstallRoot | Out-Null
    Get-ChildItem -LiteralPath $packageRoot -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $InstallRoot -Recurse -Force
    }
}

$installedWioExe = Join-Path $InstallRoot "bin\wio.exe"
if (-not (Test-Path -LiteralPath $installedWioExe)) {
    throw "The installed wio executable was not found under '$InstallRoot\bin'."
}

if (-not $SkipEnvironmentSetup) {
    $cliArgs = @("env", "setup", "--wio-root", $InstallRoot, "--set-user", "--no-prompt")
    if (-not $SkipPath) { $cliArgs += "--add-path" }

    & $installedWioExe @cliArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "Wio installed to '$InstallRoot'."
if (-not $SkipEnvironmentSetup) { Write-Host "Open a new terminal and run 'wio'." }
exit 0

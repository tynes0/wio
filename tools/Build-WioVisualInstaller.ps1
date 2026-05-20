param(
    [string]$Version,
    [string]$PackageRoot,
    [string]$PackageZip,
    [string]$OutputDir,
    [string]$InnoCompiler
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptRoot
$issPath = Join-Path $repoRoot "installer\WioInstaller.iss"

function Resolve-FullPath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $null
    }

    if (Test-Path -LiteralPath $Path) {
        return [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $Path).Path)
    }

    return [System.IO.Path]::GetFullPath($Path)
}

function Read-PackageInfoVersion([string]$Root) {
    $packageInfo = Join-Path $Root "WIO_PACKAGE_INFO.json"
    if (-not (Test-Path -LiteralPath $packageInfo)) {
        return $null
    }

    $json = Get-Content -LiteralPath $packageInfo -Raw | ConvertFrom-Json
    return $json.version
}

function Find-InnoCompiler {
    param([string]$ExplicitPath)

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        $full = Resolve-FullPath $ExplicitPath
        if (Test-Path -LiteralPath $full) { return $full }
        throw "ISCC.exe was not found at '$ExplicitPath'."
    }

    $candidates = @()

    if (-not [string]::IsNullOrWhiteSpace(${env:ProgramFiles(x86)})) {
        $candidates += Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"
    }

    if (-not [string]::IsNullOrWhiteSpace($env:ProgramFiles)) {
        $candidates += Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe"
    }

    $pathCommand = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
    if ($pathCommand) {
        $candidates += $pathCommand.Source
    }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    throw "Inno Setup compiler was not found. Install Inno Setup 6 or pass -InnoCompiler 'C:\Path\To\ISCC.exe'."
}

function Find-PackageRootInside([string]$Root) {
    $directExe = Join-Path $Root "bin\wio.exe"
    if (Test-Path -LiteralPath $directExe) {
        return [System.IO.Path]::GetFullPath($Root)
    }

    $matches = Get-ChildItem -LiteralPath $Root -Directory -Recurse -ErrorAction Stop |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "bin\wio.exe") } |
        Select-Object -First 1

    if ($matches) {
        return [System.IO.Path]::GetFullPath($matches.FullName)
    }

    return $null
}

function Expand-PackageZip([string]$ZipPath) {
    $zipFull = Resolve-FullPath $ZipPath
    if (-not (Test-Path -LiteralPath $zipFull)) {
        throw "Package zip was not found: '$ZipPath'."
    }

    $buildRoot = Join-Path $repoRoot ".build"
    $extractRoot = Join-Path $buildRoot "package"

    if (Test-Path -LiteralPath $extractRoot) {
        Remove-Item -LiteralPath $extractRoot -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null
    Expand-Archive -LiteralPath $zipFull -DestinationPath $extractRoot -Force

    $found = Find-PackageRootInside $extractRoot
    if (-not $found) {
        throw "Could not find a folder containing bin\wio.exe inside '$ZipPath'."
    }

    return $found
}

function Validate-PackageRoot([string]$Root) {
    $requiredPaths = @(
        "bin\wio.exe",
        "std",
        "sdk\include",
        "runtime\include",
        "cmake\WioProject.cmake",
        "QUICKSTART.md",
        "README.md",
        "LICENSE",
        "WIO_PACKAGE_INFO.json"
    )

    foreach ($relative in $requiredPaths) {
        $candidate = Join-Path $Root $relative
        if (-not (Test-Path -LiteralPath $candidate)) {
            throw "PackageRoot is not valid. Missing '$relative' under '$Root'."
        }
    }
}

function Get-VersionInfo([string]$SemanticVersion) {
    $parts = $SemanticVersion.Split('.')
    if ($parts.Count -eq 3) {
        return "$($parts[0]).$($parts[1]).$($parts[2]).0"
    }
    if ($parts.Count -eq 4) {
        return $SemanticVersion
    }
    throw "Version must be in 0.1.0 or 0.1.0.0 form. Got '$SemanticVersion'."
}

if (-not (Test-Path -LiteralPath $issPath)) {
    throw "WioInstaller.iss was not found at '$issPath'."
}

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "artifacts\packages-release"
}
$OutputDir = Resolve-FullPath $OutputDir
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

if (-not [string]::IsNullOrWhiteSpace($PackageZip)) {
    $PackageRoot = Expand-PackageZip $PackageZip
}

if ([string]::IsNullOrWhiteSpace($PackageRoot)) {
    $candidate = Get-ChildItem -LiteralPath $OutputDir -Directory -Filter "wio-*-windows-x64-release" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($candidate) {
        $PackageRoot = $candidate.FullName
    }
}

$PackageRoot = Resolve-FullPath $PackageRoot
if (-not $PackageRoot) {
    throw "No package source was found. Pass -PackageRoot or -PackageZip."
}

Validate-PackageRoot $PackageRoot

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = Read-PackageInfoVersion $PackageRoot
}

if ([string]::IsNullOrWhiteSpace($Version)) {
    throw "Could not determine version. Pass -Version or ensure WIO_PACKAGE_INFO.json contains 'version'."
}

$compiler = Find-InnoCompiler $InnoCompiler
$versionInfo = Get-VersionInfo $Version

Write-Host "Package root : $PackageRoot"
Write-Host "Output dir   : $OutputDir"
Write-Host "ISCC         : $compiler"
Write-Host "Version      : $Version"

$arguments = @(
    "/DAppVersion=$Version",
    "/DAppVersionInfo=$versionInfo",
    "/DPackageRoot=$PackageRoot",
    "/DOutputDir=$OutputDir",
    $issPath
)

& $compiler @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup compiler failed with exit code $LASTEXITCODE."
}

$setupExe = Join-Path $OutputDir "WioSetup-$Version-windows-x64.exe"
if (Test-Path -LiteralPath $setupExe) {
    $hash = Get-FileHash -LiteralPath $setupExe -Algorithm SHA256
    Write-Host ""
    Write-Host "Built: $setupExe"
    Write-Host "SHA256: $($hash.Hash)"
} else {
    Write-Warning "Build completed, but expected output was not found: $setupExe"
}

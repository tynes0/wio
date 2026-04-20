param(
    [string]$BuildDir = "build",
    [string]$Config = "Release",
    [string]$OutputDir = "artifacts\\packages",
    [string]$VersionSuffix = "",
    [string]$Generator = "",
    [switch]$NoZip,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path $PSScriptRoot -Parent
$invokeScript = Join-Path $PSScriptRoot "Invoke-WithSanitizedPath.ps1"
$cmakeListsPath = Join-Path $repoRoot "CMakeLists.txt"
$licensePath = Join-Path $repoRoot "LICENSE"
$readmePath = Join-Path $repoRoot "README.md"
$languageDraftPath = Join-Path $repoRoot "docs\WIO_LANGUAGE_DRAFT.md"

function Get-WioVersion {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CMakeListsPath
    )

    $content = Get-Content -LiteralPath $CMakeListsPath -Raw
    $match = [regex]::Match($content, 'project\s*\(\s*wio_lang\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    if (-not $match.Success) {
        throw "Could not determine Wio version from '$CMakeListsPath'."
    }

    return $match.Groups[1].Value
}

function Get-PlatformTag {
    if ($env:OS -eq "Windows_NT") {
        return "windows"
    }

    $platform = [System.Environment]::OSVersion.Platform
    if ($platform -eq [System.PlatformID]::Unix) {
        return "linux"
    }
    if ($platform -eq [System.PlatformID]::MacOSX) {
        return "macos"
    }

    return "unknown"
}

function Get-ArchitectureTag {
    switch ([System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture) {
        "X64" { return "x64" }
        "Arm64" { return "arm64" }
        "X86" { return "x86" }
        default { return ([System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString().ToLowerInvariant()) }
    }
}

function Write-Utf8File {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Content
    )

    $directory = Split-Path $Path -Parent
    if (-not [string]::IsNullOrWhiteSpace($directory)) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }

    [System.IO.File]::WriteAllText($Path, $Content, [System.Text.UTF8Encoding]::new($false))
}

function Invoke-Sanitized {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Command
    )

    & powershell -ExecutionPolicy Bypass -File $invokeScript @Command
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$version = Get-WioVersion -CMakeListsPath $cmakeListsPath
$platformTag = Get-PlatformTag
$architectureTag = Get-ArchitectureTag
$normalizedConfig = $Config.ToLowerInvariant()
$suffixTag = if ([string]::IsNullOrWhiteSpace($VersionSuffix)) { "" } else { "-" + ($VersionSuffix.Trim() -replace "[^A-Za-z0-9._-]+", "-") }
$packageName = "wio-$version-$platformTag-$architectureTag-$normalizedConfig$suffixTag"

$resolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    [System.IO.Path]::GetFullPath($BuildDir)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
}

$resolvedOutputDir = if ([System.IO.Path]::IsPathRooted($OutputDir)) {
    [System.IO.Path]::GetFullPath($OutputDir)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDir))
}

$packageRoot = Join-Path $resolvedOutputDir $packageName
$distPrefix = Join-Path $resolvedBuildDir "dist"
$archivePath = Join-Path $resolvedOutputDir ($packageName + ".zip")

if ($Clean) {
    if (Test-Path -LiteralPath $packageRoot) {
        Remove-Item -LiteralPath $packageRoot -Recurse -Force
    }
    if (Test-Path -LiteralPath $archivePath) {
        Remove-Item -LiteralPath $archivePath -Force
    }
}

New-Item -ItemType Directory -Force -Path $resolvedOutputDir | Out-Null

if (Test-Path -LiteralPath $distPrefix) {
    Remove-Item -LiteralPath $distPrefix -Recurse -Force
}

$configureCommand = @(
    "cmake",
    "-S", $repoRoot,
    "-B", $resolvedBuildDir,
    "-DWIO_DIST_DIR=$distPrefix"
)

if (-not [string]::IsNullOrWhiteSpace($Generator)) {
    $configureCommand += @("-G", $Generator)
}

Invoke-Sanitized -Command $configureCommand
Invoke-Sanitized -Command @("cmake", "--build", $resolvedBuildDir, "--target", "wio_dist", "--config", $Config)

if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}

Copy-Item -LiteralPath $distPrefix -Destination $packageRoot -Recurse

if (Test-Path -LiteralPath $licensePath) {
    Copy-Item -LiteralPath $licensePath -Destination (Join-Path $packageRoot "LICENSE")
}

if (Test-Path -LiteralPath $readmePath) {
    Copy-Item -LiteralPath $readmePath -Destination (Join-Path $packageRoot "README.md")
}

if (Test-Path -LiteralPath $languageDraftPath) {
    Copy-Item -LiteralPath $languageDraftPath -Destination (Join-Path $packageRoot "docs\\WIO_LANGUAGE_DRAFT.md")
}

$packageInfo = [ordered]@{
    name = $packageName
    version = $version
    platform = $platformTag
    architecture = $architectureTag
    config = $Config
    buildDir = $resolvedBuildDir
    packageRoot = $packageRoot
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
}

Write-Utf8File -Path (Join-Path $packageRoot "WIO_PACKAGE_INFO.json") -Content (($packageInfo | ConvertTo-Json -Depth 5) + [Environment]::NewLine)

if (-not $NoZip) {
    if (Test-Path -LiteralPath $archivePath) {
        Remove-Item -LiteralPath $archivePath -Force
    }

    Compress-Archive -Path $packageRoot -DestinationPath $archivePath -CompressionLevel Optimal
}

Write-Host "Wio package root :" $packageRoot
if (-not $NoZip) {
    Write-Host "Wio package zip  :" $archivePath
}

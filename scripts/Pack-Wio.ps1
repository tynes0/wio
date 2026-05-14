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

function Get-WioCliPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$BuildDir,
        [Parameter(Mandatory = $true)]
        [string]$Config
    )

    $candidates = @(
        (Join-Path $RepoRoot "bin\wio.exe"),
        (Join-Path $RepoRoot "bin\wio"),
        (Join-Path $RepoRoot "$BuildDir\app\$Config\wio.exe"),
        (Join-Path $RepoRoot "$BuildDir\app\wio.exe"),
        (Join-Path $RepoRoot "$BuildDir\app\$Config\wio"),
        (Join-Path $RepoRoot "$BuildDir\app\wio")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    return $null
}

$wioCli = Get-WioCliPath -RepoRoot $repoRoot -BuildDir $BuildDir -Config $Config
if (-not [string]::IsNullOrWhiteSpace($wioCli)) {
    $cliArgs = @("package", "--build-dir", $BuildDir, "--config", $Config, "--output-dir", $OutputDir)
    if (-not [string]::IsNullOrWhiteSpace($VersionSuffix)) {
        $cliArgs += @("--version-suffix", $VersionSuffix)
    }
    if (-not [string]::IsNullOrWhiteSpace($Generator)) {
        $cliArgs += @("--generator", $Generator)
    }
    if ($NoZip) {
        $cliArgs += "--no-zip"
    }
    if ($Clean) {
        $cliArgs += "--clean"
    }

    & $wioCli @cliArgs
    exit $LASTEXITCODE
}

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

$installScript = @'
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
'@

Write-Utf8File -Path (Join-Path $packageRoot "Install-Wio.ps1") -Content $installScript

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

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
$buildScript = Join-Path $PSScriptRoot "Build-Wio.ps1"

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

function Resolve-WioCli {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$BuildDir,
        [Parameter(Mandatory = $true)]
        [string]$Config,
        [Parameter(Mandatory = $true)]
        [string]$BuildScript
    )

    $wioCli = Get-WioCliPath -RepoRoot $RepoRoot -BuildDir $BuildDir -Config $Config
    if (-not [string]::IsNullOrWhiteSpace($wioCli)) {
        return $wioCli
    }

    & powershell -ExecutionPolicy Bypass -File $BuildScript -BuildDir $BuildDir -Config $Config -Configure
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $wioCli = Get-WioCliPath -RepoRoot $RepoRoot -BuildDir $BuildDir -Config $Config
    if (-not [string]::IsNullOrWhiteSpace($wioCli)) {
        return $wioCli
    }

    throw "Could not resolve the built wio executable."
}

$wioCli = Resolve-WioCli -RepoRoot $repoRoot -BuildDir $BuildDir -Config $Config -BuildScript $buildScript
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

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$HeaderPath,

    [Parameter(Mandatory = $true)]
    [string]$RealmName,

    [string]$OutputPath,

    [string]$HeaderInclude,

    [switch]$PreferFlagset
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

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

$wioCli = Resolve-WioCli -RepoRoot $repoRoot -BuildDir "build" -Config "Debug" -BuildScript $buildScript
$cliArgs = @("bind", "import", "--header", $HeaderPath, "--realm", $RealmName)

if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
    $cliArgs += @("--output", $OutputPath)
}

if (-not [string]::IsNullOrWhiteSpace($HeaderInclude)) {
    $cliArgs += @("--header-include", $HeaderInclude)
}

if ($PreferFlagset) {
    $cliArgs += "--prefer-flagset"
}

& $wioCli @cliArgs
exit $LASTEXITCODE

param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [switch]$Configure,
    [switch]$Test
)

$ErrorActionPreference = "Stop"

$invokeScript = Join-Path $PSScriptRoot "Invoke-WithSanitizedPath.ps1"
$repoRoot = Split-Path $PSScriptRoot -Parent

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
    $cliArgs = @("build", "--build-dir", $BuildDir, "--config", $Config)
    if ($Configure) {
        $cliArgs += "--configure"
    }
    if ($Test) {
        $cliArgs += "--test"
    }

    & $wioCli @cliArgs
    exit $LASTEXITCODE
}

if ($Configure) {
    & $invokeScript -Command @("cmake", "-S", $repoRoot, "-B", (Join-Path $repoRoot $BuildDir))
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

& $invokeScript -Command @("cmake", "--build", (Join-Path $repoRoot $BuildDir), "--config", $Config)
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if ($Test) {
    & $invokeScript -Command @("ctest", "--test-dir", (Join-Path $repoRoot $BuildDir), "-C", $Config, "--output-on-failure")
    exit $LASTEXITCODE
}

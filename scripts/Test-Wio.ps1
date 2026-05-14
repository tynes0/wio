param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [switch]$List,
    [switch]$Test,
    [string]$Filter
)

$ErrorActionPreference = "Stop"

$buildScript = Join-Path $PSScriptRoot "Build-Wio.ps1"
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

if (-not $List -and -not $Test) {
    $Test = $true
}

$wioCli = Get-WioCliPath -RepoRoot $repoRoot -BuildDir $BuildDir -Config $Config
if (-not [string]::IsNullOrWhiteSpace($wioCli)) {
    $cliArgs = @("test", "--build-dir", $BuildDir, "--config", $Config, "--configure")
    if ($List) {
        $cliArgs += "--list"
    }
    if (-not [string]::IsNullOrWhiteSpace($Filter)) {
        $cliArgs += @("--filter", $Filter)
    }

    & $wioCli @cliArgs
    exit $LASTEXITCODE
}

& powershell -ExecutionPolicy Bypass -File $buildScript -BuildDir $BuildDir -Config $Config -Configure
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$invokeScript = Join-Path $PSScriptRoot "Invoke-WithSanitizedPath.ps1"
$buildPath = Join-Path $repoRoot $BuildDir

if ($List) {
    $ctestListArgs = @("ctest", "--test-dir", $buildPath, "-C", $Config, "-N")
    if ($Filter) {
        $ctestListArgs += @("-R", $Filter)
    }

    & $invokeScript -Command $ctestListArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if ($Test) {
    $ctestRunArgs = @("ctest", "--test-dir", $buildPath, "-C", $Config, "--output-on-failure")
    if ($Filter) {
        $ctestRunArgs += @("-R", $Filter)
    }

    & $invokeScript -Command $ctestRunArgs
    exit $LASTEXITCODE
}

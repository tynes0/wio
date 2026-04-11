param(
    [string]$File,
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [ValidateSet("run", "check", "tokens", "ast")]
    [string]$Mode = "run",
    [switch]$Configure
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path $PSScriptRoot -Parent
$buildScript = Join-Path $PSScriptRoot "Build-Wio.ps1"
$invokeScript = Join-Path $PSScriptRoot "Invoke-WithSanitizedPath.ps1"

if ([string]::IsNullOrWhiteSpace($File)) {
    $File = Join-Path $repoRoot "playground\\main.wio"
}

$resolvedFile = [System.IO.Path]::GetFullPath($File)
if (-not (Test-Path -LiteralPath $resolvedFile)) {
    throw "Wio source file not found: $resolvedFile"
}

$buildArgs = @(
    "-ExecutionPolicy", "Bypass",
    "-File", $buildScript,
    "-BuildDir", $BuildDir,
    "-Config", $Config
)

$defaultExe = Join-Path $repoRoot "$BuildDir\\app\\$Config\\wio_app.exe"
$fallbackExe = Join-Path $repoRoot "$BuildDir\\app\\wio_app.exe"
$shouldConfigure = $Configure -or (
    -not (Test-Path -LiteralPath $defaultExe) -and
    -not (Test-Path -LiteralPath $fallbackExe)
)

if ($shouldConfigure) {
    $buildArgs += "-Configure"
}

& powershell @buildArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$wioExe = $defaultExe
if (-not (Test-Path -LiteralPath $wioExe)) {
    $wioExe = $fallbackExe
}

if (-not (Test-Path -LiteralPath $wioExe)) {
    throw "Compiled wio_app executable was not found under '$BuildDir'."
}

$compilerArgs = @($resolvedFile)

switch ($Mode) {
    "run" {
        $compilerArgs += "--run"
    }
    "check" {
        $compilerArgs += "--dry-run"
    }
    "tokens" {
        $compilerArgs += "--show-tokens"
        $compilerArgs += "--dry-run"
    }
    "ast" {
        $compilerArgs += "--show-ast"
        $compilerArgs += "--dry-run"
    }
}

& $invokeScript -Command (@($wioExe) + $compilerArgs)
exit $LASTEXITCODE

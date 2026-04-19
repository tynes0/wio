param(
    [Parameter(Mandatory = $true)]
    [string]$WioFile,
    [Parameter(Mandatory = $true)]
    [string]$HostSource,
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [ValidateSet("static", "shared")]
    [string]$Target = "shared",
    [string]$OutputName,
    [switch]$Configure,
    [switch]$NoRun,
    [string[]]$LibraryArgs = @(),
    [string[]]$HostArgs = @(),
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ExtraCompilerArgs
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path $PSScriptRoot -Parent
$buildScript = Join-Path $PSScriptRoot "Build-Wio.ps1"
$invokeScript = Join-Path $PSScriptRoot "Invoke-WithSanitizedPath.ps1"

$resolvedWioFile = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $WioFile))
$resolvedHostSource = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $HostSource))

if (-not (Test-Path -LiteralPath $resolvedWioFile)) {
    throw "Wio source file not found: $resolvedWioFile"
}

if (-not (Test-Path -LiteralPath $resolvedHostSource)) {
    throw "Host C++ source file not found: $resolvedHostSource"
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

if ([string]::IsNullOrWhiteSpace($OutputName)) {
    $OutputName = [System.IO.Path]::GetFileNameWithoutExtension($resolvedWioFile)
}

$interopDir = Join-Path $repoRoot "$BuildDir\\interop"
New-Item -ItemType Directory -Force -Path $interopDir | Out-Null

switch ($Target) {
    "static" {
        $libraryOutput = Join-Path $interopDir ($OutputName + ".a")
        $hostOutput = Join-Path $interopDir ($OutputName + ".host.exe")
    }
    "shared" {
        $libraryOutput = Join-Path $interopDir ($OutputName + ".dll")
        $hostOutput = Join-Path $interopDir ($OutputName + ".host.exe")
    }
}

$compilerArgs = @(
    $wioExe,
    $resolvedWioFile,
    "--target", $Target,
    "--output", $libraryOutput
)

if ($ExtraCompilerArgs) {
    $compilerArgs += $ExtraCompilerArgs
}

& $invokeScript -Command $compilerArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$hostCompileArgs = @(
    "g++",
    "-std=c++20",
    "-I",
    (Join-Path $repoRoot "sdk\\include"),
    $resolvedHostSource
)

if ($Target -eq "static") {
    $hostCompileArgs += $libraryOutput

    $runtimeLibrary = Join-Path $repoRoot "$BuildDir\\runtime\\backend\\libwio_runtime.a"
    if (-not (Test-Path -LiteralPath $runtimeLibrary)) {
        $runtimeLibrary = Join-Path $repoRoot "$BuildDir\\runtime\\libwio_runtime.a"
    }

    if (Test-Path -LiteralPath $runtimeLibrary) {
        $hostCompileArgs += $runtimeLibrary
    }
}

$hostCompileArgs += @("-o", $hostOutput)

if ($HostArgs) {
    $hostCompileArgs += $HostArgs
}

& $invokeScript -Command $hostCompileArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Library :" $libraryOutput
Write-Host "Host EXE:" $hostOutput

if (-not $NoRun) {
    $runArgs = @($hostOutput)

    if ($Target -eq "shared") {
        $runArgs += $libraryOutput
        if ($LibraryArgs) {
            $runArgs += $LibraryArgs
        }
    }
    elseif ($LibraryArgs) {
        $runArgs += $LibraryArgs
    }

    & $invokeScript -Command $runArgs
    exit $LASTEXITCODE
}

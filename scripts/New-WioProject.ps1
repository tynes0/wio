param(
    [Parameter(Mandatory = $true)]
    [string]$Name,
    [string]$OutputDir = ".",
    [ValidateSet("hybrid-module", "wio-app", "wio-native-app", "wio-module")]
    [string]$Template = "hybrid-module",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

function Convert-ToSafeIdentifier {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    $safe = $Value.ToLowerInvariant() -replace "[^a-z0-9_]+", "_"
    $safe = $safe.Trim("_")

    if ([string]::IsNullOrWhiteSpace($safe)) {
        return "wio_project"
    }

    return $safe
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

function Convert-ToMakeWioValue {
    param(
        [Parameter(Mandatory = $true)]
        [AllowNull()]
        [object]$Value
    )

    if ($null -eq $Value) {
        return '""'
    }

    if ($Value -is [bool]) {
        return $Value.ToString().ToLowerInvariant()
    }

    if ($Value -is [string]) {
        $escaped = $Value.Replace('\', '\\').Replace('"', '\"')
        return '"' + $escaped + '"'
    }

    if ($Value -is [System.Array]) {
        $items = @($Value | ForEach-Object { Convert-ToMakeWioValue -Value $_ })
        return "[" + ($items -join ", ") + "]"
    }

    return [string]$Value
}

function Convert-SectionToMakeWio {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [System.Collections.IDictionary]$Values
    )

    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add("[$Name]")
    foreach ($entry in $Values.GetEnumerator()) {
        $lines.Add("$($entry.Key) = $(Convert-ToMakeWioValue -Value $entry.Value)")
    }

    return ($lines -join [Environment]::NewLine)
}

function New-TemplateSpec {
    param(
        [Parameter(Mandatory = $true)]
        [string]$TemplateName,
        [Parameter(Mandatory = $true)]
        [string]$SafeName
    )

    $spec = [ordered]@{
        wioTarget = "shared"
        hostEnabled = $true
        hostSourceRoots = @("host")
        runPassLibraryPath = $true
        wioEntry = "wio/module.wio"
        wioSource = ""
        hostSource = ""
        nativeHeaderName = $null
        nativeHeaderContent = $null
        nativeSourceName = $null
        nativeSourceContent = $null
        description = ""
    }

    switch ($TemplateName) {
        "hybrid-module" {
            $spec.description = "Shared Wio module plus native C++ host."
            $spec.wioSource = @"
use std::io as console;

mut gCounter: i32 = 0;

@ModuleApiVersion
fn RuntimeAbi() -> u32 {
    return 1;
}

@ModuleLoad
fn BootModule() -> i32 {
    gCounter = 10;
    console::Print("[script] BootModule\n");
    return 0;
}

@ModuleUpdate
fn TickModule(deltaTime: f32) {
    gCounter += deltaTime fit i32;
}

@ModuleUnload
fn StopModule() {
    console::Print("[script] StopModule\n");
}

@Export
@Command("counter.get")
@CppName(WioGetCounter)
fn GetCounter() -> i32 {
    return gCounter;
}

@Export
@Command("counter.add")
@CppName(WioAddToCounter)
fn AddToCounter(delta: i32) -> i32 {
    gCounter += delta;
    return gCounter;
}

@Export
@Event("game.tick")
@CppName(WioApplyScriptTick)
fn ApplyScriptTick(deltaTime: f32) {
    gCounter += deltaTime fit i32;
    console::Print("[script] game.tick received\n");
}
"@
            $spec.hostSource = @"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Expected a Wio module library path." << '\n';
        return EXIT_FAILURE;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);
        module.update(1.0f);

        auto getCounter = module.load_command<std::int32_t()>("counter.get");
        auto addCounter = module.load_command<std::int32_t(std::int32_t)>("counter.add");
        auto broadcastTick = module.load_event<void(float)>("game.tick");

        const std::int32_t before = getCounter();
        const std::int32_t afterAdd = addCounter(5);
        broadcastTick(2.0f);
        const std::int32_t afterTick = getCounter();

        std::cout << "Hybrid project: before=" << before
                  << " afterAdd=" << afterAdd
                  << " afterTick=" << afterTick
                  << " exports=" << module.api().exportCount
                  << " commands=" << module.api().commandCount
                  << " events=" << module.api().eventHookCount
                  << '\n';
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
"@
        }
        "wio-app" {
            $spec.wioTarget = "exe"
            $spec.hostEnabled = $false
            $spec.runPassLibraryPath = $false
            $spec.wioSource = @"
use std::io as console;

fn Entry() -> i32 {
    console::Print("Hello from a plain Wio application.");
    return 0;
}
"@
        }
        "wio-native-app" {
            $spec.wioTarget = "exe"
            $spec.hostEnabled = $false
            $spec.runPassLibraryPath = $false
            $spec.wioSource = @"
use std::io as console;

realm ffi {
    @Native
    @CppHeader("native_math.h")
    @CppName(native_math::Multiply)
    fn Multiply(lhs: i32, rhs: i32) -> i32;
}

fn Entry() -> i32 {
    console::Print("Native multiply: ");
    console::Print(ffi::Multiply(6, 7));
    return 0;
}
"@
            $spec.nativeHeaderName = "native_math.h"
            $spec.nativeHeaderContent = @"
#pragma once

namespace native_math
{
    int Multiply(int lhs, int rhs);
}
"@
            $spec.nativeSourceName = "native_math.cpp"
            $spec.nativeSourceContent = @"
#include "native_math.h"

namespace native_math
{
    int Multiply(int lhs, int rhs)
    {
        return lhs * rhs;
    }
}
"@
        }
        "wio-module" {
            $spec.hostEnabled = $false
            $spec.runPassLibraryPath = $false
            $spec.wioSource = @"
@Export
@CppName(WioAddNumbers)
fn AddNumbers(lhs: i32, rhs: i32) -> i32 {
    return lhs + rhs;
}
"@
        }
    }

    return $spec
}

$repoRoot = Split-Path $PSScriptRoot -Parent
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $OutputDir $Name))
$safeName = Convert-ToSafeIdentifier -Value $Name
$templateSpec = New-TemplateSpec -TemplateName $Template -SafeName $safeName
$repoRootForCMake = $repoRoot -replace "\\", "/"

if (Test-Path -LiteralPath $projectRoot) {
    $existingEntries = Get-ChildItem -Force -LiteralPath $projectRoot
    if ($existingEntries.Count -gt 0 -and -not $Force) {
        throw "Project directory already exists and is not empty: $projectRoot"
    }
}
else {
    New-Item -ItemType Directory -Force -Path $projectRoot | Out-Null
}

$wioDir = Join-Path $projectRoot "wio"
$hostDir = Join-Path $projectRoot "host"
$hostIncludeDir = Join-Path $hostDir "include"
$nativeDir = Join-Path $projectRoot "native"
$nativeIncludeDir = Join-Path $nativeDir "include"
$nativeSourceDir = Join-Path $nativeDir "src"
$nativeLibDir = Join-Path $nativeDir "lib"

New-Item -ItemType Directory -Force -Path $wioDir | Out-Null
New-Item -ItemType Directory -Force -Path $hostDir | Out-Null
New-Item -ItemType Directory -Force -Path $hostIncludeDir | Out-Null
New-Item -ItemType Directory -Force -Path $nativeIncludeDir | Out-Null
New-Item -ItemType Directory -Force -Path $nativeSourceDir | Out-Null
New-Item -ItemType Directory -Force -Path $nativeLibDir | Out-Null

$manifestObject = [ordered]@{
    schemaVersion = 1
    name = $Name
    template = $Template
    toolchain = [ordered]@{
        buildDir = "build"
        config = "Debug"
    }
    wio = [ordered]@{
        entry = $templateSpec.wioEntry
        target = $templateSpec.wioTarget
        sourceRoots = @("wio")
        usePaths = @("wio")
        includeDirs = @("native/include")
        linkDirs = @("native/lib")
        linkLibraries = @()
        backendArgs = @()
    }
    host = [ordered]@{
        enabled = $templateSpec.hostEnabled
        compiler = "g++"
        sourceRoots = $templateSpec.hostSourceRoots
        includeDirs = @("host/include", "native/include")
        linkDirs = @("native/lib")
        linkLibraries = @()
        compilerArgs = @()
    }
    build = [ordered]@{
        buildDir = ".wio-build"
        config = "Debug"
    }
    outputs = [ordered]@{
        directory = ".wio-build/interop"
        baseName = $safeName
        wioName = $safeName
        hostName = "$safeName.host"
    }
    run = [ordered]@{
        passLibraryPath = $templateSpec.runPassLibraryPath
        args = @()
        workingDirectory = "."
    }
}

$manifestMakeWio = @(
    "schemaVersion = 1"
    "name = $(Convert-ToMakeWioValue -Value $Name)"
    "template = $(Convert-ToMakeWioValue -Value $Template)"
    ""
    (Convert-SectionToMakeWio -Name "toolchain" -Values $manifestObject.toolchain)
    ""
    (Convert-SectionToMakeWio -Name "wio" -Values $manifestObject.wio)
    ""
    (Convert-SectionToMakeWio -Name "host" -Values $manifestObject.host)
    ""
    (Convert-SectionToMakeWio -Name "build" -Values $manifestObject.build)
    ""
    (Convert-SectionToMakeWio -Name "outputs" -Values $manifestObject.outputs)
    ""
    (Convert-SectionToMakeWio -Name "run" -Values $manifestObject.run)
) -join [Environment]::NewLine

$wrapperBuild = @"
param(
    [switch]`$Configure,
    [string]`$Config = "Debug"
)

`$ErrorActionPreference = "Stop"
`$wioRoot = if (-not [string]::IsNullOrWhiteSpace(`$env:WIO_ROOT)) { `$env:WIO_ROOT } else { "$repoRoot" }
`$invokeScript = Join-Path `$wioRoot "scripts\Invoke-WioProject.ps1"
`$invokeParams = @{
    Project = `$PSScriptRoot
    NoRun = `$true
    Config = `$Config
}

if (`$Configure) {
    `$invokeParams.Configure = `$true
}

& `$invokeScript @invokeParams
exit `$LASTEXITCODE
"@

$wrapperRun = @"
param(
    [switch]`$Configure,
    [string]`$Config = "Debug"
)

`$ErrorActionPreference = "Stop"
`$wioRoot = if (-not [string]::IsNullOrWhiteSpace(`$env:WIO_ROOT)) { `$env:WIO_ROOT } else { "$repoRoot" }
`$invokeScript = Join-Path `$wioRoot "scripts\Invoke-WioProject.ps1"
`$invokeParams = @{
    Project = `$PSScriptRoot
    Config = `$Config
}

if (`$Configure) {
    `$invokeParams.Configure = `$true
}

& `$invokeScript @invokeParams
exit `$LASTEXITCODE
"@

$cmakeLists = @"
cmake_minimum_required(VERSION 3.24)
project($safeName LANGUAGES NONE)

set(WIO_ROOT "`$ENV{WIO_ROOT}" CACHE PATH "Path to the Wio toolchain root")
if(NOT WIO_ROOT)
    set(WIO_ROOT "$repoRootForCMake" CACHE PATH "Path to the Wio toolchain root" FORCE)
endif()

if(NOT EXISTS "`${WIO_ROOT}/cmake/WioProject.cmake")
    message(FATAL_ERROR "WioProject.cmake was not found under WIO_ROOT='`${WIO_ROOT}'.")
endif()

include("`${WIO_ROOT}/cmake/WioProject.cmake")

wio_add_project_targets($safeName
    WIO_ROOT "`${WIO_ROOT}"
)
"@

$gitignore = @"
.wio-build/
build/
cmake-build*/
"@

$readme = @"
# $Name

Template:

- `$Template`

Description:

- $($templateSpec.description)

## Commands

- Build only:
  - `powershell -ExecutionPolicy Bypass -File .\build.ps1`
- Build and run:
  - `powershell -ExecutionPolicy Bypass -File .\run.ps1`

## CMake / Visual Studio / Rider

This project also includes a `CMakeLists.txt`.

You can:

- open it directly in Visual Studio as a CMake project,
- open it in Rider/CLion,
- or integrate the same manifest into an existing CMake tree through
  `cmake/WioProject.cmake`.

## Layout

- `wio/`: Wio source files
- `host/`: host C++ source files
- `host/include/`: host-only headers
- `native/include/`: headers visible to Wio `@CppHeader(...)`
- `native/src/`: extra native `.c/.cc/.cpp/.cxx` files compiled with the Wio target
- `native/lib/`: prebuilt native libraries you want to link from Wio or the host
- `wio.makewio`: primary project manifest consumed by the Wio project runner

## Toolchain Root

If the project lives outside the Wio repo checkout, set `WIO_ROOT` to the Wio
toolchain root before using the wrapper scripts or the CMake integration.

## Guide

See:

- `docs/WIO_PROJECT_SYSTEM.md` inside the Wio toolchain root
"@

$nativeReadme = @"
Place user-native integration files here.

- `include/`: headers for `@CppHeader(...)`
- `src/`: extra native `.c/.cc/.cpp/.cxx` files compiled with the Wio target
- `lib/`: prebuilt native `.lib/.a/.dll/.so/.dylib` inputs referenced from the manifest
"@

Write-Utf8File -Path (Join-Path $projectRoot "wio.project.json") -Content ($manifestJson + [Environment]::NewLine)
Write-Utf8File -Path (Join-Path $wioDir "module.wio") -Content $templateSpec.wioSource
Write-Utf8File -Path (Join-Path $projectRoot "build.ps1") -Content $wrapperBuild
Write-Utf8File -Path (Join-Path $projectRoot "run.ps1") -Content $wrapperRun
Write-Utf8File -Path (Join-Path $projectRoot "CMakeLists.txt") -Content $cmakeLists
Write-Utf8File -Path (Join-Path $projectRoot ".gitignore") -Content $gitignore
Write-Utf8File -Path (Join-Path $projectRoot "README.md") -Content $readme
Write-Utf8File -Path (Join-Path $projectRoot "wio.makewio") -Content ($manifestMakeWio + [Environment]::NewLine)
Write-Utf8File -Path (Join-Path $nativeDir "README.md") -Content $nativeReadme
Write-Utf8File -Path (Join-Path $hostIncludeDir ".gitkeep") -Content "# placeholder`n"
Write-Utf8File -Path (Join-Path $nativeIncludeDir ".gitkeep") -Content "# placeholder`n"
Write-Utf8File -Path (Join-Path $nativeSourceDir ".gitkeep") -Content "# placeholder`n"
Write-Utf8File -Path (Join-Path $nativeLibDir ".gitkeep") -Content "# placeholder`n"

if ($templateSpec.hostEnabled -and -not [string]::IsNullOrWhiteSpace($templateSpec.hostSource)) {
    Write-Utf8File -Path (Join-Path $hostDir "main.cpp") -Content $templateSpec.hostSource
}

if (-not [string]::IsNullOrWhiteSpace($templateSpec.nativeHeaderName) -and -not [string]::IsNullOrWhiteSpace($templateSpec.nativeHeaderContent)) {
    Write-Utf8File -Path (Join-Path $nativeIncludeDir $templateSpec.nativeHeaderName) -Content $templateSpec.nativeHeaderContent
}

if (-not [string]::IsNullOrWhiteSpace($templateSpec.nativeSourceName) -and -not [string]::IsNullOrWhiteSpace($templateSpec.nativeSourceContent)) {
    Write-Utf8File -Path (Join-Path $nativeSourceDir $templateSpec.nativeSourceName) -Content $templateSpec.nativeSourceContent
}

Write-Host "Generated project :" $projectRoot
Write-Host "Template          :" $Template
Write-Host "Manifest          :" (Join-Path $projectRoot "wio.makewio")

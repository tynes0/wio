param(
    [string]$Project = ".",
    [string]$Config,
    [string]$BuildDir,
    [switch]$Configure,
    [switch]$NoRun,
    [switch]$Describe
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path $PSScriptRoot -Parent
$buildScript = Join-Path $PSScriptRoot "Build-Wio.ps1"
$invokeScript = Join-Path $PSScriptRoot "Invoke-WithSanitizedPath.ps1"
$script:IsWindowsHost = $env:OS -eq "Windows_NT"
$script:IsMacOSHost = [System.Environment]::OSVersion.Platform -eq [System.PlatformID]::MacOSX
$script:PathComparer = if ($script:IsWindowsHost) { [System.StringComparer]::OrdinalIgnoreCase } else { [System.StringComparer]::Ordinal }

function Get-ManifestProperty {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Object,
        [Parameter(Mandatory = $true)]
        [string]$Name,
        $Default = $null
    )

    if ($null -eq $Object) {
        return $Default
    }

    if ($Object -is [System.Collections.IDictionary]) {
        if ($Object.Contains($Name)) {
            return $Object[$Name]
        }

        return $Default
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $Default
    }

    return $property.Value
}

function Has-ManifestProperty {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Object,
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    if ($null -eq $Object) {
        return $false
    }

    if ($Object -is [System.Collections.IDictionary]) {
        return $Object.Contains($Name)
    }

    return $null -ne $Object.PSObject.Properties[$Name]
}

function Remove-MakeWioComment {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    $builder = [System.Text.StringBuilder]::new()
    $quote = [char]0
    $escape = $false

    for ($index = 0; $index -lt $Value.Length; $index++) {
        $current = $Value[$index]

        if ($escape) {
            [void]$builder.Append($current)
            $escape = $false
            continue
        }

        if ($quote -ne [char]0) {
            [void]$builder.Append($current)
            if ($current -eq '\') {
                $escape = $true
            }
            elseif ($current -eq $quote) {
                $quote = [char]0
            }
            continue
        }

        if ($current -eq '"' -or $current -eq "'") {
            $quote = $current
            [void]$builder.Append($current)
            continue
        }

        if ($current -eq '#') {
            break
        }

        if ($current -eq '/' -and ($index + 1) -lt $Value.Length -and $Value[$index + 1] -eq '/') {
            break
        }

        [void]$builder.Append($current)
    }

    return $builder.ToString().Trim()
}

function Convert-MakeWioEscapes {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    return $Value.Replace('\"', '"').Replace("\'", "'").Replace('\n', [Environment]::NewLine).Replace('\t', "`t").Replace('\\', '\')
}

function Split-MakeWioArrayItems {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    $items = [System.Collections.Generic.List[string]]::new()
    $builder = [System.Text.StringBuilder]::new()
    $quote = [char]0
    $escape = $false

    for ($index = 0; $index -lt $Value.Length; $index++) {
        $current = $Value[$index]

        if ($escape) {
            [void]$builder.Append($current)
            $escape = $false
            continue
        }

        if ($quote -ne [char]0) {
            [void]$builder.Append($current)
            if ($current -eq '\') {
                $escape = $true
            }
            elseif ($current -eq $quote) {
                $quote = [char]0
            }
            continue
        }

        if ($current -eq '"' -or $current -eq "'") {
            $quote = $current
            [void]$builder.Append($current)
            continue
        }

        if ($current -eq ',') {
            $items.Add($builder.ToString().Trim())
            $builder.Clear() | Out-Null
            continue
        }

        [void]$builder.Append($current)
    }

    $finalItem = $builder.ToString().Trim()
    if ($finalItem.Length -gt 0) {
        $items.Add($finalItem)
    }

    Write-Output -NoEnumerate ($items.ToArray())
}

function Convert-MakeWioValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    $trimmed = (Remove-MakeWioComment -Value $Value).Trim()
    if ([string]::IsNullOrWhiteSpace($trimmed)) {
        return ""
    }

    if ($trimmed -eq "[]") {
        Write-Output -NoEnumerate @()
        return
    }

    if ($trimmed.StartsWith("[") -and $trimmed.EndsWith("]")) {
        $inner = $trimmed.Substring(1, $trimmed.Length - 2).Trim()
        if ([string]::IsNullOrWhiteSpace($inner)) {
            Write-Output -NoEnumerate @()
            return
        }

        $values = [System.Collections.Generic.List[object]]::new()
        foreach ($item in (Split-MakeWioArrayItems -Value $inner)) {
            $values.Add((Convert-MakeWioValue -Value $item))
        }
        Write-Output -NoEnumerate ($values.ToArray())
        return
    }

    if (($trimmed.StartsWith('"') -and $trimmed.EndsWith('"')) -or ($trimmed.StartsWith("'") -and $trimmed.EndsWith("'"))) {
        return Convert-MakeWioEscapes -Value $trimmed.Substring(1, $trimmed.Length - 2)
    }

    if ($trimmed -match '^(true|false)$') {
        return $trimmed -eq "true"
    }

    if ($trimmed -match '^[+-]?\d+$') {
        return [long]$trimmed
    }

    if ($trimmed -match '^[+-]?\d+\.\d+$') {
        return [double]::Parse($trimmed, [System.Globalization.CultureInfo]::InvariantCulture)
    }

    return $trimmed
}

function Set-NestedManifestValue {
    param(
        [Parameter(Mandatory = $true)]
        [System.Collections.IDictionary]$Manifest,
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [object]$Value
    )

    $segments = $Path.Split('.', [System.StringSplitOptions]::RemoveEmptyEntries)
    if ($segments.Count -eq 0) {
        throw "Invalid makewio key path '$Path'."
    }

    $current = $Manifest
    for ($index = 0; $index -lt ($segments.Count - 1); $index++) {
        $segment = $segments[$index]
        if (-not $current.Contains($segment)) {
            $current[$segment] = [ordered]@{}
        }
        elseif (-not ($current[$segment] -is [System.Collections.IDictionary])) {
            throw "makewio key path '$Path' conflicts with an existing scalar value."
        }

        $current = $current[$segment]
    }

    $current[$segments[$segments.Count - 1]] = $Value
}

function Parse-MakeWioManifest {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $manifest = [ordered]@{}
    $currentSection = ""
    $lineNumber = 0

    foreach ($rawLine in (Get-Content -LiteralPath $Path)) {
        $lineNumber += 1
        $line = $rawLine.Trim()

        if ([string]::IsNullOrWhiteSpace($line)) {
            continue
        }

        if ($line.StartsWith("#") -or $line.StartsWith(";") -or $line.StartsWith("//")) {
            continue
        }

        if ($line -match '^\[(?<section>[A-Za-z0-9_.-]+)\]$') {
            $currentSection = $Matches["section"]
            continue
        }

        if ($line -notmatch '^(?<key>[A-Za-z0-9_.-]+)\s*=\s*(?<value>.*)$') {
            throw "Invalid makewio syntax at line $lineNumber in '$Path'. Expected 'key = value' or '[section]'."
        }

        $key = $Matches["key"]
        $value = $Matches["value"]
        $fullPath = if ([string]::IsNullOrWhiteSpace($currentSection) -or $key.Contains(".")) {
            $key
        } else {
            "$currentSection.$key"
        }

        $convertedValue = Convert-MakeWioValue -Value $value
        $setValueArgs = @{
            Manifest = $manifest
            Path = $fullPath
            Value = $convertedValue
        }
        Set-NestedManifestValue @setValueArgs
    }

    return $manifest
}

function Resolve-ProjectManifestFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    $candidate = if ([string]::IsNullOrWhiteSpace($Value)) { "." } else { $Value }
    $resolved = Resolve-Path -LiteralPath $candidate -ErrorAction Stop
    $resolvedPath = $resolved.Path
    $item = Get-Item -LiteralPath $resolvedPath

    if (-not $item.PSIsContainer) {
        return $resolvedPath
    }

    $manifestCandidates = @(
        "wio.makewio",
        "makewio",
        "wio.project.json"
    )

    foreach ($manifestCandidate in $manifestCandidates) {
        $manifestPath = Join-Path $resolvedPath $manifestCandidate
        if (Test-Path -LiteralPath $manifestPath) {
            return [System.IO.Path]::GetFullPath($manifestPath)
        }
    }

    throw "No project manifest was found under '$resolvedPath'. Expected one of: wio.makewio, makewio, wio.project.json."
}

function Read-ProjectManifest {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $fileName = [System.IO.Path]::GetFileName($Path)
    if ($fileName.Equals("makewio", [System.StringComparison]::OrdinalIgnoreCase) -or
        [System.IO.Path]::GetExtension($Path).Equals(".makewio", [System.StringComparison]::OrdinalIgnoreCase)) {
        return (Parse-MakeWioManifest -Path $Path)
    }

    return (Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json)
}

function Get-ManifestBoolean {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Object,
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [bool]$Default = $false
    )

    $value = Get-ManifestProperty -Object $Object -Name $Name -Default $null
    if ($null -eq $value) {
        return $Default
    }

    return [bool]$value
}

function Get-ManifestStringArray {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Object,
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $value = Get-ManifestProperty -Object $Object -Name $Name -Default $null
    if ($null -eq $value) {
        Write-Output -NoEnumerate @()
        return
    }

    if ($value -is [System.Array]) {
        Write-Output -NoEnumerate @($value | ForEach-Object { [string]$_ })
        return
    }

    Write-Output -NoEnumerate @([string]$value)
}

function Resolve-ProjectPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot,
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    if ([System.IO.Path]::IsPathRooted($Value)) {
        return [System.IO.Path]::GetFullPath($Value)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $ProjectRoot $Value))
}

function New-UniquePathList {
    return ,([System.Collections.ArrayList]::new())
}

function Add-UniquePath {
    param(
        [System.Collections.ArrayList]$List,
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $resolved = [System.IO.Path]::GetFullPath($Path)
    foreach ($existing in $List) {
        if ($script:PathComparer.Equals($existing, $resolved)) {
            return
        }
    }

    [void]$List.Add($resolved)
}

function Convert-ToUniquePathArray {
    param(
        [AllowEmptyCollection()]
        [string[]]$Paths
    )

    $list = New-UniquePathList
    foreach ($path in $Paths) {
        Add-UniquePath -List $list -Path $path
    }
    Write-Output -NoEnumerate @($list)
}

function Resolve-DeclaredPaths {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot,
        [AllowEmptyCollection()]
        [string[]]$Values,
        [Parameter(Mandatory = $true)]
        [string]$Label,
        [ValidateSet("Any", "File", "Directory")]
        [string]$Kind = "Any"
    )

    $resolved = New-UniquePathList
    foreach ($value in $Values) {
        if ([string]::IsNullOrWhiteSpace($value)) {
            continue
        }

        $path = Resolve-ProjectPath -ProjectRoot $ProjectRoot -Value $value
        if (-not (Test-Path -LiteralPath $path)) {
            throw "$Label path not found: $path"
        }

        $item = Get-Item -LiteralPath $path
        if ($Kind -eq "File" -and $item.PSIsContainer) {
            throw "$Label expected a file but found a directory: $path"
        }

        if ($Kind -eq "Directory" -and -not $item.PSIsContainer) {
            throw "$Label expected a directory but found a file: $path"
        }

        Add-UniquePath -List $resolved -Path $path
    }

    Write-Output -NoEnumerate @($resolved)
}

function Get-ExistingResolvedDirectories {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot,
        [AllowEmptyCollection()]
        [string[]]$RelativeCandidates
    )

    $resolved = New-UniquePathList
    foreach ($candidate in $RelativeCandidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }

        $path = Resolve-ProjectPath -ProjectRoot $ProjectRoot -Value $candidate
        if ((Test-Path -LiteralPath $path) -and (Get-Item -LiteralPath $path).PSIsContainer) {
            Add-UniquePath -List $resolved -Path $path
        }
    }

    Write-Output -NoEnumerate @($resolved)
}

function Get-FilesByExtensionFromDirectories {
    param(
        [AllowEmptyCollection()]
        [string[]]$Directories,
        [AllowEmptyCollection()]
        [string[]]$Extensions
    )

    $resolved = New-UniquePathList
    $extensionSet = [System.Collections.Generic.HashSet[string]]::new($script:PathComparer)
    foreach ($extension in $Extensions) {
        if (-not [string]::IsNullOrWhiteSpace($extension)) {
            [void]$extensionSet.Add($extension)
        }
    }

    foreach ($directory in $Directories) {
        if (-not (Test-Path -LiteralPath $directory)) {
            continue
        }

        Get-ChildItem -LiteralPath $directory -File -Recurse | ForEach-Object {
            if ($extensionSet.Contains($_.Extension)) {
                Add-UniquePath -List $resolved -Path $_.FullName
            }
        }
    }

    Write-Output -NoEnumerate @($resolved)
}

function Find-FirstExistingPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot,
        [AllowEmptyCollection()]
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }

        $path = Resolve-ProjectPath -ProjectRoot $ProjectRoot -Value $candidate
        if ((Test-Path -LiteralPath $path) -and -not (Get-Item -LiteralPath $path).PSIsContainer) {
            return $path
        }
    }

    return $null
}

function Get-DefaultWioEntry {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot
    )

    $preferred = @(
        "wio/module.wio",
        "wio/main.wio",
        "src/module.wio",
        "src/main.wio",
        "main.wio"
    )

    $first = Find-FirstExistingPath -ProjectRoot $ProjectRoot -Candidates $preferred
    if ($null -ne $first) {
        return $first
    }

    $roots = Get-ExistingResolvedDirectories -ProjectRoot $ProjectRoot -RelativeCandidates @("wio", "src")
    if ($roots.Count -eq 0) {
        $roots = @($ProjectRoot)
    }

    $candidates = Get-FilesByExtensionFromDirectories -Directories $roots -Extensions @(".wio")
    if ($candidates.Count -eq 1) {
        return $candidates[0]
    }

    throw "Unable to infer the Wio entry file. Set 'wio.entry' explicitly in the manifest."
}

function Get-DefaultWioSourceRoots {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot,
        [Parameter(Mandatory = $true)]
        [string]$ResolvedEntry
    )

    $roots = New-UniquePathList
    foreach ($candidate in (Get-ExistingResolvedDirectories -ProjectRoot $ProjectRoot -RelativeCandidates @("wio", "src"))) {
        Add-UniquePath -List $roots -Path $candidate
    }
    Add-UniquePath -List $roots -Path (Split-Path $ResolvedEntry -Parent)
    return @($roots)
}

function Get-DefaultWioIncludeDirs {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot
    )

    return Get-ExistingResolvedDirectories -ProjectRoot $ProjectRoot -RelativeCandidates @("native/include")
}

function Get-DefaultWioLinkDirs {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot
    )

    return Get-ExistingResolvedDirectories -ProjectRoot $ProjectRoot -RelativeCandidates @("native/lib")
}

function Get-DefaultWioNativeSources {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot
    )

    $roots = Get-ExistingResolvedDirectories -ProjectRoot $ProjectRoot -RelativeCandidates @("native/src")
    return Get-FilesByExtensionFromDirectories -Directories $roots -Extensions @(".c", ".cc", ".cpp", ".cxx")
}

function Get-DefaultHostSourceRoots {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot
    )

    return Get-ExistingResolvedDirectories -ProjectRoot $ProjectRoot -RelativeCandidates @("host", "host/src")
}

function Get-DefaultHostIncludeDirs {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot
    )

    return Get-ExistingResolvedDirectories -ProjectRoot $ProjectRoot -RelativeCandidates @("host/include", "native/include")
}

function Get-DefaultHostLinkDirs {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot
    )

    return Get-ExistingResolvedDirectories -ProjectRoot $ProjectRoot -RelativeCandidates @("native/lib")
}

function Resolve-LinkLibrarySpecs {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProjectRoot,
        [AllowEmptyCollection()]
        [string[]]$Values
    )

    $resolved = [System.Collections.Generic.List[string]]::new()
    foreach ($value in $Values) {
        if ([string]::IsNullOrWhiteSpace($value)) {
            continue
        }

        if ($value.StartsWith("-")) {
            $resolved.Add($value)
            continue
        }

        $explicitPath = [System.IO.Path]::IsPathRooted($value) -or
                        $value.Contains("/") -or
                        $value.Contains("\") -or
                        $value.StartsWith(".")

        $candidatePath = Resolve-ProjectPath -ProjectRoot $ProjectRoot -Value $value
        $looksLikeExistingProjectPath = Test-Path -LiteralPath $candidatePath

        if ($explicitPath -or $looksLikeExistingProjectPath) {
            $path = $candidatePath
            if (-not (Test-Path -LiteralPath $path)) {
                throw "Link library file not found: $path"
            }
            $resolved.Add($path)
            continue
        }

        $resolved.Add($value)
    }

    Write-Output -NoEnumerate @($resolved)
}

function Get-OutputExtension {
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet("exe", "shared", "static")]
        [string]$Kind
    )

    switch ($Kind) {
        "exe" {
            if ($script:IsWindowsHost) {
                return ".exe"
            }
            return ""
        }
        "shared" {
            if ($script:IsWindowsHost) {
                return ".dll"
            }
            if ($script:IsMacOSHost) {
                return ".dylib"
            }
            return ".so"
        }
        "static" {
            return ".a"
        }
    }
}

function Invoke-SanitizedCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Command
    )

    & $invokeScript -Command $Command
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

function Get-RuntimeStaticLibraryPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$ToolchainBuildDir
    )

    $candidates = @(
        (Join-Path $RepoRoot "$ToolchainBuildDir\runtime\backend\libwio_runtime.a"),
        (Join-Path $RepoRoot "$ToolchainBuildDir\runtime\libwio_runtime.a")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    return $null
}

function Get-PreferredRuntimeStaticLibraryPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$ToolchainBuildDir
    )

    $resolved = Get-RuntimeStaticLibraryPath -RepoRoot $RepoRoot -ToolchainBuildDir $ToolchainBuildDir
    if (-not [string]::IsNullOrWhiteSpace($resolved)) {
        return $resolved
    }

    return (Join-Path $RepoRoot "$ToolchainBuildDir\runtime\backend\libwio_runtime.a")
}

function Get-WioCompilerExecutablePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$ToolchainBuildDir,
        [Parameter(Mandatory = $true)]
        [string]$ToolchainConfig
    )

    $candidates = @(
        (Join-Path $RepoRoot "$ToolchainBuildDir\app\$ToolchainConfig\wio_app.exe"),
        (Join-Path $RepoRoot "$ToolchainBuildDir\app\wio_app.exe"),
        (Join-Path $RepoRoot "$ToolchainBuildDir\app\$ToolchainConfig\wio_app"),
        (Join-Path $RepoRoot "$ToolchainBuildDir\app\wio_app")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    return $null
}

$projectFile = Resolve-ProjectManifestFile -Value $Project
$projectRoot = Split-Path $projectFile -Parent
$manifest = Read-ProjectManifest -Path $projectFile

$schemaVersion = [int](Get-ManifestProperty -Object $manifest -Name "schemaVersion" -Default 1)
if ($schemaVersion -ne 1) {
    throw "Unsupported project manifest schemaVersion '$schemaVersion'. Expected 1."
}

$template = [string](Get-ManifestProperty -Object $manifest -Name "template" -Default "")
$projectName = [string](Get-ManifestProperty -Object $manifest -Name "name" -Default "")
if ([string]::IsNullOrWhiteSpace($projectName)) {
    throw "Manifest is missing a non-empty 'name'."
}

$toolchainManifest = Get-ManifestProperty -Object $manifest -Name "toolchain" -Default $null
$wioManifest = Get-ManifestProperty -Object $manifest -Name "wio" -Default $null
$hostManifest = Get-ManifestProperty -Object $manifest -Name "host" -Default $null
$buildManifest = Get-ManifestProperty -Object $manifest -Name "build" -Default $null
$outputsManifest = Get-ManifestProperty -Object $manifest -Name "outputs" -Default $null
$runManifest = Get-ManifestProperty -Object $manifest -Name "run" -Default $null

$projectConfig = if ([string]::IsNullOrWhiteSpace($Config)) {
    [string](Get-ManifestProperty -Object $buildManifest -Name "config" -Default "Debug")
} else {
    $Config
}

$toolchainBuildDir = [string](Get-ManifestProperty -Object $toolchainManifest -Name "buildDir" -Default "build")
$toolchainConfig = [string](Get-ManifestProperty -Object $toolchainManifest -Name "config" -Default $projectConfig)

$projectBuildDirValue = if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    [string](Get-ManifestProperty -Object $buildManifest -Name "buildDir" -Default ".wio-build")
} else {
    $BuildDir
}

$outputBaseName = [string](Get-ManifestProperty -Object $outputsManifest -Name "baseName" -Default "")
if ([string]::IsNullOrWhiteSpace($outputBaseName)) {
    $outputBaseName = [string](Get-ManifestProperty -Object $buildManifest -Name "outputName" -Default "")
}
if ([string]::IsNullOrWhiteSpace($outputBaseName)) {
    $outputBaseName = $projectName.ToLowerInvariant() -replace "[^a-z0-9_]+", "_"
    if ([string]::IsNullOrWhiteSpace($outputBaseName)) {
        $outputBaseName = "wio_project"
    }
}

$resolvedProjectBuildDir = Resolve-ProjectPath -ProjectRoot $projectRoot -Value $projectBuildDirValue
$outputDirectoryValue = [string](Get-ManifestProperty -Object $outputsManifest -Name "directory" -Default "")
$outputDirectory = if ([string]::IsNullOrWhiteSpace($outputDirectoryValue)) {
    Join-Path $resolvedProjectBuildDir "interop"
} else {
    Resolve-ProjectPath -ProjectRoot $projectRoot -Value $outputDirectoryValue
}

$wioTarget = [string](Get-ManifestProperty -Object $wioManifest -Name "target" -Default "shared")
if ($wioTarget -notin @("exe", "shared", "static")) {
    throw "Unsupported wio.target '$wioTarget'. Expected one of: exe, shared, static."
}

$wioEntryValue = [string](Get-ManifestProperty -Object $wioManifest -Name "entry" -Default "")
$resolvedWioEntry = if ([string]::IsNullOrWhiteSpace($wioEntryValue)) {
    Get-DefaultWioEntry -ProjectRoot $projectRoot
} else {
    (Resolve-DeclaredPaths -ProjectRoot $projectRoot -Values @($wioEntryValue) -Label "Wio entry" -Kind File)[0]
}

$wioSourceRootValues = Get-ManifestStringArray -Object $wioManifest -Name "sourceRoots"
if ($wioSourceRootValues.Count -eq 0 -and -not (Has-ManifestProperty -Object $wioManifest -Name "sourceRoots")) {
    $wioSourceRootValues = Get-ManifestStringArray -Object $wioManifest -Name "usePaths"
}
if ($wioSourceRootValues.Count -eq 0 -and -not (Has-ManifestProperty -Object $wioManifest -Name "sourceRoots") -and -not (Has-ManifestProperty -Object $wioManifest -Name "usePaths")) {
    $wioSourceRootValues = Get-ManifestStringArray -Object $wioManifest -Name "moduleDirs"
}
$wioSourceRoots = if ((Has-ManifestProperty -Object $wioManifest -Name "sourceRoots") -or
                      (Has-ManifestProperty -Object $wioManifest -Name "usePaths") -or
                      (Has-ManifestProperty -Object $wioManifest -Name "moduleDirs")) {
    Resolve-DeclaredPaths -ProjectRoot $projectRoot -Values $wioSourceRootValues -Label "Wio source root" -Kind Directory
} else {
    Get-DefaultWioSourceRoots -ProjectRoot $projectRoot -ResolvedEntry $resolvedWioEntry
}
$wioSourceRoots = Convert-ToUniquePathArray -Paths (@($wioSourceRoots) + @((Split-Path $resolvedWioEntry -Parent)))

$wioIncludeDirValues = Get-ManifestStringArray -Object $wioManifest -Name "includeDirs"
$wioIncludeDirs = if (Has-ManifestProperty -Object $wioManifest -Name "includeDirs") {
    Resolve-DeclaredPaths -ProjectRoot $projectRoot -Values $wioIncludeDirValues -Label "Wio include directory" -Kind Directory
} else {
    Get-DefaultWioIncludeDirs -ProjectRoot $projectRoot
}

$wioLinkDirValues = Get-ManifestStringArray -Object $wioManifest -Name "linkDirs"
$wioLinkDirs = if (Has-ManifestProperty -Object $wioManifest -Name "linkDirs") {
    Resolve-DeclaredPaths -ProjectRoot $projectRoot -Values $wioLinkDirValues -Label "Wio link directory" -Kind Directory
} else {
    Get-DefaultWioLinkDirs -ProjectRoot $projectRoot
}

$wioNativeSourceValues = Get-ManifestStringArray -Object $wioManifest -Name "nativeSources"
$wioNativeSources = if (Has-ManifestProperty -Object $wioManifest -Name "nativeSources") {
    Resolve-DeclaredPaths -ProjectRoot $projectRoot -Values $wioNativeSourceValues -Label "Wio native source" -Kind File
} else {
    Get-DefaultWioNativeSources -ProjectRoot $projectRoot
}

$wioLinkLibraries = Resolve-LinkLibrarySpecs -ProjectRoot $projectRoot -Values (Get-ManifestStringArray -Object $wioManifest -Name "linkLibraries")
$wioBackendArgs = Get-ManifestStringArray -Object $wioManifest -Name "backendArgs"
$wioExtraArgs = Get-ManifestStringArray -Object $wioManifest -Name "extraArgs"

$hostEnabled = Get-ManifestBoolean -Object $hostManifest -Name "enabled" -Default ($template -eq "hybrid-module")
if ($wioTarget -eq "exe" -and $hostEnabled) {
    throw "host.enabled cannot be true when wio.target is 'exe'."
}

$hostCompiler = [string](Get-ManifestProperty -Object $hostManifest -Name "compiler" -Default "g++")
$hostSourceValues = Get-ManifestStringArray -Object $hostManifest -Name "sources"
if ($hostSourceValues.Count -eq 0 -and -not (Has-ManifestProperty -Object $hostManifest -Name "sources")) {
    $legacyHostSource = [string](Get-ManifestProperty -Object $hostManifest -Name "source" -Default "")
    if (-not [string]::IsNullOrWhiteSpace($legacyHostSource)) {
        $hostSourceValues = @($legacyHostSource)
    }
}

$hostSourceRootsValues = Get-ManifestStringArray -Object $hostManifest -Name "sourceRoots"
$hostSourceFiles = @()
if ($hostEnabled) {
    if ((Has-ManifestProperty -Object $hostManifest -Name "sources") -or (Has-ManifestProperty -Object $hostManifest -Name "source")) {
        $hostSourceFiles = Resolve-DeclaredPaths -ProjectRoot $projectRoot -Values $hostSourceValues -Label "Host source" -Kind File
    }
    else {
        $hostSourceRoots = if (Has-ManifestProperty -Object $hostManifest -Name "sourceRoots") {
            Resolve-DeclaredPaths -ProjectRoot $projectRoot -Values $hostSourceRootsValues -Label "Host source root" -Kind Directory
        } else {
            Get-DefaultHostSourceRoots -ProjectRoot $projectRoot
        }

        $hostSourceFiles = Get-FilesByExtensionFromDirectories -Directories $hostSourceRoots -Extensions @(".c", ".cc", ".cpp", ".cxx")
    }

    if ($hostSourceFiles.Count -eq 0) {
        throw "No host source files were found. Set host.sources or create sources under host/."
    }
}

$hostIncludeDirValues = Get-ManifestStringArray -Object $hostManifest -Name "includeDirs"
$hostIncludeDirs = if (Has-ManifestProperty -Object $hostManifest -Name "includeDirs") {
    Resolve-DeclaredPaths -ProjectRoot $projectRoot -Values $hostIncludeDirValues -Label "Host include directory" -Kind Directory
} else {
    Get-DefaultHostIncludeDirs -ProjectRoot $projectRoot
}

$hostLinkDirValues = Get-ManifestStringArray -Object $hostManifest -Name "linkDirs"
$hostLinkDirs = if (Has-ManifestProperty -Object $hostManifest -Name "linkDirs") {
    Resolve-DeclaredPaths -ProjectRoot $projectRoot -Values $hostLinkDirValues -Label "Host link directory" -Kind Directory
} else {
    Get-DefaultHostLinkDirs -ProjectRoot $projectRoot
}

$hostLinkLibraries = Resolve-LinkLibrarySpecs -ProjectRoot $projectRoot -Values (Get-ManifestStringArray -Object $hostManifest -Name "linkLibraries")
$hostCompilerArgs = Get-ManifestStringArray -Object $hostManifest -Name "compilerArgs"

$passLibraryPath = Get-ManifestBoolean -Object $runManifest -Name "passLibraryPath" -Default ($wioTarget -eq "shared")
$runArgs = Get-ManifestStringArray -Object $runManifest -Name "args"
$runWorkingDirectoryValue = [string](Get-ManifestProperty -Object $runManifest -Name "workingDirectory" -Default "")
$runWorkingDirectory = if ([string]::IsNullOrWhiteSpace($runWorkingDirectoryValue)) {
    $projectRoot
} else {
    (Resolve-DeclaredPaths -ProjectRoot $projectRoot -Values @($runWorkingDirectoryValue) -Label "Run working directory" -Kind Directory)[0]
}

$wioName = [string](Get-ManifestProperty -Object $outputsManifest -Name "wioName" -Default "")
if ([string]::IsNullOrWhiteSpace($wioName)) {
    $wioName = $outputBaseName
}

$hostName = [string](Get-ManifestProperty -Object $outputsManifest -Name "hostName" -Default "")
if ([string]::IsNullOrWhiteSpace($hostName)) {
    $hostName = $outputBaseName + ".host"
}

$wioOutput = Join-Path $outputDirectory ($wioName + (Get-OutputExtension -Kind $wioTarget))
$hostOutput = Join-Path $outputDirectory ($hostName + (Get-OutputExtension -Kind "exe"))
$runtimeStaticLibraryHint = Get-PreferredRuntimeStaticLibraryPath -RepoRoot $repoRoot -ToolchainBuildDir $toolchainBuildDir

$projectInfo = [pscustomobject]@{
    schemaVersion = $schemaVersion
    projectFile = $projectFile
    projectRoot = $projectRoot
    projectName = $projectName
    template = $template
    config = $projectConfig
    toolchainBuildDir = $toolchainBuildDir
    toolchainConfig = $toolchainConfig
    buildDirectory = $resolvedProjectBuildDir
    outputDirectory = $outputDirectory
    sdkIncludeDir = (Join-Path $repoRoot "sdk\include")
    wio = [pscustomobject]@{
        entry = $resolvedWioEntry
        target = $wioTarget
        sourceRoots = @($wioSourceRoots)
        includeDirs = @($wioIncludeDirs)
        linkDirs = @($wioLinkDirs)
        nativeSources = @($wioNativeSources)
        linkLibraries = @($wioLinkLibraries)
        output = $wioOutput
    }
    host = [pscustomobject]@{
        enabled = $hostEnabled
        compiler = $hostCompiler
        sourceFiles = @($hostSourceFiles)
        includeDirs = @($hostIncludeDirs)
        linkDirs = @($hostLinkDirs)
        linkLibraries = @($hostLinkLibraries)
        output = $hostOutput
    }
    runtime = [pscustomobject]@{
        staticLibrary = $runtimeStaticLibraryHint
    }
    run = [pscustomobject]@{
        passLibraryPath = $passLibraryPath
        args = @($runArgs)
        workingDirectory = $runWorkingDirectory
    }
}

if ($Describe) {
    $projectInfo | ConvertTo-Json -Depth 10 -Compress
    return
}

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

$buildArgs = @(
    "-ExecutionPolicy", "Bypass",
    "-File", $buildScript,
    "-BuildDir", $toolchainBuildDir,
    "-Config", $toolchainConfig
)

if ($Configure) {
    $buildArgs += "-Configure"
}

& powershell @buildArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$wioExe = Get-WioCompilerExecutablePath -RepoRoot $repoRoot -ToolchainBuildDir $toolchainBuildDir -ToolchainConfig $toolchainConfig
if ([string]::IsNullOrWhiteSpace($wioExe)) {
    throw "Compiled wio_app executable was not found under '$repoRoot\$toolchainBuildDir'."
}

$wioCommand = @(
    $wioExe,
    $resolvedWioEntry,
    "--target", $wioTarget,
    "--output", $wioOutput
)

foreach ($sourceRoot in $wioSourceRoots) {
    $wioCommand += @("--module-dir", $sourceRoot)
}

foreach ($includeDir in $wioIncludeDirs) {
    $wioCommand += @("--include-dir", $includeDir)
}

foreach ($linkDir in $wioLinkDirs) {
    $wioCommand += @("--link-dir", $linkDir)
}

foreach ($linkLibrary in $wioLinkLibraries) {
    $wioCommand += @("--link-lib", $linkLibrary)
}

foreach ($nativeSource in $wioNativeSources) {
    $wioCommand += @("--backend-arg", $nativeSource)
}

foreach ($backendArg in $wioBackendArgs) {
    $wioCommand += @("--backend-arg", $backendArg)
}

if ($wioExtraArgs.Count -gt 0) {
    $wioCommand += $wioExtraArgs
}

Invoke-SanitizedCommand -Command $wioCommand

$runtimeStaticLibrary = $null
if ($wioTarget -eq "static") {
    $runtimeStaticLibrary = Get-RuntimeStaticLibraryPath -RepoRoot $repoRoot -ToolchainBuildDir $toolchainBuildDir
    if ([string]::IsNullOrWhiteSpace($runtimeStaticLibrary)) {
        throw "The runtime static library was not found under '$repoRoot\$toolchainBuildDir\runtime'."
    }
}

if ($hostEnabled) {
    $hostCommand = @(
        $hostCompiler,
        "-std=c++20",
        "-I", (Join-Path $repoRoot "sdk\include")
    )

    foreach ($sourceFile in $hostSourceFiles) {
        $hostCommand += $sourceFile
    }

    foreach ($includeDir in $hostIncludeDirs) {
        $hostCommand += @("-I", $includeDir)
    }

    foreach ($linkDir in $hostLinkDirs) {
        $hostCommand += @("-L", $linkDir)
    }

    foreach ($linkLibrary in $hostLinkLibraries) {
        $hostCommand += $linkLibrary
    }

    if ($wioTarget -eq "static") {
        $hostCommand += $wioOutput
        $hostCommand += $runtimeStaticLibrary
    }

    if (-not $script:IsWindowsHost) {
        $hostCommand += "-ldl"
    }

    if ($hostCompilerArgs.Count -gt 0) {
        $hostCommand += $hostCompilerArgs
    }

    $hostCommand += @("-o", $hostOutput)
    Invoke-SanitizedCommand -Command $hostCommand
}

Write-Host "Project  :" $projectName
Write-Host "Manifest :" $projectFile
Write-Host "Target   :" $wioTarget
Write-Host "Wio Out  :" $wioOutput
if ($hostEnabled) {
    Write-Host "Host EXE :" $hostOutput
}

if (-not $NoRun) {
    $runCommand = @()

    if ($hostEnabled) {
        $runCommand = @($hostOutput)
        if ($passLibraryPath -and $wioTarget -eq "shared") {
            $runCommand += $wioOutput
        }
    }
    elseif ($wioTarget -eq "exe") {
        $runCommand = @($wioOutput)
    }

    if ($runCommand.Count -gt 0) {
        if ($runArgs.Count -gt 0) {
            $runCommand += $runArgs
        }

        Push-Location $runWorkingDirectory
        try {
            Invoke-SanitizedCommand -Command $runCommand
        }
        finally {
            Pop-Location
        }
    }
}

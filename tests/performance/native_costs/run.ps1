param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [int]$Repetitions = 15,
    [int]$Count = 2000,
    [int]$TimeoutSeconds = 120,
    [string]$CppCompiler = 'g++',
    [ValidateSet('core', 'extended')][string]$Suite = 'core',
    [switch]$DisableAllocationCounters,
    [int]$LogicalProcessor = -1
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../Measurement.psm1') -Force
if ($Repetitions -lt 3 -or $Count -lt 1 -or $Count -gt 100000 -or $TimeoutSeconds -lt 1) {
    throw 'Require >=3 samples, count in [1, 100000], and a positive timeout.'
}
Initialize-MeasurementWindows
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Choose a fresh output directory.' }
$taskCpp = (Get-Command $CppCompiler).Source
$taskWio = Join-Path $taskRoot 'build/app/Debug/wio.exe'
$taskRuntime = Join-Path $taskRoot 'build/runtime/backend/libwio_runtime.a'
$taskFlags = @('-std=c++20', '-O2', '-ffp-contract=off', '-static', '-Wa,-mbig-obj')
if ($DisableAllocationCounters) { $taskFlags += '-DWIO_COST_NO_COUNTERS' }
$taskFixtureRoot = $PSScriptRoot
$taskFixtureFiles = @('costs.wio', 'reference.cpp')
if ($Suite -eq 'extended') {
    $taskFixtureRoot = Join-Path $PSScriptRoot 'extended'
    $taskFixtureFiles += 'workloads.h'
}
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'probe.h') -Destination $OutputDirectory
foreach ($taskFile in $taskFixtureFiles) {
    Copy-Item -LiteralPath (Join-Path $taskFixtureRoot $taskFile) -Destination $OutputDirectory
}
function Build([string]$program, [string[]]$arguments) {
    & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $program ($LASTEXITCODE)" }
}
$taskSourcePaths = @($PSCommandPath, (Join-Path $PSScriptRoot 'qualify.ps1'),
    (Join-Path $PSScriptRoot '../Measurement.psm1'), $taskWio, $taskCpp, $taskRuntime)
$taskSourcePaths += @(Get-ChildItem -LiteralPath $OutputDirectory -File | Select-Object -ExpandProperty FullName)
foreach ($taskTree in @('std', 'runtime/include', 'sdk/include')) {
    $taskSourcePaths += @(Get-ChildItem -LiteralPath (Join-Path $taskRoot $taskTree) -File -Recurse |
        Select-Object -ExpandProperty FullName)
}
$taskSourcesBefore = @(Get-MeasurementArtifacts $taskSourcePaths)
$taskPriorRoot = $env:WIO_ROOT
try {
    $env:WIO_ROOT = $taskRoot
    Build $taskWio @((Join-Path $OutputDirectory 'costs.wio'), '--emit-cpp', '--include-dir', $OutputDirectory)
} finally { $env:WIO_ROOT = $taskPriorRoot }
$taskGenerated = Join-Path $OutputDirectory 'costs.wio.cpp'
$taskSourcePaths += $taskGenerated
$taskSourcesBefore += @(Get-MeasurementArtifacts @($taskGenerated))
foreach ($taskVariant in @('cpp', 'wio')) {
    $taskSource = if ($taskVariant -eq 'cpp') { 'reference.cpp' } else { 'costs.wio.cpp' }
    $taskArguments = $taskFlags + @((Join-Path $OutputDirectory $taskSource), "-I$OutputDirectory",
        "-I$taskRoot/runtime/include", "-I$taskRoot/sdk/include")
    if ($taskVariant -eq 'wio' -or $Suite -eq 'extended') {
        $taskArguments += @($taskRuntime, '-lws2_32', '-lshell32')
    }
    Build $taskCpp ($taskArguments + @('-o', (Join-Path $OutputDirectory "$taskVariant.exe")))
}

$taskSourcesAfter = @(Get-MeasurementArtifacts $taskSourcePaths)
if (!(Test-MeasurementArtifacts $taskSourcesBefore $taskSourcesAfter)) {
    throw 'Sources/toolchain changed during build; qualification refused.'
}
$taskArtifactPaths = @(Get-ChildItem -LiteralPath $OutputDirectory -File |
    Where-Object { $_.Extension -in @('.exe', '.h', '.cpp', '.wio') } | Select-Object -ExpandProperty FullName)
$taskArtifacts = @(Get-MeasurementArtifacts $taskArtifactPaths | ForEach-Object {
    [pscustomobject]@{ Path=[IO.Path]::GetFileName($_.Path); Bytes=$_.Bytes; Hash=$_.Hash }
})
[pscustomobject]@{
    Schema=2; Benchmark='native-costs'; Revision=(& git -C $taskRoot rev-parse HEAD).Trim()
    Worktree=(@(& git -C $taskRoot status --short) -join "`n"); Utc=[DateTime]::UtcNow.ToString('o')
    Compiler=(& $taskCpp --version | Select-Object -First 1); Flags=$taskFlags
    Suite=$Suite; Count=$Count; CountersEnabled=(!$DisableAllocationCounters)
    SourcesBefore=$taskSourcesBefore; SourcesAfter=$taskSourcesAfter; SourcesUnchanged=$true
    Artifacts=$taskArtifacts
} | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'build.json') -Encoding utf8
# Compilation is finished before entering the pinned measurement phase.
& (Join-Path $PSScriptRoot 'qualify.ps1') -ArtifactDirectory $OutputDirectory `
    -OutputDirectory (Join-Path $OutputDirectory 'qualification') -Repetitions $Repetitions `
    -TimeoutSeconds $TimeoutSeconds -LogicalProcessor $LogicalProcessor

param([string]$ScratchDirectory = '')
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Measurement.psm1') -Force
$taskAssertions = 0
function Assert($Condition, [string]$Message) {
    if (!$Condition) { throw $Message }
    $script:taskAssertions++
}
function Must-Throw([scriptblock]$Action, [string]$Message) {
    $caught = $false
    try { & $Action | Out-Null } catch { $caught = $true }
    Assert $caught $Message
}
Assert ((Get-MeasurementMedian @(1, 4, 3, 2)) -eq 2.5) 'even median'
Assert ((Get-MeasurementMedian @(9, 1, 3)) -eq 3) 'odd median'
Assert ((Get-MeasurementStability @(1, 1.01, 1.02) 3).Accepted) 'stable series rejected'
Assert (!(Get-MeasurementStability @(1, 1, 3) 3).Accepted) '3x noise accepted'
Assert (!(Get-MeasurementStability @(1, 1.06, 1.12) 3).Accepted) 'chronological drift accepted'
Assert (!(Get-MeasurementStability @(1, 1, 1) 4).Accepted) 'missing sample accepted'
Assert (!(Get-MeasurementStability @(1, 1) 3).Accepted) 'two samples accepted'
Assert (!(Get-MeasurementStability @() 3).Accepted) 'empty series accepted'
foreach ($taskBad in @(0, -1, [double]::NaN, [double]::PositiveInfinity)) {
    Assert (!(Get-MeasurementStability @(1, $taskBad, 1) 3).Accepted) 'invalid duration accepted'
}
Must-Throw { Get-MeasurementStability @(1, 1, 1) 2 } 'invalid minimum accepted'
Must-Throw { Get-MeasurementStability @(1, 1, 1) 3 ([double]::NaN) } 'NaN limit accepted'
Must-Throw { Get-MeasurementStability @(1, 1, 1) 3 0.15 0 } 'zero limit accepted'
Assert ((Get-MeasurementStability @(1, 1, 1, 1, 1, 1.8, 1, 1, 1) 9).Reasons -contains 'range') 'outlier discarded'
$taskFirst = @{}
foreach ($taskRound in 0..5) {
    $taskOrder = @(Get-MeasurementOrder @('a', 'b', 'c') $taskRound)
    Assert (($taskOrder | Sort-Object -Unique).Count -eq 3) 'schedule duplicate/missing case'
    $taskFirst[$taskOrder[0]]++
}
foreach ($taskCase in @('a', 'b', 'c')) { Assert ($taskFirst[$taskCase] -eq 2) 'unbalanced first position' }
$taskArtifacts = @([pscustomobject]@{ Path='a'; Bytes=1; Hash='one' }, [pscustomobject]@{ Path='b'; Bytes=2; Hash='two' })
Assert (Test-MeasurementArtifacts $taskArtifacts @($taskArtifacts[1], $taskArtifacts[0])) 'artifact ordering affects identity'
Assert (!(Test-MeasurementArtifacts $taskArtifacts @($taskArtifacts[0]))) 'missing artifact accepted'
Assert (!(Test-MeasurementArtifacts $taskArtifacts @($taskArtifacts[0], [pscustomobject]@{ Path='b'; Bytes=2; Hash='changed' }))) 'changed binary accepted'

if ($IsWindows) {
    $taskOriginal = ([Diagnostics.Process]::GetCurrentProcess()).ProcessorAffinity.ToInt64()
    $taskContext = $null
    try {
        $taskContext = Enter-MeasurementAffinity
        $taskInfo = [Diagnostics.ProcessStartInfo]::new()
        $taskInfo.FileName = (Get-Process -Id $PID).Path
        $taskInfo.UseShellExecute = $false; $taskInfo.CreateNoWindow = $true
        $taskInfo.RedirectStandardOutput = $true
        foreach ($taskArgument in @('-NoProfile', '-Command', '([Diagnostics.Process]::GetCurrentProcess()).ProcessorAffinity.ToInt64()')) {
            $taskInfo.ArgumentList.Add($taskArgument)
        }
        $taskChild = [Diagnostics.Process]::Start($taskInfo)
        try {
            $taskOutput = $taskChild.StandardOutput.ReadToEndAsync()
            if (!$taskChild.WaitForExit(15000)) { $taskChild.Kill($true); throw 'affinity child timeout' }
            Assert ($taskChild.ExitCode -eq 0) 'affinity child failed'
            Assert ([long]$taskOutput.Result.Trim() -eq $taskContext.Mask) 'child did not inherit pre-launch affinity'
        } finally { $taskChild.Dispose() }
    } finally { if ($taskContext) { Exit-MeasurementAffinity $taskContext } }
    Assert (([Diagnostics.Process]::GetCurrentProcess()).ProcessorAffinity.ToInt64() -eq $taskOriginal) 'affinity not restored'
    Must-Throw { Enter-MeasurementAffinity 64 } 'invalid processor accepted'
    Assert (([Diagnostics.Process]::GetCurrentProcess()).ProcessorAffinity.ToInt64() -eq $taskOriginal) 'failed pin changed affinity'

    # Exercise the real runner's failure/evidence path without compiler builds,
    # existing workloads, or machine-speed expectations. where.exe with no args
    # intentionally fails; it is copied under the fixture's expected name.
    if (!$ScratchDirectory) {
        $ScratchDirectory = Join-Path $PSScriptRoot "../../build/measurement-contract-$([Guid]::NewGuid().ToString('N'))"
    }
    $taskScratch = [IO.Path]::GetFullPath($ScratchDirectory)
    if (Test-Path -LiteralPath $taskScratch) { throw 'Contract test requires a fresh scratch directory.' }
    $taskFixture = Join-Path $taskScratch 'artifacts/compute'
    New-Item -ItemType Directory -Path $taskFixture | Out-Null
    Copy-Item -LiteralPath (Join-Path $env:SystemRoot 'System32/where.exe') -Destination (Join-Path $taskFixture 'native.exe')
    $taskInfo = [Diagnostics.ProcessStartInfo]::new()
    $taskInfo.FileName = (Get-Process -Id $PID).Path
    $taskInfo.UseShellExecute = $false; $taskInfo.CreateNoWindow = $true
    $taskInfo.RedirectStandardOutput = $true; $taskInfo.RedirectStandardError = $true
    foreach ($taskArgument in @('-NoProfile', '-File', (Join-Path $PSScriptRoot 'mandelbrot_compute/qualify.ps1'),
        '-ArtifactDirectory', (Join-Path $taskScratch 'artifacts'), '-OutputDirectory', (Join-Path $taskScratch 'evidence'),
        '-NativeOnly', '-Repetitions', '3')) { $taskInfo.ArgumentList.Add($taskArgument) }
    $taskChild = [Diagnostics.Process]::Start($taskInfo)
    try {
        $taskOutput = $taskChild.StandardOutput.ReadToEndAsync()
        $taskError = $taskChild.StandardError.ReadToEndAsync()
        if (!$taskChild.WaitForExit(30000)) { $taskChild.Kill($true); throw 'failure evidence test timeout' }
        Assert ($taskChild.ExitCode -ne 0) 'failed benchmark returned success'
        $taskEvidence = Get-Content -LiteralPath (Join-Path $taskScratch 'evidence/results.json') -Raw | ConvertFrom-Json
        Assert (!$taskEvidence.Accepted) 'failed benchmark qualified'
        Assert ($taskEvidence.Samples.Count -eq 1 -and $taskEvidence.Samples[0].Status -eq 'failed') 'failed raw sample lost'
        Assert $taskEvidence.ArtifactsUnchanged 'failure artifact verification missing'
        Assert ($null -ne $taskEvidence.EnvironmentAfter -and $taskEvidence.Failure) 'failure finalization missing'
    } finally { $taskChild.Dispose() }
}
Write-Host "$taskAssertions measurement contract assertions passed; no timing threshold or old corpus."

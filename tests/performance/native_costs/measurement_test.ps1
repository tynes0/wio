param([string]$CppCompiler = 'g++', [string]$ScratchDirectory = '')
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../Measurement.psm1') -Force
$taskAssertions = 0
function Assert($Condition, [string]$Message) {
    if (!$Condition) { throw $Message }
    $script:taskAssertions++
}
foreach ($taskScript in @('run.ps1', 'qualify.ps1', '../Measurement.psm1')) {
    $taskTokens = $null; $taskErrors = $null
    [Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $taskScript),
        [ref]$taskTokens, [ref]$taskErrors) | Out-Null
    Assert ($taskErrors.Count -eq 0) "Parser error in $taskScript : $taskErrors"
}
$taskCulture = [Globalization.CultureInfo]::CurrentCulture
try {
    [Globalization.CultureInfo]::CurrentCulture = [Globalization.CultureInfo]::GetCultureInfo('tr-TR')
    $taskParsed = ConvertFrom-MeasurementCostOutput 'seconds=0.012345678 checksum=18446744073709551615 allocations=2 requested_bytes=16'
    Assert ($taskParsed.Status -eq 'passed') 'valid output rejected'
    Assert ($taskParsed.Seconds -eq 0.012345678) 'time parsed with current culture'
    Assert ($taskParsed.Checksum -eq '18446744073709551615') 'u64 checksum truncated'
    Assert ($taskParsed.Allocations -eq 2 -and $taskParsed.RequestedBytes -eq 16) 'counter parse failed'
} finally { [Globalization.CultureInfo]::CurrentCulture = $taskCulture }
foreach ($taskBad in @(
    'seconds=nan checksum=1 allocations=0 requested_bytes=0',
    'seconds=1,2 checksum=1 allocations=0 requested_bytes=0',
    'seconds=1.0 checksum=18446744073709551616 allocations=0 requested_bytes=0',
    'seconds=1.0 checksum=1 allocations=18446744073709551616 requested_bytes=0',
    'seconds=1.0 checksum=1 allocations=0 requested_bytes=18446744073709551616',
    'seconds=1.0 checksum=-1 allocations=0 requested_bytes=0',
    'prefix seconds=1.0 checksum=1 allocations=0 requested_bytes=0',
    'seconds=1.0 checksum=1 allocations=0 requested_bytes=0 suffix')) {
    Assert ((ConvertFrom-MeasurementCostOutput $taskBad).Status -eq 'malformed-output') 'malformed/overflow output accepted'
}
Assert ((ConvertFrom-MeasurementCostOutput 'seconds=0.000 checksum=1 allocations=0 requested_bytes=0').Status -eq 'invalid-time') 'zero duration accepted'

if (!$IsWindows) {
    Write-Host "$taskAssertions portable protocol/parser assertions passed. Windows affinity/process integration not run on this platform."
    exit 0
}
if (!$ScratchDirectory) { $ScratchDirectory = Join-Path $PSScriptRoot "../../../build/native-cost-measurement-contract-$([Guid]::NewGuid().ToString('N'))" }
$taskScratch = [IO.Path]::GetFullPath($ScratchDirectory)
if (Test-Path -LiteralPath $taskScratch) { throw 'Choose a fresh test scratch directory.' }
New-Item -ItemType Directory -Path $taskScratch | Out-Null
$taskFixture = Join-Path $taskScratch 'fixture.exe'
& $CppCompiler '-std=c++20' '-O2' '-static' '-pthread' (Join-Path $PSScriptRoot 'measurement_fixture.cpp') '-o' $taskFixture
if ($LASTEXITCODE -ne 0) { throw 'Synthetic protocol fixture compilation failed.' }
$taskOriginalMode = $env:WIO_COST_FIXTURE_MODE
try {
    $taskPreconditions = @('tampered', 'duplicate', 'nonlocal', 'invalidcount', 'invalidmode')
    foreach ($taskMode in (@('steady', 'counterfree', 'countermismatch', 'mismatch', 'malformed', 'unstable', 'mutation', 'timeout', 'large') + $taskPreconditions)) {
        $taskArtifact = Join-Path $taskScratch "$taskMode/artifacts"
        $taskOutputDir = Join-Path $taskScratch "$taskMode/evidence"
        New-Item -ItemType Directory -Path $taskArtifact | Out-Null
        Copy-Item -LiteralPath $taskFixture -Destination (Join-Path $taskArtifact 'cpp.exe')
        Copy-Item -LiteralPath $taskFixture -Destination (Join-Path $taskArtifact 'wio.exe')
        'synthetic mutable tracked header' | Set-Content -LiteralPath (Join-Path $taskArtifact 'probe.h')
        $taskArtifacts = @(foreach ($taskVariant in @('cpp', 'wio')) {
            [pscustomobject]@{ Path="$taskVariant.exe"; Hash=(Get-FileHash (Join-Path $taskArtifact "$taskVariant.exe")).Hash }
        })
        $taskManifest = [pscustomobject]@{ Suite='core'; Count=7
            CountersEnabled=($taskMode -notin @('counterfree', 'countermismatch')); Artifacts=$taskArtifacts }
        switch ($taskMode) {
            'duplicate' { $taskManifest.Artifacts += $taskArtifacts[0] }
            'nonlocal' { $taskManifest.Artifacts[0].Path = '../outside.exe' }
            'invalidcount' { $taskManifest.Count = 100001 }
            'invalidmode' { $taskManifest.CountersEnabled = 'false' }
        }
        $taskManifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $taskArtifact 'build.json')
        if ($taskMode -eq 'tampered') { [IO.File]::AppendAllText((Join-Path $taskArtifact 'cpp.exe'), 'invalidated-after-build') }
        $env:WIO_COST_FIXTURE_MODE = $taskMode
        $taskInfo = [Diagnostics.ProcessStartInfo]::new()
        $taskInfo.FileName = (Get-Process -Id $PID).Path
        $taskInfo.UseShellExecute = $false; $taskInfo.CreateNoWindow = $true
        $taskInfo.RedirectStandardOutput = $true; $taskInfo.RedirectStandardError = $true
        foreach ($taskArgument in @('-NoProfile', '-File', (Join-Path $PSScriptRoot 'qualify.ps1'),
            '-ArtifactDirectory', $taskArtifact, '-OutputDirectory', $taskOutputDir,
            '-Repetitions', '3', '-TimeoutSeconds', '1')) { $taskInfo.ArgumentList.Add($taskArgument) }
        $taskProcess = [Diagnostics.Process]::Start($taskInfo)
        try {
            $taskOut = $taskProcess.StandardOutput.ReadToEndAsync()
            $taskErr = $taskProcess.StandardError.ReadToEndAsync()
            if (!$taskProcess.WaitForExit(60000)) { $taskProcess.Kill($true); throw "Contract timeout: $taskMode" }
            if ($taskMode -in $taskPreconditions) {
                Assert ($taskProcess.ExitCode -ne 0) "Invalid input accepted: $taskMode"
                Assert (!(Test-Path -LiteralPath $taskOutputDir)) "Precondition failed after starting measurement: $taskMode"
                Assert ($taskErr.Result.Length -gt 0) "Missing input diagnostic: $taskMode"
                continue
            }
            $taskResult = Get-Content -LiteralPath (Join-Path $taskOutputDir 'results.json') -Raw | ConvertFrom-Json
            $taskSuccess = $taskMode -in @('steady', 'counterfree')
            Assert (($taskProcess.ExitCode -eq 0) -eq $taskSuccess) "Wrong exit for $taskMode : $($taskOut.Result) $($taskErr.Result)"
            Assert ($taskResult.Accepted -eq $taskSuccess) "Wrong acceptance for $taskMode"
            Assert ($null -ne $taskResult.EnvironmentAfter) "Missing finalized environment for $taskMode"
            Assert ($taskResult.ArtifactsUnchanged -eq ($taskMode -ne 'mutation')) "Wrong artifact mutation detection for $taskMode"
            switch ($taskMode) {
                'steady' {
                    Assert ($taskResult.Samples.Count -eq 76 -and $taskResult.OutputsCorrect -and $taskResult.SameBinaryControl.Accepted) 'steady fixture incomplete'
                    Assert (@($taskResult.Samples | Where-Object Warmup).Count -eq 19) 'wrong warm-up count'
                }
                'counterfree' {
                    Assert (@($taskResult.Samples | Where-Object { $null -ne $_.Allocations -or $null -ne $_.RequestedBytes }).Count -eq 0) 'counter-free metrics not null'
                    Assert ($taskResult.OutputsCorrect -and $taskResult.TimingQualified) 'counter-free acceptance failed'
                }
                'mismatch' { Assert ($taskResult.Samples[-1].Status -eq 'checksum-mismatch') 'checksum failure lost' }
                'countermismatch' { Assert ($taskResult.Samples[-1].Status -eq 'counter-mode-mismatch') 'counter mode mismatch accepted' }
                'malformed' { Assert ($taskResult.Samples[-1].Status -eq 'malformed-output') 'malformed output lost' }
                'unstable' { Assert ($taskResult.OutputsCorrect -and !$taskResult.TimingQualified -and !$taskResult.Failure) 'noise incorrectly reported as wrong output' }
                'mutation' { Assert (!$taskResult.OutputsCorrect -and $taskResult.Samples.Count -eq 76) 'mutation samples discarded' }
                'timeout' { Assert ($taskResult.Samples.Count -eq 1 -and $taskResult.Samples[0].Status -eq 'timeout') 'timeout not preserved' }
                'large' {
                    Assert ($taskResult.Samples[0].Output.Length -eq 70000 -and $taskResult.Samples[0].Error.Length -eq 70000) 'large stdout/stderr truncated or deadlocked'
                }
            }
        } finally { $taskProcess.Dispose() }
    }
} finally { $env:WIO_COST_FIXTURE_MODE = $taskOriginalMode }
Write-Host "$taskAssertions native cost measurement assertions passed. All reported fixture durations are synthetic; no machine-speed gate."

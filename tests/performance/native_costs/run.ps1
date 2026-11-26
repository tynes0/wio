param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [int]$Repetitions = 15, [int]$Count = 2000, [int]$TimeoutSeconds = 120,
    [string]$CppCompiler = 'g++',
    [ValidateSet('core', 'extended')][string]$Suite = 'core',
    [switch]$DisableAllocationCounters, [int]$LogicalProcessor = -1
)
# Compatibility forwarding only. New tests and workflows use Python directly.
$ErrorActionPreference = 'Stop'
$taskArguments = @('--output', $OutputDirectory, '--repetitions', "$Repetitions", '--count', "$Count",
    '--timeout', "$TimeoutSeconds", '--cxx', $CppCompiler, '--suite', $Suite, '--cpu', "$LogicalProcessor")
if ($DisableAllocationCounters) { $taskArguments += '--disable-allocation-counters' }
& python (Join-Path $PSScriptRoot 'run.py') @taskArguments
exit $LASTEXITCODE

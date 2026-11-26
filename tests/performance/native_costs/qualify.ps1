param(
    [Parameter(Mandatory)][string]$ArtifactDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [int]$Repetitions = 15, [int]$TimeoutSeconds = 120, [int]$LogicalProcessor = -1,
    [double]$MaxRelativeRange = 0.15, [double]$MaxRelativeDrift = 0.10,
    [switch]$LegacyInstrumentedCore
)
# Compatibility forwarding only. New tests and workflows use Python directly.
$ErrorActionPreference = 'Stop'
$taskArguments = @('--artifacts', $ArtifactDirectory, '--output', $OutputDirectory, '--repetitions', "$Repetitions",
    '--timeout', "$TimeoutSeconds", '--cpu', "$LogicalProcessor",
    '--max-range', $MaxRelativeRange.ToString([Globalization.CultureInfo]::InvariantCulture),
    '--max-drift', $MaxRelativeDrift.ToString([Globalization.CultureInfo]::InvariantCulture))
if ($LegacyInstrumentedCore) { $taskArguments += '--legacy-instrumented-core' }
& python (Join-Path $PSScriptRoot 'qualify.py') @taskArguments
exit $LASTEXITCODE

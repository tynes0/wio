param(
    [Parameter(Mandatory = $true, Position = 0, ValueFromRemainingArguments = $true)]
    [string[]]$Command
)

$ErrorActionPreference = "Stop"

if ($Command.Count -eq 0) {
    throw "No command was provided."
}

# Some Windows-hosted shells surface both 'Path' and 'PATH' in the process
# environment. MSBuild treats them as duplicate keys and fails before spawning
# CL.exe. Removing the duplicate uppercase entry keeps the effective path while
# restoring normal environment enumeration.
$effectivePath = $env:Path
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:Path = $effectivePath

$commandName = $Command[0]
$commandArgs = @()

if ($Command.Count -gt 1) {
    $commandArgs = $Command[1..($Command.Count - 1)]
}

& $commandName @commandArgs
exit $LASTEXITCODE

param(
    [string]$ReleaseDir = ".\artifacts\packages-release"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $ReleaseDir)) {
    throw "Release directory was not found: '$ReleaseDir'."
}

$releaseDirFull = [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $ReleaseDir).Path)
$outputPath = Join-Path $releaseDirFull "SHA256SUMS.txt"

$files = Get-ChildItem -LiteralPath $releaseDirFull -File |
    Where-Object { $_.Name -match '\.(zip|exe|ps1|md)$' } |
    Sort-Object Name

$lines = foreach ($file in $files) {
    $hash = Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256
    "{0} *{1}" -f $hash.Hash.ToUpperInvariant(), $file.Name
}

Set-Content -LiteralPath $outputPath -Value $lines -Encoding ascii
Write-Host "Updated: $outputPath"

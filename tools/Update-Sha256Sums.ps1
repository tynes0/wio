param(
    [string]$ReleaseDir = ".\artifacts\packages-release"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $ReleaseDir)) {
    throw "Release directory was not found: '$ReleaseDir'."
}

$releaseDirFull = [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $ReleaseDir).Path)
$outputPath = Join-Path $releaseDirFull "SHA256SUMS.txt"

function Get-Sha256Hex([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        try {
            $bytes = $sha256.ComputeHash($stream)
            return ([System.BitConverter]::ToString($bytes)).Replace("-", "")
        } finally {
            $sha256.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}

$files = Get-ChildItem -LiteralPath $releaseDirFull -File |
    Where-Object { $_.Name -match '\.(zip|exe|ps1|md)$' } |
    Sort-Object Name

$lines = foreach ($file in $files) {
    $hash = Get-Sha256Hex $file.FullName
    "{0} *{1}" -f $hash, $file.Name
}

Set-Content -LiteralPath $outputPath -Value $lines -Encoding ascii
Write-Host "Updated: $outputPath"

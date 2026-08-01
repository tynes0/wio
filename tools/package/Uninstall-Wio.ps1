param(
    [string]$InstallRoot,
    [switch]$NoPrompt,
    [switch]$KeepFiles
)

$ErrorActionPreference = "Stop"
$scriptRoot = Split-Path $MyInvocation.MyCommand.Path -Parent
if ([string]::IsNullOrWhiteSpace($InstallRoot)) { $InstallRoot = $scriptRoot }

$InstallRoot = [System.IO.Path]::GetFullPath($InstallRoot)
$wioExe = Join-Path $InstallRoot "bin\wio.exe"

if ((-not $NoPrompt) -and (-not $KeepFiles)) {
    $response = Read-Host "Remove Wio from '$InstallRoot'? [y/N]"
    if ($response -notmatch '^(?i:y|yes)$') {
        Write-Host "Uninstall cancelled."
        exit 1
    }
}

if (Test-Path -LiteralPath $wioExe) {
    & $wioExe env remove --wio-root $InstallRoot --set-user --remove-path --no-prompt
}

if ($KeepFiles) {
    Write-Host "Wio environment entries were removed. Files were kept at '$InstallRoot'."
    exit 0
}

$escapedRoot = $InstallRoot.Replace("'", "''")
$cleanupCommand = "Start-Sleep -Milliseconds 700; Remove-Item -LiteralPath '$escapedRoot' -Recurse -Force"
Start-Process -FilePath "powershell" -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", $cleanupCommand) -WindowStyle Hidden | Out-Null

Write-Host "Wio uninstall started for '$InstallRoot'."
Write-Host "This window can now be closed."
exit 0

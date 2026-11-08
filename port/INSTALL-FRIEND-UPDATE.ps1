param([string]$TargetFolder, [switch]$NoLaunch)
$ErrorActionPreference = 'Stop'
$payload = $PSScriptRoot
$manifest = Get-Content -LiteralPath (Join-Path $payload 'UPDATE-MANIFEST.json') -Raw | ConvertFrom-Json
foreach ($item in $manifest.files) {
    $file = Join-Path $payload $item.name
    if (!(Test-Path -LiteralPath $file) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $item.sha256) { throw "Update file is missing or damaged: $($item.name). Extract the complete ZIP again." }
}
if (!$TargetFolder) {
    Write-Host "Install CoD3 build $($manifest.version). Select the folder containing the CoD3MP.exe your friend launches."
    Add-Type -AssemblyName System.Windows.Forms
    $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
    $dialog.Description = 'Select your existing CoD3 game folder (contains CoD3MP.exe and game)'
    $dialog.SelectedPath = Split-Path -Parent $payload
    if ($dialog.ShowDialog() -ne 'OK') { throw 'Update cancelled; nothing installed.' }
    $TargetFolder = $dialog.SelectedPath
}
$target = (Resolve-Path -LiteralPath $TargetFolder).Path
if (!(Test-Path -LiteralPath (Join-Path $target 'game\codmp_xenonf.xex'))) {
    throw "This folder has no installed multiplayer game: $target. Select the existing game folder, possibly inside another CoD3-for-friend folder."
}
if (Get-Process -Name CoD3,CoD3MP -ErrorAction SilentlyContinue) { throw 'Close CoD3 and CoD3MP before installing.' }
# Expose duplicate installs so the user can correct an old shortcut.
Get-ChildItem -LiteralPath $target -Filter CoD3MP.exe -File -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.DirectoryName -ne $target -and $_.FullName -notlike '*friend-update-backup*' } |
    ForEach-Object { Write-Warning "Another executable exists at $($_.FullName). Launch the updated copy at $target\CoD3MP.exe." }
$backup = Join-Path $target ('friend-update-backup\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
foreach ($name in 'CoD3MP.exe','CoD3.exe','VERSION.txt','vehicles.txt','bot_kits.txt') {
    $existing = Join-Path $target $name
    if (Test-Path -LiteralPath $existing) { Copy-Item -LiteralPath $existing -Destination $backup }
}
foreach ($name in 'CoD3MP.exe','CoD3.exe','PATCH-NOTES.txt','UPDATE-README.txt','APPLY-SETTINGS.bat','Apply-Settings.ps1') {
    Copy-Item -LiteralPath (Join-Path $payload $name) -Destination $target -Force
}
# 0.5.197: the host's vehicle placements (the Kangaroo), so both games put
# the same vehicles in the same places (the old one is in the backup).
$vehicles = Join-Path $payload 'vehicles.txt'
if ((Test-Path -LiteralPath $vehicles) -and $target -ne $payload) { Copy-Item -LiteralPath $vehicles -Destination $target -Force; Write-Host 'Vehicle placements (vehicles.txt) installed: the Kangaroo shows on Merville.' }
# 0.5.204: the host's bot kits (his old one is in the backup).
$kits = Join-Path $payload 'bot_kits.txt'
if ((Test-Path -LiteralPath $kits) -and $target -ne $payload) { Copy-Item -LiteralPath $kits -Destination $target -Force; Write-Host 'Bot kits (bot_kits.txt) installed.' }
foreach ($name in 'dlc-tool','textures','kangaroo-technical-dossier') {
    $from = Join-Path $payload $name
    if ((Test-Path -LiteralPath $from) -and $target -ne $payload) { Copy-Item -LiteralPath $from -Destination $target -Recurse -Force }
}
foreach ($item in $manifest.files) {
    if ((Get-FileHash -LiteralPath (Join-Path $target $item.name) -Algorithm SHA256).Hash -ne $item.sha256) { throw "Installed file verification failed: $($item.name)" }
}
Copy-Item -LiteralPath (Join-Path $payload 'VERSION.txt') -Destination $target -Force
# Run in its own process: the settings helper uses exit on failure.
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $target 'Apply-Settings.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Executables installed, but settings could not be applied. Run APPLY-SETTINGS.bat with the game closed.' }
Write-Host "VERIFIED: build $($manifest.version) installed at $target" -ForegroundColor Green
Write-Host "Launch this exact file: $target\CoD3MP.exe"
Write-Host 'Your network addresses and player name were preserved.'
if (!$NoLaunch) { Start-Process -FilePath (Join-Path $target 'CoD3MP.exe') -WorkingDirectory $target }

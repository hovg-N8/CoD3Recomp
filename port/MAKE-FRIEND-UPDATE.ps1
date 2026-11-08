# Makes CoD3MP-update.zip beside this folder: the new CoD3MP.exe and CoD3.exe,
# the version and patch notes, the DLC tool, APPLY-SETTINGS.bat (sets the
# campaign guns and the other game settings to match yours) and, since
# 0.5.171, the upscaled textures (textures\replace). Your friend unzips it
# into their port folder and replaces the files.
$ErrorActionPreference = 'Stop'
$Here = $PSScriptRoot
$Parent = Split-Path -Parent $Here
$Out = Join-Path $Parent 'CoD3MP-update'
$Version0 = (Get-Content (Join-Path $Here 'VERSION.txt') -Raw).Trim()
$Zip = Join-Path $Parent ('CoD3MP-update-' + $Version0 + '.zip')
if ([IO.Path]::GetFullPath($Out) -ne [IO.Path]::GetFullPath((Join-Path $Parent 'CoD3MP-update'))) { throw 'Unexpected staging path' }
if (Test-Path -LiteralPath $Out) { Remove-Item -LiteralPath $Out -Recurse -Force }
if (Test-Path $Zip) { Remove-Item $Zip -Force }
New-Item -ItemType Directory $Out | Out-Null

$files = 'CoD3MP.exe', 'CoD3.exe', 'VERSION.txt', 'PATCH-NOTES.txt', 'UPDATE-README.txt', 'APPLY-SETTINGS.bat', 'Apply-Settings.ps1', 'INSTALL-FRIEND-UPDATE.bat', 'INSTALL-FRIEND-UPDATE.ps1'
foreach ($f in $files) {
    $from = Join-Path $Here $f
    if (-not (Test-Path $from)) { Write-Host "missing: $f" -ForegroundColor Red; exit 1 }
    Write-Host "  copying $f"
    Copy-Item $from $Out
}
# 0.5.197: the vehicle placements (vehicles.txt: the Kangaroo and the moved
# WC51 on Merville). Without it the friend's game made no Kangaroo: he saw
# the map's own Horch where yours is a Kangaroo.
# 0.5.204: it is required now (the Kangaroo is the point of it).
$vehicles = Join-Path $Here 'vehicles.txt'
if (-not (Test-Path $vehicles)) { Write-Host 'missing: vehicles.txt (the Kangaroo placement); put a Kangaroo down with vehicle_mark first' -ForegroundColor Red; exit 1 }
if (-not (Select-String -LiteralPath $vehicles -Pattern 'mp_kangaroo' -Quiet)) { Write-Host '  note: vehicles.txt has no mp_kangaroo line' -ForegroundColor Yellow }
Write-Host '  copying vehicles.txt (the Kangaroo)'; Copy-Item $vehicles $Out
# 0.5.204: the bots' kits (campaign guns for bots when he hosts).
$kits = Join-Path $Here 'bot_kits.txt'
if (Test-Path $kits) { Write-Host '  copying bot_kits.txt'; Copy-Item $kits $Out }
# 0.5.204: the Kangaroo write-up for him and his AI: only the dossier's own
# notes and indexes, not its old source snapshot or the generated game code
# under original-script-navigation (no game code goes between the teams).
$dossier = Join-Path $Here 'kangaroo-technical-dossier'
if (Test-Path $dossier) {
    $dOut = Join-Path $Out 'kangaroo-technical-dossier'
    New-Item -ItemType Directory $dOut | Out-Null
    Get-ChildItem -LiteralPath $dossier -File | Where-Object { $_.Extension -in '.md', '.csv', '.json', '.txt' } | ForEach-Object { Copy-Item -LiteralPath $_.FullName $dOut }
    Write-Host "  copying kangaroo-technical-dossier ($((Get-ChildItem $dOut -File).Count) notes)"
}
$dlc = Join-Path $Here 'dlc-tool'
if (-not (Test-Path $dlc)) { Write-Host 'missing: dlc-tool' -ForegroundColor Red; exit 1 }
Write-Host '  copying dlc-tool'
Copy-Item $dlc $Out -Recurse

# The upscaled textures: only the finished replacements (textures\replace),
# never the dumps (textures\dump) or used.txt.
$replace = Join-Path $Here 'textures\replace'
if (Test-Path $replace) {
    $count = (Get-ChildItem $replace -File -Recurse | Measure-Object).Count
    Write-Host "  copying textures\replace ($count pictures)"
    New-Item -ItemType Directory (Join-Path $Out 'textures') | Out-Null
    Copy-Item $replace (Join-Path $Out 'textures') -Recurse
    # 0.5.197: the HD world textures (textures\replace\hd) can be gigabytes:
    # they go in their own zip (MAKE-TEXTURE-PACK.bat), not every update.
    $hdCopy = Join-Path $Out 'textures\replace\hd'
    if (Test-Path $hdCopy) { Remove-Item $hdCopy -Recurse -Force; Write-Host '  (HD world textures left out: MAKE-TEXTURE-PACK.bat zips them)' }
} else {
    Write-Host '  no textures\replace here: the update goes without textures' -ForegroundColor Yellow
}

$version = (Get-Content (Join-Path $Here 'VERSION.txt') -Raw).Trim()
$entries = foreach ($name in 'CoD3MP.exe','CoD3.exe') {
    $original = Get-FileHash -LiteralPath (Join-Path $Here $name) -Algorithm SHA256
    $copied = Get-FileHash -LiteralPath (Join-Path $Out $name) -Algorithm SHA256
    if ($original.Hash -ne $copied.Hash) { throw "Package copy failed verification: $name" }
    [ordered]@{ name = $name; sha256 = $original.Hash }
}
[ordered]@{ version = $version; files = @($entries) } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $Out 'UPDATE-MANIFEST.json') -Encoding ASCII
Write-Host "`nZipping..."
$tar = Join-Path $env:SystemRoot 'System32\tar.exe'
Push-Location $Out
if (Test-Path $tar) { & $tar -a -c -f $Zip @(Get-ChildItem -Name) ; if ($LASTEXITCODE -ne 0) { throw "ZIP creation failed ($LASTEXITCODE)" } } else { Compress-Archive -Path (Join-Path $Out '*') -DestinationPath $Zip }
Pop-Location
Remove-Item $Out -Recurse -Force
if (-not (Test-Path $Zip)) { Write-Host 'The zip was not made.' -ForegroundColor Red; exit 1 }
$mb = [math]::Round((Get-Item $Zip).Length / 1MB)
$version = (Get-Content (Join-Path $Here 'VERSION.txt') -Raw).Trim()
Write-Host "`nDone: $Zip ($mb MB, version $version). Send that file to your friend." -ForegroundColor Green


# 2026-10-24 (0.5.433): LEAN by default: only what a friend who already has
# the port needs to update it: the two executables, the version, the patch
# notes, the installer and its settings helper (INSTALL-FRIEND-UPDATE runs
# Apply-Settings.ps1, so those two always go). About 80 MB instead of ~490.
#   MAKE-FRIEND-UPDATE.bat full    everything as before (a fresh friend: the
#                                  textures, DLC tool, vehicles, bot kits,
#                                  co-op notes, Kangaroo notes)
#   MAKE-FRIEND-UPDATE.bat source  also the source zip beside it (for his AI)
#   (both: MAKE-FRIEND-UPDATE.bat full source)
$Options = @($args | ForEach-Object { "$_".ToLowerInvariant() })
$Full = $Options -contains 'full'
$Source = $Full -or ($Options -contains 'source')
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
# 0.5.671: the Webley's files (custom\webley), in every update: CoD2's models
# (in hand and in the world), its colour, normal and specular maps, the
# texture slots (slots.txt and its grey, dark cube and test textures) and its
# shot and reload sounds, made from the host's own CoD2 by tools\cod2import.
# Without them the Webley is the Colt. Never the test dumps (fit_dump.bin,
# dump\).
$webley = Join-Path $Here 'custom\webley'
$webleyFiles = 'viewmodel_webley.mesh', 'weapon_webley.mesh', 'webley_c.png', 'webley_n.png', 'webley_s.png', 'slots.txt', 'dark_cube.png', 'grey.png', 'white.png', 'black.png', 'flat_n.png', 'flat_nm.png', 'wpnfire_webley.wav', 'wpnrld_webley_rev.wav', 'webley_reload_arms.bin', 'webley_reload_gun.bin', 'hud_webley.png'
$webleyOut = Join-Path $Out 'custom\webley'
New-Item -ItemType Directory $webleyOut -Force | Out-Null
foreach ($f in $webleyFiles) {
    $from = Join-Path $webley $f
    if (-not (Test-Path -LiteralPath $from)) { Write-Host "missing: custom\webley\$f" -ForegroundColor Red; exit 1 }
    Copy-Item -LiteralPath $from $webleyOut
}
Write-Host "  copying custom\webley ($($webleyFiles.Count) files: the Webley's models, textures and sounds)"
# 0.5.687: World at War's M2 flamethrower's files (custom\flamethrower), in
# every update: its models (in hand and in the world), textures and slot
# stand-ins, its flame's sprite sheets, kill icon and sounds, made from the
# host's own World at War by tools\waw\make_flamethrower.py. Without them it
# is a Thompson with no flame.
$flame = Join-Path $Here 'custom\flamethrower'
$flameFiles = 'viewmodel_flamethrower.mesh', 'weapon_flamethrower.mesh', 'flamethrower_c.png', 'flamethrower_world_c.png', 'slots.txt', 'world_slots.txt', 'dark_cube.png', 'flat_nm.png', 'grey.png', 'flame_fire.png', 'flame_lick.png', 'hud_m2_flamethrower.png', 'flm_ignite.wav', 'flm_loop.wav', 'flm_loop_far.wav', 'flm_cooldown.wav', 'flame_arms.bin'   # 0.5.851: flame_arms.bin, World at War's arm animations (0.5.709; missing before)
$flameOut = Join-Path $Out 'custom\flamethrower'
New-Item -ItemType Directory $flameOut -Force | Out-Null
foreach ($f in $flameFiles) {
    $from = Join-Path $flame $f
    if (-not (Test-Path -LiteralPath $from)) { Write-Host "missing: custom\flamethrower\$f" -ForegroundColor Red; exit 1 }
    Copy-Item -LiteralPath $from $flameOut
}
Write-Host "  copying custom\flamethrower ($($flameFiles.Count) files: the flamethrower's models, textures, flame and sounds)"
# 0.5.851: everything else added from other games and our new maps, in every
# update, so a friend has what the host has:
#  - custom\flare (World at War's signal flare), custom\mosin (CoD2's scoped
#    Mosin-Nagant), custom\mantle (World at War's climb spots on Seelow);
#  - the map packs Carentan (CoD2), Seelow and Seelow Night (World at War) in
#    saves\content\00000002;
#  - the game files the host made for tanks and the mortar (cut and repacked
#    from the friend's own game's archives: Merville's vehicle database and
#    effects, Saint-Lo's tanks, Crossroads' Stuka, Laison's mortar);
#  - the host's data files (vehicle placements, bot kits, gun fits, flags,
#    CTF routes).
foreach ($d in 'flare', 'mosin', 'mantle') {
    $from = Join-Path $Here ('custom\' + $d)
    if (-not (Test-Path -LiteralPath $from)) { Write-Host "missing: custom\$d" -ForegroundColor Red; exit 1 }
    $to = Join-Path $Out ('custom\' + $d)
    New-Item -ItemType Directory $to -Force | Out-Null
    Get-ChildItem -LiteralPath $from -File | Where-Object { $_.Name -notlike '*dump*' -and $_.Name -notlike 'vsconst_*' } | ForEach-Object { Copy-Item -LiteralPath $_.FullName $to }
    Write-Host "  copying custom\$d ($((Get-ChildItem -LiteralPath $to -File).Count) files)"
}
foreach ($pack in 'Carentan', 'Seelow', 'SeelowNight') {
    $from = Join-Path $Here ('saves\content\00000002\' + $pack)
    if (-not (Test-Path -LiteralPath $from)) { Write-Host "missing: map pack saves\content\00000002\$pack" -ForegroundColor Red; exit 1 }
    $to = Join-Path $Out 'saves\content\00000002'
    New-Item -ItemType Directory $to -Force | Out-Null
    Copy-Item -LiteralPath $from $to -Recurse
    $mb0 = [math]::Round(((Get-ChildItem -LiteralPath (Join-Path $to $pack) -File -Recurse | Measure-Object Length -Sum).Sum) / 1MB)
    Write-Host "  copying map pack $pack ($mb0 MB)"
}
$gameFiles = 'mp\mp_merv\mp_merv_vehicles.cod', 'mp\mp_merv\mp_merv_vehiclefx.cod', 'sp\vehfx\mp_merv_vehiclefx.cod', 'sp\saint_lo\vehicles_panzer.cod', 'sp\crssrds\vehicles_stuka.cod', 'sp\laison\laison_mortar.cod', 'sp\laison\laison_mortar_fx.cod', 'sp\laison\laison_mortar_zn08.cod'
foreach ($f in $gameFiles) {
    $from = Join-Path $Here ('game\' + $f)
    if (-not (Test-Path -LiteralPath $from)) { Write-Host "missing: game\$f" -ForegroundColor Red; exit 1 }
    $to = Join-Path $Out ('game\' + (Split-Path -Parent $f))
    New-Item -ItemType Directory $to -Force | Out-Null
    Copy-Item -LiteralPath $from $to
}
Write-Host "  copying $($gameFiles.Count) game files (tanks, tank effects, Stuka, mortar)"
foreach ($f in 'vehicles.txt', 'bot_kits.txt', 'world_models.txt', 'dom_flags.txt', 'ctf-flags.txt', 'ctf-routes.txt') {
    $from = Join-Path $Here $f
    if (Test-Path -LiteralPath $from) { Copy-Item -LiteralPath $from $Out; Write-Host "  copying $f" }
}
if ($Full) {
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
# 2026-10-18 (0.5.246): the co-op campaign launchers and notes (the co-op
# code itself is in CoD3.exe), and the newest co-op test findings zip from
# the folder above, for him and his AI to carry on from.
foreach ($f in 'PLAY-COOP-HOST.bat', 'PLAY-COOP-JOIN.bat', 'COOP-README.txt', 'LOCAL-COOP-TEST.bat') {
    $from = Join-Path $Here $f
    if (Test-Path $from) { Write-Host "  copying $f"; Copy-Item $from $Out }
}
$findings = Get-ChildItem -LiteralPath $Parent -Filter 'coop-test-*-for-friend.zip' -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
if ($findings) { Write-Host "  copying $($findings.Name) (latest co-op test findings)"; Copy-Item -LiteralPath $findings.FullName $Out }
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
} else {
    Write-Host '  (lean update: the executables, version, notes and installer only; MAKE-FRIEND-UPDATE.bat full for everything)'
}
# 0.6.4: the player launcher (V3) rides along in CoD3Launcher\ (exe, config, public key, readme),
# in lean and full zips alike.
$launcher = Join-Path $Here 'CoD3Launcher'
if (Test-Path -LiteralPath (Join-Path $launcher 'CoD3Launcher.exe')) {
    Write-Host '  copying CoD3Launcher (the player launcher)'
    Copy-Item -LiteralPath $launcher $Out -Recurse
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
# 0.5.314: the source code for him too, in its own zip beside the update:
# our own host code (CoD3Host), the build files and the docs, from
# CoD3Recomp-main. Never the generated game code (CoD3RecompLib\ppc_*), the
# game's files or the build output.
$SrcRoot = Join-Path $Parent 'CoD3Recomp-main\CoD3Recomp-main'
$SrcZip = Join-Path $Parent ('CoD3MP-source-' + $Version0 + '.zip')
if (-not $Source) {
    Write-Host "`n  (no source zip: MAKE-FRIEND-UPDATE.bat source makes one)"
} elseif (Test-Path -LiteralPath $SrcRoot) {
    $SrcOut = Join-Path $Parent 'CoD3MP-source'
    if (Test-Path -LiteralPath $SrcOut) { Remove-Item -LiteralPath $SrcOut -Recurse -Force }
    if (Test-Path -LiteralPath $SrcZip) { Remove-Item -LiteralPath $SrcZip -Force }
    New-Item -ItemType Directory $SrcOut | Out-Null
    $host0 = Join-Path $SrcRoot 'CoD3Host'
    $hostOut = Join-Path $SrcOut 'CoD3Host'
    New-Item -ItemType Directory $hostOut | Out-Null
    Get-ChildItem -LiteralPath $host0 -File | Where-Object { $_.Extension -in '.cpp', '.h', '.hpp', '.txt', '.md', '.rc', '.manifest', '.ico', '.hlsl' } | ForEach-Object { Copy-Item -LiteralPath $_.FullName $hostOut }
    foreach ($f in 'CMakeLists.txt', 'README.md', '.gitignore', '.gitattributes', '.gitmodules', 'BUILD-MULTIPLAYER.bat', 'BUILD-MULTIPLAYER.ps1', 'QUICK-REBUILD.bat') {
        $from = Join-Path $SrcRoot $f
        if (Test-Path -LiteralPath $from) { Copy-Item -LiteralPath $from $SrcOut }
    }
    $docs = Join-Path $SrcRoot 'docs'
    if (Test-Path -LiteralPath $docs) { Copy-Item -LiteralPath $docs $SrcOut -Recurse }
    # 0.5.671: the tools that make the Webley's and Carentan's files from a
    # player's own CoD2 and CoD3 (their Python only, no game data).
    foreach ($tool in 'cod2import', 'kapf', 'waw') {   # 0.5.687: waw, the flamethrower's
        $toolFrom = Join-Path $SrcRoot ('tools\' + $tool)
        if (-not (Test-Path -LiteralPath $toolFrom)) { continue }
        $toolOut = Join-Path $SrcOut ('tools\' + $tool)
        New-Item -ItemType Directory $toolOut -Force | Out-Null
        Get-ChildItem -LiteralPath $toolFrom -File | Where-Object { $_.Extension -in '.py', '.md', '.txt' } | ForEach-Object { Copy-Item -LiteralPath $_.FullName $toolOut }
    }
    $n = (Get-ChildItem -LiteralPath $SrcOut -File -Recurse | Measure-Object).Count
    Write-Host "`nZipping the source ($n files)..."
    Push-Location $SrcOut
    if (Test-Path $tar) { & $tar -a -c -f $SrcZip @(Get-ChildItem -Name) ; if ($LASTEXITCODE -ne 0) { throw "Source ZIP creation failed ($LASTEXITCODE)" } } else { Compress-Archive -Path (Join-Path $SrcOut '*') -DestinationPath $SrcZip }
    Pop-Location
    Remove-Item -LiteralPath $SrcOut -Recurse -Force
    $smb = [math]::Round((Get-Item -LiteralPath $SrcZip).Length / 1MB, 1)
    Write-Host "Source: $SrcZip ($smb MB)" -ForegroundColor Green
} else {
    Write-Host "  no source folder at ${SrcRoot}: the source zip was not made" -ForegroundColor Yellow
}
$version = (Get-Content (Join-Path $Here 'VERSION.txt') -Raw).Trim()
Write-Host "`nDone: $Zip ($mb MB, version $version). Send that file to your friend$(if ($Source) { ' (and the source zip)' })." -ForegroundColor Green


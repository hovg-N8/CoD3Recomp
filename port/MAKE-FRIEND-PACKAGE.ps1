# Makes CoD3-for-friend.zip beside this folder: everything a friend needs to
# play multiplayer with you for the first time, on the build you are on.
#   - the port (CoD3.exe, CoD3MP.exe and their DLLs), its version and notes
#   - the shader programs captured here (fewer to build in the first match)
#   - the DLC tool (to unpack the map packs)
#   - the upscaled textures (textures\replace)
#   - your settings (CoD3Recomp.ini), made theirs: their own name and
#     address left for the game to fill in, System Link pointed at you, the
#     texture dump off
# Not in it: your game data, saves, logs or debug files. Your friend needs
# their own disc image (CoD3.exe asks for it the first time).
$ErrorActionPreference = 'Stop'
$Here = $PSScriptRoot
$Parent = Split-Path -Parent $Here
$Out = Join-Path $Parent 'CoD3-for-friend'
$Zip = Join-Path $Parent 'CoD3-for-friend.zip'

if (Test-Path $Out) { Remove-Item $Out -Recurse -Force }
if (Test-Path $Zip) { Remove-Item $Zip -Force }
New-Item -ItemType Directory $Out | Out-Null

$files = 'CoD3.exe', 'CoD3MP.exe', 'avcodec-62.dll', 'avutil-60.dll', 'swresample-6.dll',
         'ffmpeg-LICENSE.txt', 'README.txt', 'START-HERE-FRIEND.txt'
foreach ($f in $files) {
    $from = Join-Path $Here $f
    if (-not (Test-Path $from)) { Write-Host "missing: $f" -ForegroundColor Red; exit 1 }
    Write-Host "  copying $f"
    Copy-Item $from $Out
}
foreach ($f in 'VERSION.txt', 'PATCH-NOTES.txt', 'PROFILE-MULTIPLAYER.bat', 'APPLY-SETTINGS.bat', 'Apply-Settings.ps1') {
    $from = Join-Path $Here $f
    if (Test-Path $from) { Write-Host "  copying $f"; Copy-Item $from $Out }
}
foreach ($d in 'shaders', 'dlc-tool') {
    $from = Join-Path $Here $d
    if (Test-Path $from) { Write-Host "  copying $d"; Copy-Item $from $Out -Recurse }
    elseif ($d -eq 'dlc-tool') { Write-Host 'missing: dlc-tool' -ForegroundColor Red; exit 1 }
}
$replace = Join-Path $Here 'textures\replace'
if (Test-Path $replace) {
    Write-Host '  copying textures\replace'
    New-Item -ItemType Directory (Join-Path $Out 'textures') | Out-Null
    Copy-Item $replace (Join-Path $Out 'textures') -Recurse
}

# Your settings, made your friend's.
$ini = Join-Path $Here 'CoD3Recomp.ini'
if (Test-Path $ini) {
    $lines = Get-Content $ini
    $mine = ($lines | Where-Object { $_ -match '^\s*lan_address\s*=' } | Select-Object -First 1)
    $myAddress = if ($mine) { ($mine -split '=', 2)[1].Trim() } else { '' }
    $theirs = foreach ($line in $lines) {
        if ($line -match '^\s*lan_address\s*=') { continue }
        elseif ($line -match '^\s*player_name\s*=') { 'player_name = Friend' }
        elseif ($line -match '^\s*system_link_peers\s*=' -and $myAddress) { "system_link_peers = $myAddress" }
        elseif ($line -match '^\s*texture_dump\s*=') { 'texture_dump = 0' }
        else { $line }
    }
    $theirs | Set-Content (Join-Path $Out 'CoD3Recomp.ini') -Encoding ASCII
    if ($myAddress) { Write-Host "  settings copied; their System Link peer is you ($myAddress)" }
    else { Write-Host '  settings copied (no lan_address in yours: they set the peer in F11 themselves)' -ForegroundColor Yellow }
}

Write-Host "`nZipping (a minute or two)..."
$tar = Join-Path $env:SystemRoot 'System32\tar.exe'
if (Test-Path $tar) {
    Push-Location $Parent
    & $tar -a -c -f 'CoD3-for-friend.zip' 'CoD3-for-friend'
    Pop-Location
} else {
    Compress-Archive -Path $Out -DestinationPath $Zip
}
if (-not (Test-Path $Zip)) { Write-Host 'The zip was not made.' -ForegroundColor Red; exit 1 }
$mb = [math]::Round((Get-Item $Zip).Length / 1MB)
$version = if (Test-Path (Join-Path $Here 'VERSION.txt')) { (Get-Content (Join-Path $Here 'VERSION.txt') -Raw).Trim() } else { '?' }
Write-Host "`nDone: $Zip ($mb MB, version $version). Send that file to your friend." -ForegroundColor Green

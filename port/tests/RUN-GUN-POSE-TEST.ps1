# The gun pose test (0.5.206): starts an Eder Dam match with cheats (the pad
# goes through the menus by itself), god mode, your own gun and HUD hidden,
# adds two bots and stands them in front of you: each campaign gun beside the
# multiplayer gun it copies, from the right side, the front and the left
# (bot_pose_test, bots.cpp), a screenshot of each. The game is closed once
# the log says "pose test: done". The log and the screenshots are then put
# together in pose-test-<date-time>\ beside CoD3MP.exe.
#
# Put this beside CoD3MP.exe and run RUN-GUN-POSE-TEST.bat. Takes about three
# minutes; leave the game window alone while it runs.
param([string]$Map = 'ederdam', [int]$Distance = 100, [int]$StartAt = 45)

$ErrorActionPreference = 'Stop'
$Here = $PSScriptRoot
$Exe = Join-Path $Here 'CoD3MP.exe'
if (-not (Test-Path $Exe)) { Write-Host "CoD3MP.exe is not beside this script ($Here)" -ForegroundColor Red; exit 1 }
$Shots = Join-Path $Here 'screenshots'
$before = @{}
if (Test-Path $Shots) { Get-ChildItem $Shots -Filter *.png | ForEach-Object { $before[$_.Name] = $true } }

$s = $StartAt
$env:COD3_CMD = "20:devmap $Map;$($s):god;$($s+1):cg_drawgun 0;$($s+2):cg_draw2D 0;$($s+4):bot_pose_test $Distance"
$env:COD3_NOPAD = '1'
$env:COD3_PAD = '5:start 8:a 28:a 31:a 34:a 37:a'
$Log = Join-Path $Here 'pose-test-output.log'
Write-Host "Starting the pose test on $Map"
$p = Start-Process -FilePath $Exe -WorkingDirectory $Here -PassThru -RedirectStandardOutput $Log
$deadline = (Get-Date).AddMinutes(6)
while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 1
    if ((Test-Path $Log) -and (Select-String -Path $Log -Pattern 'pose test: done' -Quiet)) { Start-Sleep -Seconds 2; break }
}
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
Remove-Item Env:COD3_CMD, Env:COD3_NOPAD, Env:COD3_PAD -ErrorAction SilentlyContinue

$Out = Join-Path $Here ('pose-test-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $Out | Out-Null
Copy-Item $Log $Out -ErrorAction SilentlyContinue
Copy-Item (Join-Path $Here 'world_models.txt') $Out -ErrorAction SilentlyContinue
$new = 0
if (Test-Path $Shots) { Get-ChildItem $Shots -Filter *.png | Where-Object { -not $before.ContainsKey($_.Name) } | ForEach-Object { Copy-Item $_.FullName $Out; $new++ } }
$copied = Join-Path $Out 'pose-test-output.log'
if (Test-Path $copied) { Select-String -Path $copied -Pattern '^(world fit|pose test|bots: pose)' | ForEach-Object { $_.Line } | Set-Content (Join-Path $Out 'summary.txt') }
Write-Host "Done: $new screenshots and the log in $Out" -ForegroundColor Green

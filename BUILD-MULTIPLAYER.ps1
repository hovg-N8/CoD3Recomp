# Builds CoD3MP.exe, the multiplayer program, beside CoD3.exe.
#
# Double-click BUILD-MULTIPLAYER.bat beside this file. It needs what
# BUILD-CONTROLLER-FIX already set up (the tools, XenonRecomp, the single
# player recompile), and then:
#
#   1. teaches XenonRecomp the instructions only the multiplayer executable
#      uses (tools/patches/XenonRecomp-mp.patch) and rebuilds it
#   2. recompiles codmp_xenonf.xex from your installed game into
#      CoD3RecompLib/ppc_mp (scripts/recompile_mp.py) and patches the script
#      engine's seams (scripts/patch_recomp.py mp)
#   3. builds CoD3MP.exe, and CoD3.exe again (the two share the host code),
#      and puts both in the game folder.

$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
$Port = Join-Path (Split-Path -Parent (Split-Path -Parent $Root)) 'CoD3-PC-Port-0.4.0'
$Game = Join-Path $Port 'game'

function Step([string]$text) { Write-Host "`n==> $text" -ForegroundColor Cyan }

# Claude (2026-10-23): a test started for Claude after a build, or instead of
# one. test-request.txt beside this file names one of the game folder's
# benchmark batch files (BENCH-*.bat or BIG-PROFILE-32.bat, nothing else); it
# is started (the game runs and closes itself) and the request removed. A
# build request whose text begins "test only" builds nothing, only starts
# the test. Claude's clicks in Explorer could not start the batch files.
$TestRequest = Join-Path $Root 'test-request.txt'
function Start-RequestedTest {
    if (-not (Test-Path $TestRequest)) { return }
    $bat = ((Get-Content $TestRequest -Raw -ErrorAction SilentlyContinue) -replace '\s+', ' ').Trim()
    Remove-Item $TestRequest -Force -ErrorAction SilentlyContinue
    if ($bat -notmatch '^(BENCH-[A-Za-z0-9-]+|BIG-PROFILE-32)\.bat$') { Write-Host "    test request ignored: $bat" -ForegroundColor Yellow; return }
    $path = Join-Path $Port $bat
    if (-not (Test-Path $path)) { Write-Host "    test request: $bat is not in the game folder" -ForegroundColor Yellow; return }
    Start-Process -FilePath $path -WorkingDirectory $Port
    Write-Host "    started the test $bat" -ForegroundColor Green
}
$Taken = Join-Path $Root 'build-request.taken.txt'
if ((Test-Path $Taken) -and ((Get-Content $Taken -Raw -ErrorAction SilentlyContinue) -match '^\s*test only')) {
    Start-RequestedTest
    Write-Host "`nDone. (a test only; nothing built)" -ForegroundColor Green
    exit 0
}
function Fail([string]$text) { Write-Host "`nSTOPPED: $text" -ForegroundColor Red; exit 1 }

# A native command's warnings on stderr are not failures; its exit code is.
function Run([string]$what, [scriptblock]$command) {
    $ErrorActionPreference = 'Continue'
    & $command
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($code -ne 0) { Fail "$what (exit $code)" }
}

Step 'Checking what is needed'
$vsWhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vs = if (Test-Path $vsWhere) { & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath } else { $null }
if (-not $vs) { Fail 'Visual Studio Build Tools not found: run BUILD-CONTROLLER-FIX first' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path 'C:\Program Files\LLVM\bin\clang-cl.exe')) { Fail 'LLVM / Clang not found: run BUILD-CONTROLLER-FIX first' }
$env:PATH = "C:\Program Files\LLVM\bin;$env:PATH"
$Xenon = Join-Path $Root 'tools\XenonRecomp'
if (-not (Test-Path (Join-Path $Xenon 'CMakeLists.txt'))) { Fail 'tools\XenonRecomp is missing: run BUILD-CONTROLLER-FIX first' }
$Mp = Join-Path $Game 'codmp_xenonf.xex'
if (-not (Test-Path $Mp)) { Fail "the multiplayer executable is not in the installed game ($Mp)" }
if (-not (Test-Path (Join-Path $Root 'thirdparty\ffmpeg\include\libavcodec\avcodec.h'))) { Fail 'thirdparty\ffmpeg\include is missing (the game would be silent)' }
Write-Host '    all there' -ForegroundColor Green

Step 'Teaching the recompiler the multiplayer instructions'
$recompiler = Join-Path $Xenon 'XenonRecomp\recompiler.cpp'
if (-not (Select-String -Path $recompiler -Pattern 'case PPC_INST_BDNZT:' -Quiet)) {
    Run 'the multiplayer patch did not apply to XenonRecomp' { git -C $Xenon apply (Join-Path $Root 'tools\patches\XenonRecomp-mp.patch') }
}
$ToolBuild = Join-Path $Root 'tools\build'
Run 'XenonRecomp did not build' { cmd /c "call `"$vcvars`" >nul 2>&1 && cmake -S `"$Xenon`" -B `"$ToolBuild`" -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -Wno-dev && cmake --build `"$ToolBuild`" --target XenonRecomp XenonAnalyse" }
$ScanBuild = Join-Path $Root 'tools\build-scan'
Run 'CoD3Scan did not build' { cmd /c "call `"$vcvars`" >nul 2>&1 && cmake -S `"$Root\tools\CoD3Scan`" -B `"$ScanBuild`" -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -Wno-dev && cmake --build `"$ScanBuild`"" }

Step 'Recompiling the multiplayer executable'
$Private = Join-Path $Root 'CoD3RecompLib\private'
New-Item -ItemType Directory -Force $Private | Out-Null
Copy-Item $Mp (Join-Path $Private 'codmp_xenonf.xex') -Force
Run 'recompile_mp.py failed' { python (Join-Path $Root 'scripts\recompile_mp.py') $ToolBuild (Join-Path $ScanBuild 'CoD3Scan') }
Run 'patch_recomp.py mp failed' { python (Join-Path $Root 'scripts\patch_recomp.py') mp }

# The version: the newest entry of the patch notes (docs\PATCH-NOTES.md, its
# first "## 0.5.71:" heading), so the game, VERSION.txt and the notes always
# say the same number. (It used to be one higher than the last build's,
# which fell behind the notes whenever a build failed or was skipped.)
# Without notes it is still one higher than the last build's.
$VersionFile = Join-Path $Port 'VERSION.txt'
$Current = if (Test-Path $VersionFile) { (Get-Content $VersionFile -Raw).Trim() } else { '0.5.0' }
$Notes = Join-Path $Root 'docs\PATCH-NOTES.md'
$Version = $null
if (Test-Path $Notes) {
    $Heading = Select-String -Path $Notes -Pattern '^## (\d+\.\d+\.\d+)' | Select-Object -First 1
    if ($Heading) { $Version = $Heading.Matches[0].Groups[1].Value }
}
if (-not $Version) {
    $Parts = $Current.Split('.')
    $Parts[$Parts.Count - 1] = [string]([int]$Parts[$Parts.Count - 1] + 1)
    # The last number stops at 999: after 0.5.999 comes 0.6.0.
    if ([int]$Parts[$Parts.Count - 1] -gt 999) {
        $Parts[$Parts.Count - 1] = '0'
        $Parts[$Parts.Count - 2] = [string]([int]$Parts[$Parts.Count - 2] + 1)
    }
    $Version = $Parts -join '.'
}
Set-Content -Path (Join-Path $Root 'CoD3Host\version_number.h') -Value "#define COD3_VERSION `"$Version`"" -Encoding ASCII
Write-Host "    building version $Version (the last was $Current)" -ForegroundColor Green

Step 'Building CoD3MP.exe and CoD3.exe'
$Build = Join-Path $Root 'build'
Run 'the build failed' { cmd /c "call `"$vcvars`" >nul 2>&1 && cmake -S `"$Root`" -B `"$Build`" -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -Wno-dev && cmake --build `"$Build`" --target CoD3MP CoD3" }

Step 'Putting them in your game folder'
foreach ($name in @('CoD3MP', 'CoD3')) {
    $exe = Join-Path $Build "CoD3Host\$name.exe"
    if (-not (Test-Path $exe)) { Fail "the build finished but $exe is not there" }
    Copy-Item $exe (Join-Path $Port "$name.exe") -Force
    $pdb = Join-Path $Build "CoD3Host\$name.pdb"
    if (Test-Path $pdb) { Copy-Item $pdb (Join-Path $Port "$name.pdb") -Force }
}

Set-Content -Path $VersionFile -Value $Version -Encoding ASCII
# The patch notes beside the game, as of this build.
if (Test-Path $Notes) { Copy-Item $Notes (Join-Path $Port 'PATCH-NOTES.txt') -Force }
Write-Host "`nVersion $Version is in your game folder." -ForegroundColor Green
Write-Host "`nDone. CoD3MP.exe is in $Port (its log is CoD3MP.log). Multiplayer from the single player menu starts it too." -ForegroundColor Green
Start-RequestedTest

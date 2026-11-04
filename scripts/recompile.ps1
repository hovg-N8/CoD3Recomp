# Regenerates the recompiled C++ for Call of Duty 3 from default.xex.
#
# Run this after changing config/CoD3.toml, after changing XenonRecomp, or to
# reproduce the contents of CoD3RecompLib/ppc from scratch. Everything in ppc/
# is generated, so it is safe to delete.
#
#   .\scripts\recompile.ps1                 # regenerate C++ only
#   .\scripts\recompile.ps1 -BuildTools      # rebuild XenonRecomp first
#   .\scripts\recompile.ps1 -SwitchTables    # regenerate the jump table TOML too
#   .\scripts\recompile.ps1 -BuildLib        # also compile the result

[CmdletBinding()]
param(
    [switch]$BuildTools,
    [switch]$SwitchTables,
    [switch]$BuildLib,
    [switch]$BuildHost,
    [int]$Jobs = [Environment]::ProcessorCount
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot

# --- Toolchain -------------------------------------------------------------
# Clang is required: the recompiled code uses Clang intrinsics and the alias
# attribute. The MSVC environment supplies the Windows SDK headers only.
$LlvmBin = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin'
$VsInstaller = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer'
$VcVars = @(
    'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
    'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
    'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
    'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat'
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
) | Where-Object { Test-Path $_ } | Select-Object -First 1
# Any other Visual Studio install, found the way Microsoft's own tools find it.
if (-not $VcVars) {
    $VsWhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $VsWhere) {
        $VsPath = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($VsPath) {
            $Candidate = Join-Path $VsPath 'VC\Auxiliary\Build\vcvars64.bat'
            if (Test-Path $Candidate) { $VcVars = $Candidate }
        }
    }
}

if (-not $VcVars) { throw "vcvars64.bat not found. Install Visual Studio Build Tools." }
if (-not (Test-Path "$LlvmBin\clang-cl.exe")) { throw "clang-cl not found at $LlvmBin." }
$env:PATH = "$VsInstaller;$LlvmBin;$env:PATH"

function Invoke-InVsEnv([string]$Command) {
    # CMake writes deprecation warnings to stderr, and with ErrorActionPreference
    # set to Stop PowerShell turns any stderr from a native command into a
    # terminating NativeCommandError. The exit code is what actually says
    # whether the command worked.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        cmd /c "call `"$VcVars`" >nul 2>&1 && $Command"
    }
    finally {
        $ErrorActionPreference = $previous
    }
    if ($LASTEXITCODE -ne 0) { throw "Command failed (exit $LASTEXITCODE): $Command" }
}

$Xex         = Join-Path $Root 'CoD3RecompLib\private\default.xex'
$Config      = Join-Path $Root 'CoD3RecompLib\config\CoD3.toml'
$SwitchToml  = Join-Path $Root 'CoD3RecompLib\config\CoD3_switch_tables.toml'
$PpcDir      = Join-Path $Root 'CoD3RecompLib\ppc'
$ContextHdr  = Join-Path $Root 'tools\XenonRecomp\XenonUtils\ppc_context.h'
$ToolBuild   = Join-Path $Root 'tools\build'
$Recomp      = Join-Path $ToolBuild 'XenonRecomp\XenonRecomp.exe'
$Analyse     = Join-Path $ToolBuild 'XenonAnalyse\XenonAnalyse.exe'

if (-not (Test-Path $Xex)) {
    throw "default.xex missing at $Xex. Copy it from your own Xbox 360 disc dump."
}

# --- 1. Host tools ---------------------------------------------------------
if ($BuildTools -or -not (Test-Path $Recomp)) {
    Write-Host '==> Building XenonRecomp and XenonAnalyse' -ForegroundColor Cyan
    Invoke-InVsEnv "cmake -S `"$Root\tools\XenonRecomp`" -B `"$ToolBuild`" -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5"
    Invoke-InVsEnv "cmake --build `"$ToolBuild`" --target XenonRecomp XenonAnalyse --parallel $Jobs"
}

# --- 2. Jump tables --------------------------------------------------------
# Regenerating this overwrites the checked-in table, so it is opt-in.
if ($SwitchTables) {
    Write-Host '==> Detecting jump tables' -ForegroundColor Cyan
    & $Analyse $Xex $SwitchToml
    $n = (Select-String -Path $SwitchToml -Pattern '^\[\[switch\]\]').Count
    Write-Host "    $n switch tables"
}

# --- 3. Recompile ----------------------------------------------------------
Write-Host '==> Recompiling PowerPC to C++' -ForegroundColor Cyan
New-Item -ItemType Directory -Force $PpcDir | Out-Null
Get-ChildItem $PpcDir -File | Remove-Item -Force

$Log = Join-Path $Root 'recomp.log'
cmd /c "`"$Recomp`" `"$Config`" `"$ContextHdr`" > `"$Log`" 2>&1"
if ($LASTEXITCODE -ne 0) { throw "XenonRecomp failed (exit $LASTEXITCODE). See $Log" }

# Any line that is not a progress percentage is a real diagnostic. A clean run
# produces none: no unrecognized instruction, no out-of-range switch case.
$Diagnostics = Get-Content $Log | Where-Object { $_ -notmatch '^Recompiling functions' }
$Functions = (Select-String -Path "$PpcDir\ppc_recomp.*.cpp" -Pattern '^PPC_FUNC_IMPL').Count
$Units = (Get-ChildItem "$PpcDir\*.cpp").Count

Write-Host "    $Functions functions across $Units translation units"
if ($Diagnostics) {
    Write-Host "    $($Diagnostics.Count) diagnostics:" -ForegroundColor Yellow
    $Diagnostics | Group-Object { ($_ -split ' at ')[0] } |
        Sort-Object Count -Descending | Select-Object -First 20 |
        ForEach-Object { Write-Host ("      {0,6}  {1}" -f $_.Count, $_.Name) -ForegroundColor Yellow }
    Write-Host "    Full log: $Log" -ForegroundColor Yellow
} else {
    Write-Host '    clean: every instruction translated' -ForegroundColor Green
}

# A branch the recompiler could not place lands as "// ERROR <address>" and a
# return: the function it is in is cut short (see the functions list in the
# config). None is the goal; each one is a function doing less than it should.
$Errors = (Select-String -Path "$PpcDir\ppc_recomp.*.cpp" -Pattern '// ERROR [0-9A-F]+').Count
if ($Errors -gt 0) {
    Write-Host "    $Errors branches out of their function (grep '// ERROR' in $PpcDir); extend those functions in the config" -ForegroundColor Yellow
}

# --- 3b. The seams the recompiler cannot express -------------------------
# The engine's script threads resume in the middle of a function; the host
# runs them on fibers, and patch_recomp.py wires the resume point into
# sub_824A6150. Without it the script scheduler spins forever at the first
# level, so this is not optional.
Write-Host '==> Patching the coroutine seam' -ForegroundColor Cyan
python (Join-Path $Root 'scripts\patch_recomp.py') main
if ($LASTEXITCODE -ne 0) { throw 'patch_recomp.py main failed' }

# --- 4. Compile ------------------------------------------------------------
if ($BuildLib -or $BuildHost) {
    # The kernel stubs are generated from the import list, so a XEX with
    # different imports produces a different set of stubs.
    Write-Host '==> Generating kernel import stubs' -ForegroundColor Cyan
    python (Join-Path $Root 'scripts\gen_import_stubs.py')
    if ($LASTEXITCODE -ne 0) { throw 'gen_import_stubs.py failed' }

    Write-Host '==> Compiling' -ForegroundColor Cyan
    $Build = Join-Path $Root 'build'
    Invoke-InVsEnv "cmake -S `"$Root`" -B `"$Build`" -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5"
    $Target = if ($BuildHost) { '--target CoD3' } else { '' }
    Invoke-InVsEnv "cmake --build `"$Build`" $Target --parallel $Jobs"

    $Lib = Join-Path $Build 'CoD3RecompLib\CoD3RecompLib.lib'
    if (Test-Path $Lib) {
        Write-Host ("    {0} ({1:N0} MB)" -f $Lib, ((Get-Item $Lib).Length / 1MB)) -ForegroundColor Green
    }
    $Exe = Join-Path $Build 'CoD3Host\CoD3.exe'
    if (Test-Path $Exe) {
        Write-Host ("    {0} ({1:N0} MB)" -f $Exe, ((Get-Item $Exe).Length / 1MB)) -ForegroundColor Green
    }
}

Write-Host 'Done.' -ForegroundColor Green

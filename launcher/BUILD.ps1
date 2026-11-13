$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw '.NET Framework 4.x C# compiler is required.' }
New-Item -ItemType Directory -Path (Join-Path $root 'bin') -Force | Out-Null
foreach ($name in 'Tests.exe','Publisher.exe','CoD3Launcher.exe','UITests.exe','AudioTests.exe') { Copy-Item -LiteralPath "$root\src\App.config" -Destination "$root\bin\$name.config" -Force }
$common = @('/nologo', '/optimize+', '/platform:x64', '/r:System.Web.Extensions.dll', '/r:System.Net.Http.dll', '/r:System.IO.Compression.dll', '/r:System.Windows.Forms.dll')
& $compiler @common /target:exe /main:Tests ("/out:$root\bin\Tests.exe") "$root\src\Core.cs" "$root\src\LocalState.cs" "$root\src\Publisher.cs" "$root\tests\Tests.cs"
if ($LASTEXITCODE -ne 0) { throw 'Core test compilation failed.' }
& "$root\bin\Tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Core tests failed.' }
& $compiler @common /target:exe /main:Cod3Launcher.Publisher ("/out:$root\bin\Publisher.exe") "$root\src\Core.cs" "$root\src\LocalState.cs" "$root\src\Publisher.cs"
if ($LASTEXITCODE -ne 0) { throw 'Publisher compilation failed.' }
$media = @(
    "/resource:$root\assets\convoy.png,Cod3Launcher.Battlefield"
    "/resource:$root\assets\saint-lo.png,Cod3Launcher.SaintLo"
    "/resource:$root\assets\falaise.png,Cod3Launcher.Falaise"
    "/resource:$root\assets\chambois.png,Cod3Launcher.Chambois"
    "/resource:$root\assets\ridge.png,Cod3Launcher.Ridge"
    "/resource:$root\assets\railway.png,Cod3Launcher.Railway"
    "/resource:$root\assets\farm.png,Cod3Launcher.Farm"
    "/resource:$root\assets\quarry.png,Cod3Launcher.Quarry"
    "/resource:$root\assets\village-barrage.png,Cod3Launcher.VillageBarrage"
    "/resource:$root\assets\typhoon-pass.png,Cod3Launcher.TyphoonPass"
    "/resource:$root\assets\battle-ambience.wav,Cod3Launcher.Ambience"
    "/resource:$root\assets\frontline-mix.wav,Cod3Launcher.Frontline"
    "/resource:$root\assets\patch-history.txt,Cod3Launcher.PatchHistory"
    "/resource:$root\assets\jarc-logo.png,Cod3Launcher.JarcLogo"
    "/resource:$root\assets\treyarch-logo.png,Cod3Launcher.TreyarchLogo"
)
$launcherSources = @("$root\src\Main.cs", "$root\src\PatchHistory.cs")
& $compiler @common @media /target:winexe /main:Cod3Launcher.Program /r:System.Drawing.dll ("/out:$root\bin\CoD3Launcher.exe") "$root\src\Core.cs" "$root\src\LocalState.cs" "$root\src\Ambience.cs" @launcherSources
if ($LASTEXITCODE -ne 0) { throw 'Launcher compilation failed.' }
& $compiler @common @media /target:exe /main:UITests /r:System.Drawing.dll ("/out:$root\bin\UITests.exe") "$root\src\Core.cs" "$root\src\LocalState.cs" "$root\src\Ambience.cs" @launcherSources "$root\tests\UITests.cs"
if ($LASTEXITCODE -ne 0) { throw 'UI test compilation failed.' }
& "$root\bin\UITests.exe"
if ($LASTEXITCODE -ne 0) { throw 'UI tests failed.' }
& $compiler @common /target:exe /main:AudioTests ("/out:$root\bin\AudioTests.exe") "$root\src\Ambience.cs" "$root\tests\AudioTests.cs"
if ($LASTEXITCODE -ne 0) { throw 'Audio tests compilation failed.' }
& "$root\bin\AudioTests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Audio lifecycle tests failed.' }

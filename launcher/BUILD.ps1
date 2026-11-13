$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw '.NET Framework 4.x C# compiler is required.' }
New-Item -ItemType Directory -Path (Join-Path $root 'bin') -Force | Out-Null
foreach ($name in 'Tests.exe','Publisher.exe','CoD3Launcher.exe','UITests.exe') { Copy-Item -LiteralPath "$root\src\App.config" -Destination "$root\bin\$name.config" -Force }
$common = @('/nologo', '/optimize+', '/platform:x64', '/r:System.Web.Extensions.dll', '/r:System.Net.Http.dll', '/r:System.IO.Compression.dll', '/r:System.Windows.Forms.dll')
& $compiler @common /target:exe /main:Tests ("/out:$root\bin\Tests.exe") "$root\src\Core.cs" "$root\src\LocalState.cs" "$root\src\Publisher.cs" "$root\tests\Tests.cs"
if ($LASTEXITCODE -ne 0) { throw 'Core test compilation failed.' }
& "$root\bin\Tests.exe"
if ($LASTEXITCODE -ne 0) { throw 'Core tests failed.' }
& $compiler @common /target:exe /main:Cod3Launcher.Publisher ("/out:$root\bin\Publisher.exe") "$root\src\Core.cs" "$root\src\LocalState.cs" "$root\src\Publisher.cs"
if ($LASTEXITCODE -ne 0) { throw 'Publisher compilation failed.' }
& $compiler @common /target:winexe /main:Cod3Launcher.Program /r:System.Drawing.dll /r:System.Windows.Forms.dll ("/out:$root\bin\CoD3Launcher.exe") "$root\src\Core.cs" "$root\src\LocalState.cs" "$root\src\Main.cs"
if ($LASTEXITCODE -ne 0) { throw 'Launcher compilation failed.' }
& $compiler @common /target:exe /main:UITests /r:System.Drawing.dll /r:System.Windows.Forms.dll ("/out:$root\bin\UITests.exe") "$root\src\Core.cs" "$root\src\LocalState.cs" "$root\src\Main.cs" "$root\tests\UITests.cs"
if ($LASTEXITCODE -ne 0) { throw 'UI test compilation failed.' }
& "$root\bin\UITests.exe"
if ($LASTEXITCODE -ne 0) { throw 'UI tests failed.' }

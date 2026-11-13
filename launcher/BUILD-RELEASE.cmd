@echo off
setlocal
cd /d "%~dp0"
if not exist "%~dp0bin\Publisher.exe" goto missing
if not exist "%~dp0player\CoD3Launcher.exe" goto missing
"%~dp0bin\Publisher.exe" prepare "%~dp0."
if errorlevel 1 echo Packaging stopped. No release was uploaded. Read the error above.
pause
exit /b
:missing
echo Extract the entire author ZIP first. Keep bin, player and hosting folders together.
pause
exit /b 1

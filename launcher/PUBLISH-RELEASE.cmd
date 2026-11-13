@echo off
setlocal
cd /d "%~dp0"
if not exist "%~dp0release-candidate\candidate.json" goto missing
if not exist "%~dp0player\public-key.xml" goto missing
echo This publishes the prepared release for all players using this launcher.
echo All required content is uploaded before the current release is changed.
"%~dp0bin\Publisher.exe" publish "%~dp0release-candidate" "%~dp0player\public-key.xml" "%~dp0hosting\wrangler.toml"
if errorlevel 1 echo Publication stopped. Read the error above and check the live feed before retrying.
pause
exit /b
:missing
echo Run BUILD-RELEASE.cmd successfully first, then review the candidate inventory.
pause
exit /b 1

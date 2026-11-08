@echo off
rem Upscales the textures dumped with texture_dump = 1 (bushes, trees, walls,
rem vehicles, guns) into textures\replace\hd. See UPSCALE-TEXTURES.ps1.
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0UPSCALE-TEXTURES.ps1" %*
pause

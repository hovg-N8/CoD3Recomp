@echo off
setlocal
cd /d "%~dp0"
if not exist "%~dp0hosting\wrangler.toml" goto missing
if not exist "%~dp0hosting\worker.mjs" goto missing
call npx.cmd wrangler whoami
if errorlevel 1 goto login
goto bucket
:login
call npx.cmd wrangler login
if errorlevel 1 goto failed
:bucket
call npx.cmd wrangler r2 bucket info cod3-port-updates --config "%~dp0hosting\wrangler.toml"
if not errorlevel 1 goto deploy
call npx.cmd wrangler r2 bucket create cod3-port-updates --config "%~dp0hosting\wrangler.toml"
if errorlevel 1 goto failed
:deploy
call npx.cmd wrangler deploy --config "%~dp0hosting\wrangler.toml"
if errorlevel 1 goto failed
echo Update service deployed. published:false is normal until your first release.
echo https://cod3-updates.hovgtv.workers.dev/health
pause
exit /b 0
:failed
echo Hosting setup stopped. Read the error above. No game settings changed.
echo If R2 activation is required, complete it in the Cloudflare dashboard and rerun.
pause
exit /b 1
:missing
echo Extract the ENTIRE author ZIP first. Keep the hosting and author folders together.
pause
exit /b 1

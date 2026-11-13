$ErrorActionPreference = 'Stop'
$kitRoot = Split-Path -Parent $PSScriptRoot
$config = Join-Path $kitRoot 'hosting\wrangler.toml'
Write-Host 'Setting up the separate COD3 update service in your hovgtv Cloudflare account.'
Write-Host 'This uses R2 storage. Storage/request charges can apply beyond the account allowance.'
& npx.cmd wrangler whoami
if ($LASTEXITCODE -ne 0) {
    & npx.cmd wrangler login
    if ($LASTEXITCODE -ne 0) { throw 'Cloudflare login was not completed.' }
}
$info = & npx.cmd wrangler r2 bucket info cod3-port-updates --json --config $config 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host 'Checking/creating the cod3-port-updates bucket...'
    & npx.cmd wrangler r2 bucket create cod3-port-updates --config $config
    if ($LASTEXITCODE -ne 0) { throw 'Bucket creation failed. If Cloudflare requires R2 activation, complete that in your dashboard, then rerun.' }
}
& npx.cmd wrangler deploy --config $config
if ($LASTEXITCODE -ne 0) { throw 'Update Worker deployment failed.' }
Write-Host 'Expected health URL: https://cod3-updates.hovgtv.workers.dev/health'
Write-Host 'published:false is expected until the first release is uploaded.'

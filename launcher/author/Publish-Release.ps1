param([Parameter(Mandatory=$true)][string]$CandidateFolder)
$ErrorActionPreference = 'Stop'
$kitRoot = Split-Path -Parent $PSScriptRoot
$config = Join-Path $kitRoot 'hosting\wrangler.toml'
$candidate = Join-Path $CandidateFolder 'candidate.json'
$publicKey = Join-Path $kitRoot 'player\public-key.xml'
& (Join-Path $kitRoot 'bin\Publisher.exe') verify $CandidateFolder $publicKey
if ($LASTEXITCODE -ne 0) { throw 'Candidate verification failed. Nothing published.' }
$candidateHash = (Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash
$envelope = Get-Content -LiteralPath $candidate -Raw | ConvertFrom-Json
$release = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($envelope.payload)) | ConvertFrom-Json
$keys = @($release.files | ForEach-Object { $_.chunks } | ForEach-Object { $_.key } | Sort-Object -Unique)
$service = 'https://cod3-updates.hovgtv.workers.dev'
$index = 0
foreach ($key in $keys) {
    $index++
    Write-Host "Uploading content $index / $($keys.Count)"
    $file = Join-Path $CandidateFolder ($key.Replace('/', [IO.Path]::DirectorySeparatorChar))
    & npx.cmd wrangler r2 object put "cod3-port-updates/$key" --file $file --content-type application/octet-stream --remote --config $config
    if ($LASTEXITCODE -ne 0) { throw 'Content upload failed. Current public release was not changed; rerun to retry.' }
}
if ((Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash -ne $candidateHash) { throw 'Candidate changed during upload. Public release was not changed.' }
& npx.cmd wrangler r2 object put "cod3-port-updates/releases/$($release.version)-$candidateHash.json" --file $candidate --content-type application/json --remote --config $config
if ($LASTEXITCODE -ne 0) { throw 'Release snapshot upload failed. Public release was not changed.' }
# This is deliberately last: no client sees the new release until all its objects exist.
& npx.cmd wrangler r2 object put cod3-port-updates/stable.json --file $candidate --content-type application/json --remote --config $config
if ($LASTEXITCODE -ne 0) { throw 'Stable release publication failed. Check the live feed before retrying.' }
Write-Host "Published build $($release.version). Check $service/health then test the launcher on a second PC."

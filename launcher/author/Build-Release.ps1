param([Parameter(Mandatory=$true)][string]$GameFolder, [string]$OutputFolder)
$ErrorActionPreference = 'Stop'
$kitRoot = Split-Path -Parent $PSScriptRoot
$publisher = Join-Path $kitRoot 'bin\Publisher.exe'
if (-not $OutputFolder) { $OutputFolder = Join-Path $kitRoot 'release-candidate' }
$publisherState = Join-Path $env:LOCALAPPDATA 'HOVGtv\CoD3Publisher'
$publicFile = Join-Path $kitRoot 'player\public-key.xml'
& $publisher keygen $publisherState $publicFile
if ($LASTEXITCODE -ne 0) { throw 'Publisher key creation failed.' }
& $publisher build $GameFolder $OutputFolder (Join-Path $publisherState 'private-key.xml')
if ($LASTEXITCODE -ne 0) { throw 'Release packaging failed. No release was uploaded.' }
Write-Host 'Candidate prepared. Review inventory.json and test before running Publish-Release.ps1.'
Write-Host 'Distribute the player folder with its matching public-key.xml.'

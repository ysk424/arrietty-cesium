[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$ProjectDirectory,[Parameter(Mandatory=$true)][string]$Python)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$lock=Get-Content -LiteralPath (Join-Path $root 'shared/cesium.lock.json') -Raw | ConvertFrom-Json
$plugins=Join-Path $ProjectDirectory 'Plugins'
$manifest=Join-Path $plugins 'CesiumForUnreal/CesiumForUnreal.uplugin'
if(-not (Test-Path -LiteralPath $manifest)) {
    $downloads=Join-Path $root 'artifacts/downloads'
    New-Item -ItemType Directory -Force -Path $downloads | Out-Null
    $zip=Join-Path $downloads 'cesium.zip'
    $oldZip=Join-Path $root 'apps/row/artifacts/downloads/cesium.zip'
    if(-not (Test-Path -LiteralPath $zip) -and (Test-Path -LiteralPath $oldZip)) { Copy-Item -LiteralPath $oldZip -Destination $zip }
    if(-not (Test-Path -LiteralPath $zip) -or (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -ne $lock.sha256) {
        curl.exe -L --fail --silent --show-error $lock.url -o $zip
        if($LASTEXITCODE -ne 0){throw 'Cesium download failed'}
    }
    if((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -ne $lock.sha256){throw 'Cesium checksum mismatch'}
    & $Python -c 'import zipfile,sys; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' $zip $plugins
    if($LASTEXITCODE -ne 0){throw 'Cesium extraction failed'}
}
if((Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).VersionName -ne $lock.version){throw 'Unexpected Cesium version'}
$geoid=Join-Path $root 'ThirdParty/Geoid/us_nga_egm96_15.tif'
$hash='db493027562c9b004d7220fa881f5603adada4e1c5029b933fa7de4547b0e78d'
if(-not (Test-Path -LiteralPath $geoid) -or (Get-FileHash -LiteralPath $geoid -Algorithm SHA256).Hash -ne $hash) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $geoid) | Out-Null
    Invoke-WebRequest -UseBasicParsing 'https://cdn.proj.org/us_nga_egm96_15.tif' -OutFile $geoid
}
if((Get-FileHash -LiteralPath $geoid -Algorithm SHA256).Hash -ne $hash){throw 'Geoid checksum mismatch'}

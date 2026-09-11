[CmdletBinding()]
param([string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8',[switch]$SkipBuild)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$project=Join-Path $repo 'unreal/ArriettyCesium/ArriettyCesium.uproject'
$version=Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
if($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8){throw 'UE 5.8 required'}
$logs=Join-Path $repo 'logs'; New-Item -ItemType Directory -Force -Path $logs | Out-Null
$python=Join-Path $repo '.venv/Scripts/python.exe'
if(-not (Test-Path -LiteralPath $python)){ py -3.13 -m venv (Join-Path $repo '.venv'); if($LASTEXITCODE -ne 0){throw 'Python 3.13 required'} }
& $python -m pip install --disable-pip-version-check -r (Join-Path $repo 'requirements.txt')
if($LASTEXITCODE -ne 0){throw 'Python package installation failed'}
& (Join-Path $PSScriptRoot 'bootstrap.ps1')
$lock=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'cesium.lock.json') -Raw | ConvertFrom-Json
$plugins=Join-Path $repo 'unreal/ArriettyCesium/Plugins'
$manifest=Join-Path $plugins 'CesiumForUnreal/CesiumForUnreal.uplugin'
if(-not (Test-Path -LiteralPath $manifest)) {
    $downloads=Join-Path $repo 'artifacts/downloads';New-Item -ItemType Directory -Force -Path $downloads | Out-Null
    $zip=Join-Path $downloads 'cesium.zip'
    if(-not (Test-Path -LiteralPath $zip) -or (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -ne $lock.sha256) {
        curl.exe -L --fail --silent --show-error $lock.url -o $zip
        if($LASTEXITCODE -ne 0){throw 'Cesium download failed'}
    }
    if((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -ne $lock.sha256){throw 'Cesium checksum mismatch'}
    & $python -c 'import zipfile,sys; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' $zip $plugins
    if($LASTEXITCODE -ne 0){throw 'Cesium extraction failed'}
}
if((Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).VersionName -ne $lock.version){throw 'Unexpected Cesium version; see tools/cesium.lock.json'}
$geoid=Join-Path $repo 'ThirdParty/Geoid/us_nga_egm96_15.tif'
$geoidHash='db493027562c9b004d7220fa881f5603adada4e1c5029b933fa7de4547b0e78d'
if(-not (Test-Path -LiteralPath $geoid) -or (Get-FileHash -LiteralPath $geoid -Algorithm SHA256).Hash -ne $geoidHash) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $geoid) | Out-Null
    Invoke-WebRequest -UseBasicParsing 'https://cdn.proj.org/us_nga_egm96_15.tif' -OutFile $geoid
}
if((Get-FileHash -LiteralPath $geoid -Algorithm SHA256).Hash -ne $geoidHash){throw 'Geoid checksum mismatch'}
if(-not $SkipBuild){
    & (Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat') ArriettyCesiumEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE *> (Join-Path $logs 'ue-build.log')
    if($LASTEXITCODE -ne 0){throw 'UE compile failed: logs/ue-build.log'}
}
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
& $editor $project -run=pythonscript "-script=$(Join-Path $PSScriptRoot 'build_content.py')" -unattended -nop4 -nosplash -nosound -nohmd -NullRHI "-abslog=$(Join-Path $logs 'ue-content.log')" *> (Join-Path $logs 'ue-content-console.log')
if($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath (Join-Path $logs 'ue-content.log') -Pattern 'ROW_CONTENT_READY' -Quiet)){throw 'Content generation failed: logs/ue-content.log'}
& (Join-Path $PSScriptRoot 'prepare_audio.ps1') -EngineRoot $EngineRoot
Write-Output 'Ready. Example: ./run.ps1 "Koh Hong"'

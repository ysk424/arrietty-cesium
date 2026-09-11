[CmdletBinding()]
param([string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8',[switch]$SkipBuild)
$ErrorActionPreference='Stop'
$app=Split-Path -Parent $PSScriptRoot
$root=Split-Path -Parent (Split-Path -Parent $app)
$project=Join-Path $app 'unreal/ArriettyUE/ArriettyUE.uproject'
$logs=Join-Path $root 'logs/fly'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$version=Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
if($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8){throw 'UE 5.8 required'}
$python=Join-Path $app '.venv/Scripts/python.exe'
if(-not (Test-Path -LiteralPath $python)){py -3.13 -m venv (Join-Path $app '.venv');if($LASTEXITCODE -ne 0){throw 'Python 3.13 required'}}
& $python -m pip install --disable-pip-version-check -r (Join-Path $app 'requirements.txt') *> (Join-Path $logs 'dependencies-prepare.log')
if($LASTEXITCODE -ne 0){throw 'Python dependency installation failed'}
$wheelHashes=Get-Content -LiteralPath (Join-Path $app 'wheels.lock.json') -Raw | ConvertFrom-Json
foreach($entry in $wheelHashes.PSObject.Properties) {
    if((Get-FileHash -LiteralPath (Join-Path $app ('wheels/'+$entry.Name)) -Algorithm SHA256).Hash -ne $entry.Value) {throw 'Bundled wheel checksum mismatch'}
}
$wheels=Get-ChildItem -LiteralPath (Join-Path $app 'wheels') -Filter '*.whl' | ForEach-Object FullName
& $python -m pip install --no-index @wheels >> (Join-Path $logs 'dependencies-prepare.log')
if($LASTEXITCODE -ne 0){throw 'Hardware dependency installation failed'}
& (Join-Path $root 'tools/prepare_geography.ps1') -ProjectDirectory (Split-Path -Parent $project) -Python $python
if(-not $SkipBuild){
    & (Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat') ArriettyUEEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE *> (Join-Path $logs 'ue-build.log')
    if($LASTEXITCODE -ne 0){throw 'UE build failed: logs/fly/ue-build.log'}
}
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
& $editor $project -unattended -nop4 -nosplash -nosound -nohmd -DisablePlugins=OpenXR -NullRHI -run=pythonscript "-script=$(Join-Path $PSScriptRoot 'build_ue_content.py')" "-abslog=$(Join-Path $logs 'ue-content.log')" *> (Join-Path $logs 'ue-content-console.log')
if($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath (Join-Path $logs 'ue-content.log') -Pattern 'ARRIETTY_UE_CONTENT_READY' -Quiet)){throw 'Fly content preparation failed: logs/fly/ue-content.log'}
Write-Output 'Ready: ./fly.ps1 "Mount Fuji" -StartMode Air'

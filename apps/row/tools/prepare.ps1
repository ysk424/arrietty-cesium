[CmdletBinding()]
param([string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8',[switch]$SkipBuild)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$workspace=Split-Path -Parent (Split-Path -Parent $repo)
$project=Join-Path $repo 'unreal/ArriettyCesium/ArriettyCesium.uproject'
$version=Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
if($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8){throw 'UE 5.8 required'}
$logs=Join-Path $workspace 'logs/row'; New-Item -ItemType Directory -Force -Path $logs | Out-Null
$python=Join-Path $repo '.venv/Scripts/python.exe'
if(-not (Test-Path -LiteralPath $python)){ py -3.13 -m venv (Join-Path $repo '.venv'); if($LASTEXITCODE -ne 0){throw 'Python 3.13 required'} }
& $python -m pip install --disable-pip-version-check -r (Join-Path $repo 'requirements.txt')
if($LASTEXITCODE -ne 0){throw 'Python package installation failed'}
& (Join-Path $PSScriptRoot 'bootstrap.ps1')
& (Join-Path $workspace 'tools/prepare_geography.ps1') -ProjectDirectory (Split-Path -Parent $project) -Python $python
if(-not $SkipBuild){
    & (Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat') ArriettyCesiumEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE *> (Join-Path $logs 'ue-build.log')
    if($LASTEXITCODE -ne 0){throw 'UE compile failed: logs/row/ue-build.log'}
}
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
& $editor $project -run=pythonscript "-script=$(Join-Path $PSScriptRoot 'build_content.py')" -unattended -nop4 -nosplash -nosound -nohmd -DisablePlugins=OpenXR -NullRHI "-abslog=$(Join-Path $logs 'ue-content.log')" *> (Join-Path $logs 'ue-content-console.log')
if($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath (Join-Path $logs 'ue-content.log') -Pattern 'ROW_CONTENT_READY' -Quiet)){throw 'Content generation failed: logs/row/ue-content.log'}
& (Join-Path $PSScriptRoot 'prepare_audio.ps1') -EngineRoot $EngineRoot
Write-Output 'Ready. Example: ./row.ps1 "Koh Hong"'

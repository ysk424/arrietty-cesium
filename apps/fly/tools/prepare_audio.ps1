[CmdletBinding()]
param([string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8')
$ErrorActionPreference='Stop'
$app=Split-Path -Parent $PSScriptRoot
$root=Split-Path -Parent (Split-Path -Parent $app)
$log=Join-Path $root 'logs/fly/audio-prepare.log'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $log) | Out-Null
if(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue){throw 'Close Unreal Editor before preparing audio.'}
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') (Join-Path $app 'unreal/ArriettyUE/ArriettyUE.uproject') -unattended -nop4 -nosplash -nosound -nohmd -DisablePlugins=OpenXR -NullRHI -run=pythonscript "-script=$(Join-Path $PSScriptRoot 'build_audio.py')" "-abslog=$log" *> (Join-Path $root 'logs/fly/audio-prepare-console.log')
if($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $log -Pattern 'FLY_AUDIO_CONTENT_READY assets=6' -Quiet)){throw "Fly audio preparation failed: $log"}
Write-Output 'PASS six fly sound assets prepared; original masters unchanged.'

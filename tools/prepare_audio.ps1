[CmdletBinding()]
param([string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$project=Join-Path $repo 'unreal/ArriettyCesium/ArriettyCesium.uproject'
$logs=Join-Path $repo 'logs'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$log=Join-Path $logs ('audio-content-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') $project -run=pythonscript "-script=$(Join-Path $PSScriptRoot 'build_audio.py')" -unattended -nop4 -nosplash -nosound -nohmd -NullRHI "-abslog=$log" *> "$log.console.txt"
if($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $log -Pattern 'ROW_AUDIO_CONTENT_READY' -Quiet)) { throw "Audio import failed: $log" }
Write-Output "Prepared five local sound assets: $log"

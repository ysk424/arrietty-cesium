[CmdletBinding()]
param([string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$project=Join-Path $repo 'unreal/ArriettyCesium/ArriettyCesium.uproject'
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$log=Join-Path $repo "logs/audio-runtime-$stamp.log"
$output=Join-Path $repo 'unreal/ArriettyCesium/Saved/AudioCapture/row-audio-demo.wav'
$began=Get-Date
$arguments=@(('"'+$project+'"'),'/Game/Row/Maps/CesiumRow','-game','-RowTestFixture','-RowDemo','-RowDemoStraight','-nohmd',
    '-RenderOffscreen','-unattended','-nosplash','-AudioMixer','-RowAudioCapture','-RowQuitAfter=50',
    '-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0',
    ('-abslog="'+$log+'"'),'-ExecCmds="t.MaxFPS 60,Automation RunTests ArriettyRow.Audio.Assets"')
$process=Start-Process -FilePath (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
$process.WaitForExit()
$text=Get-Content -LiteralPath $log -Raw
if($process.ExitCode -ne 0 -or $text -notmatch 'Test Completed\. Result=\{Success\}.*ArriettyRow.Audio.Assets' -or
    $text -notmatch 'ROW_AUDIO_CAPTURE_COMPLETE catches=[1-9]' -or -not (Test-Path -LiteralPath $output) -or
    (Get-Item -LiteralPath $output).LastWriteTime -lt $began) { throw "UE audio test failed: $log" }
$evidence=Join-Path $repo "artifacts/audio/ue-mix-$stamp.wav"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $evidence) | Out-Null
Copy-Item -LiteralPath $output -Destination $evidence
py -3.13 (Join-Path $PSScriptRoot 'verify_audio.py') $evidence
if($LASTEXITCODE -ne 0) { throw "UE output verification failed: $evidence" }
Write-Output "PASS UE sound assets and synthetic rowing capture. Evidence: $log / $evidence"

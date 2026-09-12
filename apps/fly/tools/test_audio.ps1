[CmdletBinding()]
param([string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8')
$ErrorActionPreference='Stop'
$app=Split-Path -Parent $PSScriptRoot
$root=Split-Path -Parent (Split-Path -Parent $app)
$project=Join-Path $app 'unreal/ArriettyUE/ArriettyUE.uproject'
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$log=Join-Path $root "logs/fly/audio-runtime-$stamp.log"
$output=Join-Path $app 'unreal/ArriettyUE/Saved/AudioCapture/fly-audio-fixture.wav'
$began=Get-Date
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $log) | Out-Null
if(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue){throw 'Close Unreal Editor before running the audio fixture.'}
$arguments=@(('"'+$project+'"'),'/Engine/Maps/Entry','-game','-FlyAudioFixture','-nohmd','-DisablePlugins=OpenXR',
    '-RenderOffscreen','-unattended','-nosplash','-AudioMixer','-FlyVolume=0.8',
    '-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0',
    ('-abslog="'+$log+'"'),'-ExecCmds="t.MaxFPS 60,Automation RunTests Arrietty.Audio."')
$process=Start-Process -FilePath (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
if(-not $process.WaitForExit(90000)){
    Stop-Process -Id $process.Id
    throw "Offline audio fixture timed out: $log"
}
$process.Refresh()
$text=Get-Content -LiteralPath $log -Raw
foreach($name in @('Mix','Assets')) {
    if($text -notmatch ('Test Completed\. Result=\{Success\}.*Arrietty.Audio.'+$name)){throw "Fly audio test failed: $name / $log"}
}
if($process.ExitCode -ne 0 -or $text -notmatch 'FLY_AUDIO_CAPTURE_COMPLETE touchdowns=1' -or
    $text -notmatch 'FLY_AUDIO_READY assets=6' -or -not (Test-Path -LiteralPath $output) -or
    (Get-Item -LiteralPath $output).LastWriteTime -lt $began) {throw "UE audio capture failed: $log"}
$evidence=Join-Path $app "artifacts/audio/ue-mix-$stamp.wav"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $evidence) | Out-Null
Copy-Item -LiteralPath $output -Destination $evidence
& (Join-Path $app '.venv/Scripts/python.exe') (Join-Path $PSScriptRoot 'verify_audio.py') $evidence
if($LASTEXITCODE -ne 0){throw "UE output verification failed: $evidence"}
Write-Output "PASS six fly voices and offline engine capture. Evidence: $log / $evidence"

[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Scene,
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name='cesium-preview',
    [switch]$Chase,[switch]$Water,
    [ValidateRange(20,180)][int]$Seconds=65,
    [string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8'
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$scenePath=(Resolve-Path -LiteralPath $Scene).Path
$project=Join-Path $repo 'unreal/ArriettyCesium/ArriettyCesium.uproject'
$output=Join-Path $repo ('artifacts/'+$Name+'.png')
$log=Join-Path $repo ('logs/'+$Name+'.log')
New-Item -ItemType Directory -Force -Path (Split-Path $output) | Out-Null
$launchArgs=@(('"'+$project+'"'),'/Game/Row/Maps/CesiumRow','-game','-RowDemo','-RowDemoStraight','-nohmd','-RenderOffscreen',
    '-unattended','-nop4','-nosplash','-nosound','-windowed','-ForceRes','-ResX=1600','-ResY=1000',
    ('-RowPlace="'+$scenePath+'"'),('-RowScreenshotAt='+($Seconds-10)),('-RowQuitAfter='+$Seconds),
    ('-RowScreenshotPath="'+$output+'"'),('-abslog="'+$log+'"'),
    '-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,r.SetRes 1600x1000w"')
if($Chase){$launchArgs+='-RowChase'}
if($Water){$launchArgs+='-RowWaterView'}
$process=Start-Process -FilePath (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -ArgumentList $launchArgs -WindowStyle Hidden -PassThru
Write-Output "Preview PID $($process.Id): $output"
$process.WaitForExit()
if($process.ExitCode -ne 0){throw "Preview exited with code $($process.ExitCode)"}
if(-not (Test-Path -LiteralPath $output)){throw 'Preview did not produce a screenshot'}
Write-Output "Preview complete: $output"

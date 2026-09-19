[CmdletBinding()]
param([string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$workspace=Split-Path -Parent (Split-Path -Parent $repo)
$project=Join-Path $repo 'unreal/ArriettyCesium/ArriettyCesium.uproject'
$log=Join-Path $workspace ('logs/row/setup-controls-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$arguments=@(('"'+$project+'"'),'/Game/Row/Maps/CesiumRow','-game','-RowTestFixture','-RowOffline','-nohmd','-DisablePlugins=OpenXR',
    '-RenderOffscreen','-unattended','-nosplash','-nosound','-RowQuitAfter=45',
    ('-abslog="'+$log+'"'),'-ExecCmds="Automation RunTests ArriettyRow.Controls.SetupEnter+ArriettyRow.Controls.Ps4+ArriettyRow.Movement.Magnification"',
    '-TestExit="Automation Test Queue Empty"')
$process=Start-Process -FilePath (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
$process.WaitForExit()
$output=Get-Content -LiteralPath $log -Raw
if($process.ExitCode -ne 0 -or $output -notmatch 'Test Completed\. Result=\{Success\}.*ArriettyRow.Controls.SetupEnter' -or
    $output -notmatch 'Test Completed\. Result=\{Success\}.*ArriettyRow.Controls.Ps4' -or
    $output -notmatch 'Test Completed\. Result=\{Success\}.*ArriettyRow.Movement.Magnification') {
    throw "UE setup control test failed (exit $($process.ExitCode)). Evidence: $log"
}
Write-Output "PASS UE setup controls and movement magnification. Evidence: $log"

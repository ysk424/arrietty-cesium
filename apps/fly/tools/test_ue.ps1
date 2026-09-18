[CmdletBinding()]
param([string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8')
$ErrorActionPreference='Stop'
$app=Split-Path -Parent $PSScriptRoot
$root=Split-Path -Parent (Split-Path -Parent $app)
$logs=Join-Path $root 'logs/fly'
Push-Location $app
try {
    & ./.venv/Scripts/python.exe -m unittest discover -s tests -v *> (Join-Path $logs 'python-tests.log')
    if($LASTEXITCODE -ne 0){throw 'Python tests failed: logs/fly/python-tests.log'}
    $log=Join-Path $logs 'ue-automation.log'
    & (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') (Join-Path $app 'unreal/ArriettyUE/ArriettyUE.uproject') -unattended -nop4 -nosound -nohmd -DisablePlugins=OpenXR -NullRHI '-ExecCmds=Automation RunTests Arrietty.' '-TestExit=Automation Test Queue Empty' "-abslog=$log" *> (Join-Path $logs 'ue-automation-console.log')
    if($LASTEXITCODE -ne 0){throw 'UE automation failed'}
    foreach($name in @('Coordinates.Attitude','Coordinates.HmdAlignment','Coordinates.Cesium','Terrain.Sweep','Audio.Mix','Controls.AutomaticPreparation')) {
        if(-not (Select-String -LiteralPath $log -Pattern ('Result=\{Success\}.*Arrietty\.'+[regex]::Escape($name)) -Quiet)){throw "UE test failed: $name"}
    }
    Write-Output 'PASS Fly Python and UE coordinate/camera/terrain tests (no hardware).'
} finally {Pop-Location}

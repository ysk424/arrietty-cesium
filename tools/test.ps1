[CmdletBinding()]
param([ValidateSet('Row','Fly','All')][string]$App='All',[string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if($App -in @('Row','All')) {
    & (Join-Path $root 'tools/build_native.ps1')
    & (Join-Path $root 'apps/row/.venv/Scripts/python.exe') -m unittest discover -s (Join-Path $root 'apps/row/tests') -p 'test_*.py' -v
    if($LASTEXITCODE -ne 0){throw 'Row Python tests failed'}
    & (Join-Path $root 'tools/test_setup.ps1') -EngineRoot $EngineRoot
}
if($App -in @('Fly','All')) {& (Join-Path $root 'apps/fly/tools/test_ue.ps1') -EngineRoot $EngineRoot}

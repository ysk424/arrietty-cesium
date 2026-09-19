[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SourceProject,
    [string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8'
)
$ErrorActionPreference='Stop'
$row=Split-Path -Parent $PSScriptRoot
$workspace=Split-Path -Parent (Split-Path -Parent $row)
$source=Join-Path (Resolve-Path -LiteralPath $SourceProject).Path 'Content/Waterline'
if(-not (Test-Path -LiteralPath (Join-Path $source '3_Ocean_Sim/BP_Waterline_Ocean_Gen_4.uasset'))){throw 'Waterline Gen 4 content is required'}
if(Get-Process UnrealEditor,UnrealEditor-Cmd,ArriettyCesium -ErrorAction SilentlyContinue){throw 'Close UE normally before importing Waterline; do not interrupt a ride'}
$content=Join-Path $row 'unreal/ArriettyCesium/Content'
$destination=Join-Path $content 'Waterline'
if(-not (Test-Path -LiteralPath $destination)){
    Copy-Item -LiteralPath $source -Destination $destination -Recurse
} else {
    Write-Output 'Using the existing local Waterline copy; vendor content will not be overwritten.'
}
$logs=Join-Path $workspace 'logs/row/waterline'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$project=Join-Path $row 'unreal/ArriettyCesium/ArriettyCesium.uproject'
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') $project -run=pythonscript "-script=$(Join-Path $PSScriptRoot 'build_waterline.py')" -unattended -nop4 -nosplash -nosound -nohmd -DisablePlugins=OpenXR -NullRHI "-abslog=$(Join-Path $logs 'prepare.log')" *> (Join-Path $logs 'prepare-console.log')
if($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath (Join-Path $logs 'prepare.log') -Pattern 'ROW_WATERLINE_CONTENT_READY' -Quiet)){throw 'Waterline preparation failed; see logs/row/waterline/prepare.log'}
Write-Output 'Waterline adapter prepared. Purchased and generated assets remain local.'

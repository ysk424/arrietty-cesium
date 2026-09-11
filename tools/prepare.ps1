[CmdletBinding()]
param([ValidateSet('Row','Fly','All')][string]$App='All',
      [string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8', [switch]$SkipBuild)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if($App -in @('Row','All')) { & (Join-Path $root 'apps/row/tools/prepare.ps1') -EngineRoot $EngineRoot -SkipBuild:$SkipBuild }
if($App -in @('Fly','All')) { & (Join-Path $root 'apps/fly/tools/prepare_ue.ps1') -EngineRoot $EngineRoot -SkipBuild:$SkipBuild }

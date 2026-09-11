[CmdletBinding()]
param(
    [Parameter(Mandatory=$true,Position=0)][ValidateNotNullOrEmpty()][string]$Place,
    [switch]$Demo,
    [switch]$ResolveOnly,
    [switch]$PrepareOnly,
    [switch]$RefreshPlace,
    [ValidateRange(0,1)][double]$Volume=0.8,
    [ValidateRange(1,10)][double]$RadiusKm=3,
    [Nullable[double]]$WaterLevelM=$null,
    [string]$Model='',
    [string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8'
)
$ErrorActionPreference='Stop'
$python=Join-Path $PSScriptRoot 'apps/row/.venv/Scripts/python.exe'
if(-not (Test-Path -LiteralPath $python)) { throw 'Run ./tools/prepare.ps1 first.' }
$culture=[Globalization.CultureInfo]::InvariantCulture
$launchArgs=@((Join-Path $PSScriptRoot 'apps/row/tools/launch.py'),$Place,'--engine-root',$EngineRoot,
    '--volume',$Volume.ToString($culture),'--radius-km',$RadiusKm.ToString($culture))
if($Demo){$launchArgs+='--demo'}
if($ResolveOnly){$launchArgs+='--resolve-only'}
if($PrepareOnly){$launchArgs+='--prepare-only'}
if($RefreshPlace){$launchArgs+='--refresh-place'}
if($null -ne $WaterLevelM){$launchArgs+=@('--water-level',$WaterLevelM.ToString($culture))}
if($Model){$launchArgs+=@('--model',$Model)}
$oldEncoding=$env:PYTHONIOENCODING
try { $env:PYTHONIOENCODING='utf-8'; & $python @launchArgs; $result=$LASTEXITCODE }
finally { $env:PYTHONIOENCODING=$oldEncoding }
if($result -ne 0){ throw "Launcher failed (exit $result)." }

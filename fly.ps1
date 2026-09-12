[CmdletBinding()]
param(
    [Parameter(Mandatory=$true,Position=0)][ValidateNotNullOrEmpty()][string]$Place,
    [switch]$Offline,[switch]$Demo,[switch]$SmokeTest,[switch]$Headless,
    [switch]$ResolveOnly,[switch]$PrepareOnly,[switch]$RefreshPlace,
    [ValidateSet('Ground','Air')][string]$StartMode='Ground',
    [ValidateRange(10,3000)][double]$StartAglM=100,
    [ValidateRange(1,10)][double]$RadiusKm=10,
    [Alias('Mag')][ValidateRange(1,10)][double]$Magnification=1,
    [ValidateRange(0,1)][double]$Volume=0.8,
    [string]$LocalDate='',[string]$LocalTime='12:00',[string]$Model='',
    [string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8'
)
$ErrorActionPreference='Stop'
$python=Join-Path $PSScriptRoot 'apps/fly/.venv/Scripts/python.exe'
if(-not (Test-Path -LiteralPath $python)){throw 'Run ./tools/prepare.ps1 -App Fly first.'}
if(($SmokeTest -or $Headless) -and -not ($Offline -or $Demo -or $PrepareOnly)){throw 'SmokeTest/Headless requires -Offline or -Demo.'}
$culture=[Globalization.CultureInfo]::InvariantCulture
$launchArgs=@((Join-Path $PSScriptRoot 'apps/fly/tools/launch.py'),$Place,
    '--engine-root',$EngineRoot,'--start-mode',$StartMode.ToLowerInvariant(),
    '--start-agl',$StartAglM.ToString($culture),'--radius-km',$RadiusKm.ToString($culture),
    '--magnification',$Magnification.ToString($culture),'--volume',$Volume.ToString($culture),'--time',$LocalTime)
if($LocalDate){$launchArgs+=@('--date',$LocalDate)}
if($Model){$launchArgs+=@('--model',$Model)}
if($Offline -or $Demo){$launchArgs+='--offline'}
if($SmokeTest){$launchArgs+='--smoke'}
if($Headless){$launchArgs+='--headless'}
if($ResolveOnly){$launchArgs+='--resolve-only'}
if($PrepareOnly){$launchArgs+='--prepare-only'}
if($RefreshPlace){$launchArgs+='--refresh-place'}
$before=$env:PYTHONIOENCODING
try{$env:PYTHONIOENCODING='utf-8'; & $python @launchArgs; $result=$LASTEXITCODE}
finally{$env:PYTHONIOENCODING=$before}
if($result -ne 0){throw "Flight launcher failed (exit $result)."}

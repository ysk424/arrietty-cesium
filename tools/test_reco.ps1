[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$root=Split-Path -Parent $PSScriptRoot
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('row-reco-test-'+[Guid]::NewGuid().ToString('N'))
$checks=0
function Assert($condition,$message){
    if(-not $condition){throw "FAIL: $message"}
    $script:checks++
}
function Must-Fail([scriptblock]$action,$message){
    $failed=$false
    try {& $action | Out-Null} catch {$failed=$true}
    Assert $failed $message
}
try {
    New-Item -ItemType Directory -Path $fixture | Out-Null
    Copy-Item -LiteralPath (Join-Path $root 'reco.ps1'),(Join-Path $root 'reco.json') -Destination $fixture
    # Only this stub can be launched: tests never call UE, place APIs or Cesium.
    @'
param($Place,$RadiusKm,$Magnification,[switch]$Demo,[switch]$LegacyWater,[switch]$RefreshPlace,
      $Volume,$WaterLevelM,$Model,$EngineRoot)
$PSBoundParameters | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'called.json') -Encoding UTF8
'@ | Set-Content -LiteralPath (Join-Path $fixture 'row.ps1') -Encoding UTF8
    $script=Join-Path $fixture 'reco.ps1'
    $dataPath=Join-Path $fixture 'reco.json'
    $callPath=Join-Path $fixture 'called.json'
    $original=Get-Content -LiteralPath $dataPath -Raw -Encoding UTF8
    $catalog=$original | ConvertFrom-Json
    Assert (@($catalog.places).Count -eq 31) '31 entries'
    & $script -List 6>$null
    & $script 1 -Show 6>$null
    & $script -Show 6>$null
    Assert (-not (Test-Path -LiteralPath $callPath)) 'list/show never launch'
    foreach($place in $catalog.places){
        & $script $place.id 6>$null
        $call=Get-Content -LiteralPath $callPath -Raw -Encoding UTF8 | ConvertFrom-Json
        Assert ($call.Place -ceq $place.launch_query) "literal place argument $($place.id)"
        Assert ($call.RadiusKm -eq $place.radius_km) "radius $($place.id)"
        Assert ([Math]::Abs(60*$call.RadiusKm/(10*$call.Magnification)-10) -lt 0.000001) "10 minute multiplier $($place.id)"
    }
    & $script 10 -Demo -LegacyWater -RefreshPlace -Volume 0.25 -WaterLevelM -5 -Model 'model with spaces' -EngineRoot 'C:/Engine Path' 6>$null
    $call=Get-Content -LiteralPath $callPath -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert ($call.Demo -and $call.LegacyWater -and $call.RefreshPlace) 'switch forwarding'
    Assert ($call.Volume -eq .25 -and $call.WaterLevelM -eq -5) 'numeric forwarding'
    Assert ($call.Model -eq 'model with spaces' -and $call.EngineRoot -eq 'C:/Engine Path') 'string forwarding'
    Remove-Item -LiteralPath $callPath
    Must-Fail { & $script 0 6>$null } 'reject zero'
    Must-Fail { & $script 32 6>$null } 'reject 32'
    function Read-Host {param($Prompt); return ''}
    & $script 6>$null
    Assert (-not (Test-Path -LiteralPath $callPath)) 'empty input/EOF cancels'
    function Read-Host {param($Prompt); return '31'}
    & $script 6>$null
    $call=Get-Content -LiteralPath $callPath -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert ($call.Place -eq $catalog.places[30].launch_query) 'interactive selection'
    Remove-Item -LiteralPath $callPath
    function Read-Host {param($Prompt); return 'abc'}
    Must-Fail { & $script 6>$null } 'invalid interactive input'
    foreach($mutation in @(
        {param($c); $c.places[1].id=1},
        {param($c); $c.places[0].magnification=2},
        {param($c); $c.places[0].radius_km=0},
        {param($c); $c.places[0].launch_query="bad`nquery"},
        {param($c); $c.places[0].sources=@('http://example.invalid')},
        {param($c); $c.places[0].water_type='river'},
        {param($c); $c.places=@($c.places[0..29])}
    )){
        $changed=$original | ConvertFrom-Json
        & $mutation $changed
        $changed | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $dataPath -Encoding UTF8
        Must-Fail { & $script 1 6>$null } 'reject invalid catalog before launch'
    }
    Assert (-not (Test-Path -LiteralPath $callPath)) 'invalid inputs never launch'
    Set-Content -LiteralPath $dataPath -Value $original -Encoding UTF8
    'throw "fixture row failure"' | Set-Content -LiteralPath (Join-Path $fixture 'row.ps1') -Encoding UTF8
    Must-Fail { & $script 1 6>$null } 'propagate ROW failure'
    Write-Output "PASS $checks recommendation checks; stub launcher only, no network or Cesium. PowerShell $($PSVersionTable.PSVersion)"
} finally {
    # Delete only this uniquely-created test directory inside the temp root.
    $resolved=[IO.Path]::GetFullPath($fixture)
    $tempRoot=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
    if($resolved.StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase) -and
       (Split-Path -Leaf $resolved) -match '^row-reco-test-[0-9a-f]{32}$' -and (Test-Path -LiteralPath $resolved)){
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}

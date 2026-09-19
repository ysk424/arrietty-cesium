[CmdletBinding(DefaultParameterSetName='Select')]
param(
    [Parameter(Position=0,ParameterSetName='Select')][ValidateRange(1,31)][int]$Number,
    [Parameter(ParameterSetName='List')][switch]$List,
    [Parameter(ParameterSetName='Select')][switch]$Show,
    [Parameter(ParameterSetName='Select')][switch]$Demo,
    [Parameter(ParameterSetName='Select')][switch]$LegacyWater,
    [Parameter(ParameterSetName='Select')][switch]$RefreshPlace,
    [Parameter(ParameterSetName='Select')][ValidateRange(0,1)][double]$Volume=0.8,
    [Parameter(ParameterSetName='Select')][Nullable[double]]$WaterLevelM=$null,
    [Parameter(ParameterSetName='Select')][string]$Model='',
    [Parameter(ParameterSetName='Select')][string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8'
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$catalog=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'reco.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if($catalog.schema_version -ne 1 -or $catalog.reference_speed_kmph -ne 10 -or $catalog.target_minutes -ne 10){
    throw 'Unsupported reco.json schema or reference speed/time.'
}
$places=@($catalog.places)
if($places.Count -ne 31){throw 'reco.json must contain exactly 31 places.'}
$seen=@{}
foreach($place in $places){
    $id=$place.id
    if($id -isnot [int] -and $id -isnot [long]){throw 'Recommendation IDs must be integers.'}
    if($id -lt 1 -or $id -gt 31 -or $seen.ContainsKey([int]$id)){throw 'Recommendation IDs must be unique numbers 1..31.'}
    $seen[[int]$id]=$true
    foreach($field in @('name','country','launch_query','comment')){
        $value=$place.$field
        if($value -isnot [string] -or [string]::IsNullOrWhiteSpace($value) -or $value -match '[\x00-\x1f\x7f]'){
            throw "Invalid $field in recommendation $id."
        }
    }
    if($place.water_type -notin @('sea','lake')){throw "Invalid water type in recommendation $id."}
    foreach($field in @('radius_km','magnification')){
        $value=$place.$field
        if(($value -isnot [int] -and $value -isnot [long] -and $value -isnot [double] -and $value -isnot [decimal]) -or
            [double]::IsNaN($value) -or [double]::IsInfinity($value) -or $value -lt 1 -or $value -gt 10){
            throw "Invalid $field in recommendation $id."
        }
    }
    $expected=[double]$place.radius_km / ($catalog.reference_speed_kmph * $catalog.target_minutes / 60.0)
    if([Math]::Abs($place.magnification-$expected) -gt 0.000001){throw "Incorrect speed multiplier in recommendation $id."}
    if(@($place.sources).Count -eq 0){throw "Missing source in recommendation $id."}
    foreach($source in $place.sources){
        $uri=$null
        if(-not [Uri]::TryCreate($source,[UriKind]::Absolute,[ref]$uri) -or $uri.Scheme -ne 'https'){
            throw "Invalid source in recommendation $id."
        }
    }
}
$culture=[Globalization.CultureInfo]::InvariantCulture
function Show-Recommendation($place){
    $radius=([double]$place.radius_km).ToString('0.##',$culture)
    $mag=([double]$place.magnification).ToString('0.##',$culture)
    Write-Host ("{0,2}. {1} / {2}  半径 {3} km・速度 {4} 倍" -f $place.id,$place.name,$place.country,$radius,$mag)
    Write-Host ('    '+$place.comment)
}
if($List -or -not $PSBoundParameters.ContainsKey('Number')){
    Write-Host 'ROW おすすめの水面31か所（範囲は出発点からの半径）'
    foreach($place in ($places | Sort-Object id)){Show-Recommendation $place}
    if($List -or $Show){return}
    $answer=Read-Host '番号 1～31 を入力してください（空入力で終了）'
    if([string]::IsNullOrWhiteSpace($answer)){return}
    $selected=0
    if(-not [int]::TryParse($answer,[ref]$selected) -or $selected -lt 1 -or $selected -gt 31){
        throw '番号は1～31で指定してください。'
    }
    $Number=$selected
}
$place=$places | Where-Object { $_.id -eq $Number }
Write-Host ''
Show-Recommendation $place
Write-Host '倍率の目安: 等倍10 km/hの漕ぎ方で約10分直進すると半径に到達します。岸・島が先にある場合を除きます。'
if($Show){
    Write-Host ('検索する場所: '+$place.launch_query)
    foreach($source in $place.sources){Write-Host ('出典: '+$source)}
    Write-Host $catalog.note
    return
}
Write-Host '選択地点だけを通常のROWへ渡します。場所確認でYを入力すると準備・起動します。'
# Keep the existing place resolution, lake-height checks and explicit-Y gate.
# A literal splat preserves spaces/apostrophes; catalog text is never executed.
$launch=@{Place=$place.launch_query; RadiusKm=[double]$place.radius_km; Magnification=[double]$place.magnification}
foreach($name in @('Demo','LegacyWater','RefreshPlace','Volume','WaterLevelM','Model','EngineRoot')){
    if($PSBoundParameters.ContainsKey($name)){$launch[$name]=$PSBoundParameters[$name]}
}
& (Join-Path $PSScriptRoot 'row.ps1') @launch

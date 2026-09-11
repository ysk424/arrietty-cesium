[CmdletBinding()]
param([switch]$Capture,[string]$EngineRoot='C:/Program Files/Epic Games/UE_5.8')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$workspace=Split-Path -Parent (Split-Path -Parent $repo)
$project=Join-Path $repo 'unreal/ArriettyCesium/ArriettyCesium.uproject'
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$log=Join-Path $workspace "logs/row/tracking-runtime-$stamp.log"
$image=Join-Path $repo "artifacts/tracking-$stamp.png"
$began=Get-Date
$arguments=@(('"'+$project+'"'),'/Game/Row/Maps/CesiumRow','-game','-RowTestFixture','-RowDemo','-RowDemoStraight',
    '-RowDemoOcclusion','-nohmd','-DisablePlugins=OpenXR','-RenderOffscreen','-unattended','-nosplash','-nosound',
    '-windowed','-ForceRes','-ResX=1600','-ResY=1000','-RowQuitAfter=34',('-abslog="'+$log+'"'),
    '-ExecCmds="t.MaxFPS 60,t.IdleWhenNotForeground 0,r.SetRes 1600x1000w"')
if($Capture) { $arguments+=@('-RowCaptureAssist','-RowScreenshotAt=999',('-RowScreenshotPath="'+$image+'"')) }
$process=Start-Process -FilePath (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
$process.WaitForExit()
$output=Get-Content -LiteralPath $log -Raw
if($process.ExitCode -ne 0) { throw "UE tracking demo failed: $log" }
if($Capture) {
    # Screenshot readback can hitch the render/game thread and trigger the normal
    # watchdog. This separate run is visual evidence, not a recovery test.
    if(-not (Test-Path -LiteralPath $image)) { throw "Assist screenshot missing: $log" }
    Write-Output "Captured synthetic HMD assist panel: $image (log: $log)"
    return
}
if($output -notmatch 'ROW_TRACKING source=hmd_assist' -or $output -notmatch 'ROW_TRACKING source=reacquiring' -or
    $output -notmatch 'ROW_TRACKING source=tracker' -or $output -match 'ROW_TRACKING .*issue=(?!none)' -or
    $output -match 'ROW_CALIBRATION_FAILED') { throw "UE assist/return transitions failed: $log" }
$session=Get-ChildItem -LiteralPath (Join-Path $repo 'unreal/ArriettyCesium/Saved/Sessions') -Filter '*.csv' |
    Where-Object CreationTime -ge $began | Sort-Object CreationTime -Descending | Select-Object -First 1
if(-not $session) { throw "No new synthetic session: $log" }
$rows=Import-Csv -LiteralPath $session.FullName
$start=$rows | Where-Object event -eq 'start' | Select-Object -First 1
if(-not $start -or [double]$start.active_s -ne 0 -or [double]$start.distance_m -ne 0) { throw 'Calibration advanced exercise metrics' }
$assist=$null; $returns=0
foreach($row in $rows) {
    if($row.source -ne 'demo') { throw 'Synthetic session must be labeled demo' }
    if($row.event -eq 'bar_source_changed') {
        if($row.bar_source -eq 'hmd_assist') { $assist=$row }
        if($assist -and $row.bar_source -eq 'tracker') {
            if([double]$row.distance_m -le [double]$assist.distance_m -or $row.strokes -ne $assist.strokes) {
                throw 'Return must preserve coasting distance and cannot add a false stroke'
            }
            ++$returns; $assist=$null
        }
    }
}
if($returns -lt 2) { throw "Expected at least two HMD-assisted occlusions and automatic returns: $log" }
Write-Output "PASS UE HMD assist and automatic return ($returns cycles), unchanged setup metrics and labeled CSV. Evidence: $log / $($session.Name)"

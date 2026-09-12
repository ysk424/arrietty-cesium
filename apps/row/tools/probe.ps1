[CmdletBinding()]
param([int]$Seconds=60,[switch]$ImuOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$workspace=Split-Path -Parent (Split-Path -Parent $repo)
$config=Get-Content -LiteralPath (Join-Path $workspace 'config/row.local.json') -Raw | ConvertFrom-Json
$private=Join-Path $workspace 'logs/row/device-probe.local.txt'
New-Item -ItemType Directory -Force -Path (Split-Path $private) | Out-Null
$lines=@($config.tracker_serial,($config.rower_address -replace '[:-]',''),($config.heart_rate_address -replace '[:-]',''),($config.imu_address -replace '[:-]',''))
$lines+=@($config.imu_address_type)
[IO.File]::WriteAllText($private,($lines -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
$exe=Join-Path $repo 'artifacts/native/device_probe.exe'
if(-not (Test-Path -LiteralPath $exe)) { & (Join-Path $PSScriptRoot 'build_native.ps1') -Devices }
$arguments=@($private,$Seconds)
if($ImuOnly) { $arguments+='--imu-only' }
& $exe @arguments
exit $LASTEXITCODE

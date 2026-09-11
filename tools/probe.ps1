[CmdletBinding()]
param([int]$Seconds=60)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$config=Get-Content -LiteralPath (Join-Path $repo 'settings.local.json') -Raw | ConvertFrom-Json
$private=Join-Path $repo 'logs/device-probe.local.txt'
New-Item -ItemType Directory -Force -Path (Split-Path $private) | Out-Null
$lines=@($config.tracker_serial,($config.rower_address -replace '[:-]',''),($config.heart_rate_address -replace '[:-]',''))
[IO.File]::WriteAllText($private,($lines -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
$exe=Join-Path $repo 'artifacts/native/device_probe.exe'
if(-not (Test-Path -LiteralPath $exe)) { & (Join-Path $PSScriptRoot 'build_native.ps1') -Devices }
& $exe $private $Seconds
exit $LASTEXITCODE

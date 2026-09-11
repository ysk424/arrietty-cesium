[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$lock=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'openvr.lock.json') -Raw | ConvertFrom-Json
foreach($entry in $lock.files.PSObject.Properties) {
    $target=Join-Path $repo ('ThirdParty/OpenVR/'+$entry.Name)
    if(-not (Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $entry.Value) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
        Invoke-WebRequest -UseBasicParsing -Uri ('https://raw.githubusercontent.com/ValveSoftware/openvr/'+$lock.commit+'/'+$entry.Name) -OutFile $target
    }
    if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $entry.Value) { throw 'OpenVR SHA256 mismatch' }
}
Write-Output 'Pinned OpenVR SDK verified.'

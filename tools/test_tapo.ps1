# Isolated CLI fixture: no network, devices, real credentials or user settings.
$ErrorActionPreference='Stop'
$workspace=Split-Path -Parent $PSScriptRoot
$fixture=Join-Path $workspace ('.runtime/tapo-tests/'+[guid]::NewGuid().ToString('N'))
$bin=Join-Path $fixture '.runtime/tapo/Scripts'
New-Item -ItemType Directory -Force -Path $bin | Out-Null
$source=Get-Content -LiteralPath (Join-Path $workspace 'tapo.ps1') -Raw
$source.Replace("'Scripts/kasa.exe'","'Scripts/kasa.ps1'") | Set-Content -LiteralPath (Join-Path $fixture 'tapo.ps1')
@'
if($env:KASA_USERNAME -ne 'fixture@example.com' -or $env:KASA_PASSWORD -ne 'fixture-only'){throw 'Credential transport failed'}
if(($args -join ' ') -match 'fixture'){throw 'Credentials leaked into argv'}
ConvertTo-Json -Compress -InputObject @($args) | Add-Content -LiteralPath (Join-Path $PSScriptRoot 'calls.jsonl')
$global:LASTEXITCODE=0
if($env:TAPO_TEST_FAIL -eq '1'){$global:LASTEXITCODE=1}
'@ | Set-Content -LiteralPath (Join-Path $bin 'kasa.ps1')
$script=Join-Path $fixture 'tapo.ps1'
$record=Join-Path $bin 'calls.jsonl'
$credential=[pscredential]::new('fixture@example.com',(ConvertTo-SecureString 'fixture-only' -AsPlainText -Force))
$savedUser=$env:KASA_USERNAME;$savedPassword=$env:KASA_PASSWORD;$savedFail=$env:TAPO_TEST_FAIL
function Assert($condition,$message){if(-not $condition){throw $message}}
function ExpectFailure([scriptblock]$operation) {
    $failed=$false
    try { & $operation } catch {$failed=$true}
    Assert $failed 'Expected rejection'
}
function Calls {if(Test-Path -LiteralPath $record){@(Get-Content -LiteralPath $record)}}
function Get-Credential {throw 'Unexpected credential prompt in fixture'}
try {
    $env:KASA_USERNAME='previous-user';$env:KASA_PASSWORD='previous-value';$env:TAPO_TEST_FAIL='0'
    ExpectFailure { & $script On -Credential $credential }
    ExpectFailure { & $script Configure -Address '192.0.2.10','192.0.2.11','192.0.2.12','192.0.2.12' }
    Assert (-not (Test-Path -LiteralPath $record)) 'Unconfigured operation contacted devices'
    & $script Configure -Address '192.0.2.10','192.0.2.11','192.0.2.12','192.0.2.13' | Out-Null
    & $script On -WhatIf
    Assert (-not (Test-Path -LiteralPath $record)) 'WhatIf contacted devices'
    foreach($action in @('On','Off','State')) {
        $before=@(Calls).Count
        & $script $action -Credential $credential
        $newCalls=@(Calls | Select-Object -Skip $before)
        $expected=if($action -eq 'State'){4}else{8}
        Assert ($newCalls.Count -eq $expected) 'Default group invocation count failed'
        $offset=0
        foreach($address in @('192.0.2.10','192.0.2.11','192.0.2.12','192.0.2.13')) {
            $entry=$newCalls[$offset] | ConvertFrom-Json
            Assert ($entry[0] -eq '--host' -and $entry[1] -eq $address -and $entry[2] -eq $action.ToLowerInvariant()) 'Wrong target/action'
            $offset+=if($action -eq 'State'){1}else{2}
        }
    }
    $before=@(Calls).Count
    & $script Off -Address '192.0.2.12' -Credential $credential
    Assert (@(Calls).Count -eq ($before+2)) 'Individual override failed'
    & $script Discover -Credential $credential
    $entry=(@(Calls)[-1] | ConvertFrom-Json)
    Assert ($entry[0] -eq '--target' -and $entry[2] -eq 'discover') 'Discovery routing failed'
    $before=@(Calls).Count;$env:TAPO_TEST_FAIL='1'
    ExpectFailure { & $script Off -Credential $credential }
    Assert (@(Calls).Count -eq ($before+1)) 'Failure did not stop later plugs'
    Assert ($env:KASA_USERNAME -eq 'previous-user' -and $env:KASA_PASSWORD -eq 'previous-value') 'Credentials not restored'
    $env:TAPO_TEST_FAIL='0'
    $env:KASA_USERNAME='fixture@example.com';$env:KASA_PASSWORD='fixture-only'
    $before=@(Calls).Count
    & $script State
    Assert (@(Calls).Count -eq ($before+4)) 'Environment credentials did not operate all four plugs'
    Assert ($env:KASA_USERNAME -eq 'fixture@example.com' -and $env:KASA_PASSWORD -eq 'fixture-only') 'Environment credentials were not preserved'
    Write-Output 'PASS Tapo default four, individual override, dry run, discovery, failure stop and credential isolation'
} finally {
    $env:KASA_USERNAME=$savedUser;$env:KASA_PASSWORD=$savedPassword;$env:TAPO_TEST_FAIL=$savedFail
}

[CmdletBinding(SupportsShouldProcess=$true)]
param(
    [Parameter(Position=0)][ValidateSet('Setup','Configure','Discover','State','On','Off')][string]$Action='State',
    [string[]]$Address,
    [System.Management.Automation.PSCredential]$Credential,
    [string]$Target='255.255.255.255'
)
$ErrorActionPreference='Stop'
$environment=Join-Path $PSScriptRoot '.runtime/tapo'
$python=Join-Path $environment 'Scripts/python.exe'
$kasa=Join-Path $environment 'Scripts/kasa.exe'
$configPath=Join-Path $PSScriptRoot 'config/tapo.local.json'
if($Action -eq 'Setup') {
    if(-not $PSCmdlet.ShouldProcess($environment,'Install python-kasa 0.10.2')){return}
    if(-not (Test-Path -LiteralPath $python)) {
        & py -3.13 -m venv $environment
        if($LASTEXITCODE -ne 0){throw 'Python 3.13 environment creation failed'}
    }
    & $python -m pip --disable-pip-version-check install 'python-kasa==0.10.2'
    if($LASTEXITCODE -ne 0){throw 'python-kasa installation failed'}
    return
}
if($Action -in @('State','On','Off') -and -not $PSBoundParameters.ContainsKey('Address')) {
    if(-not (Test-Path -LiteralPath $configPath)){throw 'Register four plugs first: ./tapo.ps1 Configure -Address IP1,IP2,IP3,IP4'}
    $config=Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
    $Address=@($config.addresses)
    if($Address.Count -ne 4){throw 'The saved Tapo group must contain four addresses; run Configure again'}
}
if($Action -ne 'Discover' -and -not $Address){throw 'Specify -Address with the IP address(es) of the intended plug(s)'}
if($Action -eq 'Discover' -and $Address){throw 'Discover uses -Target, not -Address'}
$normalized=@()
foreach($deviceAddress in $Address) {
    $parsedAddress=$null
    if(-not [System.Net.IPAddress]::TryParse($deviceAddress,[ref]$parsedAddress)) {
        throw 'Each -Address must be an IP address'
    }
    $normalized+=$parsedAddress.ToString()
}
$Address=@($normalized | Select-Object -Unique)
if(($Action -eq 'Configure' -or ($Action -in @('State','On','Off') -and -not $PSBoundParameters.ContainsKey('Address'))) -and $Address.Count -ne 4) {
    throw 'The default Tapo group must contain exactly four distinct addresses'
}
if($Action -eq 'Configure') {
    if($PSCmdlet.ShouldProcess($configPath,'Save the four default Tapo plugs')) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $configPath) | Out-Null
        @{addresses=$Address} | ConvertTo-Json | Set-Content -LiteralPath $configPath -Encoding UTF8
        Write-Output 'Four default plugs saved. Use ./tapo.ps1 On, Off, or State.'
    }
    return
}
if(-not (Test-Path -LiteralPath $kasa)){throw 'Run ./tapo.ps1 Setup first'}
$selected=@()
if($Action -eq 'Discover') {
    if(-not $PSCmdlet.ShouldProcess($Target,'Discover Tapo devices (read only)')){return}
} else {
    foreach($deviceAddress in ($Address | Select-Object -Unique)) {
        if($PSCmdlet.ShouldProcess($deviceAddress,$Action)){$selected+= $deviceAddress}
    }
    if(-not $selected.Count){return}
}
if(-not $Credential -and $env:KASA_USERNAME -and $env:KASA_PASSWORD) {
    $Credential=[pscredential]::new($env:KASA_USERNAME,(ConvertTo-SecureString $env:KASA_PASSWORD -AsPlainText -Force))
}
if(-not $Credential){$Credential=Get-Credential -Message 'Tapo app account: email and password (not saved)'}
if(-not $Credential){throw 'Tapo credentials are required'}
$previousUsername=$env:KASA_USERNAME
$previousPassword=$env:KASA_PASSWORD
try {
    # Credentials travel in the child environment, never command arguments/files.
    $env:KASA_USERNAME=$Credential.UserName
    $env:KASA_PASSWORD=$Credential.GetNetworkCredential().Password
    if($Action -eq 'Discover') {
        & $kasa --target $Target discover
        if($LASTEXITCODE -ne 0){throw 'Discovery failed; check credentials and LAN connectivity'}
    } else {
        foreach($deviceAddress in $selected) {
            & $kasa --host $deviceAddress $Action.ToLowerInvariant()
            if($LASTEXITCODE -ne 0){throw "Tapo $Action failed for $deviceAddress; later plugs were not changed"}
            if($Action -in @('On','Off')) {
                & $kasa --host $deviceAddress state
                if($LASTEXITCODE -ne 0){throw "Command sent to $deviceAddress but state readback failed"}
            }
        }
    }
} finally {
    $env:KASA_USERNAME=$previousUsername
    $env:KASA_PASSWORD=$previousPassword
}

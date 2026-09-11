[CmdletBinding()]
param([switch]$Devices)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $repo 'artifacts/native'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vs) { throw 'MSVC x64 tools required' }
$setup=Join-Path $vs 'VC/Auxiliary/Build/vcvars64.bat'
$line='cl /nologo /std:c++20 /EHsc /W4 /WX /O2 /I "'+(Join-Path $repo 'Source')+'" "'+(Join-Path $repo 'tests/core_tests.cpp')+'" /Fe:core_tests.exe'
$waterLine='cl /nologo /std:c++20 /EHsc /W4 /WX /O2 /I "'+(Join-Path $repo 'Source')+'" "'+(Join-Path $repo 'tests/water_tests.cpp')+'" /Fe:water_tests.exe'
$audioLine='cl /nologo /std:c++20 /EHsc /W4 /WX /O2 /I "'+(Join-Path $repo 'Source')+'" "'+(Join-Path $repo 'tests/audio_tests.cpp')+'" /Fe:audio_tests.exe'
$trackingLine='cl /nologo /std:c++20 /EHsc /W4 /WX /O2 /I "'+(Join-Path $repo 'Source')+'" "'+(Join-Path $repo 'tests/tracking_tests.cpp')+'" /Fe:tracking_tests.exe'
if($Devices) {
    & (Join-Path $PSScriptRoot 'bootstrap.ps1')
    $sdk=Join-Path $repo 'ThirdParty/OpenVR'
    $line='cl /nologo /std:c++20 /EHsc /W3 /O2 /I "'+(Join-Path $repo 'Source')+'" /I "'+(Join-Path $sdk 'headers')+'" "'+(Join-Path $repo 'Source/RowDevices.cpp')+'" "'+(Join-Path $repo 'tools/device_probe.cpp')+'" /Fe:device_probe.exe /link windowsapp.lib "'+(Join-Path $sdk 'lib/win64/openvr_api.lib')+'"'
    Copy-Item -LiteralPath (Join-Path $sdk 'bin/win64/openvr_api.dll') -Destination $out -Force
}
$bat=Join-Path $out 'build.cmd'
$commands=@('@echo off',('call "'+$setup+'" >nul'),$line,'if errorlevel 1 exit /b %errorlevel%')
if(-not $Devices) { $commands+=@($waterLine,'if errorlevel 1 exit /b %errorlevel%',$audioLine,'if errorlevel 1 exit /b %errorlevel%',$trackingLine) }
$commands+='exit /b %errorlevel%'
$commands | Set-Content -LiteralPath $bat -Encoding ascii
Push-Location $out
try {
    & $bat; if($LASTEXITCODE -ne 0) { throw 'Native compilation failed' }
    if(-not $Devices) {
        & './core_tests.exe'; if($LASTEXITCODE -ne 0) { throw 'Core tests failed' }
        & './water_tests.exe' (Join-Path $repo 'artifacts/water'); if($LASTEXITCODE -ne 0) { throw 'Water tests failed' }
        & './audio_tests.exe'; if($LASTEXITCODE -ne 0) { throw 'Audio tests failed' }
        & './tracking_tests.exe'; if($LASTEXITCODE -ne 0) { throw 'Tracking tests failed' }
    }
}
finally { Pop-Location }

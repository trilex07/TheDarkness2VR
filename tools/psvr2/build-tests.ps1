# Offline PSVR2 bridge tests. Does not launch Steam or install into the game.
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$out = Join-Path $root 'tests\psvr2\out'
$complete = Join-Path $out 'complete'
$incomplete = Join-Path $out 'incomplete'
$missing = Join-Path $out 'missing'
$bridgeDir = Join-Path $out 'bridge'
foreach ($dir in @($out, $complete, $incomplete, $missing, $bridgeDir)) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ tools not found' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'

function Invoke-Vc([string]$arch, [string]$command) {
    Write-Host "== $arch : $command"
    cmd /c "`"$vcvars`" $arch && $command"
    if ($LASTEXITCODE -ne 0) { throw "command failed ($LASTEXITCODE): $command" }
}

Write-Host '== x86 d3d9.dll'
& (Join-Path $root 'build.ps1')
if ($LASTEXITCODE -ne 0) { throw 'x86 d3d9 build failed' }

$src = Join-Path $root 'src\psvr2'
$tests = Join-Path $root 'tests\psvr2'
Invoke-Vc 'x64' "cl /nologo /LD /EHsc /W3 /O2 /MD /std:c++17 /Fo`"$complete\\`" `"$tests\fake_loader.cpp`" /Fe:`"$complete\psvr2_toolkit_capi_loader.dll`" /link /DLL"
Invoke-Vc 'x64' "cl /nologo /LD /EHsc /W3 /O2 /MD /std:c++17 /Fo`"$incomplete\\`" `"$tests\fake_incomplete_loader.cpp`" /Fe:`"$incomplete\psvr2_toolkit_capi_loader.dll`" /link /DLL"
Invoke-Vc 'x64' "cl /nologo /EHsc /W3 /O2 /MD /std:c++17 /Fo`"$bridgeDir\\`" `"$src\BridgeMain.cpp`" `"$src\ToolkitBackend.cpp`" /Fe:`"$bridgeDir\Darkness2Psvr2Bridge.exe`" /link advapi32.lib"
Copy-Item (Join-Path $complete 'psvr2_toolkit_capi_loader.dll') (Join-Path $bridgeDir 'psvr2_toolkit_capi_loader.dll') -Force
Invoke-Vc 'x64' "cl /nologo /EHsc /W3 /O2 /MD /std:c++17 /Fo`"$out\\`" `"$tests\policy_tests.cpp`" /Fe:`"$out\policy_tests.exe`""
Invoke-Vc 'x64' "cl /nologo /EHsc /W3 /O2 /MD /std:c++17 /Fo`"$out\\`" `"$tests\protocol_tests.cpp`" /Fe:`"$out\protocol_tests.exe`""
Invoke-Vc 'x64' "cl /nologo /EHsc /W3 /O2 /MD /std:c++17 /Fo`"$out\\`" `"$tests\backend_tests.cpp`" `"$src\ToolkitBackend.cpp`" /Fe:`"$out\backend_tests.exe`""
Invoke-Vc 'x86' "cl /nologo /EHsc /W3 /O2 /MD /std:c++17 /Fo`"$out\\`" `"$tests\loopback_client.cpp`" /Fe:`"$out\loopback_client.exe`""

& (Join-Path $out 'policy_tests.exe')
if ($LASTEXITCODE -ne 0) { throw 'policy tests failed' }
& (Join-Path $out 'protocol_tests.exe')
if ($LASTEXITCODE -ne 0) { throw 'protocol tests failed' }
& (Join-Path $out 'backend_tests.exe') $complete $incomplete $missing
if ($LASTEXITCODE -ne 0) { throw 'backend tests failed' }
$status = Join-Path $out 'loopback-status.bin'
& (Join-Path $out 'loopback_client.exe') (Join-Path $bridgeDir 'Darkness2Psvr2Bridge.exe') $status
if ($LASTEXITCODE -ne 0) { throw 'loopback test failed' }
Write-Host 'PSVR2 offline tests PASS'

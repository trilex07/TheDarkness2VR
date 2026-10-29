# Production helper only. No fake Toolkit and no Steam/driver installation.
param([string]$OutputDirectory=(Join-Path $env:TEMP 'd2vr-psvr2-production'))
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Visual Studio C++ tools not found'}
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$out=(Resolve-Path $OutputDirectory).Path
$src=Join-Path $root 'src\psvr2'
cmd /c "`"$vc`" x64 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 /Fo`"$out\\`" `"$src\BridgeMain.cpp`" `"$src\ToolkitBackend.cpp`" /Fe:`"$out\Darkness2Psvr2Bridge.exe`" /link advapi32.lib"
if($LASTEXITCODE){throw 'Production PSVR2 bridge build failed'}
Get-FileHash (Join-Path $out 'Darkness2Psvr2Bridge.exe') -Algorithm SHA256

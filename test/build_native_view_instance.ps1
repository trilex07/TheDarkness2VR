$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'native_view_instance_test.cpp'
$exe=Join-Path $env:TEMP 'd2vr-native-view-instance.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$source`" /Fo:`"$env:TEMP\d2vr-native-view-instance.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Instance test build failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Instance test failed'}

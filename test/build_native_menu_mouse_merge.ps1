$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'native_menu_mouse_merge_test.cpp'
$exe=Join-Path $env:TEMP 'd2vr-native-menu-mouse-merge.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 `"$source`" /Fo:`"$env:TEMP\d2vr-native-menu-mouse-merge.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Menu mouse merge regression compile failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Menu mouse merge regression failed'}

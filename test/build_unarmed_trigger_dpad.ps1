$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'unarmed_trigger_dpad_test.cpp'
$exe=Join-Path $env:TEMP 'd2vr-unarmed-trigger-dpad.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 `"$source`" /Fo:`"$env:TEMP\d2vr-unarmed-trigger-dpad.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Unarmed trigger D-pad regression compile failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Unarmed trigger D-pad regression failed'}

$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$sdk=$env:OPENXR_SDK
if(!$sdk){$sdk='C:\Users\Trilex\openxr-sdk'}
$source=Join-Path $PSScriptRoot 'vr_menu_pointer_test.cpp'
$exe=Join-Path $env:TEMP 'd2vr-vr-menu-pointer.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 /I`"$sdk/include`" `"$source`" /Fo:`"$env:TEMP\d2vr-vr-menu-pointer.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Menu pointer core compile failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Menu pointer core regression failed'}

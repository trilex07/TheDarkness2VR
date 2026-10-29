$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$sdk=$env:OPENXR_SDK
if(!$sdk){$sdk='C:\Users\Trilex\openxr-sdk'}
$inc=Join-Path $sdk 'include'
$source=Join-Path $PSScriptRoot 'xr_display_refresh_test.cpp'
$exe=Join-Path $env:TEMP 'd2vr-xr-display-refresh.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 /I`"$inc`" `"$source`" /Fo:`"$env:TEMP\d2vr-xr-display-refresh.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Refresh adapter regression compile failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Refresh adapter regression failed'}

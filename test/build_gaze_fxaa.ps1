$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$exe=Join-Path $env:TEMP 'd2vr-gaze-fxaa-test.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /wd4324 /O2 /MD /std:c++17 `"$PSScriptRoot/gaze_fxaa_test.cpp`" /Fo:`"$env:TEMP/d2vr-gaze-fxaa.obj`" /Fe:`"$exe`" /link d3d11.lib"
if($LASTEXITCODE -ne 0){throw 'FXAA fixture build failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'FXAA fixture failed'}

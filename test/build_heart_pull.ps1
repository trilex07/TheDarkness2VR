$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$out=Join-Path $env:TEMP 'd2vr-heart-pull.exe'
$source=Join-Path $PSScriptRoot 'heart_pull_test.cpp'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$source`" /Fo:`"$env:TEMP\d2vr-heart-pull.obj`" /Fe:`"$out`""
if($LASTEXITCODE -ne 0){throw 'Heart pull test build failed'}
& $out
if($LASTEXITCODE -ne 0){throw 'Heart pull tests failed'}

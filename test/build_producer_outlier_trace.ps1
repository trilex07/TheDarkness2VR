$ErrorActionPreference='Stop'
$src=Join-Path $PSScriptRoot 'producer_outlier_trace_test.cpp'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$exe=Join-Path $env:TEMP 'd2vr-producer-outlier-trace.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$src`" /Fo:`"$env:TEMP\d2vr-producer-outlier-trace.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Producer outlier trace build failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Producer outlier trace regression failed'}

$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$src=Join-Path $PSScriptRoot 'recorder_lock_test.cpp'
$exe=Join-Path $env:TEMP 'd2vr-recorder-lock.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 `"$src`" /Fo:`"$env:TEMP\d2vr-recorder-lock.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Recorder lock test build failed'}
$global:LASTEXITCODE=$null
& $exe
if($null -eq $LASTEXITCODE -or $LASTEXITCODE -ne 0){throw 'Recorder lock test failed or did not start'}

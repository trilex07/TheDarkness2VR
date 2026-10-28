$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'nis_eye_host_warp_test.cpp'
$exe=Join-Path $env:TEMP 'd2vr-nis-eye-host.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$source`" /Fo:`"$env:TEMP\d2vr-nis-eye-host.obj`" /Fe:`"$exe`" /link d3d11.lib"
if($LASTEXITCODE -ne 0){throw 'NIS host fixture build failed'}
$global:LASTEXITCODE=$null
& $exe
if($null -eq $LASTEXITCODE -or $LASTEXITCODE -ne 0){throw 'NIS host fixture failed or did not start'}

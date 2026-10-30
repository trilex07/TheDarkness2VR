$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $env:TEMP 'd2vr-submission-epoch'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'submission_epoch_test.cpp'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 `"$source`" /Fo:`"$out\test.obj`" /Fe:`"$out\test.exe`""
if($LASTEXITCODE){throw 'Submission epoch x86 build failed'}
& "$out\test.exe"
if($LASTEXITCODE){throw 'Submission epoch regression failed'}

$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$vs=& "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$out=Join-Path $env:TEMP 'd2vr-matrix-cpu'
New-Item -ItemType Directory -Force -Path $out|Out-Null
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 `"$PSScriptRoot/matrix_cpu_test.cpp`" /Fo:`"$out/test.obj`" /Fe:`"$out/test.exe`""
if($LASTEXITCODE -ne 0){throw 'Matrix CPU build failed'}
& "$out/test.exe"
if($LASTEXITCODE -ne 0){throw 'Matrix CPU regression failed'}

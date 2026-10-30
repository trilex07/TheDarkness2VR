$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$out=Join-Path $env:TEMP 'd2vr-mapped-vertex-copy'
New-Item -ItemType Directory -Force -Path $out|Out-Null
cmd /c "`"$vc`" x86 && cl /nologo /O2 /EHsc /W4 /WX /MD /std:c++17 /I`"$repo\src`" `"$PSScriptRoot\mapped_vertex_copy_test.cpp`" /Fo:`"$out\test.obj`" /Fe:`"$out\test.exe`""
if($LASTEXITCODE -ne 0){throw 'Compile failed'}
& (Join-Path $out 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Mapped copy regression failed'}

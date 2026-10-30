$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$config=Get-Content (Join-Path $repo 'src/d3d9.cpp') -Raw
$ini=Get-Content (Join-Path $repo 'Darkness2VR.ini') -Raw
if($config -notmatch 'g_replayConstantReuse = false;' -or $config -notmatch '"ReplayConstantReuse", 0, chosenIni' -or $ini -notmatch 'ReplayConstantReuse=0'){throw 'Float reuse must stay independently OFF by default'}
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'replay_constant_reuse_test.cpp'
$exe=Join-Path $env:TEMP 'd2vr-replay-constant-reuse.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /std:c++17 `"$source`" /Fo:`"$env:TEMP\d2vr-replay-constant-reuse.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'Float reuse regression compile failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Float reuse regression failed'}

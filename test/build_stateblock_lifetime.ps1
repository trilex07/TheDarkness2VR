param([switch]$Pure)
$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$out=Join-Path $env:TEMP ('d2vr-stateblock-lifetime-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out|Out-Null
$source=Join-Path $PSScriptRoot 'stateblock_lifetime_test.cpp'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 `"$source`" /Fo:`"$out\test.obj`" /Fe:`"$out\test.exe`" /link user32.lib"
if($LASTEXITCODE -ne 0){throw 'StateBlock lifetime build failed'}
if($Pure){& (Join-Path $out 'test.exe') --pure}else{& (Join-Path $out 'test.exe')}
if($LASTEXITCODE -ne 0){throw 'StateBlock lifetime test failed'}

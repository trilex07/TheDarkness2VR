param([switch]$Hardware)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$source=Get-Content -LiteralPath (Join-Path $repo 'src\d3d9.cpp') -Raw
$begin=$source.IndexOf('struct EyeColorHistory {')
$end=$source.IndexOf('static IDirect3DSurface9* g_liveColorBackup',$begin)
if($begin -lt 0 -or $end -le $begin){throw 'Actual history implementation missing'}
$fixture=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'eye_color_history_d3d9_test.cpp') -Raw
$out=Join-Path $env:TEMP 'd2vr-eye-history-driver'
New-Item -ItemType Directory -Path $out -Force|Out-Null
$generated=Join-Path $out 'eye_color_history_d3d9_test.cpp'
$fixture.Replace('@IMPLEMENTATION@',$source.Substring($begin,$end-$begin))|Set-Content -LiteralPath $generated -Encoding UTF8
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$exe=Join-Path $out 'eye_color_history_d3d9_test.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$generated`" /Fo:`"$out\history.obj`" /Fe:`"$exe`" /link user32.lib"
if($LASTEXITCODE -ne 0){throw 'Driver history test build failed'}
$global:LASTEXITCODE=$null
if($Hardware){& $exe --hardware}else{& $exe}
if($null -eq $LASTEXITCODE -or $LASTEXITCODE -ne 0){throw 'Driver history test failed or did not start'}

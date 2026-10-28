$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$source=Get-Content -LiteralPath (Join-Path $repo 'src\d3d9.cpp') -Raw
$begin=$source.IndexOf('struct EyeColorHistory {')
$end=$source.IndexOf('static IDirect3DSurface9* g_liveColorBackup',$begin)
if($begin -lt 0 -or $end -le $begin){throw 'Actual history implementation missing'}
$body=$source.Substring($begin,$end-$begin)
$call=[regex]::Match($source,'const bool historyOk = SeedRightEyeBackground\(dev, liveEnd.targets\[0\], [^;]+;')
if(-not $call.Success){throw 'Actual history caller missing'}
$body+="`nstatic bool SeedActualCaller(IDirect3DDevice9* dev, IDirect3DSurface9* output) { struct {IDirect3DSurface9* targets[1];} liveEnd{{output}}; "+$call.Value+" return historyOk; }`n"
$fixture=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'eye_color_history_test.cpp') -Raw
$generated=Join-Path $env:TEMP 'd2vr-eye-color-history.cpp'
$fixture.Replace('@IMPLEMENTATION@',$body)|Set-Content -LiteralPath $generated -Encoding UTF8
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$exe=Join-Path $env:TEMP 'd2vr-eye-color-history.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$generated`" /Fo:`"$env:TEMP\d2vr-eye-color-history.obj`" /Fe:`"$exe`""
if($LASTEXITCODE -ne 0){throw 'History fixture build failed'}
$global:LASTEXITCODE=$null
& $exe
if($null -eq $LASTEXITCODE -or $LASTEXITCODE -ne 0){throw 'History fixture failed or did not start'}

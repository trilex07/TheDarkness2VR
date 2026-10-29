$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$source=Get-Content -LiteralPath (Join-Path $repo 'src\d3d9.cpp') -Raw
$begin=$source.IndexOf('struct EyeColorHistory {')
$end=$source.IndexOf('static IDirect3DSurface9* g_liveColorBackup',$begin)
if($begin -lt 0 -or $end -le $begin){throw 'Actual history implementation missing'}
$body=(Get-Content (Join-Path $repo 'src/replay_state_cache.h') -Raw)+"`n"+$source.Substring($begin,$end-$begin)
$call=[regex]::Match($source,'const bool historyOk = SeedRightEyeBackground\(dev, liveEnd.targets\[0\], [^;]+;')
if(-not $call.Success){throw 'Actual history caller missing'}
$body+="`nstatic bool SeedActualCaller(IDirect3DDevice9* dev, IDirect3DSurface9* output) { struct {IDirect3DSurface9* targets[1];} liveEnd{{output}}; "+$call.Value+" return historyOk; }`n"
$save=[regex]::Match($source,'bool eye1Ok = FinishRightEyeHistory\(dev, liveEnd.targets\[0\], replayOk, historyOk\);')
if(-not $save.Success){throw 'Actual history publication missing'}
$body+="`nstatic bool SaveActualCaller(IDirect3DDevice9* dev, IDirect3DSurface9* output, bool replayOk, bool historyOk) { struct {IDirect3DSurface9* targets[1];} liveEnd{{output}}; "+$save.Value+" return eye1Ok; }`n"
$replay=Get-Content -LiteralPath (Join-Path $repo 'src\stereo_replay.inl') -Raw
$drawBegin=$replay.IndexOf('        case OP_DrawPrimitive: {',$replay.IndexOf('static bool ReplayStream('))
$drawEnd=$replay.IndexOf('        case OP_SetVertexDeclaration:', $drawBegin)
if($drawBegin -lt 0 -or $drawEnd -le $drawBegin){throw 'Actual replay draw cases missing'}
$body+="`nstatic bool ActualDraw(IDirect3DDevice9* dev, unsigned op) { bool replayComplete=true; d2vr_replay_state::Cache stateCache;d2vr_replay_state::Bindings bindings;using Binding=d2vr_replay_state::Bindings; struct {unsigned a[6]{};unsigned n=0;} r; const unsigned char dataBase[256]{};const void* replayTex[16]{};int eye=1;switch(op){`n"+$replay.Substring($drawBegin,$drawEnd-$drawBegin)+"`n}return replayComplete;}`n"
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

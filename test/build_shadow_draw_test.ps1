$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$src=Get-Content (Join-Path $repo 'src/stereo_replay.inl') -Raw
$main=Get-Content (Join-Path $repo 'src/d3d9.cpp') -Raw
if(-not $main.Contains('static bool g_skipShadowAtlasDraws = false;')){throw 'Must default OFF'}
$start=$src.IndexOf('static bool SkipShadowDrawForTest(')
$end=$src.IndexOf('static HRESULT STDMETHODCALLTYPE Hook_SetScissorRect',$start)
$classStart=$src.IndexOf('static bool IsShadowAtlas(')
$classEnd=$src.IndexOf('struct ShadowPassState',$classStart)
if($start -lt 0 -or $end -lt 0 -or $classStart -lt 0 -or $classEnd -lt 0){throw 'Missing actual source'}
# The neighboring world-draw bookkeeping has its own actual-source fixture.
# Extract only the classifier and draw wrappers exercised here.
$noteStart=$src.IndexOf('static void NoteRecordedWorldDraw() {',$start)
$noteEnd=$src.IndexOf('static HRESULT DrawPrimitiveWithDiffuse(',$noteStart)
if($noteStart -lt $start -or $noteEnd -le $noteStart -or $noteEnd -ge $end){throw 'World draw source boundaries missing'}
$drawSource=$src.Substring($start,$noteStart-$start)+$src.Substring($noteEnd,$end-$noteEnd)
$out=Join-Path $env:TEMP ('d2vr-shadow-draw-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out -Force|Out-Null
$prefix=@'
#include <windows.h>
#include <d3d9.h>
#include <cassert>
#include <cstdio>
bool g_skipShadowAtlasDraws=false,g_skipObjectDraws=false,shadow=false;
int g_replayEye=-1,queries=0,calls=0,scopes=0,observed=0;
const void* g_boundVs=nullptr;const void* g_boundPs=nullptr;
namespace d2vr_pipeline_warmup {void ObserveDraw(IDirect3DDevice9*,const void*,const void*){++observed;}}
bool IsShadowAtlasPass(IDirect3DDevice9*){++queries;return shadow;}
void Log(const char*,...){}
struct WorldDiffuseScope {explicit WorldDiffuseScope(IDirect3DDevice9*){++scopes;}};
struct TentacleHeadScope {explicit TentacleHeadScope(IDirect3DDevice9*){++scopes;}};
struct Originals {
 HRESULT DrawPrimitive(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT){++calls;return E_FAIL;}
 HRESULT DrawIndexedPrimitive(IDirect3DDevice9*,D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT){++calls;return E_FAIL;}
 HRESULT DrawPrimitiveUP(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,const void*,UINT){++calls;return E_FAIL;}
 HRESULT DrawIndexedPrimitiveUP(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT,UINT,const void*,D3DFORMAT,const void*,UINT){++calls;return E_FAIL;}
}g_orig;
'@
$suffix=@'
void Run(HRESULT expected,UINT pc=1){
 assert(DrawPrimitiveWithDiffuse(nullptr,D3DPT_TRIANGLELIST,0,pc)==expected);
 assert(DrawIndexedPrimitiveWithDiffuse(nullptr,D3DPT_TRIANGLELIST,0,0,3,0,pc)==expected);
 assert(DrawPrimitiveUPWithDiffuse(nullptr,D3DPT_TRIANGLELIST,pc,nullptr,12)==expected);
 assert(DrawIndexedPrimitiveUPWithDiffuse(nullptr,D3DPT_TRIANGLELIST,0,3,pc,nullptr,D3DFMT_INDEX16,nullptr,12)==expected);
}
int main(){
 D3DSURFACE_DESC rt{};rt.Format=D3DFMT_R5G6B5;rt.Width=rt.Height=2048;
 D3DVIEWPORT9 vp{};vp.Width=vp.Height=1024;
 assert(IsShadowAtlas(rt,vp));
 vp.Width=vp.Height=2048;assert(!IsShadowAtlas(rt,vp));
 vp.Width=0;assert(!IsShadowAtlas(rt,vp));
 vp.Width=1024;vp.Height=512;assert(!IsShadowAtlas(rt,vp));
 vp.Height=1024;rt.Format=D3DFMT_A8R8G8B8;assert(!IsShadowAtlas(rt,vp));
 rt.Format=D3DFMT_R5G6B5;rt.Height=1024;assert(!IsShadowAtlas(rt,vp));
 shadow=true;Run(E_FAIL);assert(calls==4 && queries==0 && observed==4);
 g_skipShadowAtlasDraws=true;shadow=false;Run(E_FAIL);assert(calls==8);
 shadow=true;
 for(int eye=-1;eye<=1;++eye){g_replayEye=eye;int before=scopes;Run(D3D_OK);assert(calls==8 && scopes==before);}
 g_skipShadowAtlasDraws=false;Run(E_FAIL);assert(calls==12);
 shadow=false;Run(E_FAIL,12);assert(calls==16);
 g_skipObjectDraws=true;
 for(int eye=-1;eye<=1;++eye){g_replayEye=eye;int before=scopes;Run(D3D_OK,12);assert(calls==16 && scopes==before);}
 Run(E_FAIL,2);assert(calls==20);
 assert(!SkipObjectDrawForTest(D3DPT_LINELIST,100));
 assert(!SkipObjectDrawForTest(D3DPT_POINTLIST,100));
 assert(SkipObjectDrawForTest(D3DPT_TRIANGLESTRIP,3));
 assert(SkipObjectDrawForTest(D3DPT_TRIANGLEFAN,3));
 g_skipObjectDraws=false;Run(E_FAIL,12);assert(calls==24);
 // Pipeline learning: forwarded live draws only; skipped and replay-eye draws never learn.
 const int learned=observed;
 for(int eye=0;eye<=1;++eye){g_replayEye=eye;Run(E_FAIL);}
 assert(calls==32 && observed==learned);
 g_replayEye=-1;g_skipObjectDraws=true;Run(D3D_OK,12);assert(calls==32 && observed==learned);
 g_skipObjectDraws=false;Run(E_FAIL);assert(calls==36 && observed==learned+4);
 puts("PASS actual four draw wrappers: default OFF/no query, native HRESULT, non-atlas forwarding, live+both replay eyes skipped; classifier excludes camera/full/invalid targets; pipeline learning live forwarded draws only. No FPS claim.");
}
'@
[IO.File]::WriteAllText((Join-Path $out 'test.cpp'),$prefix+"`n"+$src.Substring($classStart,$classEnd-$classStart)+$drawSource+$suffix)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $out
try {
 cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 test.cpp /Fe:shadow_draw_regression.exe"
 if($LASTEXITCODE -ne 0){throw 'Compile failed'}
 & .\shadow_draw_regression.exe
 if($LASTEXITCODE -ne 0){throw 'Regression failed'}
}finally{Pop-Location}

$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$src=Get-Content (Join-Path $root 'src/stereo_replay.inl') -Raw
$start=$src.IndexOf('struct ShadowClipScope {')
$end=$src.IndexOf('struct ReflectionEyeScope {',$start)
if($start -lt 0 -or $end -le $start){throw 'Scope missing'}
if(([regex]::Matches($src,'const ShadowClipScope shadowClip\(dev\);')).Count -ne 4){throw 'All four draw wrappers required'}
$out=Join-Path $root 'test-output/edge-shadow-20261104/scope'
New-Item -ItemType Directory -Force $out|Out-Null
$prefix=@'
#include <windows.h>
#include <cassert>
#include <cstring>
#include <cstdio>
struct IDirect3DDevice9 {bool fail=false;float current[4]={7,8,9,10};int reads=0;
 HRESULT GetPixelShaderConstantF(unsigned r,float* out,unsigned n){assert(r==223&&n==1);++reads;if(fail)return E_FAIL;memcpy(out,current,16);return S_OK;}};
bool g_shadowClipCoordinates=true,g_noInvProj=false;
int g_replayEye=-1,g_projectionFix=3,g_lightingPreset=0,g_activePsInverse=27;
const void* g_boundPs=nullptr;
struct Metadata {bool shadowClip=true;} metadata;
struct Bank {Metadata Lookup(const void*){return metadata;}} g_pixelShaders;
struct Params {bool headGeometry=true;int mode=0;} params;
Params MakeEyeParams(int){return params;}
namespace d2vr_submission {int invalidations=0;void Invalidate(){++invalidations;}}
struct Driver {int writes=0;HRESULT SetPixelShaderConstantF(IDirect3DDevice9* d,unsigned r,const float* v,unsigned n){assert(r==223&&n==1);++writes;memcpy(d->current,v,16);return S_OK;}} g_orig;
'@
$suffix=@'
void run(IDirect3DDevice9& d,float expected){const float old[4]={7,8,9,10};memcpy(d.current,old,16);int before=g_orig.writes;{ShadowClipScope s(&d);assert(d.current[0]==expected);}assert(!memcmp(d.current,old,16));assert(g_orig.writes==before+2);}
int main(){IDirect3DDevice9 d;
 for(int eye=-1;eye<=1;++eye){g_replayEye=eye;run(d,1);params.mode=3;run(d,0);params.mode=0;params.headGeometry=false;run(d,0);params.headGeometry=true;}
 g_lightingPreset=-1;run(d,1);g_lightingPreset=0;
 g_noInvProj=true;run(d,0);g_noInvProj=false;g_projectionFix=1;run(d,0);g_projectionFix=3;
 for(int p=1;p<=9;++p){g_lightingPreset=p;run(d,0);}g_lightingPreset=0;
 g_activePsInverse=-1;run(d,0);g_activePsInverse=27;
 int writes=g_orig.writes;metadata.shadowClip=false;{ShadowClipScope s(&d);}assert(writes==g_orig.writes);
 metadata.shadowClip=true;g_shadowClipCoordinates=false;{ShadowClipScope s(&d);}assert(writes==g_orig.writes);
 g_shadowClipCoordinates=true;d.fail=true;{ShadowClipScope s(&d);}assert(writes==g_orig.writes);
 puts("PASS actual draw scope: both eyes, cinema/native branch, incompatible settings, unknown shader, failed getter, exact constant restore, all four wrappers wired");
}
'@
[IO.File]::WriteAllText((Join-Path $out 'scope.cpp'),$prefix+$src.Substring($start,$end-$start)+$suffix)
$vs=& "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $out
try {
 cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 scope.cpp /Fe:scope.exe"
 if($LASTEXITCODE){throw 'Compile failed'}
 & ./scope.exe
 if($LASTEXITCODE){throw 'Scope regression failed'}
}finally{Pop-Location}

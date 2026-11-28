$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$src=Get-Content (Join-Path $root 'src/stereo_replay.inl') -Raw
$live=Get-Content (Join-Path $root 'src/d3d9.cpp') -Raw
if(!$live.Contains('!d2vr_replay::SkipLightVolumeShift(d2vr_replay::g_activeVs)') -or
   !$src.Contains('!SkipLightVolumeShift(g_activeVs)')){throw 'Both eyes must use the same light-volume shift gate'}
$gateStart=$src.IndexOf('static bool SkipLightVolumeShift(')
$gateEnd=$src.IndexOf('static int ActiveWvpRegister(',$gateStart)
$gate=$src.Substring($gateStart,$gateEnd-$gateStart)
$capture=Get-Content (Join-Path $root 'src/matched_capture.inl') -Raw
$captureFinish=[regex]::Match($capture,'(?ms)^static void FinishSubmission\(.*?^\}')
if(!$captureFinish.Success -or !$live.Contains('matched_capture::FinishSubmission(endR,worldLayerCount,layerCount,weaponLaserLayerCount,ammoLayerCount,')){throw 'Actual matched capture submission gate missing'}
$start=$src.IndexOf('struct ShadowClipScope {')
$end=$src.IndexOf('struct ReflectionEyeScope {',$start)
if($start -lt 0 -or $end -le $start){throw 'Scope missing'}
if(([regex]::Matches($src,'const ShadowClipScope shadowClip\(dev\);')).Count -ne 4){throw 'All four draw wrappers required'}
$out=Join-Path $root 'test-output/edge-shadow-20261104/scope'
New-Item -ItemType Directory -Force $out|Out-Null
$prefix=@'
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include "vr_input.h"
struct IDirect3DDevice9 {bool fail=false;float current[4]={7,8,9,10};int reads=0;
 HRESULT GetPixelShaderConstantF(unsigned r,float* out,unsigned n){assert(r==223&&n==1);++reads;if(fail)return E_FAIL;memcpy(out,current,16);return S_OK;}};
bool g_shadowClipCoordinates=true,g_noInvProj=false;
struct VsInfo {unsigned hash;};
const VsInfo* g_activeVs=nullptr;
int g_skipLightVsShift=0;
#define XR_SUCCEEDED(result) ((result)>=0)
using XrResult=int;
const bool g_had3DCameraThisFrame=true,g_pairSubmitMetaValid=true;
const struct {unsigned id;} g_trackedFrame{41};const unsigned g_eyePairFrameId=g_trackedFrame.id;
namespace matched_capture {
bool complete=false,active=true;FILE* meta=nullptr;
const unsigned long long frame=41;
const unsigned mask=15,inputs=3,blits=3,projectionObserved=3;
void Finish(bool value){complete=value;}
ACTUAL_CAPTURE_FINISH
}
int g_replayEye=-1,g_projectionFix=3,g_lightingPreset=0,g_activePsInverse=27;
const void* g_boundPs=nullptr;
struct Metadata {bool shadowClip=true,compositeHeightCoordinates=false;} metadata;
struct Bank {Metadata Lookup(const void*){return metadata;}} g_pixelShaders;
struct Params {bool headGeometry=true;int mode=0;} params;
Params MakeEyeParams(int){return params;}
namespace d2vr_submission {int invalidations=0;void Invalidate(){++invalidations;}}
struct Driver {int writes=0;HRESULT SetPixelShaderConstantF(IDirect3DDevice9* d,unsigned r,const float* v,unsigned n){assert(r==223&&n==1);++writes;memcpy(d->current,v,16);return S_OK;}} g_orig;
'@
$suffix=@'
void run(IDirect3DDevice9& d,float expected){const float old[4]={7,8,9,10};memcpy(d.current,old,16);int before=g_orig.writes;{ShadowClipScope s(&d);assert(d.current[0]==expected);}assert(!memcmp(d.current,old,16));assert(g_orig.writes==before+2);}
bool capture(unsigned worldLayerCount,unsigned weaponLaserLayerCount,unsigned ammoLayerCount,unsigned extra,int endR=0){
 const unsigned layerCount=worldLayerCount+weaponLaserLayerCount+ammoLayerCount+extra;
 matched_capture::FinishSubmission(endR,worldLayerCount,layerCount,weaponLaserLayerCount,ammoLayerCount,915,900);
 return matched_capture::complete;
}
void testCaptureWorldActivity() {
 d2vr_input::WorldActivity world;
 assert(!world.Update(true,1000));
 assert(!world.Update(true,1810,800)); // capture time does not satisfy startup dwell
 assert(world.Update(true,1970));
 assert(world.Update(true,2900,900)); // a verified world frame spent 900ms capturing
 assert(world.Update(true,2920));   // the following frame also stays in gameplay
 assert(!world.Update(false,3500,500)); // genuine loss of world observations expires
 assert(!world.Update(true,3510));
 assert(world.Update(true,3700));
 assert(!world.Update(true,4400)); // a genuine gap still requires renewed dwell
 d2vr_input::WorldActivity sameTick;
 assert(!sameTick.Update(true,1000000));
 assert(sameTick.Update(true,1000190));
 assert(sameTick.Update(true,1000990,800)); // arm and previous submission shared a tick
}
int main(){IDirect3DDevice9 d;
 testCaptureWorldActivity();
 assert(capture(1,2,0,0));
 assert(capture(1,2,2,0));
 assert(!capture(1,2,2,1) && !capture(1,0,0,0,-1));
 assert(!capture(0,0,0,1) && !capture(2,0,0,0));
 VsInfo volume{0x68EEE79C},other{0x293F456B};
 assert(!SkipLightVolumeShift(&volume));
 g_skipLightVsShift=1;
 assert(SkipLightVolumeShift(&volume) && !SkipLightVolumeShift(&other) && !SkipLightVolumeShift(nullptr));
 for(int eye=-1;eye<=1;++eye){g_replayEye=eye;run(d,1);params.mode=3;run(d,0);params.mode=0;params.headGeometry=false;run(d,0);params.headGeometry=true;}
 g_lightingPreset=-1;run(d,1);g_lightingPreset=0;
 g_noInvProj=true;run(d,0);g_noInvProj=false;g_projectionFix=1;run(d,0);g_projectionFix=3;
 for(int p=1;p<=9;++p){g_lightingPreset=p;run(d,0);}g_lightingPreset=0;
 g_activePsInverse=-1;run(d,0);g_activePsInverse=27;
 int writes=g_orig.writes;metadata.shadowClip=false;{ShadowClipScope s(&d);}assert(writes==g_orig.writes);
 metadata.shadowClip=true;g_shadowClipCoordinates=false;{ShadowClipScope s(&d);}assert(writes==g_orig.writes);
 g_shadowClipCoordinates=true;d.fail=true;{ShadowClipScope s(&d);}assert(writes==g_orig.writes);
 d.fail=false;metadata.shadowClip=false;metadata.compositeHeightCoordinates=true;
 const VsInfo composite{0x24E3903D};g_activeVs=&composite;g_activePsInverse=10;
 for(int eye=-1;eye<=1;++eye){g_replayEye=eye;run(d,1);params.headGeometry=false;run(d,0);params.headGeometry=true;}
 g_activeVs=&other;run(d,0);g_activeVs=nullptr;run(d,0);g_activeVs=&composite;
 g_activePsInverse=27;run(d,0);g_activePsInverse=10;
 g_noInvProj=true;run(d,0);g_noInvProj=false;g_lightingPreset=4;run(d,0);
 puts("PASS shadow and fog selectors, both-eye light-volume gate and matched capture with ammo/aim overlays");
}
'@
[IO.File]::WriteAllText((Join-Path $out 'scope.cpp'),$prefix.Replace('ACTUAL_CAPTURE_FINISH',$captureFinish.Value.Trim())+$gate+$src.Substring($start,$end-$start)+$suffix)
$vs=& "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
Push-Location $out
try {
 cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 /I`"$root/src`" scope.cpp /Fe:scope.exe"
 if($LASTEXITCODE){throw 'Compile failed'}
 & ./scope.exe
 if($LASTEXITCODE){throw 'Scope regression failed'}
}finally{Pop-Location}

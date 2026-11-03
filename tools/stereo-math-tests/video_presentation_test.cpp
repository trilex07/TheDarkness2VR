// Exercise the actual production observer with fake COM objects. No GPU needed.
#include <cstdint>
#include <cassert>
#include <cstdio>
using HRESULT=int32_t;using DWORD=uint32_t;
#define SUCCEEDED(x) ((x)>=0)
constexpr HRESULT D3D_OK=0,E_FAIL=-1;
constexpr int D3DBACKBUFFER_TYPE_MONO=0,D3DRS_ZENABLE=7;
struct D3DSURFACE_DESC {unsigned Width=800,Height=600;};
struct D3DVIEWPORT9 {unsigned X=0,Y=0,Width=800,Height=600;};
struct Object {int refs=1;void AddRef(){++refs;}void Release(){assert(refs>1);--refs;}};
struct IDirect3DSurface9:Object {D3DSURFACE_DESC desc;HRESULT GetDesc(D3DSURFACE_DESC* o){*o=desc;return 0;}};
struct IDirect3DVertexShader9:Object {};
struct IDirect3DPixelShader9:Object {};
struct IDirect3DDevice9 {
 IDirect3DSurface9* rt=nullptr;IDirect3DVertexShader9 vs;IDirect3DPixelShader9 ps;
 D3DVIEWPORT9 vp;DWORD depth=0;bool fail=false;int backReads=0;
 HRESULT GetRenderTarget(unsigned,IDirect3DSurface9** out){if(fail)return E_FAIL;*out=rt;if(rt)rt->AddRef();return 0;}
 HRESULT GetBackBuffer(int,int,int,IDirect3DSurface9** out){++backReads;*out=nullptr;return 0;}
 HRESULT GetViewport(D3DVIEWPORT9* out){*out=vp;return 0;}
 HRESULT GetPixelShader(IDirect3DPixelShader9** out){*out=&ps;ps.AddRef();return 0;}
 HRESULT GetVertexShader(IDirect3DVertexShader9** out){*out=&vs;vs.AddRef();return 0;}
 HRESULT GetRenderState(int,DWORD* out){*out=depth;return 0;}
 HRESULT GetFVF(DWORD* out){*out=0;return 0;}
};
static bool g_cinematicTheater=true;
static unsigned g_renderThreadId=5;
static unsigned GetCurrentThreadId(){return 5;}
static int GetPrivateProfileIntA(const char*,const char*,int,const char*){return 0;}
template<class... T> static void Log(const char*,T...){}
struct {bool open=true;uint64_t id=1;} g_trackedFrame;
namespace d2vr_replay {
 static int g_replayEye=-1;
 static unsigned g_activePsHash=0xC1DC2D91u;
 struct Vs {bool videoClip=true;uint32_t hash=0x7ACDFC64;int wvpBase=-1;} info;
 struct {const Vs* Lookup(IDirect3DVertexShader9*){return &info;}} g_vsInfo;
 struct Ps {bool videoYuv=true;} pixel;
 struct {Ps Lookup(IDirect3DPixelShader9*){return pixel;}} g_pixelShaders;
}
#include "../../src/video_presentation.inl"
int main(){
 IDirect3DSurface9 a,b;IDirect3DDevice9 d;d.rt=&a;
 assert(!PresentedVideoThisFrame());
 assert(ObserveVideoDrawResult(&d,E_FAIL)==E_FAIL);assert(a.refs==1);
 ObserveVideoDrawResult(&d,D3D_OK);assert(a.refs==2);
 ObserveVideoDrawResult(&d,D3D_OK);assert(a.refs==2); // no duplicate retain
 ObserveVideoPresentation(&b);assert(a.refs==1);assert(!PresentedVideoThisFrame()); // world TV
 ObserveVideoDrawResult(&d,D3D_OK);ObserveVideoPresentation(&a);assert(PresentedVideoThisFrame());assert(a.refs==1);
 ++g_trackedFrame.id;assert(!PresentedVideoThisFrame());
 d.depth=1;ObserveVideoDrawResult(&d,0);assert(a.refs==1);d.depth=0;
 d.vp.Width=400;ObserveVideoDrawResult(&d,0);assert(a.refs==1);d.vp.Width=800;
 d2vr_replay::info.videoClip=false;ObserveVideoDrawResult(&d,0);assert(a.refs==1);d2vr_replay::info.videoClip=true;
 d2vr_replay::pixel.videoYuv=false;ObserveVideoDrawResult(&d,0);assert(a.refs==1);d2vr_replay::pixel.videoYuv=true;
 d2vr_replay::g_replayEye=1;ObserveVideoDrawResult(&d,0);assert(a.refs==1);d2vr_replay::g_replayEye=-1;
 g_cinematicTheater=false;ObserveVideoDrawResult(&d,0);assert(a.refs==1);g_cinematicTheater=true;
 d.fail=true;ObserveVideoDrawResult(&d,0);assert(a.refs==1);d.fail=false;
 ObserveVideoDrawResult(&d,0);++g_trackedFrame.id;ObserveVideoPresentation(&a);assert(a.refs==1);assert(!PresentedVideoThisFrame());
 IDirect3DSurface9 targets[17];for(auto& target:targets){d.rt=&target;ObserveVideoDrawResult(&d,0);}
 assert(g_videoProbeTargetCount==16);assert(targets[16].refs==1);
 ClearVideoProbeTargets();for(auto& target:targets)assert(target.refs==1);
 assert(d.vs.refs==1 && d.ps.refs==1 && d.backReads==0);
 puts("PASS actual production video observer: successful draw, final target, stale frames, replay, metadata, bounded retention, balanced COM, no diagnostic backbuffer query");
}

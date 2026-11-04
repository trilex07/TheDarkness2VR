#include <initializer_list>
#include <windows.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "../../src/stereo_math.h"
constexpr int D3DRTYPE_TEXTURE=3,D3DFMT_DXT1=827611204,D3DLOCK_READONLY=16;
struct D3DSURFACE_DESC{UINT Width=256,Height=256;int Format=D3DFMT_DXT1,Pool=2;};
struct D3DLOCKED_RECT{int Pitch=512;void* pBits=nullptr;};
struct D3DVIEWPORT9{DWORD X=0,Y=0,Width=100,Height=100;float MinZ=0,MaxZ=.05f;};
struct D3DVERTEXBUFFER_DESC{UINT Size=0;};struct D3DINDEXBUFFER_DESC{UINT Size=0;};
struct IDirect3DVertexBuffer9{void Release(){} HRESULT GetDesc(D3DVERTEXBUFFER_DESC*){return E_FAIL;}};
struct IDirect3DIndexBuffer9{void Release(){} HRESULT GetDesc(D3DINDEXBUFFER_DESC*){return E_FAIL;}};
struct IDirect3DBaseTexture9{
 bool cached=true;uint32_t digest=0x2ecbf95d;unsigned locks=0;bool lockFailure=false;D3DSURFACE_DESC desc;
 int GetType(){return D3DRTYPE_TEXTURE;}UINT GetLevelCount(){return 1;}void Release(){}
 HRESULT GetPrivateData(const GUID&,void* p,DWORD* n){if(!cached)return E_FAIL;std::memcpy(p,&digest,4);*n=4;return S_OK;}
 HRESULT SetPrivateData(const GUID&,const void* p,DWORD,int){cached=true;std::memcpy(&digest,p,4);return S_OK;}
 HRESULT GetLevelDesc(UINT,D3DSURFACE_DESC* p){*p=desc;return S_OK;}
 HRESULT LockRect(UINT,D3DLOCKED_RECT* p,void*,DWORD){++locks;return E_FAIL;}void UnlockRect(UINT){}
};using IDirect3DTexture9=IDirect3DBaseTexture9;
struct IDirect3DDevice9{
 float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,1,0,0,.4f,.5f};int sets=0;bool failSet=false;D3DVIEWPORT9 vp;IDirect3DBaseTexture9 tex;
 HRESULT GetViewport(D3DVIEWPORT9* p){*p=vp;return S_OK;}
 HRESULT GetVertexShaderConstantF(UINT,float* p,UINT){std::memcpy(p,matrix,64);return S_OK;}
 HRESULT GetTexture(UINT,IDirect3DBaseTexture9** p){*p=&tex;return S_OK;}
 HRESULT GetStreamSource(UINT,IDirect3DVertexBuffer9**,UINT*,UINT*){return E_FAIL;}
 HRESULT GetIndices(IDirect3DIndexBuffer9**){return E_FAIL;}
};
static HRESULT Set(IDirect3DDevice9* d,UINT,const float* p,UINT){++d->sets;std::memcpy(d->matrix,p,64);return d->failSet&&d->sets==1?E_FAIL:S_OK;}
static struct{decltype(&Set) SetVertexShaderConstantF=Set;}g_orig;
static bool g_headAttachedTentacles=true,allowed=true;static int g_submitMode=4,g_stereoMode=1,g_replayEye=-1;
static struct VS{bool tentacleGlow=true;uint32_t hash=0x7070c5bc;}vi;static VS* g_activeVs=&vi;
static uint32_t g_activePsHash=0x594c16ee;
struct StereoParams{bool headGeometry=true;float inverseHeadClip[16]={1,0,0,0,0,1,0,0,0,0,1,0,-.2f,.3f,0,1};};
static StereoParams MakeEyeParams(int){return {};}
static bool NativeTentacleHeadAttachmentAllowed(){return allowed;}
static bool IsShadowAtlasPass(IDirect3DDevice9*){return false;}
static void Log(const char*,...){}
namespace d2vr_submission{static void Invalidate(){}}
#include "d2vr-glow-under-test.inl"
static unsigned checks=0;static void Check(bool p){++checks;if(!p){std::puts("FAIL actual glow scope");std::exit(1);}}
int main(){
 for(int eye:{-1,1}){g_replayEye=eye;IDirect3DDevice9 d;float before[16];std::memcpy(before,d.matrix,64);{TentacleHeadScope s(&d);Check(d.sets==1);Check(std::memcmp(before,d.matrix,64)!=0);}Check(d.sets==2&&std::memcmp(before,d.matrix,64)==0);Check(d.tex.locks==0);}
 for(unsigned reject=0;reject<4;++reject){IDirect3DDevice9 d;g_activePsHash=reject==0?0:0x594c16ee;allowed=reject!=1;if(reject==2)d.vp.MaxZ=1;if(reject==3)d.tex.digest=123;{TentacleHeadScope s(&d);}Check(d.sets==0);}allowed=true;g_activePsHash=0x594c16ee;
 IDirect3DDevice9 d;d.failSet=true;float before[16];std::memcpy(before,d.matrix,64);{TentacleHeadScope s(&d);}Check(d.sets==2&&std::memcmp(before,d.matrix,64)==0);
 IDirect3DBaseTexture9 t;t.cached=false;Check(TentacleGlowTextureDigest(&t)==0&&t.locks==1);Check(TentacleGlowTextureDigest(&t)==0&&t.locks==1);
 std::printf("PASS %u actual-source glow scope checks: both eyes, gates, cache, failed setter restoration\n",checks);
}

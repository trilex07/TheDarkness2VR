#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "../src/material_sampling.h"
using Microsoft::WRL::ComPtr;
#define CHECK(x) do{if(!(x)){printf("FAIL line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define HR(x) CHECK(SUCCEEDED(x))
constexpr unsigned W=128;
static bool rejectSetter=false;
static HRESULT STDMETHODCALLTYPE Set(IDirect3DDevice9*d,DWORD sampler,D3DSAMPLERSTATETYPE type,DWORD value){
    return rejectSetter?E_FAIL:d->SetSamplerState(sampler,type,value);
}
int main(){
    CHECK(d2vr_sampling::IsFirstPersonSkin(0x02A9CDE3,0xBC938BC9));
    CHECK(!d2vr_sampling::IsFirstPersonSkin(0x293F456B,0xBC938BC9));
    CHECK(!d2vr_sampling::IsFirstPersonSkin(0x02A9CDE3,0xE345B150));
    char system[MAX_PATH];CHECK(GetSystemDirectoryA(system,MAX_PATH));strcat_s(system,"\\d3d9.dll");
    auto lib=LoadLibraryA(system);CHECK(lib);
    auto create=reinterpret_cast<HRESULT(WINAPI*)(UINT,IDirect3D9Ex**)>(GetProcAddress(lib,"Direct3DCreate9Ex"));CHECK(create);
    ComPtr<IDirect3D9Ex>api;HR(create(D3D_SDK_VERSION,&api));
    HWND win=CreateWindowA("STATIC","Skin mip GPU fixture",WS_OVERLAPPEDWINDOW,0,0,W,W,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);CHECK(win);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=win;
    pp.BackBufferWidth=pp.BackBufferHeight=W;pp.BackBufferFormat=D3DFMT_A8R8G8B8;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    ComPtr<IDirect3DDevice9Ex>d;HR(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,win,D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&pp,nullptr,&d));
    ComPtr<IDirect3DTexture9>normal,staging;HR(d->CreateTexture(1024,1024,0,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&normal,nullptr));
    HR(d->CreateTexture(1024,1024,0,0,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&staging,nullptr));
    CHECK(normal->GetLevelCount()==11);
    for(unsigned mip=0;mip<11;++mip){D3DLOCKED_RECT lock{};HR(staging->LockRect(mip,&lock,nullptr,0));
        unsigned n=1024>>mip;for(unsigned y=0;y<n;++y)for(unsigned x=0;x<n;++x)
            reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+y*lock.Pitch)[x]=(mip&1)?0xffffffff:0xff000000;
        HR(staging->UnlockRect(mip));}
    HR(d->UpdateTexture(staging.Get(),normal.Get()));HR(d->SetTexture(2,normal.Get()));
    D3DVIEWPORT9 viewport{0,0,W,W,0,.05f};HR(d->SetViewport(&viewport));
    HR(d->SetSamplerState(2,D3DSAMP_MINFILTER,D3DTEXF_LINEAR));HR(d->SetSamplerState(2,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR));
    HR(d->SetSamplerState(2,D3DSAMP_MIPFILTER,D3DTEXF_POINT));HR(d->SetSamplerState(2,D3DSAMP_MIPMAPLODBIAS,0));
    ComPtr<ID3DBlob>blob,errors;const char* source="sampler2D n:register(s2);float4 main(float2 uv:TEXCOORD0):COLOR0{return tex2D(n,uv);}";
    HR(D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,"main","ps_3_0",0,0,&blob,&errors));
    ComPtr<IDirect3DPixelShader9>ps;HR(d->CreatePixelShader(static_cast<const DWORD*>(blob->GetBufferPointer()),&ps));
    HR(d->SetPixelShader(ps.Get()));HR(d->SetVertexShader(nullptr));HR(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1));
    HR(d->SetRenderState(D3DRS_ZENABLE,FALSE));HR(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));HR(d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE));
    ComPtr<IDirect3DSurface9>rt,read;HR(d->CreateRenderTarget(W,W,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&rt,nullptr));
    HR(d->CreateOffscreenPlainSurface(W,W,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&read,nullptr));HR(d->SetRenderTarget(0,rt.Get()));HR(d->SetViewport(&viewport));
    auto sample=[&](double lod,bool linear){
        const float scale=float(std::pow(2.,lod)*W/1024.);
        struct V{float x,y,z,w,u,v;};V v[]={{-.5f,-.5f,.5f,1,0,0},{W-.5f,-.5f,.5f,1,scale,0},{-.5f,W-.5f,.5f,1,0,scale},{W-.5f,W-.5f,.5f,1,scale,scale}};
        {d2vr_sampling::ScopedSkinMipFilter scope;if(linear)CHECK(scope.Apply(d.Get(),Set));
            HR(d->BeginScene());HR(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(V)));HR(d->EndScene());}
        DWORD restored=0;HR(d->GetSamplerState(2,D3DSAMP_MIPFILTER,&restored));CHECK(restored==D3DTEXF_POINT);
        HR(d->GetRenderTargetData(rt.Get(),read.Get()));D3DLOCKED_RECT lock{};HR(read->LockRect(&lock,nullptr,D3DLOCK_READONLY));
        unsigned red=(reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+(W/2)*lock.Pitch)[W/2]>>16)&255;
        HR(read->UnlockRect());return int(red);
    };
    const int a=sample(1.49,false),b=sample(1.51,false),c=sample(1.49,true),e=sample(1.51,true);
    CHECK(abs(a-b)>240);CHECK(abs(c-e)<16);CHECK(c>100&&c<155&&e>100&&e<155);
    // Actual state admission and rollback, including failed writes, full-world
    // viewport, missing/non-mipped texture and already-linear state.
    {d2vr_sampling::ScopedSkinMipFilter scope;rejectSetter=true;CHECK(!scope.Apply(d.Get(),Set));rejectSetter=false;}
    viewport.MaxZ=1;HR(d->SetViewport(&viewport));{d2vr_sampling::ScopedSkinMipFilter scope;CHECK(!scope.Apply(d.Get(),Set));}
    viewport.MaxZ=.05f;HR(d->SetViewport(&viewport));HR(d->SetTexture(2,nullptr));
    {d2vr_sampling::ScopedSkinMipFilter scope;CHECK(!scope.Apply(d.Get(),Set));}
    ComPtr<IDirect3DTexture9>single;HR(d->CreateTexture(1024,1024,1,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&single,nullptr));HR(d->SetTexture(2,single.Get()));
    {d2vr_sampling::ScopedSkinMipFilter scope;CHECK(!scope.Apply(d.Get(),Set));}
    HR(d->SetTexture(2,normal.Get()));HR(d->SetSamplerState(2,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR));
    {d2vr_sampling::ScopedSkinMipFilter scope;CHECK(!scope.Apply(d.Get(),Set));}
    printf("PASS actual D3D9 mip transition native %d->%d; scoped linear %d->%d; state restoration, viewport/material/texture and failed-set guards\n",a,b,c,e);
    DestroyWindow(win);return 0;
}

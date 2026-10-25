#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../../src/material_sampling.h"
#include <cstdio>
#include <cstdlib>
#include <type_traits>

static unsigned checks;
static void Check(bool ok, const char* text) {
    ++checks;
    if (!ok) { std::fprintf(stderr,"FAIL: %s\n",text); std::exit(1); }
}
static HRESULT STDMETHODCALLTYPE Set(IDirect3DDevice9* d,DWORD s,D3DSAMPLERSTATETYPE t,DWORD v) {
    return d->SetSamplerState(s,t,v);
}
static HRESULT STDMETHODCALLTYPE FailMin(IDirect3DDevice9* d,DWORD s,D3DSAMPLERSTATETYPE t,DWORD v) {
    return t==D3DSAMP_MINFILTER ? E_FAIL : d->SetSamplerState(s,t,v);
}
static HRESULT STDMETHODCALLTYPE FailMax(IDirect3DDevice9*,DWORD,D3DSAMPLERSTATETYPE,DWORD) {return E_FAIL;}
static DWORD Value(IDirect3DDevice9* d,unsigned sampler,D3DSAMPLERSTATETYPE type) {
    DWORD v=0;Check(SUCCEEDED(d->GetSamplerState(sampler,type,&v)),"driver sampler read");return v;
}
static void Snapshot(IDirect3DDevice9* d,DWORD out[2][13]) {
    for(unsigned s=0;s<2;++s)for(unsigned t=1;t<=13;++t)out[s][t-1]=Value(d,s,(D3DSAMPLERSTATETYPE)t);
}
static void Same(IDirect3DDevice9* d,const DWORD before[2][13]) {
    DWORD after[2][13];Snapshot(d,after);
    for(unsigned s=0;s<2;++s)for(unsigned t=0;t<13;++t)
        Check(before[s][t]==after[s][t],"all sampler state restored; neighbouring sampler unchanged");
}
int main() {
    using namespace d2vr_sampling;
    static_assert(!std::is_copy_constructible<ScopedDiffuseFilter>::value,"scope cannot restore twice");
    Check(IsWorldDiffuseMaterial(0x293F456B,0xCA08BDC4),"captured material eligible");
    Check(!IsWorldDiffuseMaterial(0x293F456B,0x33DE98C1),"composite excluded");
    Check(!IsWorldDiffuseMaterial(0x746D51CD,0xCA08BDC4),"normal/depth pass excluded");
    Check(!IsWorldDiffuseMaterial(0x34D4EEE8,0xCA08BDC4),"tentacles excluded");
    Check(!IsWorldDiffuseMaterial(0x7070C5BC,0x5104FEF7),"UI/FXAA excluded");
    HWND window=CreateWindowExA(0,"STATIC","D2VR sampling regression",WS_POPUP,0,0,32,32,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
    Check(window!=nullptr,"hidden test window");
    IDirect3D9* d3d=Direct3DCreate9(D3D_SDK_VERSION);Check(d3d!=nullptr,"real D3D9 runtime");
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.hDeviceWindow=window;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth=pp.BackBufferHeight=32;pp.BackBufferFormat=D3DFMT_UNKNOWN;
    IDirect3DDevice9* dev=nullptr;
    Check(SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&dev)),"real HAL device");
    D3DCAPS9 caps{};Check(SUCCEEDED(dev->GetDeviceCaps(&caps)),"driver capabilities");
    Check(caps.MaxAnisotropy>1 && (caps.TextureFilterCaps&D3DPTFILTERCAPS_MINFANISOTROPIC),"HAL supports anisotropy");
    IDirect3DTexture9 *mips=nullptr,*single=nullptr,*target=nullptr;
    Check(SUCCEEDED(dev->CreateTexture(128,128,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&mips,nullptr)),"mipmapped material texture");
    Check(SUCCEEDED(dev->CreateTexture(128,128,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&single,nullptr)),"single-level texture");
    Check(SUCCEEDED(dev->CreateTexture(128,128,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&target,nullptr)),"postprocess RT texture");
    dev->SetTexture(0,mips);dev->SetTexture(1,mips);
    dev->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);dev->SetSamplerState(0,D3DSAMP_MAXANISOTROPY,2);
    dev->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);dev->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,1);
    DWORD before[2][13];Snapshot(dev,before);
    {
        ScopedDiffuseFilter filter;Check(filter.Apply(dev,Set,16),"linear diffuse promoted on actual driver");
        Check(Value(dev,0,D3DSAMP_MINFILTER)==D3DTEXF_ANISOTROPIC,"actual MINFILTER anisotropic");
        Check(Value(dev,0,D3DSAMP_MAXANISOTROPY)==std::min<DWORD>(caps.MaxAnisotropy,16),"hardware quality clamp");
        Check(Value(dev,0,D3DSAMP_MIPFILTER)==before[0][D3DSAMP_MIPFILTER-1],"mip policy preserved");
        Check(Value(dev,0,D3DSAMP_MIPMAPLODBIAS)==before[0][D3DSAMP_MIPMAPLODBIAS-1],"LOD bias preserved");
        Check(!filter.Apply(dev,Set,16),"active scope cannot overwrite restoration state");
    }
    Same(dev,before);
    {ScopedDiffuseFilter filter;Check(!filter.Apply(dev,FailMin,16),"failed MINFILTER rejected");}Same(dev,before);
    {ScopedDiffuseFilter filter;Check(!filter.Apply(dev,FailMax,16),"failed quality change rejected");}Same(dev,before);
    {ScopedDiffuseFilter filter;Check(!filter.Apply(dev,Set,1),"quality one remains unchanged");}Same(dev,before);
    for(auto texture:{single,target}) {
        dev->SetTexture(0,texture);
        {ScopedDiffuseFilter filter;Check(!filter.Apply(dev,Set,16),"nonmaterial/single-mip input rejected");}Same(dev,before);
    }
    dev->SetTexture(0,mips);dev->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);
    {ScopedDiffuseFilter filter;Check(!filter.Apply(dev,Set,16),"intentional point sampler preserved");}
    Check(Value(dev,0,D3DSAMP_MINFILTER)==D3DTEXF_POINT,"point state unchanged");
    dev->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
    IDirect3DStateBlock9* block=nullptr;Check(SUCCEEDED(dev->CreateStateBlock(D3DSBT_ALL,&block)),"stateblock capture");
    {ScopedDiffuseFilter filter;Check(filter.Apply(dev,Set,8),"second eye / repeated draw scope");}
    Check(SUCCEEDED(block->Apply()),"stateblock restores original game state");Same(dev,before);
    block->Release();dev->SetTexture(0,nullptr);dev->SetTexture(1,nullptr);
    target->Release();single->Release();mips->Release();dev->Release();d3d->Release();DestroyWindow(window);
    std::printf("PASS: %u checks, real D3D9 HAL sampler/rollback/stateblock; no Steam or XR\n",checks);
}

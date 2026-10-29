#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
static unsigned checks=0,calls=0,locks=0,records=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL line%d %s\n",__LINE__,#x);exit(1);}}while(0)
static bool g_recActive=true,g_recOverflow=false;
static HRESULT status=S_OK;
struct RecLockGuard{RecLockGuard(){++locks;}};
static std::vector<uintptr_t> expected;
template<class T> static uintptr_t Bits(T x){return uintptr_t(x);}
static uintptr_t Bits(float x){uint32_t bits;memcpy(&bits,&x,4);return bits;}
template<class... A> static void Expect(A... args){expected={Bits(args)...};calls=locks=records=0;}
struct Forward {
    template<class... A> HRESULT operator()(A... args)const {
        ++calls;CHECK(std::vector<uintptr_t>({Bits(args)...})==expected);return status;
    }
};
struct {
    Forward SetRenderTarget,Clear,SetTransform,SetViewport,SetClipPlane,SetRenderState,
        SetTextureStageState,SetSamplerState,SetScissorRect,SetVertexDeclaration,SetFVF,
        SetVertexShaderConstantI,SetVertexShaderConstantB,SetStreamSource,SetStreamSourceFreq,
        SetIndices,SetPixelShaderConstantI,SetPixelShaderConstantB,StretchRect,ColorFill;
} g_orig;
@ENUM@
template<class... A> static void RecEmit(A...){if(g_recActive&&!g_recOverflow)++records;}
template<class... A> static void RecEmitData(A...){if(g_recActive&&!g_recOverflow)++records;}
// These calls use an explicit empty ownership proof, which a variadic stub
// cannot deduce. Match the actual shared data-emission signature instead.
struct Proof{};
static void RecEmitData(unsigned,const void*,size_t,uint32_t=0,uint32_t=0,uint32_t=0,uint32_t=0,
    const Proof& = {},unsigned=4,uint32_t=UINT32_MAX){if(g_recActive&&!g_recOverflow)++records;}
static uint32_t RecPush(const void*,size_t){return 0;}
static void InvalidateShadowAtlasPass(){}
static void CollectLightReadbackTarget(IDirect3DSurface9*){}
static void RecordPreSceneWrite(IDirect3DDevice9*){}
static DWORD ResolveSamplerValue(IDirect3DDevice9*,D3DSAMPLERSTATETYPE,DWORD value){return value;}
static std::vector<unsigned> g_rec;
namespace d2vr_trace {
static bool g_enabled=false;
template<class... A> static void Emit(A...){}
}
@IMPLEMENTATION@
#define RUN(name,...) do{Expect(__VA_ARGS__);CHECK(Hook_##name(__VA_ARGS__)==status);CHECK(calls==1);CHECK(g_recOverflow==(oldOverflow||(active&&FAILED(status))));CHECK(locks==unsigned(active&&FAILED(status)));CHECK(records==unsigned(active&&!oldOverflow));g_recOverflow=oldOverflow;}while(0)
int main(){
    auto* dev=reinterpret_cast<IDirect3DDevice9*>(0x1000);
    auto* surface=reinterpret_cast<IDirect3DSurface9*>(0x2000);
    auto* vertex=reinterpret_cast<IDirect3DVertexBuffer9*>(0x3000);
    auto* indices=reinterpret_cast<IDirect3DIndexBuffer9*>(0x4000);
    auto* declaration=reinterpret_cast<IDirect3DVertexDeclaration9*>(0x5000);
    RECT rect{2,3,8,9};D3DRECT clearRect{1,2,7,8};D3DMATRIX matrix{};
    D3DVIEWPORT9 viewport{1,2,64,48,0.25f,0.75f};float plane[4]{1,2,3,4};int values[4]{1,2,3,4};
    for(bool active:{false,true})for(bool oldOverflow:{false,true})
    for(HRESULT result:{S_OK,S_FALSE,D3DERR_INVALIDCALL,D3DERR_DEVICELOST,E_OUTOFMEMORY}){
        g_recActive=active;g_recOverflow=oldOverflow;status=result;
        RUN(SetRenderTarget,dev,0u,surface);
        RUN(Clear,dev,1u,&clearRect,DWORD(D3DCLEAR_TARGET),D3DCOLOR(0x12345678),0.375f,7u);
        RUN(SetTransform,dev,D3DTS_WORLD,&matrix);
        RUN(SetViewport,dev,&viewport);
        RUN(SetClipPlane,dev,2u,plane);
        RUN(SetRenderState,dev,D3DRS_ZWRITEENABLE,DWORD(TRUE));
        RUN(SetTextureStageState,dev,1u,D3DTSS_COLOROP,DWORD(D3DTOP_MODULATE));
        RUN(SetSamplerState,dev,2u,D3DSAMP_ADDRESSU,DWORD(D3DTADDRESS_CLAMP));
        RUN(SetScissorRect,dev,&rect);
        RUN(SetVertexDeclaration,dev,declaration);
        RUN(SetFVF,dev,DWORD(D3DFVF_XYZ));
        RUN(SetVSConstI,dev,1u,values,1u);
        RUN(SetVSConstB,dev,1u,values,1u);
        RUN(SetStreamSource,dev,0u,vertex,32u,40u);
        RUN(SetStreamSourceFreq,dev,0u,3u);
        RUN(SetIndices,dev,indices);
        RUN(SetPSConstI,dev,1u,values,1u);
        RUN(SetPSConstB,dev,1u,values,1u);
        RUN(StretchRect,dev,surface,&rect,surface,&rect,D3DTEXF_NONE);
        RUN(ColorFill,dev,surface,&rect,D3DCOLOR(0x87654321));
    }
    // A successful later call cannot certify a packet containing a failed call.
    g_recActive=true;g_recOverflow=false;status=D3DERR_DEVICELOST;
    Expect(dev,0u,surface);CHECK(Hook_SetRenderTarget(dev,0,surface)==status);
    CHECK(g_recOverflow&&records==1);
    status=S_OK;Expect(dev,surface,&rect,D3DCOLOR(7));
    CHECK(Hook_ColorFill(dev,surface,&rect,7)==S_OK&&g_recOverflow&&records==0);
    printf("PASS actual live hooks: %u checks, 20 APIs x 20 recording/status cases; exact arguments/HRESULT/one forward, sticky incomplete packet, success path without locking; CPU fixture, not game evidence\n",checks);
}

static bool g_replayBufferVersions=true;
#include "recorder_sync.h"
#include "stateblock_recording.h"
#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "up_geometry_capture.h"
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
        SetIndices,SetPixelShaderConstantI,SetPixelShaderConstantB,StretchRect,ColorFill,
        DrawPrimitive,DrawIndexedPrimitive,DrawPrimitiveUP,DrawIndexedPrimitiveUP;
} g_orig;
@ENUM@
template<class... A> static void RecEmit(A...){if(g_recActive&&!g_recOverflow)++records;}
template<class... A> static void RecEmitLocked(A...){if(g_recActive&&!g_recOverflow)++records;}
template<class... A> static void RecEmitData(A...){if(g_recActive&&!g_recOverflow)++records;}
// These calls use an explicit empty ownership proof, which a variadic stub
// cannot deduce. Match the actual shared data-emission signature instead.
struct Proof{};
static void RecEmitData(unsigned,const void*,size_t,uint32_t=0,uint32_t=0,uint32_t=0,uint32_t=0,
    const Proof& = {},unsigned=4,uint32_t=UINT32_MAX){if(g_recActive&&!g_recOverflow)++records;}
static uint32_t RecPush(const void*,size_t){return 0;}
static uint32_t RecPushLocked(const void*,size_t){return 0;}
static std::vector<uint8_t> g_recData;
static LONG g_lastHookId=0;
#define HOOK_MARK(n) (g_lastHookId=(n))
static bool skipDraw=false;
static bool SkipObjectDrawForTest(D3DPRIMITIVETYPE,UINT){return skipDraw;}
static bool SkipShadowDrawForTest(IDirect3DDevice9*){return false;}
static bool RecordUiPrimitive(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT){return false;}
struct WorldDiffuseScope {explicit WorldDiffuseScope(IDirect3DDevice9*){}};
static int g_replayEye=-1;
static const void* g_boundVs=nullptr;
static const void* g_boundPs=nullptr;
namespace d2vr_pipeline_warmup {static void ObserveDraw(IDirect3DDevice9*,const void*,const void*){}}
struct TentacleHeadScope {explicit TentacleHeadScope(IDirect3DDevice9*){}};
static unsigned clipBegins=0,clipEnds=0;
struct UiClipState{};
static UiClipState BeginScaledUiClip(IDirect3DDevice9*){++clipBegins;return {};}
static void EndScaledUiClip(IDirect3DDevice9*,const UiClipState&){++clipEnds;}
static void InvalidateShadowAtlasPass(){}
static void CollectLightReadbackTarget(IDirect3DSurface9*){}
static void RecordPreSceneWrite(IDirect3DDevice9*){}
static DWORD ResolveSamplerValue(IDirect3DDevice9*,D3DSAMPLERSTATETYPE,DWORD value){return value;}
static std::vector<unsigned> g_rec;
namespace d2vr_trace {
static bool g_enabled=false;
static void NoteDraw(){}
template<class... A> static void Emit(A...){}
}
@IMPLEMENTATION@
#define RUN(name,...) do{Expect(__VA_ARGS__);CHECK(Hook_##name(__VA_ARGS__)==status);CHECK(calls==1);CHECK(g_recOverflow==(oldOverflow||(active&&FAILED(status))));CHECK(locks==((!std::strcmp(#name,"DrawPrimitive")||!std::strcmp(#name,"DrawIndexedPrimitive"))&&g_replayBufferVersions?1u:unsigned(active&&FAILED(status))));CHECK(records==unsigned(active&&!oldOverflow));g_recOverflow=oldOverflow;}while(0)
int main(){
    auto* dev=reinterpret_cast<IDirect3DDevice9*>(0x1000);
    auto* surface=reinterpret_cast<IDirect3DSurface9*>(0x2000);
    auto* vertex=reinterpret_cast<IDirect3DVertexBuffer9*>(0x3000);
    auto* indices=reinterpret_cast<IDirect3DIndexBuffer9*>(0x4000);
    auto* declaration=reinterpret_cast<IDirect3DVertexDeclaration9*>(0x5000);
    RECT rect{2,3,8,9};D3DRECT clearRect{1,2,7,8};D3DMATRIX matrix{};
    D3DVIEWPORT9 viewport{1,2,64,48,0.25f,0.75f};float plane[4]{1,2,3,4};int values[4]{1,2,3,4};
    for(bool versions:{false,true})for(bool active:{false,true})for(bool oldOverflow:{false,true})
    for(HRESULT result:{S_OK,S_FALSE,D3DERR_INVALIDCALL,D3DERR_DEVICELOST,E_OUTOFMEMORY}){
        g_replayBufferVersions=versions;g_recActive=active;g_recOverflow=oldOverflow;status=result;
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
        RUN(DrawPrimitive,dev,D3DPT_TRIANGLELIST,3u,2u);
        RUN(DrawIndexedPrimitive,dev,D3DPT_TRIANGLELIST,-2,3u,6u,4u,2u);
        float vertices[18]{};uint16_t elements[6]{0,1,2,3,4,5};
        // UP forwarding owns one CPU scope; failure reporting may reenter it.
        Expect(dev,D3DPT_TRIANGLELIST,2u,vertices,12u);
        CHECK(Hook_DrawPrimitiveUP(dev,D3DPT_TRIANGLELIST,2,vertices,12)==status);
        CHECK(calls==1 && locks==1u+unsigned(active&&FAILED(status)));
        CHECK(g_recOverflow==(oldOverflow||(active&&FAILED(status))));
        CHECK(records==unsigned(active&&!oldOverflow));g_recOverflow=oldOverflow;
        Expect(dev,D3DPT_TRIANGLELIST,0u,6u,2u,elements,D3DFMT_INDEX16,vertices,12u);
        CHECK(Hook_DrawIndexedPrimitiveUP(dev,D3DPT_TRIANGLELIST,0,6,2,elements,D3DFMT_INDEX16,vertices,12)==status);
        CHECK(calls==1 && locks==1u+unsigned(active&&FAILED(status)));
        CHECK(g_recOverflow==(oldOverflow||(active&&FAILED(status))));
        CHECK(records==unsigned(active&&!oldOverflow));g_recOverflow=oldOverflow;
    }
    CHECK(clipBegins==160 && clipEnds==clipBegins);
    // Setters inside Begin/EndStateBlock build an object, not the live device.
    // Preserve raw arguments/HRESULT without records or sticky draw failure.
#define BUILD(name,...) do{Expect(__VA_ARGS__);CHECK(Hook_##name(__VA_ARGS__)==status);CHECK(calls==1 && records==0 && locks==0 && !g_recOverflow);}while(0)
    for(HRESULT result:{S_OK,S_FALSE,D3DERR_INVALIDCALL,D3DERR_DEVICELOST}) {
        g_recActive=true;g_recOverflow=false;status=result;
        d2vr_stateblock::Began(dev,S_OK);
        BUILD(SetTransform,dev,D3DTS_WORLD,&matrix);
        BUILD(SetViewport,dev,&viewport);
        BUILD(SetClipPlane,dev,2u,plane);
        BUILD(SetRenderState,dev,D3DRS_ZWRITEENABLE,DWORD(TRUE));
        BUILD(SetTextureStageState,dev,1u,D3DTSS_COLOROP,DWORD(D3DTOP_MODULATE));
        BUILD(SetSamplerState,dev,2u,D3DSAMP_ADDRESSU,DWORD(D3DTADDRESS_CLAMP));
        BUILD(SetScissorRect,dev,&rect);
        BUILD(SetVertexDeclaration,dev,declaration);
        BUILD(SetFVF,dev,DWORD(D3DFVF_XYZ));
        BUILD(SetVSConstI,dev,1u,values,1u);
        BUILD(SetVSConstB,dev,1u,values,1u);
        BUILD(SetStreamSource,dev,0u,vertex,32u,40u);
        BUILD(SetStreamSourceFreq,dev,0u,3u);
        BUILD(SetIndices,dev,indices);
        BUILD(SetPSConstI,dev,1u,values,1u);
        BUILD(SetPSConstB,dev,1u,values,1u);
        d2vr_stateblock::Ended(dev,S_OK);
    }
#undef BUILD
    // Intentional draw suppression still returns success without calling the
    // driver or making a previously complete recording invalid.
    skipDraw=true;g_recActive=true;g_recOverflow=false;status=D3DERR_DEVICELOST;
    Expect(dev,D3DPT_TRIANGLELIST,0u,2u);
    CHECK(Hook_DrawPrimitive(dev,D3DPT_TRIANGLELIST,0,2)==D3D_OK);
    CHECK(calls==0 && !g_recOverflow);skipDraw=false;
    // A successful later call cannot certify a packet containing a failed call.
    g_recActive=true;g_recOverflow=false;status=D3DERR_DEVICELOST;
    Expect(dev,0u,surface);CHECK(Hook_SetRenderTarget(dev,0,surface)==status);
    CHECK(g_recOverflow&&records==1);
    status=S_OK;Expect(dev,surface,&rect,D3DCOLOR(7));
    CHECK(Hook_ColorFill(dev,surface,&rect,7)==S_OK&&g_recOverflow&&records==0);
    printf("PASS actual live hooks: %u checks, 24 APIs x 40 versioning/recording/status cases; four draw paths including UP, clip cleanup, intentional suppression, exact arguments/HRESULT/one forward, sticky incomplete packet; CPU fixture, not game evidence\n",checks);
}

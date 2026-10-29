#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
@ENUM@
struct Rec {unsigned op=0,n=0,a[6]{};};
static unsigned calls=0,failAt=0;
static HRESULT success=S_OK,failure=D3DERR_INVALIDCALL;
static std::vector<unsigned> order;
static HRESULT Receipt(unsigned op){order.push_back(op);return ++calls==failAt?failure:success;}
struct Original {
#define METHOD(name) template<class... Args>HRESULT name(IDirect3DDevice9*,Args...){return Receipt(OP_##name);}
    METHOD(BeginScene) METHOD(EndScene) METHOD(SetTransform) METHOD(SetViewport)
    METHOD(SetClipPlane) METHOD(SetRenderState) METHOD(SetTexture)
    METHOD(SetTextureStageState) METHOD(SetSamplerState) METHOD(SetScissorRect)
    METHOD(SetVertexDeclaration) METHOD(SetFVF) METHOD(SetVertexShader)
    METHOD(SetVSConstI) METHOD(SetVSConstB) METHOD(SetStreamSource)
    METHOD(SetStreamSourceFreq) METHOD(SetIndices) METHOD(SetPixelShader)
    METHOD(SetPSConstI) METHOD(SetPSConstB)
#undef METHOD
} g_orig;
// The production forwarding names differ from their recorder opcode names.
#define SetVertexShaderConstantI SetVSConstI
#define SetVertexShaderConstantB SetVSConstB
#define SetPixelShaderConstantI SetPSConstI
#define SetPixelShaderConstantB SetPSConstB
static void InvalidateShadowAtlasPass(){}
static const void* g_replayVs=nullptr;
static const void* g_replayPs=nullptr;
static void SetActiveVertexShaderMetadata(IDirect3DVertexShader9*){}
static void SetActivePixelShaderMetadata(const void*){}
static bool Actual(const std::vector<Rec>& records){
    alignas(16) const uint8_t data[512]{};const uint8_t* dataBase=data;
    const void* replayTex[16]{};IDirect3DDevice9* dev=nullptr;
    bool replayComplete=true;
    for(const auto& r:records)switch((RecOp)r.op){
@CASES@
    default:CHECK(false);
    }
    return replayComplete;
}
int main(){
    const unsigned ops[]={OP_BeginScene,OP_SetTransform,OP_SetViewport,OP_SetClipPlane,
        OP_SetRenderState,OP_SetTexture,OP_SetTextureStageState,OP_SetSamplerState,
        OP_SetScissorRect,OP_SetVertexDeclaration,OP_SetFVF,OP_SetVertexShader,
        OP_SetVSConstI,OP_SetVSConstB,OP_SetStreamSource,OP_SetStreamSourceFreq,
        OP_SetIndices,OP_SetPixelShader,OP_SetPSConstI,OP_SetPSConstB,OP_EndScene};
    std::vector<Rec> records;std::vector<unsigned> expected;
    for(unsigned op:ops){Rec r;r.op=op;r.n=1;r.a[0]=0;r.a[1]=1;r.a[4]=64;records.push_back(r);expected.push_back(op);}
    // A later successful command must never erase an earlier failure.
    records.push_back(records[4]);expected.push_back(OP_SetRenderState);
    unsigned streams=0,attempts=0;
    for(unsigned positive=0;positive<2;++positive)for(unsigned negative=0;negative<2;++negative)
    for(failAt=0;failAt<=records.size();++failAt){
        success=positive?S_FALSE:D3D_OK;failure=negative?D3DERR_DEVICELOST:D3DERR_INVALIDCALL;
        calls=0;order.clear();CHECK(Actual(records)==(failAt==0));
        CHECK(order==expected && calls==records.size());++streams;attempts+=calls;
    }
    printf("PASS actual replay state receipts: %u checks / %u streams / %u calls; 21 API kinds, every failure position sticky, D3D_OK/S_FALSE, INVALIDCALL/DEVICELOST, exact call order, cleanup continued, no retry\n",checks,streams,attempts);
}

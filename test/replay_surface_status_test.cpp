#include <d3d9.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#define CHECK(x) do {if(!(x)){std::printf("FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
@ENUM@
struct Rec {unsigned op,n,a[6];};
static HRESULT result=0;
static unsigned failAt=0,attempts=0,invalidations=0;
static std::vector<unsigned> calls;
static HRESULT Receipt(unsigned op) {
    calls.push_back(op);++attempts;
    return attempts==failAt ? D3DERR_INVALIDCALL : result;
}
static void InvalidateShadowAtlasPass(){++invalidations;}
static IDirect3DSurface9* const source=(IDirect3DSurface9*)0x1000;
static IDirect3DSurface9* const dest=(IDirect3DSurface9*)0x2000;
static const unsigned char* arena=nullptr;
static unsigned mask=0;
static bool nullBinding=false;
static unsigned bankClears=0,shaderSyncs=0;
static struct {struct {void Clear(){++bankClears;}} replay;} g_vsConstantHistory;
static void SyncActiveShader(IDirect3DDevice9*){++shaderSyncs;}
static HRESULT STDMETHODCALLTYPE ApplyBlock(IDirect3DStateBlock9* block){CHECK(block==(IDirect3DStateBlock9*)0x3000);return Receipt(OP_ApplyStateBlock);}
static auto g_origSBApply=&ApplyBlock;
#define HOOK_MARK(x) ((void)0)
struct Original {
    HRESULT SetRenderTarget(IDirect3DDevice9*,DWORD slot,IDirect3DSurface9* surface) {
        CHECK(slot==0 && surface==(nullBinding?nullptr:dest));return Receipt(OP_SetRenderTarget);
    }
    HRESULT SetDepthStencilSurface(IDirect3DDevice9*,IDirect3DSurface9* surface) {
        CHECK(surface==(nullBinding?nullptr:source));return Receipt(OP_SetDepthStencilSurface);
    }
    HRESULT Clear(IDirect3DDevice9*,DWORD count,const D3DRECT* rect,DWORD flags,D3DCOLOR color,float z,DWORD stencil) {
        CHECK(count==(mask&1) && rect==((mask&1)?(const D3DRECT*)(arena+16):nullptr));
        CHECK(flags==D3DCLEAR_TARGET && color==0x12345678 && z==0.375f && stencil==7);
        return Receipt(OP_Clear);
    }
    HRESULT StretchRect(IDirect3DDevice9*,IDirect3DSurface9* src,const RECT* sr,IDirect3DSurface9* dst,const RECT* dr,D3DTEXTUREFILTERTYPE filter) {
        CHECK(src==source && dst==dest && filter==D3DTEXF_NONE);
        CHECK(sr==((mask&1)?(const RECT*)(arena+16):nullptr));
        CHECK(dr==((mask&2)?(const RECT*)(arena+32):nullptr));return Receipt(OP_StretchRect);
    }
    HRESULT ColorFill(IDirect3DDevice9*,IDirect3DSurface9* dst,const RECT* rect,D3DCOLOR color) {
        CHECK(dst==dest && color==0x87654321);
        CHECK(rect==((mask&1)?(const RECT*)(arena+32):nullptr));return Receipt(OP_ColorFill);
    }
    HRESULT EndScene(IDirect3DDevice9*) {return Receipt(OP_EndScene);}
} g_orig;
static bool Actual(const std::vector<Rec>& records,const unsigned char* dataBase) {
    bool replayComplete=true;
    IDirect3DDevice9* dev=nullptr;
    for(const auto& r:records) switch((RecOp)r.op) {
@CASES@
    default: CHECK(false);
    }
    return replayComplete;
}
int main() {
    unsigned char bytes[64]{};arena=bytes;
    const std::vector<unsigned> expected={OP_SetRenderTarget,OP_SetDepthStencilSurface,OP_Clear,OP_StretchRect,OP_ColorFill,OP_ApplyStateBlock,OP_EndScene};
    for(mask=0;mask<4;++mask)for(unsigned nullCase=0;nullCase<2;++nullCase)for(unsigned status=0;status<2;++status)for(failAt=0;failAt<=7;++failAt) {
        nullBinding=nullCase!=0;result=status?S_FALSE:D3D_OK;
        uint32_t z;const float depth=0.375f;std::memcpy(&z,&depth,4);
        const std::vector<Rec> records={
            {OP_SetRenderTarget,0,{0,nullBinding?0u:0x2000u}},
            {OP_SetDepthStencilSurface,0,{nullBinding?0u:0x1000u}},
            {OP_Clear,mask&1,{D3DCLEAR_TARGET,0x12345678,z,7,16}},
            {OP_StretchRect,mask,{0x1000,0x2000,D3DTEXF_NONE,16,32}},
            {OP_ColorFill,mask&1,{0x2000,0x87654321,32}},
            {OP_ApplyStateBlock,0,{0x3000}},
            {OP_EndScene,0,{}}};
        attempts=invalidations=bankClears=shaderSyncs=0;calls.clear();
        CHECK(Actual(records,bytes)==(failAt==0));
        CHECK(calls==expected && attempts==7 && invalidations==2 && bankClears==1 && shaderSyncs==1);
    }
    failAt=attempts=0;calls.clear();
    CHECK(!Actual({{OP_ApplyStateBlock,0,{0}}},bytes) && calls.empty());
    g_origSBApply=nullptr;
    CHECK(!Actual({{OP_ApplyStateBlock,0,{0x3000}}},bytes) && calls.empty());
    std::puts("PASS actual-source replay surface/state-block status:128 streams,896 calls; each of seven failures sticky including EndScene; no retries; null/unavailable block, null bindings, rectangle flags, exact arguments, D3D_OK/S_FALSE");
}

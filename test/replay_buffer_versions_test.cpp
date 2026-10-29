// The device double models execution order, not GPU timing. Production recorder,
// binding hooks, draw hooks and replay cases are extracted by the build script.
#include <d3d9.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "frame_resource_refs.h"
#define CHECK(x) do {if(!(x)){std::printf("FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
@ENUM@
@RECORD@
struct Buffer final : IUnknown {
    ULONG refs=1;
    uint32_t words[32]{};
    bool locked=false;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override{return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{CHECK(refs);return --refs;}
    HRESULT Lock(UINT offset,UINT bytes,void** out,DWORD flags) {
        if(locked || offset>sizeof(words) || bytes>sizeof(words)-offset)return D3DERR_INVALIDCALL;
        if(flags&D3DLOCK_DISCARD)std::memset(words,0xcd,sizeof(words));
        locked=true;*out=reinterpret_cast<unsigned char*>(words)+offset;return D3D_OK;
    }
    HRESULT Unlock(){if(!locked)return D3DERR_INVALIDCALL;locked=false;return D3D_OK;}
    void Write(UINT offset,uint32_t value,DWORD flags) {
        void* p=nullptr;CHECK(SUCCEEDED(Lock(offset,4,&p,flags)));
        std::memcpy(p,&value,4);CHECK(SUCCEEDED(Unlock()));
    }
};
static std::vector<Rec> g_rec;
static bool g_recActive=false,g_recOverflow=false;
static uint64_t g_recCounts[40]{};
static d2vr_replay::FrameResourceRefs<IUnknown> g_recordRefs;
struct RecLockGuard{RecLockGuard(){} ~RecLockGuard(){}};
static void NoteRecordedWorldDraw(){}
static HRESULT RecordLiveResult(HRESULT hr){if(FAILED(hr))g_recOverflow=true;return hr;}
namespace d2vr_trace {static bool g_enabled=false;static void NoteDraw(){}}
struct UiClipState{};
static UiClipState BeginScaledUiClip(IDirect3DDevice9*){return {};}
static void EndScaledUiClip(IDirect3DDevice9*,const UiClipState&){}
static int g_lightRbBudget=0;
static IDirect3DPixelShader9* g_replayPs=nullptr;
static IDirect3DBaseTexture9* replayTex[16]{};
static void LightInputReadback(IDirect3DDevice9*,IDirect3DPixelShader9*,IDirect3DBaseTexture9**,int){CHECK(false);}
static Buffer* boundVB=nullptr;
static Buffer* boundIB=nullptr;
static UINT streamOffset=0,streamStride=0;
static std::vector<uint32_t> observed;
struct Original {
    HRESULT SetStreamSource(IDirect3DDevice9*,UINT stream,IDirect3DVertexBuffer9* vb,UINT offset,UINT stride){
        CHECK(stream==0);boundVB=reinterpret_cast<Buffer*>(vb);streamOffset=offset;streamStride=stride;return D3D_OK;
    }
    HRESULT SetIndices(IDirect3DDevice9*,IDirect3DIndexBuffer9* ib){boundIB=reinterpret_cast<Buffer*>(ib);return D3D_OK;}
} g_orig;
static HRESULT DrawPrimitiveWithDiffuse(IDirect3DDevice9*,D3DPRIMITIVETYPE type,UINT start,UINT count){
    CHECK(type==D3DPT_POINTLIST && count==1 && boundVB && !boundVB->locked);
    observed.push_back(boundVB->words[(streamOffset+start*streamStride)/4]);return D3D_OK;
}
static HRESULT DrawIndexedPrimitiveWithDiffuse(IDirect3DDevice9* dev,D3DPRIMITIVETYPE type,INT base,UINT min,UINT num,UINT start,UINT count){
    CHECK(boundIB && !boundIB->locked && num==2);
    CHECK(type==D3DPT_TRIANGLELIST && count==1);
    const uint32_t index=boundIB->words[start];CHECK(index>=min && index<min+num);
    return DrawPrimitiveWithDiffuse(dev,D3DPT_POINTLIST,UINT(base+INT(index)),count);
}
@FUNCTIONS@
static bool Replay(){
    bool replayComplete=true;IDirect3DDevice9* dev=nullptr;const int eye=1;
    for(const auto& r:g_rec)switch(RecOp(r.op)){
@CASES@
    default:CHECK(false);
    }
    return replayComplete;
}
static void Begin(){g_rec.clear();g_recordRefs.Begin(true);g_recOverflow=false;g_recActive=true;observed.clear();}
static void End(){g_recActive=false;g_recordRefs.Clear();g_rec.clear();}
int main(int argc,char**){
    static_assert(sizeof(void*)==4,"Recorder pointer packets require x86");
    unsigned reproduced=0;
    // Same handle, explicit and inherited binding, DISCARD and ordinary overwrite.
    for(bool inherited:{false,true})for(DWORD flags:{DWORD(0),DWORD(D3DLOCK_DISCARD)}){
        Buffer vb;vb.Write(12,101,D3DLOCK_DISCARD);
        if(inherited)g_orig.SetStreamSource(nullptr,0,reinterpret_cast<IDirect3DVertexBuffer9*>(&vb),4,4);
        Begin();
        if(!inherited)CHECK(SUCCEEDED(Hook_SetStreamSource(nullptr,0,reinterpret_cast<IDirect3DVertexBuffer9*>(&vb),4,4)));
        CHECK(SUCCEEDED(Hook_DrawPrimitive(nullptr,D3DPT_POINTLIST,2,1)));
        vb.Write(12,202,flags);
        CHECK(SUCCEEDED(Hook_DrawPrimitive(nullptr,D3DPT_POINTLIST,2,1)));
        CHECK((observed==std::vector<uint32_t>{101,202}));
        g_recActive=false;observed.clear();CHECK(Replay());
        CHECK(observed.size()==2 && observed[1]==202);if(observed[0]!=101)++reproduced;
        End();CHECK(vb.refs==1);
    }
    // Index-only overwrite selects a different vertex; signed base and IB offset survive.
    for(DWORD flags:{DWORD(0),DWORD(D3DLOCK_DISCARD)}){
        Buffer vb,ib;vb.Write(8,301,D3DLOCK_DISCARD);vb.Write(12,302,D3DLOCK_NOOVERWRITE);
        ib.Write(4,2,D3DLOCK_DISCARD);ib.Write(8,2,D3DLOCK_NOOVERWRITE);ib.Write(12,2,D3DLOCK_NOOVERWRITE);Begin();
        Hook_SetStreamSource(nullptr,0,reinterpret_cast<IDirect3DVertexBuffer9*>(&vb),4,4);
        Hook_SetIndices(nullptr,reinterpret_cast<IDirect3DIndexBuffer9*>(&ib));
        // Both draws permit either index; fixture checks individual native arguments below.
        Hook_DrawIndexedPrimitive(nullptr,D3DPT_TRIANGLELIST,-1,2,2,1,1);
        ib.Write(4,3,flags);ib.Write(8,2,D3DLOCK_NOOVERWRITE);ib.Write(12,2,D3DLOCK_NOOVERWRITE);
        Hook_DrawIndexedPrimitive(nullptr,D3DPT_TRIANGLELIST,-1,2,2,1,1);
        CHECK((observed==std::vector<uint32_t>{301,302}));
        CHECK(g_rec[2].a[1]==UINT(-1) && g_rec[2].a[2]==2 && g_rec[2].a[4]==1);
        g_recActive=false;observed.clear();CHECK(Replay());
        CHECK(observed.size()==2 && observed[1]==302);if(observed[0]!=301)++reproduced;
        End();CHECK(vb.refs==1 && ib.refs==1);
    }
    // Legal NOOVERWRITE appends do not mutate a previously submitted range.
    Buffer vb;vb.Write(0,401,D3DLOCK_DISCARD);Begin();
    Hook_SetStreamSource(nullptr,0,reinterpret_cast<IDirect3DVertexBuffer9*>(&vb),0,4);
    Hook_DrawPrimitive(nullptr,D3DPT_POINTLIST,0,1);
    vb.Write(4,402,D3DLOCK_NOOVERWRITE);Hook_DrawPrimitive(nullptr,D3DPT_POINTLIST,1,1);
    CHECK((observed==std::vector<uint32_t>{401,402}));g_recActive=false;observed.clear();CHECK(Replay());
    CHECK((observed==std::vector<uint32_t>{401,402}));End();CHECK(vb.refs==1);
    if(argc>1){std::printf("%s consistency acceptance: %u recorded buffer overwrites replay the wrong version.\n",reproduced?"FAIL":"PASS",reproduced);return reproduced?2:0;}
    CHECK(reproduced==6);
    std::printf("PASS known-defect witness: %u overwrites reproduced; disjoint NOOVERWRITE control agrees. This is NOT a fixed-replay acceptance test.\n",reproduced);
}

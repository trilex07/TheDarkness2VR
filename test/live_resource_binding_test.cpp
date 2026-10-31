#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <vector>
#include "../src/frame_resource_refs.h"
#include "../src/replay_state_cache.h"
#include "../src/stateblock_recording.h"
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %u line %d: %s\n",checks,__LINE__,#x);exit(1);}}while(0)
template<class... T> static void Log(const char*,T...){}
static const D3DFORMAT FOURCC_INTZ=D3DFORMAT(MAKEFOURCC('I','N','T','Z'));
static const D3DFORMAT FOURCC_RAWZ=D3DFORMAT(MAKEFOURCC('R','A','W','Z'));
static const D3DFORMAT FOURCC_DF24=D3DFORMAT(MAKEFOURCC('D','F','2','4'));
struct Resource {
    unsigned refs=1,queries=0;
    void AddRef(){CHECK(refs);++refs;}
    void Release(){CHECK(refs);--refs;}
};
struct Surface;
struct __declspec(uuid("85C31227-3DE5-4F00-9B3A-0F1A1A83EB5E")) Texture:Resource {
    Surface* surface=nullptr;
    D3DFORMAT format=FOURCC_INTZ;
    D3DRESOURCETYPE type=D3DRTYPE_TEXTURE;
    D3DRESOURCETYPE GetType(){++queries;return type;}
    HRESULT GetSurfaceLevel(UINT,Surface**);
    HRESULT GetLevelDesc(UINT,D3DSURFACE_DESC* d){++queries;*d={};d->Width=1920;d->Height=1080;d->Format=format;return S_OK;}
};
struct Surface:Resource {
    Texture* texture=nullptr;
    D3DFORMAT format=FOURCC_INTZ;
    HRESULT GetContainer(REFIID,void** out){++queries;*out=texture;if(texture)texture->AddRef();return texture?S_OK:E_NOINTERFACE;}
    HRESULT GetDesc(D3DSURFACE_DESC* d){++queries;*d={};d->Width=1920;d->Height=1080;d->Format=format;return S_OK;}
};
HRESULT Texture::GetSurfaceLevel(UINT,Surface** out){++queries;*out=surface;if(surface)surface->AddRef();return surface?S_OK:E_FAIL;}
struct Device {Texture* textures[261]{};Surface* depth=nullptr;};
enum {OP_SetTexture=9,OP_SetDepthStencilSurface=2};
struct Rec {unsigned op=0,a[6]{};};
namespace d2vr_replay {
static constexpr unsigned OP_SetTexture=::OP_SetTexture,OP_SetDepthStencilSurface=::OP_SetDepthStencilSurface;
static bool g_recActive=false;
static std::vector<Rec> records;
static FrameResourceRefs<Resource> refs;
static void RecEmit(unsigned op,unsigned,unsigned a,unsigned b=0) {
    CHECK(g_recActive);Rec r;r.op=op;r.a[0]=a;r.a[1]=b;records.push_back(r);
    auto* resource=reinterpret_cast<Resource*>(uintptr_t(op==OP_SetTexture?b:a));
    refs.Retain(resource);
}
}
static HRESULT liveResult=S_OK;
static void (*nativeCallback)(Device*)=nullptr;
static Surface* nestedDepth=nullptr;
static unsigned nativeCalls=0,replayCalls=0;
static HRESULT STDMETHODCALLTYPE SetTexture(Device* d,DWORD slot,Texture* t){++nativeCalls;if(nativeCallback){auto callback=nativeCallback;nativeCallback=nullptr;callback(d);}if(SUCCEEDED(liveResult) && !d2vr_stateblock::Active(d))d->textures[slot]=t;return liveResult;}
static HRESULT STDMETHODCALLTYPE SetDepth(Device* d,Surface* s){++nativeCalls;if(SUCCEEDED(liveResult))d->depth=s;return liveResult;}
static auto g_origSetTexture=&SetTexture;
static auto g_origSetDepthStencilSurface=&SetDepth;
static Texture* g_intzTex9=nullptr;
namespace d2vr_pipeline_warmup {template<class... A> static void TrackTexture(A...){} template<class... A> static void TrackDepth(A...){}}
#include "actual_hooks.inl"
static struct Original {
    HRESULT SetTexture(Device* d,DWORD slot,Texture* t){++replayCalls;CHECK(!t||t->refs);d->textures[slot]=t;return S_OK;}
    HRESULT SetDepthStencilSurface(Device* d,Surface* s){++replayCalls;CHECK(!s||s->refs);d->depth=s;return S_OK;}
} g_orig;
static bool Replay(Device* dev,bool filter) {
    using Binding=d2vr_replay_state::Bindings;Binding bindings;bindings.Configure(filter);
    const void* replayTex[16]{};bool replayComplete=true;
    for(const auto& r:d2vr_replay::records)switch(r.op){
#include "actual_replay.inl"
    }
    (void)replayTex;return replayComplete;
}
static void Reset() {
    d2vr_replay::refs.Clear();d2vr_replay::refs.Begin(true);d2vr_replay::records.clear();
    if(g_intzTex9)g_intzTex9->Release();g_intzTex9=nullptr;
    nativeCalls=replayCalls=0;
}
int main(){
    Texture a,b;Surface da,db;a.surface=&da;b.surface=&db;da.texture=&a;db.texture=&b;
    for(bool recording:{true,false})for(bool filter:{false,true})for(unsigned depth=0;depth<2;++depth)
    for(unsigned slot:{0u,7u,256u})for(bool nullBinding:{false,true})
    for(HRESULT result:{S_OK,S_FALSE,D3DERR_INVALIDCALL,D3DERR_DEVICELOST,D3DERR_OUTOFVIDEOMEMORY}) {
        Reset();d2vr_replay::g_recActive=recording;liveResult=result;
        Device left,right;left.depth=right.depth=&da;left.textures[slot]=right.textures[slot]=&a;
        const auto textureQueries=b.queries,depthQueries=db.queries;
        const HRESULT actual=depth?Hook_SetDepthStencilSurface(&left,nullBinding?nullptr:&db):Hook_SetTexture(&left,slot,nullBinding?nullptr:&b);
        CHECK(actual==result && nativeCalls==1);
        if(recording) {
            CHECK(Replay(&right,filter));
            CHECK(right.depth==left.depth && right.textures[slot]==left.textures[slot]);
            CHECK(replayCalls==unsigned(SUCCEEDED(result)));
        }
        if(FAILED(result)) {
            // A failed bind leaves the device and its depth observer unchanged.
            CHECK(g_intzTex9==nullptr);
            CHECK(d2vr_replay::records.empty());
            CHECK(b.queries==textureQueries && db.queries==depthQueries);
        }
        CHECK(d2vr_replay::records.size()==unsigned(recording && SUCCEEDED(result)));
        if(SUCCEEDED(result) && !nullBinding && (depth||slot==0))CHECK(g_intzTex9==&b);
    }
    // Simulated nested native callback: the nested bind completes first and
    // must record first, with no recursion back through the original pointer.
    // Verify ordinary successful caller order over several dependent binds too.
    for(bool filter:{false,true}) {
        Reset();d2vr_replay::g_recActive=true;liveResult=S_OK;Device left,right;
        nestedDepth=&da;
        nativeCallback=[](Device* dev){
            CHECK(d2vr_replay::records.empty());
            CHECK(Hook_SetDepthStencilSurface(dev,nestedDepth)==S_OK);
            CHECK(d2vr_replay::records.size()==1);
        };
        CHECK(Hook_SetTexture(&left,0,&a)==S_OK);
        CHECK(nativeCalls==2 && !nativeCallback);
        CHECK(d2vr_replay::records.size()==2);
        CHECK(d2vr_replay::records[0].op==OP_SetDepthStencilSurface);
        CHECK(d2vr_replay::records[1].op==OP_SetTexture);
        CHECK(Hook_SetTexture(&left,0,&b)==S_OK);
        CHECK(Hook_SetDepthStencilSurface(&left,&db)==S_OK);
        CHECK(Hook_SetTexture(&left,7,&a)==S_OK);
        CHECK(Replay(&right,filter));
        CHECK(right.depth==left.depth && right.textures[0]==left.textures[0] && right.textures[7]==left.textures[7]);
        CHECK(nativeCalls==5 && replayCalls==5);
    }
    // Same-frame rebind failure must preserve the last accepted resource and
    // never retain the rejected pointer, including after the game releases it.
    for(bool depth:{false,true})for(bool filter:{false,true}) {
        Reset();d2vr_replay::g_recActive=true;Device left,right;
        liveResult=S_OK;
        CHECK((depth?Hook_SetDepthStencilSurface(&left,&da):Hook_SetTexture(&left,0,&a))==S_OK);
        liveResult=D3DERR_INVALIDCALL;
        CHECK((depth?Hook_SetDepthStencilSurface(&left,&db):Hook_SetTexture(&left,0,&b))==D3DERR_INVALIDCALL);
        CHECK(g_intzTex9==&a);
        CHECK(b.refs==1 && db.refs==1);b.Release();db.Release();
        CHECK(Replay(&right,filter) && right.depth==left.depth && right.textures[0]==left.textures[0]);
        CHECK(replayCalls==1);b.refs=db.refs=1;
    }
    Reset();CHECK(a.refs==1 && b.refs==1 && da.refs==1 && db.refs==1);
    for(bool filter:{false,true})for(unsigned slot:{0u,7u,256u}) {
        Reset();d2vr_replay::g_recActive=true;liveResult=S_OK;
        Device left,right;left.textures[slot]=right.textures[slot]=&a;
        d2vr_stateblock::Began(&left,S_OK);
        CHECK(Hook_SetTexture(&left,slot,&b)==S_OK && nativeCalls==1);
        d2vr_stateblock::Ended(&left,S_OK);
        CHECK(Replay(&right,filter));
        CHECK(left.textures[slot]==right.textures[slot]);
        CHECK(d2vr_replay::records.empty() && g_intzTex9==nullptr);
        CHECK(b.refs==1 && db.refs==1);
    }
    Reset();
    printf("PASS actual-source live texture/depth binding: %u checks; 240 status/binding/recording/filter cases, same-frame failed replacement, exact HRESULT/original-once, no rejected queries/retention, balanced lifetimes, native callback completion and accepted call order; CPU fixture\n",checks);
}

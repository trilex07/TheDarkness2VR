#define NOMINMAX
#include <d3d9.h>
#include "../src/stateblock_recording.h"
#include "../src/recorder_sync.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include "../src/pixel_shader_metadata.h"
#include "../src/replay_state_cache.h"
#include "../src/composite_shader_patch.h"
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
enum {SI_END=0xffff,SI_COMMENT=0xfffe,OP_SetPixelShader=1,OP_SetVertexShader=2};
static bool g_shadowScreenDepth=false,g_disableDepthOfField=false,g_disableCompositeHeightFog=false;
static bool g_disableFxaa=false,g_shaderDebugDump=false,g_recActive=false;
static int g_submitMode=4,g_activePsInverse=-1,g_activePsViewExtents=-1;
static uint32_t g_activePsHash=0;
static const void* g_boundPs=nullptr;
template<class... T> static void Log(const char*,T...){}
static unsigned records=0;
static bool g_recOverflow=false;
static void RecEmit(unsigned,unsigned,uint32_t){++records;}
template<class... A> static bool RecEmitBinding(A... args){RecEmit(args...);return g_recActive&&!g_recOverflow;}
static d2vr_shader_metadata::Pixels g_pixelShaders;
#ifdef D2VR_EXPECT_LEGACY_STALE
static std::unordered_map<const void*,uint32_t> g_psHashByPtr;
static std::unordered_map<IDirect3DPixelShader9*,int> g_psInverse,g_psViewExtents;
#endif
struct RecLockGuard {};
static unsigned vertexSyncs=0;
static const void* activeVertex=nullptr;
static struct {uint32_t hash=7;} vertexInfo;
static decltype(&vertexInfo) g_activeVs=nullptr;
static void SetActiveVertexShaderMetadata(const void* shader){++vertexSyncs;activeVertex=shader;g_activeVs=shader?&vertexInfo:nullptr;}
static unsigned creates=0,binds=0,reads=0;
static HRESULT createResult=S_OK,bindResult=S_OK;
static IDirect3DDevice9* const expectedDevice=reinterpret_cast<IDirect3DDevice9*>(0x1234);
struct Shader:IDirect3DPixelShader9 {
    std::vector<DWORD> code;
    unsigned failRead=0;
    int lengthDelta=0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**)override{return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return 1;}
    ULONG STDMETHODCALLTYPE Release()override{return 1;}
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9**)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetFunction(void* data,UINT* bytes)override {
        ++reads;
        if((!data && failRead==1) || (data && failRead==2))return E_FAIL;
        if(!data){*bytes=UINT(int(code.size()*4)+lengthDelta);return S_OK;}
        if(failRead==3){*bytes+=4;return S_OK;}
        CHECK(*bytes>=code.size()*4);memcpy(data,code.data(),code.size()*4);
        *bytes=UINT(code.size()*4);return S_OK;
    }
} shader;
static HRESULT STDMETHODCALLTYPE Create(IDirect3DDevice9* dev,const DWORD*,IDirect3DPixelShader9** out) {
    CHECK(dev==expectedDevice);++creates;
    if(SUCCEEDED(createResult) && out)*out=&shader;
    return createResult;
}
static HRESULT STDMETHODCALLTYPE Bind(IDirect3DDevice9* dev,IDirect3DPixelShader9* ps) {
    CHECK(dev==expectedDevice && (!ps || ps==&shader));++binds;return bindResult;
}
static HRESULT STDMETHODCALLTYPE BindVertex(IDirect3DDevice9* dev,IDirect3DVertexShader9*) {CHECK(dev==expectedDevice);++binds;return bindResult;}
static struct {
    decltype(&Create) CreatePixelShader=&Create;
    decltype(&Bind) SetPixelShader=&Bind;
    decltype(&BindVertex) SetVertexShader=&BindVertex;
} g_orig;
namespace d2vr_shader_preload {
template<class F> static HRESULT Create(IDirect3DDevice9* dev,const DWORD* code,IDirect3DPixelShader9** out,F f){return f(dev,code,out);}
}
#include "actual_pixel_metadata.inl"
#ifndef D2VR_EXPECT_LEGACY_STALE
static const void *g_replayVs=nullptr,*g_replayPs=nullptr;
static bool ReplayBind(unsigned op,uintptr_t pointer) {
    bool replayComplete=true;auto dev=expectedDevice;
    d2vr_replay_state::Bindings bindings;using Binding=d2vr_replay_state::Bindings;
    struct {uint32_t a[1];} r{{uint32_t(pointer)}};
    switch(op) {
#include "actual_shader_replay.inl"
    default:CHECK(false);
    }
    return replayComplete;
}
static const uint32_t kShadowLightPsHash=0xC4D1FB10;
static bool g_lightPosShift=true;
struct FixtureDevice {
    HRESULT result=S_OK;
    HRESULT GetVertexShader(IDirect3DVertexShader9** out){*out=nullptr;return S_OK;}
    HRESULT GetPixelShader(IDirect3DPixelShader9** out){*out=SUCCEEDED(result)?&shader:nullptr;return result;}
};
#include "actual_pixel_sync.inl"
static bool AdjustLight(float* adjusted,const float* values,unsigned first,unsigned count,float x,float y) {
    struct {unsigned a[2];} r{{first,count}};
    struct {float transX,transY;} sp{x,y};
    bool copied=false;
#include "actual_pixel_light.inl"
    return copied;
}
#endif
static std::vector<DWORD> Bytecode(int inverse,int extents) {
    const char names[]="InverseProjection\0ViewExtents\0";
    const size_t payloadBytes=28+40+sizeof(names);
    std::vector<DWORD> payload((payloadBytes+3)/4);
    auto* bytes=reinterpret_cast<unsigned char*>(payload.data());
    const DWORD header[]={28,0,0,2,28,0,0};memcpy(bytes,header,sizeof(header));
    const DWORD entries[]={68,(DWORD(inverse)<<16)|2,4,0,0,
        68+18,(DWORD(extents)<<16)|2,1,0,0};
    memcpy(bytes+28,entries,sizeof(entries));memcpy(bytes+68,names,sizeof(names));
    std::vector<DWORD> result={0xffff0300,DWORD((payload.size()+1)<<16)|0xfffe,0x42415443};
    result.insert(result.end(),payload.begin(),payload.end());result.push_back(0xffff);return result;
}
static d2vr_shader_metadata::Pixel Snapshot() {
#ifdef D2VR_EXPECT_LEGACY_STALE
    return {g_psHashByPtr[&shader],g_psInverse[&shader],g_psViewExtents[&shader]};
#else
    return g_pixelShaders.Lookup(&shader);
#endif
}
static void Submit(const std::vector<DWORD>& code) {
    const unsigned before=creates;IDirect3DPixelShader9* result=nullptr;
    CHECK(Hook_CreatePixelShader(expectedDevice,code.data(),&result)==createResult);
    CHECK(creates==before+1);
    CHECK(FAILED(createResult)?result==nullptr:result==&shader);
}
int main() {
    const auto first=Bytecode(27,38),second=Bytecode(44,55);
    shader.code=first;Submit(first);
    CHECK(Snapshot().inverse==27 && Snapshot().extents==38);
    // Exact same COM address, different shader. Reflection failure must never
    // inherit an old shader's projection registers.
    shader.code=second;shader.failRead=1;Submit(second);
#ifdef D2VR_EXPECT_LEGACY_STALE
    CHECK(Snapshot().hash==HashBytes(second.data(),second.size()*4));
    CHECK(Snapshot().inverse==27 && Snapshot().extents==38);
    puts("REPRODUCED actual CreatePixelShader hook: recycled COM address retains prior InverseProjection/ViewExtents after new shader reflection fails");
    return 0;
#else
    CHECK(Snapshot().hash==HashBytes(second.data(),second.size()*4));
    CHECK(Snapshot().inverse==-1 && Snapshot().extents==-1);
    for(unsigned failure=0;failure<4;++failure) {
        shader.failRead=0;shader.code=first;Submit(first);
        shader.code=second;shader.failRead=failure;Submit(second);
        CHECK(Snapshot().hash==HashBytes(second.data(),second.size()*4));
        CHECK(Snapshot().inverse==(failure?-1:44));
        CHECK(Snapshot().extents==(failure?-1:55));
    }
    // Invalid size and success-with-oversized-result cannot index outside the
    // allocated bytecode. A valid creation still returns its original result.
    for(int delta:{-int(second.size()*4),1,32768}) {
        shader.code=first;shader.failRead=0;shader.lengthDelta=0;Submit(first);
        shader.code=second;shader.lengthDelta=delta;Submit(second);
        CHECK(Snapshot().inverse==-1 && Snapshot().extents==-1);
    }
    shader.lengthDelta=0;shader.failRead=0;shader.code=second;Submit(second);
    for(const auto& malformed:std::vector<std::vector<DWORD>>{
        {0xffff0300,0x7ffffffe}, // comment beyond returned allocation
        {0xffff0300,0x0f000001,0xffff}, // instruction beyond returned allocation
        {0xfffe0300,0xffff}, // vertex bytecode in a pixel object
        {0xffff0300,0xffff,0}, // trailing token
        {0xffff0300,0}, // missing END
        {0xffff0101,0xffff}}) {
        shader.code=first;Submit(first);shader.code=malformed;Submit(second);
        CHECK(Snapshot().inverse==-1 && Snapshot().extents==-1);
    }
    shader.code=second;Submit(second);
    const auto accepted=Snapshot();createResult=D3DERR_INVALIDCALL;shader.code=first;Submit(first);
    CHECK(Snapshot().inverse==accepted.inverse && Snapshot().hash==accepted.hash);
    createResult=S_OK;
    SetActivePixelShaderMetadata(&shader);
    CHECK(g_activePsInverse==44 && g_activePsViewExtents==55 && g_activePsHash==accepted.hash);
    for(HRESULT result:{S_OK,D3DERR_INVALIDCALL}) {
        bindResult=result;const auto before=binds;
        CHECK(Hook_SetPixelShader(expectedDevice,&shader)==result && binds==before+1);
    }
    g_recActive=true;records=0;
    const auto retained=g_activePsHash;
    CHECK(Hook_SetPixelShader(expectedDevice,nullptr)==bindResult);
    CHECK(g_activePsHash==retained && records==0); // rejected null bind leaves old shader active
    SetActivePixelShaderMetadata(nullptr);
    CHECK(Hook_SetPixelShader(expectedDevice,&shader)==bindResult);
    CHECK(!g_activePsHash && records==0); // rejected nonnull bind must not install its metadata
    bindResult=S_FALSE;
    CHECK(Hook_SetPixelShader(expectedDevice,&shader)==S_FALSE && records==1);
    bindResult=S_OK;
    CHECK(Hook_SetPixelShader(expectedDevice,nullptr)==S_OK && records==2);
    CHECK(g_activePsInverse==-1 && g_activePsViewExtents==-1 && !g_activePsHash);
    CHECK(g_pixelShaders.Lookup(reinterpret_cast<void*>(0x8765)).inverse==-1);
    auto vs=reinterpret_cast<IDirect3DVertexShader9*>(0x8888);
    records=0;SetActiveVertexShaderMetadata(vs);
    bindResult=D3DERR_INVALIDCALL;
    CHECK(Hook_SetVertexShader(expectedDevice,nullptr)==bindResult && activeVertex==vs && records==0);
    SetActiveVertexShaderMetadata(nullptr);
    CHECK(Hook_SetVertexShader(expectedDevice,vs)==bindResult && !activeVertex && records==0);
    bindResult=S_FALSE;CHECK(Hook_SetVertexShader(expectedDevice,vs)==S_FALSE && activeVertex==vs && records==1);
    bindResult=S_OK;CHECK(Hook_SetVertexShader(expectedDevice,nullptr)==S_OK && !activeVertex && records==2);
    for(HRESULT result:{S_OK,S_FALSE,D3DERR_INVALIDCALL}) {
        bindResult=result;g_recActive=true;records=0;
        SetActiveVertexShaderMetadata(vs);SetActivePixelShaderMetadata(&shader);
        const auto hash=g_activePsHash;
        d2vr_stateblock::Began(expectedDevice,S_OK);
        CHECK(Hook_SetVertexShader(expectedDevice,nullptr)==result && activeVertex==vs && records==0);
        CHECK(Hook_SetPixelShader(expectedDevice,nullptr)==result && g_activePsHash==hash && records==0);
        d2vr_stateblock::Ended(expectedDevice,S_OK);
    }
    g_recActive=false;vertexSyncs=0;
    for(unsigned op:{unsigned(OP_SetVertexShader),unsigned(OP_SetPixelShader)}) {
        const auto identity=op==OP_SetVertexShader?uintptr_t(vs):uintptr_t(&shader);
        bindResult=S_OK;CHECK(ReplayBind(op,identity));
        const auto pixelHash=g_activePsHash;
        bindResult=D3DERR_INVALIDCALL;CHECK(!ReplayBind(op,0));
        CHECK(op==OP_SetVertexShader?activeVertex==vs:g_activePsHash==pixelHash);
        CHECK(op==OP_SetVertexShader?g_replayVs==vs:g_replayPs==&shader);
        bindResult=S_FALSE;CHECK(ReplayBind(op,0));
        CHECK(op==OP_SetVertexShader?!activeVertex:!g_activePsHash);
        CHECK(op==OP_SetVertexShader?!g_replayVs:!g_replayPs);
        bindResult=E_FAIL;CHECK(!ReplayBind(op,identity));
        CHECK(op==OP_SetVertexShader?!activeVertex:!g_activePsHash);
    }
    vertexSyncs=0;
    // Snapshot restoration can inherit a PS before any OP_SetPixelShader in
    // this replay. Its light upload must use the restored binding's hash.
    g_pixelShaders.Publish(&shader,{kShadowLightPsHash,44,55});
    FixtureDevice device;CHECK(SyncActiveShader(&device));
    CHECK(vertexSyncs==1 && g_activePsHash==kShadowLightPsHash && g_activePsInverse==44 && g_activePsViewExtents==55);
    float values[8]={1,2,3,4,5,6,7,8},adjusted[8]{};
    CHECK(AdjustLight(adjusted,values,0,2,.25f,-.5f));
    CHECK(adjusted[0]==1.25f && adjusted[1]==1.5f && !memcmp(adjusted+2,values+2,6*sizeof(float)));
    CHECK(!AdjustLight(adjusted,values,1,2,.25f,-.5f));
    CHECK(!AdjustLight(adjusted,values,0,225,.25f,-.5f));
    device.result=E_FAIL;CHECK(!SyncActiveShader(&device));
    CHECK(!g_activePsHash && g_activePsInverse==-1 && g_activePsViewExtents==-1);
    CHECK(!AdjustLight(adjusted,values,0,2,.25f,-.5f));
    // Actual native shader hooks: successful publication permits elision;
    // failures and callbacks cannot publish a false proof.
    d2vr_submission::requested=true;d2vr_submission::bindingsRequested=true;
    d2vr_submission::deviceIdentity=expectedDevice;d2vr_submission::gate.Configure(true);
    g_recActive=true;g_recOverflow=false;bindResult=S_OK;
    unsigned beforeBind=binds,beforeRecord=records;
    CHECK(Hook_SetPixelShader(expectedDevice,&shader)==S_OK);
    CHECK(Hook_SetPixelShader(expectedDevice,&shader)==S_OK);
    CHECK(binds==beforeBind+1 && records==beforeRecord+1);
    auto* vertex=reinterpret_cast<IDirect3DVertexShader9*>(&shader);
    beforeBind=binds;beforeRecord=records;
    CHECK(Hook_SetVertexShader(expectedDevice,vertex)==S_OK);
    activeVertex=nullptr; // metadata can be republished independently of binding identity
    CHECK(Hook_SetVertexShader(expectedDevice,vertex)==S_OK);
    CHECK(activeVertex==vertex);
    CHECK(binds==beforeBind+1 && records==beforeRecord+1);
    d2vr_submission::Invalidate();bindResult=E_FAIL;beforeBind=binds;beforeRecord=records;
    CHECK(Hook_SetPixelShader(expectedDevice,&shader)==E_FAIL);
    CHECK(Hook_SetPixelShader(expectedDevice,&shader)==E_FAIL);
    CHECK(binds==beforeBind+2 && records==beforeRecord);
    bindResult=S_OK;
    // A worker repeatedly replaces a complete snapshot while two render-like
    // readers access it and new identities force map rehashes.
    d2vr_shader_metadata::Pixels concurrent;
    concurrent.Publish(&shader,{1,3,7});
    std::atomic<bool> start{false},done{false};std::atomic<unsigned> snapshots{0};
    auto read=[&]{while(!start.load())YieldProcessor();do {
        const auto value=concurrent.Lookup(&shader);
        CHECK(value.inverse==int(value.hash*3) && value.extents==int(value.hash*7));
        ++snapshots;
    }while(!done.load());};
    std::thread reader1(read),reader2(read);
    start.store(true);
    for(uint32_t i=1;i<=100000;++i) {
        concurrent.Publish(&shader,{i,int(i*3),int(i*7)});
        if(i<16384)concurrent.Publish(reinterpret_cast<void*>(uintptr_t(i)*16),{i,1,2});
    }
    done.store(true);reader1.join();reader2.join();CHECK(snapshots.load()>0);
    printf("PASS actual-source pixel creation/bind: reused address, failed reflection, malformed sizes, native HRESULT/call count, null binding; 100000 concurrent publications, %u coherent snapshots, 16383 rehash identities; CPU fixture, no GPU/HMD claim\n",snapshots.load());
#endif
}

static bool g_replayBufferVersions=true;
// Actual recorder + binding/draw hooks + buffer interception + replay cases.
// Only the D3D9 driver is doubled. Lock returns writable memory; draws consume
// the bound bytes immediately. This proves ordering, not hardware timings.
#include <d3d9.h>
#include "replay_state_cache.h"
#include "recorder_sync.h"
#include "stateblock_recording.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>
#include "frame_resource_refs.h"
#include "record_payload_storage.h"
#define CHECK(x) do {if(!(x)){std::printf("FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static std::atomic<size_t> allocations{0};
void* operator new(size_t n){++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,size_t) noexcept {std::free(p);}
@ENUM@
@RECORD@
static std::vector<Rec> g_rec;
static d2vr_record_payload::Storage g_recData;
static std::atomic<bool> g_recActive{false},g_recOverflow{false};
static uint64_t g_recCounts[40]{};
static d2vr_replay::FrameResourceRefs<IUnknown,d2vr_record_sync::DriverScope> g_recordRefs;
@LOCK@
static void NoteRecordedWorldDraw(){}
static HRESULT RecordLiveResult(HRESULT hr){if(FAILED(hr))g_recOverflow=true;return hr;}
namespace d2vr_trace {static bool g_enabled=false;static void NoteDraw(){}}
struct UiClipState{};
static UiClipState BeginScaledUiClip(IDirect3DDevice9*){return {};}
static void EndScaledUiClip(IDirect3DDevice9*,const UiClipState&){}
static int g_lightRbBudget=0;
static IDirect3DPixelShader9* g_replayPs=nullptr;
static const void* replayTex[16]{};
static void LightInputReadback(IDirect3DDevice9*,IDirect3DPixelShader9*,const void**,int){CHECK(false);}
static size_t liveBuffers=0,driverLocks=0,privateQueries=0;
static void (*driverCallback)()=nullptr;
static unsigned callbackKind=0;
static void DriverCallback(unsigned kind){if(driverCallback && callbackKind==kind){auto f=driverCallback;driverCallback=nullptr;f();}}
struct DriverBuffer {
    void** table;
    ULONG refs=1;
    std::vector<uint8_t> data;
    unsigned locks=0;
    DWORD tag=0;
    IUnknown* metadata=nullptr;
    bool failLock=false,failUnlock=false;
    explicit DriverBuffer(UINT size,void** vt):table(vt),data(size){++liveBuffers;}
    ~DriverBuffer(){if(metadata)metadata->Release();--liveBuffers;}
};
static IUnknown* driverAlias=nullptr;
static HRESULT STDMETHODCALLTYPE DriverQuery(IUnknown* object,REFIID,void** out){auto value=driverAlias?driverAlias:object;*out=value;value->AddRef();return D3D_OK;}
static ULONG STDMETHODCALLTYPE DriverAddRef(IUnknown* object){return ++reinterpret_cast<DriverBuffer*>(object)->refs;}
static ULONG STDMETHODCALLTYPE DriverRelease(IUnknown* object){auto b=reinterpret_cast<DriverBuffer*>(object);auto n=--b->refs;if(!n)delete b;return n;}
static HRESULT STDMETHODCALLTYPE DriverLock(IUnknown* object,UINT offset,UINT bytes,void** out,DWORD flags){
    auto b=reinterpret_cast<DriverBuffer*>(object);++driverLocks;DriverCallback(1);
    if(b->failLock || offset>b->data.size() || bytes>b->data.size()-offset)return D3DERR_INVALIDCALL;
    if(flags&D3DLOCK_DISCARD)std::memset(b->data.data(),0xcd,b->data.size());
    ++b->locks;*out=b->data.data()+offset;return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE DriverUnlock(IUnknown* object){
    DriverCallback(2);
    auto b=reinterpret_cast<DriverBuffer*>(object);
    if(b->failUnlock || !b->locks)return D3DERR_INVALIDCALL;
    --b->locks;return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE DriverSetPrivate(IUnknown* object,REFGUID,const void* data,DWORD bytes,DWORD flags){
    auto b=reinterpret_cast<DriverBuffer*>(object);CHECK(bytes==4);
    if(flags&D3DSPD_IUNKNOWN) {auto metadata=const_cast<IUnknown*>(static_cast<const IUnknown*>(data));metadata->AddRef();if(b->metadata)b->metadata->Release();b->metadata=metadata;}
    else std::memcpy(&b->tag,data,4);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE DriverGetPrivate(IUnknown* object,REFGUID key,void* data,DWORD* bytes){
    auto b=reinterpret_cast<DriverBuffer*>(object);++privateQueries;CHECK(*bytes==4);DriverCallback(4);
    if(key.Data1==0x447458e9) {if(!b->metadata)return D3DERR_NOTFOUND;b->metadata->AddRef();std::memcpy(data,&b->metadata,4);}
    else {if(!b->tag)return D3DERR_NOTFOUND;std::memcpy(data,&b->tag,4);}
    return D3D_OK;
}
static void* PatchVTable(void* object,int index,void* replacement){auto table=*reinterpret_cast<void***>(object);auto previous=table[index];table[index]=replacement;return previous;}
static void* driverTable[14]{};
static HRESULT STDMETHODCALLTYPE DriverCreateVB(IDirect3DDevice9*,UINT size,DWORD,DWORD,D3DPOOL,IDirect3DVertexBuffer9** out,HANDLE*){
    *out=reinterpret_cast<IDirect3DVertexBuffer9*>(new DriverBuffer(size,driverTable));return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE DriverCreateIB(IDirect3DDevice9*,UINT size,DWORD,D3DFORMAT,D3DPOOL,IDirect3DIndexBuffer9** out,HANDLE*){
    *out=reinterpret_cast<IDirect3DIndexBuffer9*>(new DriverBuffer(size,driverTable));return D3D_OK;
}
static IDirect3DVertexBuffer9* boundVB[16]{};
static IDirect3DIndexBuffer9* boundIB=nullptr;
static UINT streamOffset[16]{},streamStride[16]{};
static std::vector<uint32_t> observed;
struct Original {
    HRESULT SetStreamSource(IDirect3DDevice9*,UINT stream,IDirect3DVertexBuffer9* vb,UINT offset,UINT stride){
        CHECK(stream<16);if(vb)vb->AddRef();if(boundVB[stream])boundVB[stream]->Release();
        boundVB[stream]=vb;streamOffset[stream]=offset;streamStride[stream]=stride;return D3D_OK;
    }
    HRESULT SetIndices(IDirect3DDevice9*,IDirect3DIndexBuffer9* ib){if(ib)ib->AddRef();if(boundIB)boundIB->Release();boundIB=ib;return D3D_OK;}
} g_orig;
static HRESULT STDMETHODCALLTYPE DriverGetStream(IDirect3DDevice9*,UINT slot,IDirect3DVertexBuffer9** out,UINT* offset,UINT* stride){
    CHECK(slot<16);*out=boundVB[slot];if(*out)(*out)->AddRef();*offset=streamOffset[slot];*stride=streamStride[slot];return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE DriverGetIndices(IDirect3DDevice9*,IDirect3DIndexBuffer9** out){*out=boundIB;if(*out)(*out)->AddRef();return D3D_OK;}
static void* deviceTable[119]{};
static void** deviceObject=deviceTable;
static auto device=reinterpret_cast<IDirect3DDevice9*>(&deviceObject);
static uint32_t Word(IUnknown* buffer,UINT byte){uint32_t v;auto b=reinterpret_cast<DriverBuffer*>(buffer);CHECK(byte+4<=b->data.size());std::memcpy(&v,b->data.data()+byte,4);return v;}
static HRESULT DrawPrimitiveWithDiffuse(IDirect3DDevice9*,D3DPRIMITIVETYPE type,UINT start,UINT count){
    CHECK(g_recLock.load()==0);DriverCallback(3);
    CHECK(type==D3DPT_POINTLIST && count==1 && boundVB[0]);
    observed.push_back(Word(boundVB[0],streamOffset[0]+start*streamStride[0]));return D3D_OK;
}
static HRESULT DrawIndexedPrimitiveWithDiffuse(IDirect3DDevice9* dev,D3DPRIMITIVETYPE type,INT base,UINT min,UINT num,UINT start,UINT count){
    CHECK(boundIB && type==D3DPT_TRIANGLELIST && count==1);
    for(UINT i=0;i<3;++i){const auto index=Word(boundIB,(start+i)*4);CHECK(index>=min && index<min+num);}
    return DrawPrimitiveWithDiffuse(dev,D3DPT_POINTLIST,UINT(base+INT(Word(boundIB,start*4))),count);
}
@RECORD_FUNCTIONS@
#include "replay_buffers.inl"
@HOOK_FUNCTIONS@
static IDirect3DVertexBuffer9* VB(UINT bytes=128,DWORD usage=D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY){IDirect3DVertexBuffer9* b=nullptr;CHECK(SUCCEEDED(Hook_CreateVB(device,bytes,usage,0,D3DPOOL_DEFAULT,&b,nullptr)));return b;}
static IDirect3DIndexBuffer9* IB(D3DFORMAT format=D3DFMT_INDEX32){IDirect3DIndexBuffer9* b=nullptr;CHECK(SUCCEEDED(Hook_CreateIB(device,128,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,format,D3DPOOL_DEFAULT,&b,nullptr)));return b;}
template<class T> static void Write(T* buffer,UINT offset,uint32_t value,DWORD flags){void* p=nullptr;CHECK(SUCCEEDED(buffer->Lock(offset,4,&p,flags)));std::memcpy(p,&value,4);CHECK(SUCCEEDED(buffer->Unlock()));}
static void Begin(){RecLockGuard guard;FinishBufferRecording();g_rec.clear();g_recData.clear();g_recordRefs.Begin(true);g_recOverflow=false;d2vr_record_sync::conflict=false;g_recActive=true;g_bufferFrameOpen=true;g_bufferWritesApplied=false;g_bufferUnsafeAlias.store(false);observed.clear();}
static void End(){RecLockGuard guard;g_recActive=false;FinishBufferRecording();g_rec.clear();g_recData.clear();g_recordRefs.Clear();for(UINT slot=0;slot<16;++slot)g_orig.SetStreamSource(device,slot,nullptr,0,0);g_orig.SetIndices(device,nullptr);}
static bool Replay(){
    if(!BeginReplayRecording())return false;
    BeginReplayBufferLookups();
    d2vr_replay_state::Cache stateCache;
    d2vr_replay_state::Bindings bindings;using Binding=d2vr_replay_state::Bindings;
    bool replayComplete=!g_recOverflow && !g_bufferUnsafeAlias.load();IDirect3DDevice9* dev=device;const int eye=1;
    const uint8_t* dataBase=g_recData.data();
    if(!ReplayInheritedBuffers(dev))replayComplete=false;
    for(const auto& r:g_rec)switch(RecOp(r.op)){
@CASES@
    default:CHECK(false);
    }
    RecLockGuard guard;ClearReplayBufferLookups();return EndReplayRecording(replayComplete);
}
static void BufferCases(){
    unsigned corrected=0;
    for(bool inherited:{false,true})for(DWORD flags:{DWORD(0),DWORD(D3DLOCK_DISCARD)}){
        auto vb=VB();Write(vb,12,101,D3DLOCK_DISCARD);
        if(inherited)g_orig.SetStreamSource(device,0,vb,4,4);
        Begin();
        if(!inherited)Hook_SetStreamSource(device,0,vb,4,4);
        Hook_DrawPrimitive(device,D3DPT_POINTLIST,2,1);
        Write(vb,12,202,flags);Hook_DrawPrimitive(device,D3DPT_POINTLIST,2,1);
        CHECK((observed==std::vector<uint32_t>{101,202}));observed.clear();CHECK(Replay());
        CHECK((observed==std::vector<uint32_t>{101,202}));++corrected;
        CHECK(g_bufferFrameBytes==4);End();CHECK(vb->Release()==0);
    }
    for(DWORD flags:{DWORD(0),DWORD(D3DLOCK_DISCARD)}){
        auto vb=VB();auto ib=IB();Write(vb,8,301,D3DLOCK_DISCARD);Write(vb,12,302,D3DLOCK_NOOVERWRITE);
        Write(ib,4,2,D3DLOCK_DISCARD);Write(ib,8,2,D3DLOCK_NOOVERWRITE);Write(ib,12,2,D3DLOCK_NOOVERWRITE);
        Begin();Hook_SetStreamSource(device,0,vb,4,4);Hook_SetIndices(device,ib);
        Hook_DrawIndexedPrimitive(device,D3DPT_TRIANGLELIST,-1,2,2,1,1);
        Write(ib,4,3,flags);Write(ib,8,2,D3DLOCK_NOOVERWRITE);Write(ib,12,2,D3DLOCK_NOOVERWRITE);
        Hook_DrawIndexedPrimitive(device,D3DPT_TRIANGLELIST,-1,2,2,1,1);
        CHECK((observed==std::vector<uint32_t>{301,302}));observed.clear();CHECK(Replay());
        CHECK((observed==std::vector<uint32_t>{301,302}));++corrected;
        CHECK(g_bufferFrameBytes==12);End();CHECK(vb->Release()==0 && ib->Release()==0);
    }
    auto vb=VB();Write(vb,0,401,D3DLOCK_DISCARD);Begin();Hook_SetStreamSource(device,0,vb,0,4);
    Hook_DrawPrimitive(device,D3DPT_POINTLIST,0,1);Write(vb,4,402,D3DLOCK_NOOVERWRITE);Hook_DrawPrimitive(device,D3DPT_POINTLIST,1,1);
    CHECK((observed==std::vector<uint32_t>{401,402}));observed.clear();CHECK(Replay());
    CHECK((observed==std::vector<uint32_t>{401,402}));
    // Reapplying a state block that does not change buffers sees already remapped replicas.
    CHECK(ReplayInheritedBuffers(device));CHECK(!Replay());End();vb->Release();
    CHECK(corrected==6 && !liveBuffers && !g_bufferObjects);
    std::puts("PASS six prior VB/IB failures, inherited binding, exact offsets/base/index range, NOOVERWRITE control, replica rebind");
}
static void LifecycleCases(){
    auto vb=VB();Write(vb,0,1,D3DLOCK_DISCARD);Begin();Write(vb,0,2,0);
    CloseBufferFrame();CHECK(Word(BufferInfo(vb)->replica,0)==2); // no replay
    Write(vb,4,3,D3DLOCK_NOOVERWRITE);CHECK(Word(BufferInfo(vb)->replica,4)==3);End();
    Begin();Hook_SetStreamSource(device,0,vb,0,4);Write(vb,0,4,0);
    CHECK(vb->Release()>0);observed.clear();CHECK(Replay());End();CHECK(!liveBuffers);
    // Read-only and failed locks emit nothing and preserve the native HRESULT.
    vb=VB();Write(vb,0,5,D3DLOCK_DISCARD);Begin();void* p=nullptr;
    CHECK(SUCCEEDED(vb->Lock(0,4,&p,D3DLOCK_READONLY)));CHECK(SUCCEEDED(vb->Unlock()));CHECK(g_rec.empty());
    auto raw=reinterpret_cast<DriverBuffer*>(vb);raw->failLock=true;
    CHECK(FAILED(vb->Lock(0,4,&p,0)));CHECK(g_rec.empty());raw->failLock=false;
    // Size zero means the remainder, not an empty update.
    CHECK(SUCCEEDED(vb->Lock(124,0,&p,0)));std::memset(p,7,4);CHECK(SUCCEEDED(vb->Unlock()));
    CHECK(g_bufferFrameBytes==4);End();CHECK(Word(BufferInfo(vb)->replica,124)==0x07070707);
    Begin();raw->failUnlock=true;CHECK(SUCCEEDED(vb->Lock(0,4,&p,0)));CHECK(FAILED(vb->Unlock()));
    CHECK(g_recOverflow && !BufferInfo(vb)->valid);End();vb->Release();
    // Static modifications and untracked resources cannot silently pass replay.
    vb=VB(128,0);Write(vb,0,9,0);Begin();Write(vb,0,10,0);CHECK(g_recOverflow);End();vb->Release();
    IDirect3DVertexBuffer9* unknown=nullptr;DriverCreateVB(device,128,0,0,D3DPOOL_DEFAULT,&unknown,nullptr);
    bool complete=true;CHECK(!ReplayBuffer(unknown,complete) && !complete);unknown->Release();
    // Reset drops every default-pool replica, without retaining its live owner.
    vb=VB();CHECK(g_bufferReplicaBytes==128);ResetBufferReplicas();CHECK(g_bufferReplicaBytes==0);
    complete=true;CHECK(!ReplayBuffer(vb,complete) && !complete);vb->Release();CHECK(!liveBuffers);
    // Capacity rejection is sticky and never changes the original write result.
    vb=VB();Begin();g_bufferFrameBytes=kBufferPayloadLimit;Write(vb,0,11,0);
    CHECK(g_recOverflow && BufferInfo(vb)->valid);CHECK(Word(BufferInfo(vb)->replica,0)==11);End();
    Begin();Hook_SetStreamSource(device,0,vb,0,4);Hook_DrawPrimitive(device,D3DPT_POINTLIST,0,1);
    observed.clear();CHECK(Replay());CHECK(observed.size()==1 && observed[0]==11);End();vb->Release();
    // An unrelated failed command drains pending ranges once and keeps later uploads.
    vb=VB();Write(vb,0,1,D3DLOCK_DISCARD);Begin();Write(vb,0,2,0);g_recOverflow=true;
    Write(vb,4,3,D3DLOCK_NOOVERWRITE);Write(vb,0,4,0);CHECK(!Replay());End();
    CHECK(BufferInfo(vb)->valid && Word(BufferInfo(vb)->replica,0)==4 && Word(BufferInfo(vb)->replica,4)==3);vb->Release();
    // Ordered worker uploads retain every range and command; no device queries.
    vb=VB(4096);Write(vb,0,0,D3DLOCK_DISCARD);Begin();
    std::thread worker([&]{for(UINT i=0;i<256;++i)Write(vb,i*4,i,D3DLOCK_NOOVERWRITE);});
    worker.join();
    std::thread recorder([]{for(UINT i=0;i<256;++i)RecEmit(OP_SetIndices,0,0);});recorder.join();
    CHECK(g_bufferFrameBytes==1024 && g_rec.size()==512);CHECK(Replay());
    for(UINT i=0;i<256;++i)CHECK(Word(BufferInfo(vb)->replica,i*4)==i);
    End();vb->Release();CHECK(!liveBuffers && !g_bufferObjects && !g_bufferReplicaBytes);
    // Concurrent uploads/recording may overlap a native lease. Never invent
    // an order: reject that pair if needed, but retain every baseline write.
    vb=VB(4096);Write(vb,0,0,D3DLOCK_DISCARD);Begin();
    std::thread uploader([&]{for(UINT i=0;i<256;++i)Write(vb,i*4,i,D3DLOCK_NOOVERWRITE);});
    std::thread concurrentRecorder([]{for(UINT i=0;i<256;++i)RecEmit(OP_SetIndices,0,0);});
    uploader.join();concurrentRecorder.join();
    const bool conflicted=g_recOverflow || d2vr_record_sync::conflict.load();
    CHECK(Replay()==!conflicted);End();
    for(UINT i=0;i<256;++i)CHECK(Word(BufferInfo(vb)->replica,i*4)==i);
    Begin();Hook_SetStreamSource(device,0,vb,0,4);Hook_DrawPrimitive(device,D3DPT_POINTLIST,255,1);
    observed.clear();CHECK(Replay());CHECK(observed.size()==1 && observed[0]==255);End();vb->Release();
    CHECK(!liveBuffers && !g_bufferObjects && !g_bufferReplicaBytes);
    std::puts("PASS abandoned frame, pre-frame upload, owner release, READONLY, failed Lock/Unlock, zero size, static/unknown rejection, Reset, quota, worker uploads");
}
static HRESULT STDMETHODCALLTYPE FailReplica(IDirect3DDevice9*,UINT,DWORD,DWORD,D3DPOOL,IDirect3DVertexBuffer9** out,HANDLE*){*out=nullptr;return E_OUTOFMEMORY;}
static void BoundaryCases(){
    // A large allocation does not cause a whole-buffer snapshot for a small edit.
    auto vb=VB(1u<<20);Write(vb,100,17,D3DLOCK_DISCARD);Begin();Write(vb,100,18,0);
    CHECK(g_recData.size()==4 && g_bufferFrameBytes==4);CHECK(Replay());End();vb->Release();
    // Two-byte index payload and inherited index binding preserve the byte offset.
    auto ib=IB(D3DFMT_INDEX16);void* data=nullptr;
    CHECK(SUCCEEDED(ib->Lock(2,2,&data,D3DLOCK_DISCARD)));std::memset(data,0x12,2);CHECK(SUCCEEDED(ib->Unlock()));
    g_orig.SetIndices(device,ib);Begin();CHECK(SUCCEEDED(ib->Lock(2,2,&data,0)));std::memset(data,0x34,2);CHECK(SUCCEEDED(ib->Unlock()));
    CHECK(g_bufferFrameBytes==2);CHECK(Replay());
    CHECK(reinterpret_cast<DriverBuffer*>(boundIB)->data[2]==0x34);End();ib->Release();
    // Multiple stream bindings retain independent offsets/strides to one replica.
    vb=VB();Write(vb,0,20,D3DLOCK_DISCARD);g_orig.SetStreamSource(device,1,vb,24,8);Begin();
    Hook_SetStreamSource(device,0,vb,4,4);Write(vb,0,21,0);CHECK(Replay());
    CHECK(boundVB[0]==boundVB[1] && streamOffset[0]==4 && streamOffset[1]==24 && streamStride[1]==8);
    End();vb->Release();
    // Explicit resource and metadata ceilings never fall back to mutable live data.
    g_bufferReplicaBytes=kBufferReplicaLimit;vb=VB();g_bufferReplicaBytes=0;
    bool complete=true;CHECK(!ReplayBuffer(vb,complete) && !complete);vb->Release();
    g_bufferObjects=kBufferObjectLimit;vb=VB();g_bufferObjects=0;
    complete=true;CHECK(!ReplayBuffer(vb,complete) && !complete);vb->Release();
    DriverCreateVB(device,128,0,0,D3DPOOL_DEFAULT,&vb,nullptr);
    g_createVB=&FailReplica;TrackBuffer(device,vb,128,D3DUSAGE_DYNAMIC,0,D3DPOOL_DEFAULT,false,false);g_createVB=&DriverCreateVB;
    complete=true;CHECK(!ReplayBuffer(vb,complete) && !complete);vb->Release();
    vb=VB();auto alias=VB();driverAlias=alias;Begin();
    {RecLockGuard guard;void* result=nullptr;CHECK(SUCCEEDED(vb->QueryInterface(__uuidof(IUnknown),&result)));CHECK(result==alias);static_cast<IUnknown*>(result)->Release();}
    driverAlias=nullptr;CHECK(!Replay());End();vb->Release();alias->Release();
    HANDLE shared=nullptr;CHECK(SUCCEEDED(Hook_CreateVB(device,128,D3DUSAGE_DYNAMIC,0,D3DPOOL_DEFAULT,&vb,&shared)));
    complete=true;CHECK(!ReplayBuffer(vb,complete) && !complete);vb->Release();
    CHECK(!liveBuffers && !g_bufferObjects && !g_bufferReplicaBytes);
    std::puts("PASS bounded range capture, INDEX16, inherited IB, multiple streams, resource/metadata caps, replica allocation failure, shared-resource rejection");
}
static void Benchmark(){
    constexpr UINT bytes=4096,writes=256,frames=300;
    auto vb=VB(bytes*writes);auto frame=[&]{
        Begin();for(UINT i=0;i<writes;++i){void* p=nullptr;CHECK(SUCCEEDED(vb->Lock(i*bytes,bytes,&p,i?D3DLOCK_NOOVERWRITE:D3DLOCK_DISCARD)));std::memset(p,int(i),bytes);CHECK(SUCCEEDED(vb->Unlock()));}
        CHECK(Replay());End();
    };
    frame();const auto before=allocations.load();const auto start=std::chrono::steady_clock::now();
    for(UINT i=0;i<frames;++i)frame();
    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/frames;
    const auto added=allocations.load()-before;CHECK(added==0);
    std::printf("BENCH 1 MiB/frame, 256 range uploads, %u frames: %.3f ms/frame capture+mock replay; steady allocations=%zu; payload cap=%zu MiB; replica cap=%zu MiB\n",frames,ms,added,kBufferPayloadLimit>>20,kBufferReplicaLimit>>20);
    // Same driver loop without interception or replay, to isolate incremental CPU work.
    IDirect3DVertexBuffer9* native=nullptr;DriverCreateVB(device,bytes*writes,0,0,D3DPOOL_DEFAULT,&native,nullptr);
    const auto baseStart=std::chrono::steady_clock::now();
    for(UINT f=0;f<frames;++f)for(UINT i=0;i<writes;++i){void* p=nullptr;DriverLock(native,i*bytes,bytes,&p,i?D3DLOCK_NOOVERWRITE:D3DLOCK_DISCARD);std::memset(p,int(i),bytes);DriverUnlock(native);}
    const double baseMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-baseStart).count()/frames;
    std::printf("BENCH baseline %.3f ms; added capture+mock replay %.3f ms/frame; metadata bound=%zu KiB. Driver/GPU stalls excluded.\n",baseMs,ms-baseMs,kBufferObjectLimit*sizeof(BufferVersion)/1024);
    g_orig.SetStreamSource(device,0,vb,0,4);
    const auto remapStart=std::chrono::steady_clock::now();
    for(UINT i=0;i<10000;++i)CHECK(ReplayInheritedBuffers(device));
    const double remapMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-remapStart).count()/10000;
    std::printf("BENCH inherited binding scan: %.3f us/apply (16 streams, one bound); no per-draw scan.\n",remapMs*1000);
    g_orig.SetStreamSource(device,0,nullptr,0,0);native->Release();vb->Release();
}
static void ReplayLookupCases(){
    RecLockGuard guard;
    auto vb=VB();auto object=static_cast<IUnknown*>(vb);auto version=BufferInfo(object);
    const auto before=privateQueries;BeginReplayBufferLookups();bool complete=true;
    for(unsigned i=0;i<10000;++i)CHECK(ReplayBuffer(object,complete)==version->replica && complete);
    CHECK(privateQueries-before==1 && reinterpret_cast<DriverBuffer*>(object)->refs==2);
    version->valid=false;CHECK(!ReplayBuffer(object,complete) && !complete);version->valid=true;
    complete=true;version->locks=1;CHECK(!ReplayBuffer(object,complete) && !complete);version->locks=0;
    complete=true;version->unsupportedAlias.store(true);CHECK(!ReplayBuffer(object,complete) && !complete);version->unsupportedAlias.store(false);
    ClearReplayBufferLookups();ClearReplayBufferLookups();CHECK(vb->Release()==0);
    // A cached source cannot die or recycle its address during this replay.
    vb=VB();object=vb;BeginReplayBufferLookups();complete=true;CHECK(ReplayBuffer(object,complete));
    CHECK(vb->Release()==1);ClearReplayBufferLookups();CHECK(!liveBuffers);
    // Collisions replace references without leaking or shortening native owners.
    std::vector<IDirect3DVertexBuffer9*> buffers;
    for(unsigned i=0;i<300;++i)buffers.push_back(VB(16));
    BeginReplayBufferLookups();
    for(auto buffer:buffers){complete=true;CHECK(ReplayBuffer(buffer,complete) && complete);}
    ClearReplayBufferLookups();
    for(auto buffer:buffers)CHECK(buffer->Release()==0);
    CHECK(!liveBuffers && !g_bufferObjects);
    // Unknowns cannot become successful merely because a lookup was attempted.
    DriverCreateVB(device,16,0,0,D3DPOOL_DEFAULT,&vb,nullptr);
    BeginReplayBufferLookups();complete=true;CHECK(!ReplayBuffer(vb,complete) && !complete);
    complete=true;CHECK(!ReplayBuffer(vb,complete) && !complete);ClearReplayBufferLookups();vb->Release();
    puts("PASS replay metadata lookup: live validity/locks/alias rejection on hits, source pin/release, collisions, unknowns, idempotent teardown");
}
static void ReplayLookupResetCase(){
    auto vb=VB();
    {RecLockGuard guard;BeginReplayBufferLookups();bool complete=true;CHECK(ReplayBuffer(vb,complete));}
    ResetBufferReplicas();
    CHECK(!g_replayBufferLookupActive && reinterpret_cast<DriverBuffer*>(vb)->refs==1);
    {RecLockGuard guard;bool complete=true;CHECK(!ReplayBuffer(vb,complete) && !complete);}
    CHECK(vb->Release()==0 && !liveBuffers && !g_bufferObjects);
    puts("PASS Reset drains replay metadata pins before dropping replicas");
}
static void ReplayLookupBenchmark(){
    RecLockGuard guard;
    auto vb=VB();bool complete=true;constexpr unsigned frames=1000,binds=4000;
    for(bool cached:{false,true}){
        const auto before=privateQueries,allocBefore=allocations.load();
        const auto start=std::chrono::steady_clock::now();
        for(unsigned f=0;f<frames;++f){
            if(cached)BeginReplayBufferLookups();
            for(unsigned i=0;i<binds;++i)CHECK(ReplayBuffer(vb,complete));
            if(cached)ClearReplayBufferLookups();
        }
        CHECK(complete && allocations.load()==allocBefore);
        printf("BENCH actual buffer lookup %s %.3f ms/frame, %.0f private-data calls/frame, zero allocations; cache %zu bytes; driver/GPU excluded\n",
            cached?"memoized":"baseline",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/frames,
            double(privateQueries-before)/frames,sizeof(g_replayBufferLookups));
    }
    vb->Release();
}

static void ReenterRecorder(){CHECK(SUCCEEDED(Hook_SetIndices(device,nullptr)));}
static void WorkerRecorder(){std::thread worker(ReenterRecorder);worker.join();}
static void RaiseDriverFault(){RaiseException(0xe0d20001,0,0,nullptr);}
static bool FaultedUpload(BufferVersion* v,const void* data){
    __try{return UploadBufferRange(v,0,4,0,data);}
    __except(EXCEPTION_EXECUTE_HANDLER){d2vr_record_sync::RecoverReplayException();return false;}
}
static bool FaultedMetadata(IUnknown* source){
    __try{BufferInfo(source);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){d2vr_record_sync::RecoverReplayException();return false;}
}
static void SynchronizationFaultCases(){
    auto vb=VB();Write(vb,0,111,D3DLOCK_DISCARD);auto version=BufferInfo(vb);uint32_t data=222;
    callbackKind=1;driverCallback=RaiseDriverFault;CHECK(!FaultedUpload(version,&data));
    CHECK(!d2vr_record_sync::Busy() && !d2vr_record_sync::depth && !d2vr_record_sync::nativeDepth && !g_recLock.load());
    bool complete=true;CHECK(!ReplayBuffer(vb,complete) && !complete);ResetBufferReplicas();vb->Release();
    vb=VB();callbackKind=4;driverCallback=RaiseDriverFault;CHECK(!FaultedMetadata(vb));
    CHECK(!d2vr_record_sync::Busy() && !d2vr_record_sync::depth && !d2vr_record_sync::nativeDepth);
    Write(vb,0,333,D3DLOCK_DISCARD);Begin();Hook_SetStreamSource(device,0,vb,0,4);
    Hook_DrawPrimitive(device,D3DPT_POINTLIST,0,1);observed.clear();CHECK(Replay());CHECK(observed[0]==333);End();vb->Release();
    CHECK(!liveBuffers && !g_bufferObjects && !g_bufferReplicaBytes);
    puts("PASS native upload/metadata SEH: abandoned CPU/native leases recovered, unknown replica rejected, next frame works");
}
static IDirect3DVertexBuffer9* callbackBuffer=nullptr;
static void CallbackWrite(){Write(callbackBuffer,0,333,D3DLOCK_DISCARD);}
static void WorkerWrite(){std::thread worker(CallbackWrite);worker.join();}
static void CallbackWriteAgain(){Write(callbackBuffer,0,444,D3DLOCK_NOOVERWRITE);}
static void WriteAndArmDrain(){CallbackWrite();callbackKind=1;driverCallback=CallbackWriteAgain;}
static void CallbackReset(){ResetBufferReplicas();CHECK(g_recOverflow);CHECK(BufferInfo(callbackBuffer)->replica);}
static void SynchronizationProbe(const char* mode){
    auto vb=VB();callbackBuffer=vb;Write(vb,0,111,D3DLOCK_DISCARD);Begin();Hook_SetStreamSource(device,0,vb,0,4);
    const bool writes=strstr(mode,"write")!=nullptr;
    callbackKind=!strcmp(mode,"lock")?1:!strcmp(mode,"unlock")?2:3;
    driverCallback=!strcmp(mode,"worker")?WorkerRecorder:writes?CallbackWrite:!strcmp(mode,"reset")?CallbackReset:ReenterRecorder;
    if(!strcmp(mode,"worker-write"))driverCallback=WorkerWrite;
    if(!strcmp(mode,"drain-write"))driverCallback=WriteAndArmDrain;
    if(callbackKind==1 || callbackKind==2)Write(vb,0,222,D3DLOCK_DISCARD);
    else if(!strncmp(mode,"replay",6) || !strcmp(mode,"upload-write")){
        driverCallback=nullptr;if(!strcmp(mode,"upload-write"))Write(vb,0,222,0);
        Hook_DrawPrimitive(device,D3DPT_POINTLIST,0,1);
        const auto records=g_rec.size(),bytes=g_recData.size();
        if(!strcmp(mode,"upload-write"))callbackKind=1;
        driverCallback=writes?CallbackWrite:ReenterRecorder;
        CHECK(!Replay());CHECK(g_rec.size()==records && g_recData.size()==bytes);
    }else Hook_DrawPrimitive(device,D3DPT_POINTLIST,0,1);
    CHECK(g_recOverflow || d2vr_record_sync::conflict.load());CHECK(!Replay());
    End();CHECK(BufferInfo(vb)->valid);
    if(!strcmp(mode,"drain-write")){
        CHECK(DeferredBufferWritesPending());CHECK(Word(BufferInfo(vb)->replica,0)==333);
        End();CHECK(!DeferredBufferWritesPending());
    }
    const uint32_t expected=!strcmp(mode,"drain-write")?444:writes?333:(callbackKind==1 || callbackKind==2)?222:111;
    CHECK(Word(BufferInfo(vb)->replica,0)==expected);
    // A rejected ambiguous pair must not poison geometry or ownership next frame.
    Begin();Hook_SetStreamSource(device,0,vb,0,4);Hook_DrawPrimitive(device,D3DPT_POINTLIST,0,1);
    CHECK(observed.size()==1 && observed[0]==expected);observed.clear();CHECK(Replay());
    CHECK(observed.size()==1 && observed[0]==expected);End();vb->Release();
    CHECK(!liveBuffers && !g_bufferObjects && !g_bufferReplicaBytes);
    CHECK(!d2vr_record_sync::Busy() && !d2vr_record_sync::depth && !d2vr_record_sync::nativeDepth);
    std::printf("PASS %s: callback completes, ambiguous pair rejected, exact range baseline and next eye recover, no leaks\n",mode);
}
static void DeferredBenchmark(){
    auto vb=VB();callbackBuffer=vb;Write(vb,0,111,D3DLOCK_DISCARD);
    auto frame=[&]{Begin();Hook_SetStreamSource(device,0,vb,0,4);callbackKind=3;driverCallback=CallbackWrite;
        Hook_DrawPrimitive(device,D3DPT_POINTLIST,0,1);CHECK(!Replay());End();};
    frame();frame();const auto before=allocations.load();const auto start=std::chrono::steady_clock::now();
    for(unsigned f=0;f<1000;++f)frame();
    const auto added=allocations.load()-before;CHECK(added==0);
    const auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/1000;
    std::printf("BENCH callback-write recovery %.3f us/frame, steady allocations=%zu, retained range bytes=%zu (two bounded arenas)\n",
        us,added,g_deferredBufferData.capacity()+g_applyingBufferData.capacity());
    CHECK(!DeferredBufferWritesPending());vb->Release();CHECK(!liveBuffers && !g_bufferObjects);
}
int main(int argc,char** argv){
    static_assert(sizeof(void*)==4,"x86 packets");
    driverTable[0]=reinterpret_cast<void*>(&DriverQuery);driverTable[1]=reinterpret_cast<void*>(&DriverAddRef);driverTable[2]=reinterpret_cast<void*>(&DriverRelease);
    driverTable[4]=reinterpret_cast<void*>(&DriverSetPrivate);driverTable[5]=reinterpret_cast<void*>(&DriverGetPrivate);
    driverTable[11]=reinterpret_cast<void*>(&DriverLock);driverTable[12]=reinterpret_cast<void*>(&DriverUnlock);
    deviceTable[101]=reinterpret_cast<void*>(&DriverGetStream);deviceTable[105]=reinterpret_cast<void*>(&DriverGetIndices);
    g_createVB=&DriverCreateVB;g_createIB=&DriverCreateIB;
    g_rec.reserve(4096);g_recData.reserve(4u<<20);observed.reserve(16);
    if(argc==2 && !strcmp(argv[1],"--cpu-probe")){Benchmark();return 0;}
    if(argc==3 && !strcmp(argv[1],"--sync-probe")){SynchronizationProbe(argv[2]);return 0;}
    BufferCases();LifecycleCases();BoundaryCases();Benchmark();ReplayLookupCases();ReplayLookupResetCase();ReplayLookupBenchmark();DeferredBenchmark();SynchronizationFaultCases();CHECK(!liveBuffers && !g_bufferObjects);
    // Disabled startup path must not dereference the supplied device/resource,
    // scan inherited bindings, or mark writes applied. No driver object needed.
    g_replayBufferVersions=false;
    const auto beforeDisabledQueries=privateQueries;BeginReplayBufferLookups();CHECK(!g_replayBufferLookupActive);
    bool complete=true;auto* untouched=reinterpret_cast<IUnknown*>(0x1234);
    CHECK(ReplayBuffer(untouched,complete)==untouched && complete);
    CHECK(ReplayInheritedBuffers(nullptr));
    g_bufferFrameOpen=false;g_bufferWritesApplied=false;
    FinishBufferRecording();CloseBufferFrame();ResetBufferReplicas();
    CHECK(!g_bufferFrameOpen && !g_bufferWritesApplied && privateQueries==beforeDisabledQueries);
    std::puts("PASS disabled buffer path: original identity, no inherited scan or replica lifecycle");
}

#define NOMINMAX
#include "../src/stateblock_lifetime.h"
#include "../src/stateblock_replay_versions.h"
#include <cstdio>
#include <cstdlib>
#include <thread>
namespace life=d2vr_stateblock_lifetime;
#define CHECK(x) do{if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned beforeCalls=0,afterCalls=0;
static void Before(IDirect3DStateBlock9* self,life::Operation op) {
    ++beforeCalls;
    // A synchronous callback's worker can acquire the registry. Holding its
    // SRW lock across native code or an observer deadlocks this bounded test.
    HANDLE done=CreateEventW(nullptr,TRUE,FALSE,nullptr);CHECK(done);
    std::thread worker([=]{CHECK(life::Tracked()==1);CHECK(life::Lookup(self));SetEvent(done);});
    CHECK(WaitForSingleObject(done,2000)==WAIT_OBJECT_0);worker.join();CloseHandle(done);
    if(op==life::Operation::Capture)CHECK(!life::Observe(self).Current());
}
static void After(IDirect3DStateBlock9*,life::Operation,HRESULT result){CHECK(result==D3D_OK);++afterCalls;}
static unsigned escapes=0;static HRESULT observed=0;
static void Escaped(IDirect3DStateBlock9*){++escapes;}
static void Result(IDirect3DStateBlock9*,life::Operation,HRESULT result){observed=result;}
// Deliberate HRESULT/alias/capacity faults supplement the real HAL path above.
// No native D3D9 failure is inferred from a modelled object.
struct FaultBlock:IDirect3DStateBlock9 {
    ULONG refs=1;
    bool captureOnApply=false;
    void (*addCallback)(FaultBlock*)=nullptr;
    void (*releaseCallback)(FaultBlock*)=nullptr;
    void (*applyCallback)(FaultBlock*)=nullptr;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void** out)override{
        if(!out)return E_POINTER;
        *out=this;AddRef();return S_OK; // unknown successful IID must notify Escape
    }
    ULONG STDMETHODCALLTYPE AddRef()override{if(addCallback)addCallback(this);return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{if(releaseCallback)releaseCallback(this);const auto left=--refs;if(!left)delete this;return left;}
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9**)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Capture()override{return E_FAIL;}
    HRESULT STDMETHODCALLTYPE Apply()override{if(applyCallback)applyCallback(this);if(captureOnApply)life::Invoke(this,life::Operation::Capture);return S_FALSE;}
};
static void Faults(){
    auto block=new FaultBlock;
    CHECK(life::Install(block,{nullptr,Result,Escaped}));
    auto version=life::Observe(block);CHECK(version.Current());
    CHECK(block->Capture()==E_FAIL && observed==E_FAIL && !version.Current());
    version=life::Observe(block);CHECK(version.Current());
    CHECK(block->Apply()==S_FALSE && observed==S_FALSE && version.Current());
    GUID missing{0x73ce3311,0x6745,0x4a03,{1,2,3,4,5,6,7,8}};void* out=nullptr;
    CHECK(block->QueryInterface(missing,&out)==S_OK && out==block && escapes==1);
    CHECK(!version.Current() && !life::Observe(block).Current());
    static_cast<IDirect3DStateBlock9*>(out)->Release();
    CHECK(block->Release()==0 && !version.Current());
    std::array<FaultBlock*,life::kCapacity> full{};
    for(auto& entry:full){entry=new FaultBlock;CHECK(life::Install(entry));}
    CHECK(life::Tracked()==life::kCapacity);
    auto excess=new FaultBlock;const auto table=*reinterpret_cast<void***>(excess);
    CHECK(!life::Install(excess) && table==*reinterpret_cast<void***>(excess));
    CHECK(excess->Release()==0);
    for(auto entry:full)CHECK(entry->Release()==0);
    CHECK(life::Tracked()==0);
    puts("PASS modelled HRESULT/unknown-QI escape and 4096-object capacity fallback: original pointers/results unchanged, all records retired");
}

struct TestNativeScope {
    static inline unsigned entered=0,depth=0;
    TestNativeScope(){++entered;++depth;}
    ~TestNativeScope(){--depth;}
};
using ReplayDependency=life::ReplayVersions<TestNativeScope>;
static ReplayDependency* activeReplay=nullptr;
static void ReenterRetain(FaultBlock* block){CHECK(activeReplay->Retain(block)==ReplayDependency::Invalid);}
static void ReenterClear(FaultBlock*){activeReplay->Clear();}
static void ReplayFaults() {
    using Replay=life::ReplayVersions<TestNativeScope>;
    Replay replay(1);
    auto block=new FaultBlock;
    CHECK(replay.Retain(block)==Replay::Invalid && block->refs==1);replay.Clear();
    CHECK(life::Install(block));
    const auto index=replay.Retain(block);CHECK(index==0 && block->refs==2 && replay.Valid());
    for(unsigned i=0;i<1000;++i)CHECK(replay.Retain(block)==index && block->refs==2);
    CHECK(block->Release()==1); // only the replay dependency now owns this COM object
    auto result=replay.Apply(index);
    CHECK(result.executed && result.consistent && result.nativeResult==S_FALSE);
    block->captureOnApply=true;
    result=replay.Apply(index);
    CHECK(result.executed && !result.consistent && result.nativeResult==S_FALSE);
    CHECK(!replay.Valid() && !replay.Apply(index).executed);
    replay.Clear();CHECK(life::Tracked()==0 && replay.Valid());
    block=new FaultBlock;CHECK(life::Install(block));
    CHECK(replay.Retain(block)==0);
    CHECK(block->Capture()==E_FAIL); // even failed Capture invalidates the recorded dependency
    CHECK(!replay.Valid() && !replay.Apply(0).executed);
    CHECK(replay.Retain(block)==Replay::Invalid);
    replay.Clear();CHECK(block->refs==1 && replay.Valid());
    CHECK(replay.Retain(block)==0);
    auto excess=new FaultBlock;CHECK(life::Install(excess));
    CHECK(replay.Retain(excess)==Replay::Invalid && excess->refs==1);
    CHECK(!replay.Valid());replay.Clear();CHECK(block->refs==1);
    CHECK(block->Release()==0 && excess->Release()==0 && life::Tracked()==0);
    activeReplay=&replay;
    block=new FaultBlock;CHECK(life::Install(block));block->addCallback=ReenterRetain;
    CHECK(replay.Retain(block)==Replay::Invalid && block->refs==1 && !replay.Valid());
    block->addCallback=nullptr;replay.Clear();CHECK(replay.Retain(block)==0);
    block->applyCallback=ReenterClear;
    result=replay.Apply(0);CHECK(result.executed && !result.consistent && block->refs==2);
    block->applyCallback=nullptr;block->releaseCallback=ReenterRetain;
    replay.Clear();CHECK(replay.Valid() && !replay.Size() && block->refs==1);
    block->releaseCallback=nullptr;CHECK(block->Release()==0 && life::Tracked()==0);
    block=new FaultBlock;CHECK(life::Install(block));CHECK(replay.Retain(block)==0);
    GUID unknownIid{0x73ce3311,0x6745,0x4a03,{1,2,3,4,5,6,7,8}};void* escapedAlias=nullptr;
    CHECK(block->QueryInterface(unknownIid,&escapedAlias)==S_OK && escapedAlias==block);
    CHECK(!replay.Valid() && !replay.Apply(0).executed);
    static_cast<IDirect3DStateBlock9*>(escapedAlias)->Release();
    replay.Clear();CHECK(replay.Retain(block)==Replay::Invalid);replay.Clear();
    CHECK(block->Release()==0 && life::Tracked()==0);
    activeReplay=nullptr;
    CHECK(!TestNativeScope::depth && TestNativeScope::entered>0);
    puts("PASS replay versions: exact native HRESULT, one COM lease per identity, Capture-after-record rejection, reentrant Capture during Apply rejection, no execution after rejection, bounded capacity and release/reuse");
}

int main(int argc,char**){
    Faults();ReplayFaults();
    wchar_t path[MAX_PATH];CHECK(GetSystemDirectoryW(path,MAX_PATH));wcscat_s(path,L"\\d3d9.dll");
    HMODULE dll=LoadLibraryW(path);CHECK(dll);
    auto create=reinterpret_cast<HRESULT(WINAPI*)(UINT,IDirect3D9Ex**)>(GetProcAddress(dll,"Direct3DCreate9Ex"));CHECK(create);
    IDirect3D9Ex* api=nullptr;CHECK(SUCCEEDED(create(D3D_SDK_VERSION,&api)));
    HWND window=CreateWindowExW(0,L"STATIC",L"offline StateBlock ownership",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(window);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.BackBufferWidth=pp.BackBufferHeight=64;
    IDirect3DDevice9Ex* dev=nullptr;
    CHECK(SUCCEEDED(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,window,(argc>1?(D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_PUREDEVICE):D3DCREATE_SOFTWARE_VERTEXPROCESSING),&pp,nullptr,&dev)));
    printf("Native device flags: 0x%lX\n",DWORD(argc>1?(D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_PUREDEVICE):D3DCREATE_SOFTWARE_VERTEXPROCESSING));
    CHECK(!life::Install(nullptr));
    for(unsigned frame=0;frame<128;++frame){
        IDirect3DStateBlock9* block=nullptr;
        CHECK(SUCCEEDED(dev->BeginStateBlock()));
        CHECK(SUCCEEDED(dev->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE)));
        CHECK(SUCCEEDED(dev->EndStateBlock(&block)) && block);
        const auto identity=block;auto oldTable=*reinterpret_cast<void***>(block);
        CHECK(life::Install(block,{Before,After,nullptr}));
        CHECK(block==identity && *reinterpret_cast<void***>(block)!=oldTable);
        CHECK(life::Install(block));CHECK(life::Tracked()==1);
        auto version=life::Observe(block);CHECK(version.Current());
        IUnknown* unknown=nullptr;CHECK(SUCCEEDED(block->QueryInterface(__uuidof(IUnknown),reinterpret_cast<void**>(&unknown))));
        CHECK(unknown==static_cast<IUnknown*>(block));unknown->Release();
        IDirect3DStateBlock9* alias=nullptr;
        CHECK(SUCCEEDED(block->QueryInterface(__uuidof(IDirect3DStateBlock9),reinterpret_cast<void**>(&alias))));
        CHECK(alias==block);alias->Release();
        GUID missing{0x73ce3311,0x6745,0x4a03,{1,2,3,4,5,6,7,8}};void* output=nullptr;
        CHECK(block->QueryInterface(missing,&output)==E_NOINTERFACE && !output);
        IDirect3DDevice9* owner=nullptr;CHECK(SUCCEEDED(block->GetDevice(&owner)) && owner==dev);owner->Release();
        CHECK(SUCCEEDED(dev->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE)));
        CHECK(SUCCEEDED(block->Apply()));CHECK(version.Current());
        ReplayDependency replay;const auto replayIndex=replay.Retain(block);CHECK(replayIndex==0 && replay.Valid());
        DWORD value=1;CHECK(SUCCEEDED(dev->GetRenderState(D3DRS_ALPHABLENDENABLE,&value)) && value==FALSE);
        CHECK(SUCCEEDED(dev->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE)));
        CHECK(SUCCEEDED(block->Capture()));CHECK(!version.Current());version=life::Observe(block);CHECK(version.Current());
        CHECK(!replay.Valid() && !replay.Apply(replayIndex).executed);replay.Clear();CHECK(replay.Retain(block)==0);
        CHECK(SUCCEEDED(dev->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE)));
        CHECK(SUCCEEDED(block->Apply()));CHECK(SUCCEEDED(dev->GetRenderState(D3DRS_ALPHABLENDENABLE,&value)) && value==TRUE);
        // A render-state-only block can outlive ResetEx: no registry clearing.
        if(frame%32==0){CHECK(SUCCEEDED(dev->ResetEx(&pp,nullptr)));CHECK(version.Current());CHECK(SUCCEEDED(block->Apply()));}
        auto replayed=replay.Apply(0);CHECK(replayed.executed && replayed.consistent && replayed.nativeResult==D3D_OK);
        CHECK(SUCCEEDED(dev->GetRenderState(D3DRS_ALPHABLENDENABLE,&value)) && value==TRUE);
        replay.Clear();
        block->AddRef();CHECK(block->Release()!=0);CHECK(version.Current());
        CHECK(block->Release()==0);CHECK(!version.Current() && life::Tracked()==0);
        CHECK(!life::Observe(identity).Current());
    }
    CHECK(beforeCalls==afterCalls && beforeCalls==388);
    dev->Release();api->Release();DestroyWindow(window);FreeLibrary(dll);
    puts("PASS actual x86 HAL/Ex StateBlock lifetime: native COM identity/HRESULT, 128 lifetimes, native replay dependency leases, Capture-after-record rejection, Apply state, four ResetEx survivals, bounded callback worker, no renderer admission or FPS claim");
}

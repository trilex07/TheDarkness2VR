#include "../src/native_submission_cache.h"
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <vector>
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %u line %d: %s\n",checks,__LINE__,#x);exit(1);}}while(0)
static unsigned checks=0,attempts=0,failAt=0,syncs=0;
static HRESULT success=S_OK;
static std::vector<unsigned> order;
static HRESULT Receipt(unsigned op){order.push_back(op);return ++attempts==failAt?D3DERR_DEVICELOST:success;}
struct FakeSurface{unsigned refs=1;void AddRef(){++refs;}void Release(){CHECK(refs>1);--refs;}};
struct FakeState{unsigned refs=1;void Release(){CHECK(refs>1);--refs;}
    HRESULT Capture(){return Receipt(1);}HRESULT Apply(){return Receipt(2);}};
struct FakeDevice{
    FakeState state;FakeSurface rt[4],depth;unsigned bound=4;bool haveDepth=true;
    HRESULT GetDeviceCaps(D3DCAPS9* caps){caps->NumSimultaneousRTs=4;caps->MaxVertexShaderConst=256;caps->PixelShaderVersion=D3DPS_VERSION(3,0);return Receipt(3);}
    HRESULT CreateStateBlock(D3DSTATEBLOCKTYPE type,FakeState** out){CHECK(type==D3DSBT_ALL);auto hr=Receipt(4);if(SUCCEEDED(hr)){++state.refs;*out=&state;}return hr;}
    HRESULT GetRenderTarget(DWORD slot,FakeSurface** out){CHECK(slot<4);auto hr=Receipt(10+slot);if(FAILED(hr))return hr;if(slot>=bound)return D3DERR_NOTFOUND;rt[slot].AddRef();*out=&rt[slot];return hr;}
    HRESULT GetDepthStencilSurface(FakeSurface** out){auto hr=Receipt(14);if(FAILED(hr))return hr;if(!haveDepth)return D3DERR_NOTFOUND;depth.AddRef();*out=&depth;return hr;}
    HRESULT SetRenderTarget(DWORD slot,FakeSurface* p){CHECK(slot<4 && p==(slot<bound?&rt[slot]:nullptr));return Receipt(20+slot);}
    HRESULT SetDepthStencilSurface(FakeSurface* p){CHECK(p==(haveDepth?&depth:nullptr));return Receipt(24);}
};
using ApplyFn=HRESULT(*)(FakeState*);
static HRESULT OriginalApply(FakeState* state){return state->Apply();}
static ApplyFn g_origSBApply=nullptr;
static bool syncResult=true;
static bool SyncActiveShader(FakeDevice*){++syncs;return syncResult;}
#include "actual_snapshot_cpu.inl"
template<class Snapshot> static bool ApplyResult(Snapshot& snapshot,FakeDevice* device){
    if constexpr(std::is_void_v<decltype(snapshot.Apply(device))>){snapshot.Apply(device);return true;}
    else return snapshot.Apply(device);
}
static void Reset(){attempts=failAt=syncs=0;order.clear();}
static void Released(const FakeDevice& dev){CHECK(dev.state.refs==1 && dev.depth.refs==1);for(const auto& rt:dev.rt)CHECK(rt.refs==1);}
int main(){
    for(unsigned optional=0;optional<2;++optional)for(unsigned status=0;status<2;++status)
    for(unsigned useOriginal=0;useOriginal<2;++useOriginal)for(unsigned failure=0;failure<=6;++failure){
        FakeDevice dev;dev.bound=optional?1:4;dev.haveDepth=!optional;
        success=status?S_FALSE:S_OK;g_origSBApply=useOriginal?OriginalApply:nullptr;
        DeviceSnapshot snapshot;Reset();CHECK(snapshot.Capture(&dev));Reset();failAt=failure;
        CHECK(ApplyResult(snapshot,&dev)==(failure==0));
        CHECK((order==std::vector<unsigned>{20,21,22,23,24,2}));CHECK(syncs==1);
        snapshot.Release();Released(dev);
    }
    for(unsigned failure=1;failure<=7;++failure){
        FakeDevice dev;DeviceSnapshot snapshot;Reset();failAt=failure;
        CHECK(!snapshot.Capture(&dev));CHECK(!snapshot.state && !snapshot.owner && !snapshot.targets[0]);Released(dev);
    }
    {FakeDevice dev,other;DeviceSnapshot snapshot;Reset();CHECK(snapshot.Capture(&dev));Reset();
        CHECK(!ApplyResult(snapshot,&other) && attempts==0);snapshot.Release();Released(dev);}
    {FakeDevice dev;DeviceSnapshot snapshot;Reset();CHECK(!ApplyResult(snapshot,&dev) && attempts==0);}
    {FakeDevice dev;DeviceSnapshot snapshot;Reset();CHECK(snapshot.Capture(&dev));Reset();
        syncResult=false;CHECK(!ApplyResult(snapshot,&dev));
        CHECK(syncs==1 && attempts==6);syncResult=true;snapshot.Release();Released(dev);}
    // A failed reusable Capture retries with a fresh state block. Repeated
    // successful captures keep one owned reference and release every surface.
    {FakeDevice dev;DeviceSnapshot snapshot;Reset();CHECK(snapshot.Capture(&dev,true));
        Reset();failAt=1;CHECK(snapshot.Capture(&dev,true));
        CHECK((order==std::vector<unsigned>{1,3,4,10,11,12,13,14}));
        for(unsigned i=0;i<100;++i){Reset();CHECK(snapshot.Capture(&dev,true));
            CHECK((order==std::vector<unsigned>{1,10,11,12,13,14}));}
        snapshot.Release();Released(dev);}
    printf("PASS actual snapshot CPU receipts: %u checks, all6 restore failures, S_OK/S_FALSE, optional RT/depth, both apply routes, all7 fresh capture failures, ownership/ref balance; no GPU/game run\n",checks);
}

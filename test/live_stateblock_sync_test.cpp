#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../src/shader_register_history.h"
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %u line%d: %s\n",checks,__LINE__,#x);exit(1);}}while(0)
struct FakeShader { unsigned refs=1; void Release(){CHECK(refs>1);--refs;} };
static const void* activeVs=nullptr;
static const void* activePs=nullptr;
static void SetActiveVertexShaderMetadata(const void* p){activeVs=p;}
static void SetActivePixelShaderMetadata(const void* p){activePs=p;}
static std::vector<unsigned> order;
static unsigned lockDepth=0;
struct FakeDevice {
    unsigned refs=1;
    FakeShader *vs=nullptr,*ps=nullptr;
    HRESULT vsResult=S_OK,psResult=S_OK;
    HRESULT GetVertexShader(FakeShader** out){CHECK(!lockDepth);order.push_back(3);if(SUCCEEDED(vsResult)){*out=vs;if(vs)++vs->refs;}return vsResult;}
    HRESULT GetPixelShader(FakeShader** out){CHECK(!lockDepth);order.push_back(4);if(SUCCEEDED(psResult)){*out=ps;if(ps)++ps->refs;}return psResult;}
    void Release(){order.push_back(5);CHECK(refs>1);--refs;}
};
struct FakeBlock {
    FakeDevice* device=nullptr;
    FakeShader *vs=nullptr,*ps=nullptr;
    HRESULT applyResult=S_OK,deviceResult=S_OK;
    bool returnDevice=true;
    HRESULT GetDevice(FakeDevice** out){CHECK(!lockDepth);order.push_back(2);if(returnDevice){*out=device;++device->refs;}return deviceResult;}
};
static unsigned applies=0,invalidations=0,records=0;
static bool g_recActive=false,g_recOverflow=false;
static d2vr_shader_registers::History g_vsConstantHistory;
static void InvalidateShadowAtlasPass(){++invalidations;}
struct RecLockGuard{RecLockGuard(){++lockDepth;}~RecLockGuard(){--lockDepth;}};
enum {OP_ApplyStateBlock=50};
static void RecEmit(unsigned op,unsigned n,uint32_t pointer){
    if(!g_recActive || g_recOverflow)return;
    CHECK(op==OP_ApplyStateBlock && n==0 && pointer);++records;order.push_back(6);
}
static HRESULT STDMETHODCALLTYPE Apply(FakeBlock* b){
    CHECK(!lockDepth);
    ++applies;order.push_back(1);
    if(SUCCEEDED(b->applyResult)){b->device->vs=b->vs;b->device->ps=b->ps;}
    return b->applyResult;
}
static auto g_origSBApply=&Apply;
#include "actual_stateblock_sync.inl"
static void Reset(FakeDevice& d,FakeShader& a,FakeShader& b,bool recording){
    d={};d.vs=&a;d.ps=&a;activeVs=&a;activePs=&a;
    order.clear();applies=invalidations=records=0;g_recOverflow=false;g_recActive=recording;
    g_vsConstantHistory.Clear();float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    CHECK(g_vsConstantHistory.live.Write(0,4,matrix,sizeof(matrix)));
    CHECK(a.refs==1 && b.refs==1 && !lockDepth);g_origSBApply=&Apply;
}
int main(){
    FakeDevice d;FakeShader a,b;
    for(unsigned frame=0;frame<1000;++frame)for(bool recording:{false,true})
    for(HRESULT result:{S_OK,S_FALSE})for(unsigned nullBinding=0;nullBinding<4;++nullBinding){
        Reset(d,a,b,recording);
        FakeBlock block{&d,(nullBinding&1)?nullptr:&b,(nullBinding&2)?nullptr:&b,result};
        CHECK(Hook_SBApply(&block)==result);
        CHECK(applies==1 && activeVs==d.vs && activePs==d.ps);
        CHECK(d.refs==1 && a.refs==1 && b.refs==1 && !g_recOverflow);
        CHECK(records==unsigned(recording) && invalidations==1);
        float matrix[16];CHECK(!g_vsConstantHistory.live.Matrix(0,matrix));
        auto expected=std::vector<unsigned>{1,2,3,4,5};if(recording)expected.push_back(6);
        CHECK(order==expected);
    }
    for(HRESULT result:{D3DERR_INVALIDCALL,D3DERR_DEVICELOST}){
        Reset(d,a,b,true);FakeBlock block{&d,&b,&b,result};
        CHECK(Hook_SBApply(&block)==result && applies==1 && records==0);
        CHECK(activeVs==&a && activePs==&a && order==std::vector<unsigned>{1});
        float matrix[16];CHECK(g_vsConstantHistory.live.Matrix(0,matrix));
    }
    for(bool recording:{false,true})for(bool pointer:{false,true})for(HRESULT result:{S_OK,D3DERR_INVALIDCALL}){
        if(pointer && SUCCEEDED(result))continue;
        Reset(d,a,b,recording);FakeBlock block{&d,&b,&b,S_OK,result,pointer};
        CHECK(Hook_SBApply(&block)==S_OK && applies==1);
        CHECK(!activeVs && !activePs && d.refs==1 && g_recOverflow==recording);
        CHECK(records==0);
    }
    Reset(d,a,b,true);g_origSBApply=nullptr;FakeBlock block{&d,&b,&b};
    CHECK(Hook_SBApply(&block)==D3DERR_INVALIDCALL && applies==0 && records==0 && order.empty());
    // A failed shader query cannot leave the previous shader's interpretation.
    for(bool recording:{false,true})for(unsigned failure=1;failure<=3;++failure){
        Reset(d,a,b,recording);d.vsResult=(failure&1)?D3DERR_INVALIDCALL:S_OK;d.psResult=(failure&2)?D3DERR_INVALIDCALL:S_OK;
        CHECK(Hook_SBApply(&block)==S_OK);
        CHECK(activeVs==((failure&1)?nullptr:&b) && activePs==((failure&2)?nullptr:&b));
        CHECK(g_recOverflow==recording && records==0);
        CHECK(d.refs==1 && a.refs==1 && b.refs==1);
    }
    printf("PASS actual live stateblock+shader sync: %u checks; 16000 successful transitions, failed Apply/GetDevice/query, null shaders, raw-bank invalidation, original receipt, exact order and references; CPU fixture\n",checks);
}

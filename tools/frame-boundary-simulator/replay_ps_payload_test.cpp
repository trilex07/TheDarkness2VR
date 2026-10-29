#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cassert>
#include <cstdio>
#include "../../src/replay_constant_batch.h"
#include "../../src/replay_constant_window.h"
#include "../../src/shader_register_history.h"
static d2vr_shader_registers::EyeHistory g_psEyeHistory;
static_assert(sizeof(void*)==4,"production x86");
enum {OP_SetPSConstF=27};
struct Rec {uint32_t op,n,a[6];};
struct Params {float transX=0,transY=0;};
static unsigned copies=0,apiCalls=0;static bool compensation=false;
static const float* forwarded=nullptr;static std::vector<float> submitted;
static unsigned forwardedStart,forwardedCount;
static void* CountCopy(void* dst,const void* src,size_t n){++copies;return memcpy(dst,src,n);}
static bool CompensatePsConstants(float* dst,const float* src,UINT,UINT n,int,int,const Params&){
    if(!compensation || !n || n>224)return false;
    memcpy(dst,src,n*16);dst[n*4-1]+=10.f;return true;
}
static HRESULT Send(void*,UINT s,const float* p,UINT n){
    ++apiCalls;forwarded=p;forwardedStart=s;forwardedCount=n;
    if(n<=225)submitted.assign(p,p+n*4);else submitted.clear();return S_OK;
}
struct {HRESULT(*SetPixelShaderConstantF)(void*,UINT,const float*,UINT)=Send;} g_orig;
static int g_activePsInverse=-1,g_activePsViewExtents=-1;
static bool g_lightPosShift=false;
static const uint32_t kShadowLightPsHash=0xC4D1FB10u;
static uint32_t g_activePsHash=0;
#ifdef D2VR_MATCHED_CAPTURE
static void MatchedProjectionObserved(const float*,const float*,UINT,UINT,int,HRESULT) {}
#endif
static void Actual(const Rec& r,const uint8_t* dataBase,const Params& sp,bool batchEnabled,bool windowEnabled){
    void* dev=nullptr;
    d2vr_replay_batch::Constants constants(batchEnabled,256,224);
    d2vr_replay_batch::UploadCounts uploadCounts;
    [[maybe_unused]] const bool auditUploads=true;
    d2vr_replay_batch::ConstantWindow constantWindow;
    if(windowEnabled) constantWindow.Reset(256,224);
    auto sendConstants=[&](d2vr_replay_batch::Stage stage,UINT s,const float* p,UINT n){
        return uploadCounts.Forward(stage,s,p,n,[dev](auto,UINT a,const float* b,UINT c){return Send(dev,a,b,c);});
    };
#define memcpy CountCopy
    switch(r.op){
#include "actual_replay_ps_case.inl"
    default:assert(false);}
#undef memcpy
    constants.Flush(sendConstants);
    if(windowEnabled) constantWindow.Flush(sendConstants);
    assert(uploadCounts.calls[1]==1 && uploadCounts.vectors[1]==r.a[1]);
}
int main(){
    unsigned cases=0;
    for(bool window:{false,true})for(bool batched:{false,true})for(bool compensated:{false,true})for(bool light:{false,true})
    for(uint32_t hash:{0u,kShadowLightPsHash})for(UINT start:{0u,1u,0xffffffffu})
    for(UINT count:{0u,1u,17u,224u,225u,0xffffffffu}) {
        std::vector<float> values(225*4);for(unsigned i=0;i<values.size();++i)values[i]=float(i)+.25f;
        const auto original=values;Params sp;sp.transX=-.032f;sp.transY=.012f;
        compensation=compensated;g_lightPosShift=light;g_activePsHash=hash;
        copies=apiCalls=0;Rec rec{};rec.op=OP_SetPSConstF;rec.a[0]=start;rec.a[1]=count;
        Actual(rec,reinterpret_cast<const uint8_t*>(values.data()),sp,batched,window && batched);
        const bool changed=light && hash==kShadowLightPsHash && start==0 && count>0 && count<=224;
        const bool hasComp=compensated && count>0 && count<=224;
        assert(values==original && apiCalls==1 && forwardedStart==start && forwardedCount==count);
        assert(copies==unsigned(changed && !hasComp));
        if(count<=225){auto expected=values;expected.resize(count*4);
            if(hasComp)expected.back()+=10.f;
            if(changed){expected[0]+=sp.transX;expected[1]+=sp.transY;}
            assert(submitted==expected);
        }
        if(!batched && !changed && !hasComp)assert(forwarded==values.data());
        ++cases;
    }
    printf("PASS actual OP_SetPSConstF: %u cases; zero copies for unchanged payloads, exact light/projection output, original invalid arguments, 224-vector bound, batching on/off\n",cases);
}

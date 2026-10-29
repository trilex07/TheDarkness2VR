#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <array>
#include <vector>
#include <unordered_map>
#include <type_traits>
#include <cstdio>
#include <cassert>
#include "../../src/replay_constant_window.h"
#include "../../src/shader_register_history.h"
using namespace d2vr_replay_batch;
static_assert(sizeof(void*)==4,"production ABI");
static_assert(std::is_trivially_destructible<ConstantWindow>::value,"SEH caller");
#include "actual_record_ops.inl"
struct Rec {unsigned op=0,n=0,a[6]{};};
struct Params {float transX=0,transY=0;bool projectionSnapshot=false;float projectionPx=0,projectionPy=0;};
using StereoParams=Params;
struct Banks {std::array<uint32_t,256*4> vs{},ps{};};
static Banks driver;
static std::vector<Banks> expected;
static std::vector<Rec> g_rec;
#include "actual_constant_binding_helpers.inl"
static std::vector<uint8_t> arena;
static unsigned checkAt=0,checks=0,apiCalls=0,apiFail=0,throwAt=0;
static unsigned lastVsFirst=0,lastVsCount=0;
static unsigned rejectAt=0;
static bool g_replayConstantBatch=false,g_replayConstantWindow=false,capture=false,compensate=false;
static bool g_replayConstantBindWindow=false;
static bool g_recOverflow=false;
static d2vr_shader_registers::History g_vsConstantHistory;
static struct {unsigned vertexFloatLimit=256,pixelFloatLimit=224;} g_frameStart;
[[maybe_unused]] static bool MatchedCaptureRecordingActive(){return capture;}
namespace d2vr_cpu_sync {static struct {bool enabled=true;} timing;}
static struct {
    UploadCounts counts;unsigned streams=0,otherEyes=0,batched=0,windowed=0,bindWindowed=0,captureBypass=0;
} g_constantUploadWindow;
static bool g_skipLightVsShift=false;
static struct Vs {uint32_t hash=0;} vertexMetadata;
using VsInfo=Vs;
static Vs* g_activeVs=&vertexMetadata;
static bool bindingTransforms=false,collectReference=false,checkBindingSnapshots=false;
static std::vector<std::array<const void*,2>> expectedBindings;
static std::array<uint32_t,10> otherState{};
static std::vector<std::array<uint32_t,10>> expectedState;
static std::vector<std::vector<uint32_t>> stateOrder;
static unsigned stateCalls=0,stateFailAt=0;
static bool checkOtherState=false;
static HRESULT State(unsigned slot,std::initializer_list<uint32_t> args,const void* payload=nullptr,unsigned bytes=0){
    std::vector<uint32_t> call{slot};call.insert(call.end(),args.begin(),args.end());
    const auto* p=static_cast<const unsigned char*>(payload);
    uint32_t hash=2166136261u;
    for(auto word:call){hash^=word;hash*=16777619u;}
    for(unsigned i=0;i<bytes;++i){call.push_back(p[i]);hash^=p[i];hash*=16777619u;}
    stateOrder.push_back(call);
    if(++stateCalls==stateFailAt)return D3DERR_INVALIDCALL;
    otherState[slot]=hash;return S_OK;
}
static HRESULT Stream(void*,UINT s,IDirect3DVertexBuffer9* p,UINT off,UINT stride){return State(0,{s,uint32_t(uintptr_t(p)),off,stride});}
static HRESULT Frequency(void*,UINT s,UINT f){return State(1,{s,f});}
static HRESULT Indices(void*,IDirect3DIndexBuffer9* p){return State(2,{uint32_t(uintptr_t(p))});}
static HRESULT Declaration(void*,IDirect3DVertexDeclaration9* p){return State(3,{uint32_t(uintptr_t(p))});}
static HRESULT Viewport(void*,const D3DVIEWPORT9* p){return State(4,{},p,sizeof(*p));}
static HRESULT Scissor(void*,const RECT* p){return State(5,{p?1u:0u},p,p?sizeof(*p):0);}
static HRESULT VSI(void*,UINT first,const int* p,UINT n){return State(6,{first,n},p,n*16);}
static HRESULT VSB(void*,UINT first,const BOOL* p,UINT n){return State(7,{first,n},p,n*4);}
static HRESULT PSI(void*,UINT first,const int* p,UINT n){return State(8,{first,n},p,n*16);}
static HRESULT PSB(void*,UINT first,const BOOL* p,UINT n){return State(9,{first,n},p,n*4);}
static void InvalidateShadowAtlasPass(){}
static int activeWvpBase=-1;
static int ActiveWvpRegister(Vs*){return activeWvpBase;}
static bool IsShadowAtlasPass(void*){return false;}
static bool ApplyShiftToUpload(float* dst,const float* src,UINT,UINT n,Vs* vi,const Params&,
                              const d2vr_shader_registers::Reference* =nullptr,bool* =nullptr){
    if(!n || n>72)return false;
    memcpy(dst,src,size_t(n)*16);uint32_t bits;memcpy(&bits,dst,4);
    bits^=0x10000;if(bindingTransforms){bits^=vi?vi->hash:0x1234;bits^=otherState[4];}
    memcpy(dst,&bits,4);return true;
}
#include "actual_partial_wvp.inl"
static int g_activePsInverse=-1,g_activePsViewExtents=-1;
static bool g_lightPosShift=false;
static const void* g_replayPs=nullptr;
static const void* g_replayVs=nullptr;
static uint32_t g_activePsHash=0;
static unsigned bindCalls=0,bindFailAt=0;
static std::vector<uintptr_t> bindingOrder;
static HRESULT Bind(void*,void* p){
    ++bindCalls;bindingOrder.push_back(reinterpret_cast<uintptr_t>(p));
    return bindCalls==bindFailAt?D3DERR_INVALIDCALL:S_FALSE;
}
static void SetActiveVertexShaderMetadata(IDirect3DVertexShader9* p){
    vertexMetadata.hash=uint32_t(reinterpret_cast<uintptr_t>(p));g_activeVs=p?&vertexMetadata:nullptr;
}
static void SetActivePixelShaderMetadata(const void* p){g_activePsInverse=int(reinterpret_cast<uintptr_t>(p));}
static const uint32_t kShadowLightPsHash=0xC4D1FB10u;
static std::unordered_map<const void*,uint32_t> g_psHashByPtr;
static bool CompensatePsConstants(float* dst,const float* src,UINT,UINT n,int inverse,int,const Params&){
    if(!compensate || !n || n>224)return false;
    memcpy(dst,src,size_t(n)*16);uint32_t bits;memcpy(&bits,dst,4);bits^=0x20000;
    if(bindingTransforms)bits^=uint32_t(inverse);memcpy(dst,&bits,4);return true;
}
[[maybe_unused]] static void MatchedProjectionObserved(const float*,const float*,UINT,UINT,int,HRESULT){}
static void Log(const char*,...){}
#define HOOK_MARK(value) ((void)0)
static HRESULT Submit(bool vertex,UINT first,const float* values,UINT count){
    ++apiCalls;
    if(vertex){lastVsFirst=first;lastVsCount=count;}
    if(throwAt && apiCalls==throwAt)RaiseException(0xe0420011,0,0,nullptr);
    if(rejectAt && apiCalls==rejectAt){++apiFail;return D3DERR_INVALIDCALL;}
    const unsigned limit=vertex?256:224;
    if(!values || !count || first>=limit || count>limit-first){++apiFail;return D3DERR_INVALIDCALL;}
    auto& bank=vertex?driver.vs:driver.ps;
    memcpy(bank.data()+first*4,values,size_t(count)*16);return S_OK;
}
static HRESULT VS(void*,UINT first,const float* data,UINT n){return Submit(true,first,data,n);}
static HRESULT PS(void*,UINT first,const float* data,UINT n){return Submit(false,first,data,n);}
static struct {decltype(&VS) SetVertexShaderConstantF=VS;decltype(&PS) SetPixelShaderConstantF=PS;
    decltype(&Bind) SetVertexShader=Bind;decltype(&Bind) SetPixelShader=Bind;
    decltype(&Stream) SetStreamSource=Stream;decltype(&Frequency) SetStreamSourceFreq=Frequency;
    decltype(&Indices) SetIndices=Indices;decltype(&Declaration) SetVertexDeclaration=Declaration;
    decltype(&Viewport) SetViewport=Viewport;decltype(&Scissor) SetScissorRect=Scissor;
    decltype(&VSI) SetVertexShaderConstantI=VSI;decltype(&VSB) SetVertexShaderConstantB=VSB;
    decltype(&PSI) SetPixelShaderConstantI=PSI;decltype(&PSB) SetPixelShaderConstantB=PSB;} g_orig;
static void CheckBanks(){
    if(collectReference){expected.push_back(driver);expectedBindings.push_back({g_replayVs,g_replayPs});expectedState.push_back(otherState);++checkAt;++checks;return;}
    assert(checkAt<expected.size());
    if(checkBindingSnapshots)assert((expectedBindings[checkAt]==std::array<const void*,2>{g_replayVs,g_replayPs}));
    if(checkOtherState)assert(otherState==expectedState[checkAt]);
    assert(driver.vs==expected[checkAt].vs && driver.ps==expected[checkAt].ps);++checkAt;++checks;
}
static bool Actual(){
    void* dev=nullptr;const int eye=1;const bool dump=false;const Params sp;
    const uint8_t* dataBase=arena.data();
    g_vsConstantHistory.BeginReplay();
#include "actual_replay_float_loop.inl"
    return replayComplete;
}
static uint32_t rng=0xa18478ed;
static uint32_t Random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void Build(){
    g_rec.clear();arena.clear();expected.clear();Banks ref;
    d2vr_shader_registers::Bank nativeRegisters;
    for(unsigned i=0;i<6000;++i){
        Rec r;
        if(i%7==6){r.op=1+Random()%18;if(r.op==OP_SetVSConstF || r.op==OP_SetPSConstF)r.op=36;
            if(!IsConstantMaterialBinding(r.op))expected.push_back(ref);g_rec.push_back(r);continue;}
        r.op=(Random()%3)?OP_SetVSConstF:OP_SetPSConstF;
        r.a[0]=Random()%256;r.a[1]=Random()%69;
        if(i%19==0)r.a[0]=0; // projection/partial ranges and duplicated overlaps
        const unsigned bytes=r.a[1]*16;r.a[4]=unsigned(arena.size());arena.resize(arena.size()+bytes);
        for(unsigned b=0;b<bytes;b+=4){const auto bits=Random();memcpy(arena.data()+r.a[4]+b,&bits,4);}
        if(r.op==OP_SetVSConstF)nativeRegisters.Write(r.a[0],r.a[1],arena.data()+r.a[4],bytes);
        const unsigned limit=r.op==OP_SetVSConstF?256:224;
        if(r.a[1] && r.a[0]<limit && r.a[1]<=limit-r.a[0]){
            float tmp[kVertexStereoUploadRows*4];const float* data=reinterpret_cast<const float*>(arena.data()+r.a[4]);
            UINT first=r.a[0],count=r.a[1];
            if(r.op==OP_SetVSConstF){
                const d2vr_shader_registers::Reference reference(nativeRegisters,ActiveWvpRegister(g_activeVs));
                if(ApplyVertexStereoUpload(tmp,data,first,count,g_activeVs,Params{},&reference))data=tmp;
            }
            else if(CompensatePsConstants(tmp,data,r.a[0],r.a[1],-1,-1,Params{}))data=tmp;
            auto& bank=r.op==OP_SetVSConstF?ref.vs:ref.ps;
            memcpy(bank.data()+first*4,data,size_t(count)*16);
        }
        g_rec.push_back(r);
    }
    Rec end;end.op=36;g_rec.push_back(end);expected.push_back(ref);
}
static void HeaderCases(){
    ConstantWindow window;window.Reset(256,224);Banks wanted;
    auto send=[](Stage stage,unsigned first,const float* data,unsigned count){return Submit(stage==Stage::Vertex,first,data,count);};
    driver={};apiCalls=apiFail=0;
    for(unsigned i=0;i<10000;++i){
        if(i%9==8){window.Flush(send);assert(driver.vs==wanted.vs && driver.ps==wanted.ps);++checks;continue;}
        const Stage stage=(i%2)?Stage::Pixel:Stage::Vertex;
        const unsigned limit=stage==Stage::Vertex?256:224;
        const unsigned first=Random()%limit,n=1+Random()%(limit-first);
        std::array<uint32_t,256*4> bits;for(auto& b:bits)b=Random();
        auto& bank=stage==Stage::Vertex?wanted.vs:wanted.ps;
        memcpy(bank.data()+first*4,bits.data(),size_t(n)*16);
        window.Upload(stage,first,reinterpret_cast<float*>(bits.data()),n,send);
        bits.fill(0xdeadbeef); // no source-pointer retention
    }
    window.Flush(send);assert(driver.vs==wanted.vs && driver.ps==wanted.ps);++checks;
    window.Reset(256,224);driver={};apiCalls=0;float data[16]{};
    UploadCounts receipt;auto counted=[&](Stage stage,unsigned first,const float* p,unsigned n){return receipt.Forward(stage,first,p,n,send);};
    window.Upload(Stage::Vertex,0,data,4,counted);window.Upload(Stage::Pixel,0,data,4,counted);
    assert(apiCalls==0);window.Flush(counted);window.Flush(counted);assert(apiCalls==2);
    assert(receipt.calls[0]==1 && receipt.calls[1]==1 && receipt.vectors[0]==4);
    window.Reset(256,224);window.Upload(Stage::Vertex,0,data,4,counted);window.Abort();window.Flush(counted);assert(apiCalls==2);
    // Real driver failure consumes the dirty range once, drains other stage,
    // disables future deferred writes. Input arguments of a later invalid call remain intact.
    window.Reset(256,224);unsigned sent=0;auto fail=[&](Stage,unsigned,const float*,unsigned){++sent;return long(D3DERR_DEVICELOST);};
    window.Upload(Stage::Vertex,0,data,4,fail);window.Upload(Stage::Pixel,0,data,4,fail);
    window.Flush(fail);window.Flush(fail);assert(sent==2);
    window.Upload(Stage::Vertex,4,data,4,fail);assert(sent==3);++checks;
}
static void ActualReduction(){
    capture=false;compensate=false;
    g_frameStart.vertexFloatLimit=256;g_frameStart.pixelFloatLimit=224;
    unsigned counts[3]{};
    for(unsigned mode=0;mode<3;++mode){
        g_rec.clear();arena.assign(4*64,0);expected.clear();
        Banks final;
        for(unsigned i=0;i<4;++i){
            Rec r;r.op=i%2?OP_SetPSConstF:OP_SetVSConstF;
            r.a[0]=i<2?0:4;r.a[1]=4;r.a[4]=i*64;
            float tmp[16]{};ApplyShiftToUpload(tmp,reinterpret_cast<const float*>(arena.data()+r.a[4]),r.a[0],4,g_activeVs,Params{});
            auto& bank=i%2?final.ps:final.vs;
            memcpy(bank.data()+r.a[0]*4,i%2?arena.data()+r.a[4]:reinterpret_cast<const uint8_t*>(tmp),64);
            g_rec.push_back(r);
        }
        Rec barrier;barrier.op=36;g_rec.push_back(barrier);expected.push_back(final);
        driver={};g_vsConstantHistory.Clear();apiCalls=apiFail=checkAt=0;g_constantUploadWindow={};
        g_replayConstantBatch=mode!=0;g_replayConstantWindow=mode==2;Actual();
        counts[mode]=apiCalls;assert(checkAt==1 && !apiFail);
    }
    assert(counts[0]==4 && counts[1]==4 && counts[2]==2);++checks;
}
static void BindingReduction(){
    capture=false;compensate=false;
    // Every opcode outside the explicit whitelist, including unknown ones,
    // remains a barrier. Values are observed at all these boundaries.
    for(unsigned op=0;op<80;++op) {
        assert(IsConstantMaterialBinding(op)==(op==OP_SetTexture || op==OP_SetTextureStageState ||
            op==OP_SetSamplerState || op==OP_SetVertexShader || op==OP_SetPixelShader ||
            op==OP_SetStreamSource || op==OP_SetStreamSourceFreq || op==OP_SetIndices ||
            op==OP_SetVertexDeclaration || op==OP_SetViewport || op==OP_SetScissorRect ||
            op==OP_SetVSConstI || op==OP_SetVSConstB || op==OP_SetPSConstI || op==OP_SetPSConstB));++checks;
    }
    unsigned counts[2]{};
    for(unsigned extended=0;extended<2;++extended) {
        g_rec.clear();arena.assign(128,0);expected.clear();Banks final;
        for(unsigned i=0;i<4;++i) {
            Rec r;r.op=OP_SetVSConstF;r.a[0]=i;r.a[1]=1;r.a[4]=i*16;
            float tmp[4];ApplyShiftToUpload(tmp,reinterpret_cast<const float*>(arena.data()+r.a[4]),i,1,g_activeVs,Params{});
            memcpy(final.vs.data()+i*4,tmp,16);g_rec.push_back(r);
            Rec binding;binding.op=9+i%3;g_rec.push_back(binding);
        }
        Rec barrier;barrier.op=36;g_rec.push_back(barrier);expected.push_back(final);
        driver={};apiCalls=apiFail=checkAt=0;g_constantUploadWindow={};
        g_replayConstantBatch=g_replayConstantWindow=true;g_replayConstantBindWindow=extended!=0;
        Actual();counts[extended]=apiCalls;assert(checkAt==1 && !apiFail);
    }
    assert(counts[0]==4 && counts[1]==1);++checks;
    g_replayConstantBindWindow=false;
}
static void ShaderBindingReduction(){
    capture=false;compensate=bindingTransforms=true;checkBindingSnapshots=true;
    g_frameStart.vertexFloatLimit=256;g_frameStart.pixelFloatLimit=224;
    g_rec.clear();arena.assign(1024,0);
    for(unsigned i=0;i<8;++i){
        Rec upload;upload.op=i%2?OP_SetPSConstF:OP_SetVSConstF;
        upload.a[0]=(i/2)%2;upload.a[1]=1;upload.a[4]=i*16;
        uint32_t bits[4]={i+19,i+31,i+79,i+97};memcpy(arena.data()+i*16,bits,16);
        g_rec.push_back(upload);
        Rec binding;binding.op=i%2?OP_SetVertexShader:OP_SetPixelShader;
        binding.a[0]=i%3?(0x1000+i*16):0;g_rec.push_back(binding);
        if(i==3 || i==7){Rec draw;draw.op=OP_DrawIndexedPrimitive;g_rec.push_back(draw);}
    }
    unsigned counts[4]{};
    for(bindFailAt=0;bindFailAt<=8;++bindFailAt){
        std::vector<uintptr_t> expectedOrder;
        expected.clear();expectedBindings.clear();
        for(unsigned mode=0;mode<4;++mode){
            driver={};apiCalls=apiFail=checkAt=bindCalls=0;bindingOrder.clear();
            g_replayVs=g_replayPs=nullptr;g_activePsInverse=-1;
            g_activeVs=&vertexMetadata;vertexMetadata.hash=0;g_vsConstantHistory.Clear();
            g_replayConstantBatch=mode!=0;g_replayConstantWindow=mode>=2;g_replayConstantBindWindow=mode==3;
            collectReference=mode==0;
            assert(Actual()==(bindFailAt==0));
            assert(checkAt==2 && bindCalls==8 && apiFail==0);
            if(mode==0)expectedOrder=bindingOrder;else assert(bindingOrder==expectedOrder);
            counts[mode]=apiCalls;++checks;
        }
        // Two draw boundaries, one VS and PS upload each: 8 setters -> 4.
        assert(counts[0]==8 && counts[1]==8 && counts[2]==8 && counts[3]==4);++checks;
    }
    bindFailAt=0;bindingTransforms=collectReference=checkBindingSnapshots=false;
    compensate=false;g_activeVs=&vertexMetadata;vertexMetadata.hash=0;
    g_activePsInverse=-1;g_replayVs=g_replayPs=nullptr;
}
static void IndependentBindingReduction(){
    capture=false;compensate=bindingTransforms=checkOtherState=true;
    g_frameStart.vertexFloatLimit=256;g_frameStart.pixelFloatLimit=224;
    const unsigned ops[]={OP_SetStreamSource,OP_SetStreamSourceFreq,OP_SetIndices,
        OP_SetVertexDeclaration,OP_SetViewport,OP_SetScissorRect,
        OP_SetVSConstI,OP_SetVSConstB,OP_SetPSConstI,OP_SetPSConstB};
    g_rec.clear();arena.assign(2048,0);
    for(unsigned i=0;i<2048;++i)arena[i]=uint8_t(i*13+31);
    for(unsigned segment=0;segment<2;++segment){
        const D3DVIEWPORT9 viewport{segment*4,segment*8,640,480,0.f,1.f};
        memcpy(arena.data()+512+segment*32,&viewport,sizeof(viewport));
        const RECT rect{4,8,320,240};memcpy(arena.data()+640+segment*32,&rect,sizeof(rect));
        for(unsigned i=0;i<10;++i){
            Rec upload;upload.op=i%2?OP_SetPSConstF:OP_SetVSConstF;
            upload.a[0]=i/2;upload.a[1]=1;upload.a[4]=(segment*10+i)*16;
            g_rec.push_back(upload);
            Rec state;state.op=ops[i];state.n=segment==0?1:0;
            state.a[0]=i==4?512+segment*32:i==5?640+segment*32:i;
            state.a[1]=i>=6?1:0x1000+segment*16+i;
            state.a[2]=segment*32;state.a[3]=16+i;state.a[4]=768+i*16;
            if(segment==1 && (i==2 || i==3))state.a[0]=0;
            if(segment==1 && i==0)state.a[1]=0;
            g_rec.push_back(state);
        }
        Rec draw;draw.op=OP_DrawIndexedPrimitive;g_rec.push_back(draw);
    }
    const auto owned=arena;
    for(stateFailAt=0;stateFailAt<=20;++stateFailAt){
        expected.clear();expectedBindings.clear();expectedState.clear();
        std::vector<std::vector<uint32_t>> referenceOrder;
        unsigned calls[4]{};
        for(unsigned mode=0;mode<4;++mode){
            driver={};otherState={};stateOrder.clear();stateCalls=apiCalls=apiFail=checkAt=0;
            g_vsConstantHistory.Clear();g_activeVs=&vertexMetadata;vertexMetadata.hash=0;
            g_replayVs=g_replayPs=nullptr;g_activePsInverse=-1;
            g_replayConstantBatch=mode!=0;g_replayConstantWindow=mode>=2;g_replayConstantBindWindow=mode==3;
            collectReference=mode==0;
            // Preserve existing replay's HRESULT contract for these setters.
            assert(Actual());assert(checkAt==2 && stateCalls==20 && !apiFail && arena==owned);
            if(mode==0)referenceOrder=stateOrder;else assert(stateOrder==referenceOrder);
            calls[mode]=apiCalls;++checks;
        }
        assert(calls[0]==20 && calls[1]==20 && calls[2]==20 && calls[3]==4);++checks;
    }
    stateFailAt=0;bindingTransforms=collectReference=checkOtherState=compensate=false;
    otherState={};g_replayConstantBindWindow=false;
}
static void PartialReplay(){
    capture=compensate=bindingTransforms=collectReference=checkOtherState=checkBindingSnapshots=false;
    g_activeVs=&vertexMetadata;g_recOverflow=false;
    g_frameStart.vertexFloatLimit=256;g_frameStart.pixelFloatLimit=224;
    for(unsigned base:{0u,61u,252u})for(unsigned mode=0;mode<4;++mode){
        activeWvpBase=int(base);g_vsConstantHistory.Clear();
        uint32_t raw[16];for(unsigned i=0;i<16;++i)raw[i]=0x3f000000+i;
        assert(g_vsConstantHistory.begin.Write(base,4,raw,sizeof(raw)));
        arena.assign(16,0);uint32_t partial[4]={31,37,41,43};memcpy(arena.data(),partial,16);
        memcpy(raw+12,partial,16);
        g_rec.clear();Rec upload;upload.op=OP_SetVSConstF;upload.a[0]=base+3;upload.a[1]=1;
        g_rec.push_back(upload);Rec draw;draw.op=OP_DrawIndexedPrimitive;g_rec.push_back(draw);
        Banks wanted;memcpy(wanted.vs.data()+base*4,raw,sizeof(raw));wanted.vs[base*4]^=0x10000;
        expected={wanted};driver={};checkAt=apiCalls=apiFail=0;
        g_replayConstantBatch=mode!=0;g_replayConstantWindow=mode>=2;g_replayConstantBindWindow=mode==3;
        assert(Actual() && checkAt==1 && !apiFail && apiCalls==1);
        assert(lastVsFirst==base && lastVsCount==4);
        float observed[16];assert(g_vsConstantHistory.replay.Matrix(int(base),observed));
        assert(!memcmp(observed,raw,sizeof(raw)) && !memcmp(arena.data(),partial,16));++checks;
    }
    // A rejected completed upload keeps the eye incomplete and is not retried.
    g_rec.pop_back();rejectAt=1;checkAt=apiCalls=apiFail=0;
    assert(!Actual() && apiCalls==1 && apiFail==1);++checks;
    rejectAt=0;activeWvpBase=-1;g_vsConstantHistory.Clear();
}
int main(){
    HeaderCases();ActualReduction();BindingReduction();ShaderBindingReduction();IndependentBindingReduction();PartialReplay();unsigned streams=0,records=0;
    for(bool diagnostics:{false,true})for(bool captured:{false,true})for(bool psComp:{false,true})
    for(unsigned caps:{0u,96u,256u})for(unsigned mode:{0u,1u,2u,3u}){
        d2vr_cpu_sync::timing.enabled=diagnostics;capture=captured;compensate=psComp;
        g_replayConstantBatch=mode!=0;g_replayConstantWindow=mode>=2;g_replayConstantBindWindow=mode==3;
        g_frameStart.vertexFloatLimit=caps;g_frameStart.pixelFloatLimit=caps==96?32:224;
        Build();const auto dataBefore=arena;
        driver={};g_vsConstantHistory.Clear();apiCalls=apiFail=checkAt=0;g_constantUploadWindow={};
        const bool complete=Actual();assert(complete==(apiFail==0));
        assert(checkAt==expected.size() && arena==dataBefore);
#ifdef D2VR_MATCHED_CAPTURE
        if(diagnostics){
            const auto& c=g_constantUploadWindow.counts;
            assert(c.calls[0]+c.calls[1]==apiCalls && c.failures[0]+c.failures[1]==apiFail);
            const auto inputs=c.records[0]+c.records[1];
            unsigned wanted=0;for(const auto& r:g_rec)wanted+=r.op==OP_SetVSConstF || r.op==OP_SetPSConstF;
            assert(inputs==wanted && c.exceptions==0 && g_constantUploadWindow.streams==1);
            if(mode==0 || captured)assert(apiCalls==inputs);
        } else assert(g_constantUploadWindow.streams==0);
#endif
        ++streams;records+=unsigned(g_rec.size());
    }
    // Diagnostic forwarding records entry before an SEH fault and never retries it.
    g_rec.clear();arena.assign(16*4,0);Rec one;one.op=OP_SetVSConstF;one.a[1]=4;g_rec.push_back(one);
    capture=false;g_replayConstantBatch=true;g_replayConstantWindow=true;
    g_frameStart.vertexFloatLimit=256;g_frameStart.pixelFloatLimit=224;
    d2vr_cpu_sync::timing.enabled=true;g_constantUploadWindow={};apiCalls=0;throwAt=1;
    assert(!Actual());throwAt=0;
    assert(apiCalls==1);
#ifdef D2VR_MATCHED_CAPTURE
    assert(g_constantUploadWindow.counts.exceptions==1 && g_constantUploadWindow.counts.calls[0]==1);
#endif
    printf("PASS actual replay float loop: %u streams / %u records / %u checks; OFF/adjacent/window/independent-bind/capture, shader+viewport-dependent transformed bits, shader 8->4 and independent 20->4 setters, exact setter args/order/state at draws, failed/null binds, caps, failures, forwarding receipts, SEH no retry\n",streams,records,checks);
}

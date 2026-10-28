#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <utility>
#include <atomic>
#include <thread>
#include "../src/stereo_math.h"
#include "../src/stereo_geometry.h"
#include "../src/projection_history.h"
#include "../src/shader_register_history.h"
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %u line %d: %s\n",checks,__LINE__,#x);exit(1);}}while(0)
static int g_projectionFix=1,g_lightingPreset=0;
static uint32_t g_diagnosticSkipVs=0;
static bool g_scale2DContent=false,g_fixMotionVectorProjection=true;
static volatile LONG g_shiftCount=0;
static int g_replayEye=-1;
static bool g_recActive=false,g_cameraProjectionKnown=true;
static float g_cameraPx=1,g_cameraPy=1;
static uint64_t g_wvpHistoryGeneration=0;
static void PublishProjectionEstimate(float,float){}
template<class... T> static void Log(const char*,T...){}
#include "actual_projection_history.inl"
#include "../src/vertex_shader_metadata.h"
struct IDirect3DVertexShader9 {};
static d2vr_shader_metadata::Vertices<VsInfo> g_vsInfo;
static std::shared_ptr<VsInfo> g_activeVsOwner;
static VsInfo* g_activeVs=nullptr;
static unsigned lockDepth=0;
static d2vr_shader_registers::History g_vsConstantHistory;
struct RecLockGuard {RecLockGuard(){CHECK(!lockDepth++);}~RecLockGuard(){CHECK(!--lockDepth);}};
#include "actual_shader_identity.inl"
static void Perspective(float (&m)[16],float offset) {
    memset(m,0,sizeof(m));m[0]=1.3f;m[5]=1.7f;m[10]=1.01f;m[11]=1;m[12]=offset;m[14]=-.1f;
}
static void Screen(const float* wvp,float (&m)[16]) {
    memcpy(m,wvp,sizeof(m));d2vr_math::RebiasScreenTextureProjection(m);
    CHECK(d2vr_math::IsScreenTextureProjection(m,wvp));
}
static void Seed(VsInfo& vi,const float* matrix,const StereoParams& sp) {
    float ignored[16];g_replayEye=-1;g_recActive=false;
    CHECK(ApplyShiftToUpload(ignored,matrix,0,4,&vi,sp));
}
static void Frame(){++g_wvpHistoryGeneration;g_recActive=true;g_replayEye=-1;}
static bool ApplyWithRegisters(float* dst,const float* src,UINT first,UINT count,VsInfo* vi,
                               const StereoParams& sp,const d2vr_shader_registers::Bank* bank) {
    const d2vr_shader_registers::Reference reference(*bank,ActiveWvpRegister(vi));
    return ApplyShiftToUpload(dst,src,first,count,vi,sp,&reference);
}
int main(int argc,char** argv) {
    const bool legacy=argc==2 && !strcmp(argv[1],"--expect-legacy-mismatch");
    VsInfo vi;vi.analyzed=true;vi.wvpBase=0;vi.stpBase=4;
    StereoParams sp;sp.transX=.0315f;sp.transY=.01f;sp.fovScaleX=.82f;sp.fovScaleY=.91f;
    float a[16],b[16],stp[16],live[16],replay[16],ignored[16];
    Perspective(a,.2f);Perspective(b,4);Screen(a,stp);Seed(vi,a,sp);
    Frame();CHECK(ApplyShiftToUpload(live,stp,4,4,&vi,sp));
    CHECK(ApplyShiftToUpload(ignored,b,0,4,&vi,sp)); // Later object in SAME live frame.
    g_recActive=false;g_replayEye=1;
    CHECK(ApplyShiftToUpload(replay,stp,4,4,&vi,sp));
    if(legacy){CHECK(memcmp(live,replay,sizeof(live))!=0);puts("REPRODUCED actual-source right-eye mismatch: replay's first STP uses the final live object's WVP; identical eye parameters/data");return 0;}
    CHECK(!memcmp(live,replay,sizeof(live)));
    {
        VsInfo first;first.analyzed=true;first.wvpBase=0;first.stpBase=4;
        VsInfo second=first,reference=first;
        float firstMatrix[16],secondMatrix[16],screen[16],actual[16],expected[16],discard[16];
        d2vr_shader_registers::History registers;
        Perspective(firstMatrix,.1f);Perspective(secondMatrix,7.f);Screen(secondMatrix,screen);
        g_replayEye=-1;g_recActive=false;
        CHECK(registers.live.Write(0,4,firstMatrix,sizeof(firstMatrix)));
        CHECK(ApplyWithRegisters(discard,firstMatrix,0,4,&first,sp,&registers.live));
        CHECK(registers.live.Write(0,4,secondMatrix,sizeof(secondMatrix)));
        CHECK(ApplyWithRegisters(discard,secondMatrix,0,4,&second,sp,&registers.live));
        CHECK(ApplyShiftToUpload(discard,secondMatrix,0,4,&reference,sp));
        CHECK(ApplyShiftToUpload(expected,screen,4,4,&reference,sp));
        CHECK(registers.live.Write(4,4,screen,sizeof(screen)));
        CHECK(ApplyWithRegisters(actual,screen,4,4,&first,sp,&registers.live));
        CHECK(!memcmp(actual,expected,sizeof(actual)));
        registers.BeginRecord();
        CHECK(registers.live.Write(0,4,firstMatrix,sizeof(firstMatrix))); // future live data
        for(unsigned pass=0;pass<3;++pass) {
            registers.BeginReplay();
            g_replayEye=1;
            CHECK(ApplyWithRegisters(actual,screen,4,4,&first,sp,&registers.replay));
            CHECK(!memcmp(actual,expected,sizeof(actual)));
            registers.replay.Invalidate(0,4);
        }
        g_replayEye=-1;
        // Partial writes by another shader update the DEVICE's WVP reference,
        // including boundaries between validity bit-mask words.
        for(unsigned base:{0u,61u,125u,189u,252u}) {
            d2vr_shader_registers::Bank bank;
            for(unsigned row=0;row<4;++row)CHECK(bank.Write(base+row,1,secondMatrix+row*4,16));
            float observed[16];CHECK(bank.Matrix(int(base),observed));
            CHECK(!memcmp(observed,secondMatrix,sizeof(observed)));
            for(unsigned row=0;row<4;++row) {
                bank.Invalidate(base+row,1);CHECK(!bank.Matrix(int(base),observed));
                CHECK(bank.Write(base+row,1,secondMatrix+row*4,16));
            }
            bank.Clear();CHECK(!bank.Matrix(int(base),observed));
        }
        registers.live.Clear();CHECK(!registers.live.Matrix(0,actual));
        CHECK(!registers.live.Write(255,2,secondMatrix,32));
        CHECK(!registers.live.Write(0,4,secondMatrix,63));
        CHECK(!registers.live.Write(UINT32_MAX,1,secondMatrix,16));
        // A different shader's WVP at c8 must NOT replace the active c0 bank.
        CHECK(registers.live.Write(0,4,firstMatrix,sizeof(firstMatrix)));
        CHECK(registers.live.Write(8,4,secondMatrix,sizeof(secondMatrix)));
        Screen(firstMatrix,screen);
        CHECK(ApplyShiftToUpload(discard,firstMatrix,0,4,&reference,sp));
        CHECK(ApplyShiftToUpload(expected,screen,4,4,&reference,sp));
        CHECK(ApplyWithRegisters(actual,screen,4,4,&first,sp,&registers.live));
        CHECK(!memcmp(actual,expected,sizeof(actual)));
        VsInfo reflected=first;reflected.reflected=true;reflected.reflectedWvp=8;
        CHECK(ActiveWvpRegister(&reflected)==8 && ActiveWvpRegister(nullptr)==-1);
        const auto oldFix=g_projectionFix;g_projectionFix=0;
        CHECK(ActiveWvpRegister(&reflected)==0);g_projectionFix=oldFix;
    }
    CHECK(ApplyShiftToUpload(ignored,b,0,4,&vi,sp));
    // Rotating projections, two shaders, repeated replay and frame inheritance.
    VsInfo peer=vi;peer.stpBase=8;
    for(unsigned n=0;n<200;++n) {
        const unsigned start=n%2?4:8;auto& active=n%2?vi:peer;
        Perspective(a,float(n)*.03125f);Perspective(b,float(n+7)*.125f);Screen(a,stp);
        Seed(active,a,sp);Frame();CHECK(ApplyShiftToUpload(live,stp,start,4,&active,sp));
        CHECK(ApplyShiftToUpload(ignored,b,0,4,&active,sp));
        g_recActive=false;g_replayEye=1;CHECK(ApplyShiftToUpload(replay,stp,start,4,&active,sp));
        CHECK(!memcmp(live,replay,sizeof(live)));CHECK(ApplyShiftToUpload(ignored,b,0,4,&active,sp));
        // Next frame's first STP must inherit the LIVE raw WVP, not a future
        // replay-only observation; this also exercises separate eye storage.
        Perspective(a,-9.f);CHECK(ApplyShiftToUpload(ignored,a,0,4,&active,sp));
        Screen(b,stp);Frame();CHECK(ApplyShiftToUpload(live,stp,start,4,&active,sp));
        g_recActive=false;g_replayEye=1;CHECK(ApplyShiftToUpload(replay,stp,start,4,&active,sp));
        CHECK(!memcmp(live,replay,sizeof(live)));
    }
    // A deduplicated identical bytecode keeps the existing ordered history.
    // Reused COM address / even equal 32-bit hash cannot retain old semantics.
    IDirect3DVertexShader9 shader;
    VsInfo original;original.hash=17;original.bytecode={1,2,3};original.wvpBase=0;original.stpBase=4;
    CHECK(RememberVertexShader(&shader,original));
    auto stored=g_vsInfo.Lookup(&shader);stored->wvpHistory.live.valid=true;stored->wvpHistory.live.values[12]=7;
    const DWORD sameCode[]={1,2,3},differentCode[]={1,2,4};IDirect3DVertexShader9 foreign;
    CHECK(VertexShaderAnalysisKnown(&shader,sameCode,3));
    CHECK(!VertexShaderAnalysisKnown(&shader,differentCode,3));
    CHECK(!VertexShaderAnalysisKnown(&shader,sameCode,2));
    CHECK(!VertexShaderAnalysisKnown(&foreign,sameCode,3));
    CHECK(!VertexShaderAnalysisKnown(nullptr,sameCode,3));
    CHECK(!VertexShaderAnalysisKnown(&shader,nullptr,3));
    CHECK(!VertexShaderAnalysisKnown(&shader,sameCode,8193));
    VsInfo duplicate;duplicate.hash=17;duplicate.bytecode={1,2,3};
    CHECK(!RememberVertexShader(&shader,duplicate));
    CHECK(stored->wvpBase==0 && stored->stpBase==4 && stored->wvpHistory.live.valid && stored->wvpHistory.live.values[12]==7);
    VsInfo recycled;recycled.hash=17;recycled.bytecode={1,2,4};recycled.wvpBase=8;recycled.stpBase=-1;
    CHECK(RememberVertexShader(&shader,recycled));
    CHECK(!VertexShaderAnalysisKnown(&shader,sameCode,3));
    CHECK(VertexShaderAnalysisKnown(&shader,differentCode,3));
    // An active old binding remains alive and unchanged across publication.
    CHECK(stored->wvpBase==0 && stored->stpBase==4 && stored->wvpHistory.live.values[12]==7);
    stored=g_vsInfo.Lookup(&shader);
    CHECK(stored->wvpBase==8 && stored->stpBase==-1 && !stored->wvpHistory.live.valid && stored->bytecode==std::vector<DWORD>({1,2,4}));
    stored->wvpHistory.live.valid=stored->wvpHistory.begin.valid=stored->wvpHistory.replay.valid=true;
    stored->wvpHistory.capturedFrame=stored->wvpHistory.replayFrame=99;
    ResetProjectionHistories();
    CHECK(!stored->wvpHistory.live.valid && !stored->wvpHistory.begin.valid && !stored->wvpHistory.replay.valid);
    CHECK(!stored->wvpHistory.capturedFrame && !stored->wvpHistory.replayFrame);
    CHECK(stored->wvpBase==8 && stored->bytecode==std::vector<DWORD>({1,2,4}));
    // Creation analysis must not acquire the recorder's whole-replay lock.
    lockDepth=1;
    CHECK(VertexShaderAnalysisKnown(&shader,differentCode,3));
    VsInfo duringReplay;duringReplay.bytecode={1,2,4};
    CHECK(!RememberVertexShader(&shader,duringReplay));CHECK(lockDepth==1);lockDepth=0;
    SetActiveVertexShaderMetadata(&shader);CHECK(g_activeVs==stored.get());
    std::weak_ptr<VsInfo> retired=stored;
    VsInfo next;next.hash=17;next.bytecode={1,2,5};next.wvpBase=12;
    CHECK(RememberVertexShader(&shader,next));stored.reset();
    CHECK(!retired.expired() && g_activeVs->wvpBase==8);
    SetActiveVertexShaderMetadata(&shader);
    CHECK(retired.expired() && g_activeVs->wvpBase==12);
    SetActiveVertexShaderMetadata(&foreign);CHECK(!g_activeVs && !g_activeVsOwner);
    SetActiveVertexShaderMetadata(nullptr);CHECK(!g_activeVs && !g_activeVsOwner);
    CHECK(!lockDepth);
    // Actual production registry: creation can replace identities/rehash while
    // the render thread retains and updates an older node's ordered history.
    d2vr_shader_metadata::Vertices<VsInfo> concurrent;
    VsInfo initial;initial.hash=17;initial.bytecode={1,0,3};initial.wvpBase=0;
    CHECK(concurrent.Remember(&shader,initial));
    std::atomic<bool> start{false},done{false};std::atomic<unsigned> invalid{0},snapshots{0};
    std::thread reader([&]{while(!start.load())YieldProcessor();do {
        auto node=concurrent.Lookup(&shader);
        if(!node || node->hash!=17 || node->bytecode.size()!=3 ||
            node->wvpBase!=int(node->bytecode[1]))++invalid;
        else {
            // Only this render-like reader writes the history; the creation
            // writer must never mutate it, even when a COM address is reused.
            const auto expected=node->wvpBase;
            node->wvpHistory.live.valid=true;node->wvpHistory.live.values[12]=float(expected);
            for(unsigned i=0;i<8;++i)YieldProcessor();
            if(node->wvpBase!=expected || node->wvpHistory.live.values[12]!=float(expected))++invalid;
        }
        ++snapshots;
    }while(!done.load());});
    start.store(true);
    for(unsigned i=1;i<=20000;++i) {
        VsInfo item;item.hash=17;item.bytecode={1,i,3};item.wvpBase=int(i);
        CHECK(concurrent.Remember(&shader,item));
        if(i<2048) {
            VsInfo extra;extra.bytecode={1,i,3};
            CHECK(concurrent.Remember(reinterpret_cast<void*>(uintptr_t(i)*16),extra));
        }
    }
    done.store(true);reader.join();CHECK(!invalid.load() && snapshots.load()>0);
    printf("PASS vertex metadata concurrency: 20000 replacements, %u stable binding/history snapshots, 2047 rehash identities, retired-node reclamation\n",snapshots.load());
    printf("PASS actual-source split projection history: %u checks; identical-eye regression, future live WVP isolation, frame inheritance, independent shaders, exact bytecode/address reuse; no GPU/HMD claim\n",checks);
}

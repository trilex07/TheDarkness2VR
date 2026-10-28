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
#include "../src/stereo_math.h"
#include "../src/stereo_geometry.h"
#include "../src/projection_history.h"
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
struct IDirect3DVertexShader9 {};
static std::unordered_map<IDirect3DVertexShader9*,VsInfo> g_vsInfo;
static unsigned lockDepth=0;
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
    auto& stored=g_vsInfo[&shader];stored.wvpHistory.live.valid=true;stored.wvpHistory.live.values[12]=7;
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
    CHECK(stored.wvpBase==0 && stored.stpBase==4 && stored.wvpHistory.live.valid && stored.wvpHistory.live.values[12]==7);
    VsInfo recycled;recycled.hash=17;recycled.bytecode={1,2,4};recycled.wvpBase=8;recycled.stpBase=-1;
    CHECK(RememberVertexShader(&shader,recycled));
    CHECK(!VertexShaderAnalysisKnown(&shader,sameCode,3));
    CHECK(VertexShaderAnalysisKnown(&shader,differentCode,3));
    CHECK(stored.wvpBase==8 && stored.stpBase==-1 && !stored.wvpHistory.live.valid && stored.bytecode==std::vector<DWORD>({1,2,4}));
    stored.wvpHistory.live.valid=stored.wvpHistory.begin.valid=stored.wvpHistory.replay.valid=true;
    stored.wvpHistory.capturedFrame=stored.wvpHistory.replayFrame=99;
    ResetProjectionHistories();
    CHECK(!stored.wvpHistory.live.valid && !stored.wvpHistory.begin.valid && !stored.wvpHistory.replay.valid);
    CHECK(!stored.wvpHistory.capturedFrame && !stored.wvpHistory.replayFrame);
    CHECK(stored.wvpBase==8 && stored.bytecode==std::vector<DWORD>({1,2,4}));
    CHECK(!lockDepth);
    printf("PASS actual-source split projection history: %u checks; identical-eye regression, future live WVP isolation, frame inheritance, independent shaders, exact bytecode/address reuse; no GPU/HMD claim\n",checks);
}

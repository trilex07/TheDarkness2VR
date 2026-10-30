#define NOMINMAX
#include <windows.h>
#include "../src/recorder_sync.h"
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
static bool g_noInvProj=false;
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
#include "actual_pixel_projection.inl"
#include "../src/vertex_shader_metadata.h"
struct IDirect3DVertexShader9 {};
static d2vr_shader_metadata::Vertices<VsInfo> g_vsInfo;
static std::shared_ptr<VsInfo> g_activeVsOwner;
static VsInfo* g_activeVs=nullptr;
static unsigned lockDepth=0;
static d2vr_shader_registers::History g_vsConstantHistory;
static d2vr_shader_registers::EyeHistory g_psEyeHistory;
struct RecLockGuard {RecLockGuard(){CHECK(!lockDepth++);}~RecLockGuard(){CHECK(!--lockDepth);}};
#include "actual_shader_identity.inl"
static bool g_hadWorldDrawThisFrame=false,g_had3DCameraThisFrame=true;
static bool g_skipObjectDraws=false,g_skipShadowAtlasDraws=false;
#include "actual_world_draw_activity.inl"
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
static void PartialWvpCases(bool legacy){
    for(unsigned base:{0u,61u,125u,189u,252u})for(unsigned eye=0;eye<2;++eye)
    for(unsigned geometry=0;geometry<2;++geometry)for(unsigned firstRow=0;firstRow<4;++firstRow)
    for(unsigned rows=1;rows<=4-firstRow;++rows){
        if(firstRow==0 && rows==4)continue;
        VsInfo vi;vi.analyzed=true;vi.wvpBase=int(base);
        StereoParams params;params.transX=eye?.0315f:-.0315f;params.transY=.01f;
        params.fovScaleX=.8f;params.fovScaleY=.9f;params.headGeometry=geometry!=0;
        params.headClip[0]=params.headClip[5]=params.headClip[10]=params.headClip[15]=1;
        params.headClip[8]=.03f;params.headClip[12]=.02f;
        float raw[16],actual[72*4],expected[16];Perspective(raw,float(base)*.1f);
        // This is the raw register bank AFTER this native partial write.
        raw[firstRow*4]+=.25f;
        d2vr_shader_registers::Bank bank;CHECK(bank.Write(base,4,raw,sizeof(raw)));
        const d2vr_shader_registers::Reference reference(bank,int(base));
        CHECK(ApplyShiftToUpload(expected,raw,base,4,&vi,params,&reference));
        UINT first=base+firstRow,count=rows;bool perspective=false;
        const bool modified=legacy?
            ApplyShiftToUpload(actual,raw+firstRow*4,first,count,&vi,params,&reference,&perspective):
            ApplyVertexStereoUpload(actual,raw+firstRow*4,first,count,&vi,params,&reference,&perspective);
        CHECK(modified);CHECK(first==base && count==4);CHECK(perspective);
        CHECK(!memcmp(actual,expected,sizeof(expected)));
        // Missing data never borrows a different shader's stale matrix.
        auto unknown=reference;unknown.valid=false;
        first=base+firstRow;count=rows;
        CHECK(!ApplyVertexStereoUpload(actual,raw+firstRow*4,first,count,&vi,params,&unknown));
        CHECK(first==base+firstRow && count==rows);
    }
    // Worst expansion on either side, including payload outside the WVP.
    for(unsigned side=0;side<2;++side){
        VsInfo vi;vi.analyzed=true;vi.wvpBase=68;
        StereoParams params;params.transX=.03f;params.fovScaleX=.8f;
        float native[256*4];for(unsigned i=0;i<256*4;++i)native[i]=float(i)*.001f;
        float matrix[16];Perspective(matrix,7);memcpy(native+68*4,matrix,sizeof(matrix));
        d2vr_shader_registers::Bank bank;CHECK(bank.Write(0,256,native,sizeof(native)));
        const d2vr_shader_registers::Reference reference(bank,68);
        UINT first=side?71:1,count=68;
        const UINT expectedFirst=side?68:1,expectedCount=71;
        float expected[72*4]{},actual[72*4+4];
        for(auto& f:actual)f=-12345.f;
        CHECK(ApplyShiftToUpload(expected,native+expectedFirst*4,expectedFirst,expectedCount,&vi,params,&reference));
        CHECK(ApplyVertexStereoUpload(actual,native+first*4,first,count,&vi,params,&reference));
        CHECK(first==expectedFirst && count==expectedCount);
        CHECK(!memcmp(actual,expected,expectedCount*16));
        for(unsigned i=count*4;i<72*4+4;++i)CHECK(actual[i]==-12345.f);
    }
    printf("PASS actual partial WVP completion: both eyes, head geometry, every partial subrange, register word boundaries, unknown-bank refusal, 68-to71 rows and guard bytes\n");
}
static void PartialScreenCases(){
    for(unsigned base:{4u,61u,125u,189u,252u})for(unsigned named=0;named<2;++named)
    for(unsigned eye=0;eye<2;++eye)for(unsigned geometry=0;geometry<2;++geometry)
    for(unsigned firstRow=0;firstRow<4;++firstRow)for(unsigned rows=1;rows<=4-firstRow;++rows){
        if(firstRow==0 && rows==4)continue;
        VsInfo vi;vi.analyzed=true;vi.wvpBase=0;vi.stpBase=int(base);
        if(named){vi.reflected=true;vi.reflectedWvp=0;vi.reflectedStp=int(base);}
        StereoParams params;params.transX=eye?.0315f:-.0315f;params.fovScaleX=.8f;params.fovScaleY=.9f;
        params.headGeometry=geometry!=0;
        params.headClip[0]=params.headClip[5]=params.headClip[10]=params.headClip[15]=1;
        params.headClip[8]=.03f;params.headClip[12]=.02f;
        float wvp[16],screen[16],expected[16],actual[76*4];Perspective(wvp,3);Screen(wvp,screen);
        d2vr_shader_registers::Bank bank;CHECK(bank.Write(0,4,wvp,sizeof(wvp)));
        CHECK(bank.Write(base,4,screen,sizeof(screen)));
        const d2vr_shader_registers::Reference reference(bank,0,int(base),base+firstRow,rows);
        CHECK(reference.partialBase==int(base));
        CHECK(ApplyShiftToUpload(expected,screen,base,4,&vi,params,&reference));
        UINT first=base+firstRow,count=rows;bool perspective=true;
        for(auto& f:actual)f=-12345.f;
        CHECK(ApplyVertexStereoUpload(actual,screen+firstRow*4,first,count,&vi,params,&reference,&perspective));
        CHECK(first==base && count==4);CHECK(!perspective);
        CHECK(!memcmp(actual,expected,sizeof(expected)));
        for(unsigned i=16;i<76*4;++i)CHECK(actual[i]==-12345.f);
        // Missing rows and a different shader's auxiliary base cannot authorize expansion.
        bank.Invalidate(base+(firstRow+rows)%4,1);
        const d2vr_shader_registers::Reference unknown(bank,0,int(base),base+firstRow,rows);
        CHECK(unknown.partialBase<0);
        first=base+firstRow;count=rows;
        CHECK(!ApplyVertexStereoUpload(actual,screen+firstRow*4,first,count,&vi,params,&unknown));
        CHECK(first==base+firstRow && count==rows);
        auto changed=vi;changed.reflectedStp=changed.stpBase=-1;
        CHECK(!ApplyVertexStereoUpload(actual,screen+firstRow*4,first,count,&changed,params,&reference));
        g_lightingPreset=7;
        CHECK(!ApplyVertexStereoUpload(actual,screen+firstRow*4,first,count,&vi,params,&reference));
        CHECK(first==base+firstRow && count==rows);g_lightingPreset=0;
    }
    // Simultaneous WVP/STP completion at opposite ends is a 68-to74-row write.
    for(unsigned reverse=0;reverse<2;++reverse){
        VsInfo vi;vi.analyzed=true;vi.wvpBase=reverse?70:0;vi.stpBase=reverse?0:70;
        StereoParams params;params.transX=.03f;params.fovScaleX=.8f;params.fovScaleY=.9f;
        float native[256*4];for(unsigned i=0;i<256*4;++i)native[i]=float(i)*.001f;
        float wvp[16],screen[16];Perspective(wvp,7);Screen(wvp,screen);
        memcpy(native+vi.wvpBase*4,wvp,sizeof(wvp));memcpy(native+vi.stpBase*4,screen,sizeof(screen));
        d2vr_shader_registers::Bank bank;CHECK(bank.Write(0,256,native,sizeof(native)));
        const d2vr_shader_registers::Reference reference(bank,vi.wvpBase,vi.stpBase,3,68);
        float expected[76*4],actual[76*4+4];for(auto& f:actual)f=-12345.f;
        CHECK(ApplyShiftToUpload(expected,native,0,74,&vi,params,&reference));
        UINT first=3,count=68;
        CHECK(ApplyVertexStereoUpload(actual,native+12,first,count,&vi,params,&reference));
        CHECK(first==0 && count==74);CHECK(!memcmp(actual,expected,count*16));
        for(unsigned i=count*4;i<76*4+4;++i)CHECK(actual[i]==-12345.f);
        CHECK(!memcmp(native+vi.wvpBase*4,wvp,sizeof(wvp)) && !memcmp(native+vi.stpBase*4,screen,sizeof(screen)));
    }
    puts("PASS actual partial STP completion: both eyes, head geometry, named/analyzed identities, every subrange, unknown-row/shader/lighting refusal, 68-to74 WVP+STP and guard bytes");
}
// Exercise carry using the real stereo math, not the replay-loop fixture's
// bitwise upload sentinel. Inherited device constants keep their previous-eye
// transform; a new upload uses the new pose, FOV and raw register reference.
static void InheritedEyeTransforms() {
    for(unsigned geometry=0;geometry<2;++geometry)for(unsigned frame=0;frame<32;++frame) {
        VsInfo vi;vi.analyzed=true;vi.wvpBase=0;vi.stpBase=4;
        float native[32],wvp[16],screen[16];
        Perspective(wvp,float(frame)*.07f);Screen(wvp,screen);
        memcpy(native,wvp,sizeof(wvp));memcpy(native+16,screen,sizeof(screen));
        d2vr_shader_registers::History registers;
        CHECK(registers.live.Write(0,8,native,sizeof(native)));
        registers.BeginRecord();registers.BeginReplay();
        const d2vr_shader_registers::Reference reference(registers.replay,0,4,0,8);
        StereoParams params;params.transX=.0315f;params.transY=.01f;
        params.fovScaleX=.8f;params.fovScaleY=.9f;params.headGeometry=geometry!=0;
        const float angle=float(frame)*.003f;
        params.headClip[0]=params.headClip[10]=std::cos(angle);
        params.headClip[2]=-std::sin(angle);params.headClip[8]=std::sin(angle);
        params.headClip[5]=params.headClip[15]=1;
        params.headClip[12]=float(frame)*.002f;
        float left[32],right[32];UINT first=0,count=8;
        g_replayEye=1;
        CHECK(ApplyVertexStereoUpload(right,native,first,count,&vi,params,&reference));
        CHECK(first==0 && count==8);
        CHECK(registers.eyeNext.Write(first,count,right,sizeof(right)));
        params.transX=-params.transX;g_replayEye=-1;first=0;count=8;
        CHECK(ApplyVertexStereoUpload(left,native,first,count,&vi,params,&reference));
        CHECK(memcmp(left,right,sizeof(left))!=0);
        registers.EndReplay(true);
        // Frame-start Apply restores left. No upload means no second transform
        // of the carry, even if the next frame's HMD parameters have changed.
        params.transX=.04f;params.headClip[12]+=.05f;params.fovScaleX=.73f;
        float driver[32];memcpy(driver,left,sizeof(driver));
        registers.BeginRecord();registers.BeginReplay();
        registers.eyeCarry.ForEachValidRun(256,[&](unsigned start,const float* values,unsigned rows) {
            CHECK(start==0 && rows==8);memcpy(driver+start*4,values,rows*16);
        });
        CHECK(!memcmp(driver,right,sizeof(driver)));
        float newRight[32];first=0;count=8;g_replayEye=1;
        CHECK(ApplyVertexStereoUpload(newRight,native,first,count,&vi,params,&reference));
        CHECK(memcmp(newRight,right,sizeof(right))!=0);
        CHECK(registers.eyeNext.Write(first,count,newRight,sizeof(newRight)));
        registers.EndReplay(true);
        registers.eyeCarry.ForEachValidRun(256,[&](unsigned start,const float* values,unsigned rows) {
            memcpy(driver+start*4,values,rows*16);
        });
        CHECK(!memcmp(driver,newRight,sizeof(driver)));
        // State-block Apply destroys knowledge of both the raw replay rows and
        // carried transformed rows. Reset must also clear the global bank.
        registers.BeginRecord();registers.BeginReplay();registers.ReplayStateBlock();
        registers.EndReplay(true);unsigned runs=0;
        registers.eyeCarry.ForEachValidRun(256,[&](unsigned,const float*,unsigned){++runs;});
        CHECK(!runs);
        g_vsConstantHistory=registers;
        CHECK(g_vsConstantHistory.eyeCarry.Write(0,8,right,sizeof(right)));
        CHECK(g_vsConstantHistory.eyeNext.Write(0,8,right,sizeof(right)));
        ResetProjectionHistories();
        g_vsConstantHistory.eyeCarry.ForEachValidRun(256,[&](unsigned,const float*,unsigned){++runs;});
        g_vsConstantHistory.eyeNext.ForEachValidRun(256,[&](unsigned,const float*,unsigned){++runs;});
        CHECK(!runs && !g_vsConstantHistory.recordOpen);
    }
    g_vsConstantHistory.Clear();g_replayEye=-1;
    puts("PASS actual stereo math carry: WVP+STP, 64 changing pose/FOV cases, inherited bytes not transformed twice, fresh uploads use new parameters, state-block and Reset invalidation");
}
static void InheritedPixelTransforms(){
    g_projectionFix=2;
    for(unsigned geometry=0;geometry<2;++geometry)for(unsigned frame=0;frame<32;++frame){
        float native[20]{},projection[16];Perspective(projection,float(frame)*.01f);
        CHECK(d2vr_math::Invert(projection,native));native[16]=1.2f;native[17]=.8f;
        StereoParams params;params.transX=.0315f;params.transY=.01f;
        params.fovScaleX=.8f;params.fovScaleY=.9f;params.headGeometry=geometry!=0;
        params.inverseHeadClip[0]=params.inverseHeadClip[5]=params.inverseHeadClip[10]=params.inverseHeadClip[15]=1;
        params.inverseHeadClip[12]=float(frame)*.002f;
        float right[20],left[20];
        CHECK(CompensatePsConstants(right,native,0,5,0,4,params));
        params.transX=-params.transX;
        CHECK(CompensatePsConstants(left,native,0,5,0,4,params));
        CHECK(memcmp(left,right,sizeof(left))!=0);
        d2vr_shader_registers::EyeHistory history;history.BeginRecord();history.BeginReplay();
        CHECK(history.eyeNext.Write(0,5,right,sizeof(right)));history.EndReplay(true);
        params.transX=.06f;params.fovScaleX=.7f;params.inverseHeadClip[12]+=.03f;
        float driver[20];memcpy(driver,left,sizeof(left));history.BeginRecord();history.BeginReplay();
        history.eyeCarry.ForEachValidRun(224,[&](unsigned first,const float* values,unsigned count){
            CHECK(first==0&&count==5);memcpy(driver+first*4,values,count*16);
        });
        CHECK(!memcmp(driver,right,sizeof(right))); // inherited bytes, no double transform
        float current[20];CHECK(CompensatePsConstants(current,native,0,5,0,4,params));
        CHECK(memcmp(current,right,sizeof(right))!=0);
        CHECK(history.eyeNext.Write(0,5,current,sizeof(current)));history.EndReplay(true);
        g_psEyeHistory=history;ResetProjectionHistories();unsigned runs=0;
        g_psEyeHistory.eyeCarry.ForEachValidRun(224,[&](unsigned,const float*,unsigned){++runs;});
        CHECK(!runs&&!g_psEyeHistory.recordOpen);
    }
    g_projectionFix=1;
    puts("PASS actual PS inverse-projection/extents math: 64 changing pose/FOV cases, inherited eye bytes, current upload replacement, Reset invalidation");
}
int main(int argc,char** argv) {
    InheritedPixelTransforms();
    InheritedEyeTransforms();
    PartialScreenCases();
    PartialWvpCases(argc==2 && !strcmp(argv[1],"--partial-legacy"));
    const bool legacy=argc==2 && !strcmp(argv[1],"--expect-legacy-mismatch");
    VsInfo vi;vi.analyzed=true;vi.wvpBase=0;vi.stpBase=4;
    StereoParams sp;sp.transX=.0315f;sp.transY=.01f;sp.fovScaleX=.82f;sp.fovScaleY=.91f;
    float a[16],b[16],stp[16],live[16],replay[16],ignored[16];
    Perspective(a,.2f);Perspective(b,4);Screen(a,stp);Seed(vi,a,sp);
    {
        float ui[16]{};ui[0]=ui[5]=.01f;ui[10]=ui[15]=1;
        bool perspective=true;g_scale2DContent=true;
        CHECK(ApplyShiftToUpload(ignored,ui,0,4,&vi,sp,nullptr,&perspective));
        CHECK(!perspective); // adjusted UI is NOT a 3D-camera witness
        g_scale2DContent=false;
        CHECK(ApplyShiftToUpload(ignored,a,0,4,&vi,sp,nullptr,&perspective));CHECK(perspective);
        CHECK(ApplyShiftToUpload(ignored,stp,4,4,&vi,sp,nullptr,&perspective));CHECK(!perspective);
        g_activeVs=&vi;
        for(unsigned segment=0;segment<1000;++segment) {
            g_hadWorldDrawThisFrame=false;
            CHECK(g_vsConstantHistory.live.Write(0,4,ui,sizeof(ui)));
            {RecLockGuard lock;NoteRecordedWorldDraw();}CHECK(!g_hadWorldDrawThisFrame);
            CHECK(g_vsConstantHistory.live.Write(0,4,a,sizeof(a)));
            {RecLockGuard lock;NoteRecordedWorldDraw();}CHECK(g_hadWorldDrawThisFrame);
        }
        g_hadWorldDrawThisFrame=false;g_vsConstantHistory.live.Invalidate(0,1);
        {RecLockGuard lock;NoteRecordedWorldDraw();}CHECK(!g_hadWorldDrawThisFrame);
        CHECK(g_vsConstantHistory.live.Write(0,4,a,sizeof(a)));
        for(unsigned refusal=0;refusal<5;++refusal) {
            g_had3DCameraThisFrame=refusal!=0;vi.depthOnly=refusal==1;
            g_replayEye=refusal==2?1:-1;g_skipObjectDraws=refusal==3;g_skipShadowAtlasDraws=refusal==4;
            {RecLockGuard lock;NoteRecordedWorldDraw();}CHECK(!g_hadWorldDrawThisFrame);
        }
        g_had3DCameraThisFrame=true;vi.depthOnly=false;g_replayEye=-1;
        g_skipObjectDraws=g_skipShadowAtlasDraws=false;g_activeVs=nullptr;
    }
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

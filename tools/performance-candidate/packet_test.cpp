#define NOMINMAX
#include <windows.h>
#include "../../src/render_hook_cost.h"
#include "../../src/replay_packet_worker.h"
#include "../../src/projection_history.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>
using UINT=unsigned;
struct VsInfo : d2vr_packet_prepare::Shader {d2vr_projection_history::History wvpHistory;};
using StereoParams=d2vr_packet_prepare::Params;
bool g_cameraProjectionKnown=false,g_scale2DContent=true,g_fixMotionVectorProjection=true,g_recActive=false;
float g_cameraPx=0,g_cameraPy=0;
unsigned g_projectionFix=3,g_diagnosticSkipVs=0,g_wvpHistoryGeneration=0;
int g_lightingPreset=0,g_replayEye=1;
volatile LONG g_shiftCount=0;
d2vr_packet_prepare::Effects effects;
void PublishProjectionEstimate(float x,float y){effects.publish=true;effects.px=x;effects.py=y;}
void Log(const char*,...){}
float testUiX=1.17f,testUiY=.93f;
void NativeCameraUiScale(float& x,float& y){x*=testUiX;y*=testUiY;}
bool MatchedCaptureRecordingActive(){return false;}
int ActiveWvpRegister(const VsInfo* s){return s?s->activeWvp:-1;}
int ActiveStpRegister(const VsInfo* s){return s?s->activeStp:-1;}
#include "d2vr_prepare_wrapper.inl"
#include "d2vr_packet_oracle.inl"
unsigned checks=0;
void Check(bool ok){++checks;if(!ok){printf("FAIL packet check %u\n",checks);exit(1);}}
using namespace d2vr_packet_prepare;
void Configure(Job&,unsigned);
void Fill(Job&,unsigned);
enum {OP_SetVertexShader=1,OP_ApplyStateBlock,OP_SetVSConstF};
struct Record {unsigned op=0;unsigned a[5]{};};
std::vector<Record> g_rec;std::vector<unsigned char> g_recData;
std::shared_ptr<VsInfo> g_activeVsOwner=std::make_shared<VsInfo>();
VsInfo* g_activeVs=g_activeVsOwner.get();
struct Registry {std::shared_ptr<VsInfo> Lookup(void*){return g_activeVsOwner;}} g_vsInfo;
d2vr_shader_registers::History g_vsConstantHistory;
bool g_fullPacketPrepareWorker=true,g_recOverflow=false;
#include "../../src/replay_packet_prepare.inl"
void Lifecycle() {
    auto& vi=*g_activeVsOwner;vi.analyzed=true;vi.wvpBase=vi.activeWvp=0;vi.stpBase=vi.activeStp=8;
    auto fixture=std::make_shared<Job>();Configure(*fixture,0);Fill(*fixture,256);
    // Real source lifecycle, every record is a valid known upload here.
    g_rec.resize(256);g_recData.resize(256*17*16);
    float raw[17*4]{};raw[0]=1.2f;raw[5]=1.6f;raw[11]=1.f;raw[15]=0.f;
    for(unsigned i=0;i<256;++i){auto& r=g_rec[i];r.op=OP_SetVSConstF;r.a[0]=0;r.a[1]=17;r.a[4]=i*17*16;
        const float px=1.2f,py=1.6f;memcpy(&r.a[2],&px,4);memcpy(&r.a[3],&py,4);
        memcpy(g_recData.data()+r.a[4],raw,sizeof(raw));}
    float ui[17*4]{};ui[0]=ui[5]=.001f;
    memcpy(g_recData.data()+g_rec[5].a[4],ui,sizeof(ui));
    g_vsConstantHistory.begin.Clear();StartPacketPreparation(fixture->params);
    Check(g_packetPreparation && g_packetPreparation->ticket);
    auto owned=g_packetPreparation->job;Check(owned->input[0]==1.2f);
    memset(g_recData.data(),0,g_recData.size());Check(owned->input[0]==1.2f);
    const auto wait=g_packetPreparation->queue.Wait(g_packetPreparation->ticket);Check(wait.complete&&!wait.firstFailure);
    Check(PreparedVertexPacket(0,true)!=nullptr);Check(PreparedVertexPacket(0,false)==nullptr);
    Check(PreparedVertexPacket(5,true)!=nullptr);Check(owned->output[5].effects.uiDependent);
#ifdef D2VR_MATCHED_CAPTURE
    const float savedUi=testUiX;testUiX*=2;
    Check(PreparedVertexPacket(5,true)==nullptr);testUiX=savedUi;
#endif
    ++g_lightingPreset;Check(PreparedVertexPacket(0,true)==nullptr);--g_lightingPreset;
    ++vi.hash;Check(PreparedVertexPacket(0,true)==nullptr);--vi.hash;
    FinishPacketPreparation();Check(!g_packetPreparation->ticket && !g_packetPreparation->job);
    Check(g_packetPreparation->spare==owned);
    g_rec[0].op=OP_ApplyStateBlock;
    StartPacketPreparation(fixture->params);Check(!g_packetPreparation->ticket); // unknown shader remainder
    g_rec[1].op=OP_SetVertexShader;g_rec.resize(260,g_rec[2]);
    StartPacketPreparation(fixture->params);Check(g_packetPreparation->ticket!=0);
    FinishPacketPreparation();
    // Early prepass must not interpret the live-END shader as frame-start.
    StartPacketPreparation(fixture->params,false);
    Check(EarlyPacketParametersMatch(fixture->params));
    auto changed=fixture->params;changed.transX+=1.f;
    Check(!EarlyPacketParametersMatch(changed));
    const auto earlyWait=g_packetPreparation->queue.Wait(g_packetPreparation->ticket);
    Check(earlyWait.complete);
    Check(PreparedVertexPacket(0,true)==nullptr);
    Check(PreparedVertexPacket(1,true)==nullptr); // shader command, not an upload
    Check(PreparedVertexPacket(2,true)!=nullptr);
    {PacketPreparationLease lease;}
    Check(!g_packetPreparation->ticket && !g_packetPreparation->early);
    g_rec[2].a[4]=UINT32_MAX;StartPacketPreparation(fixture->params);Check(!g_packetPreparation->ticket);
    // Actual production prepass + exact ready consumer. Keep this cost visible:
    // worker compute alone excludes the copies and metadata scan on the owner.
    for(unsigned packetCount:{2048u,8192u}) {
    g_rec.resize(packetCount);g_recData.resize(packetCount*sizeof(raw));
    for(unsigned i=0;i<packetCount;++i){auto& r=g_rec[i];r.op=OP_SetVSConstF;r.a[0]=0;r.a[1]=17;r.a[4]=i*sizeof(raw);
        const float px=1.2f,py=1.6f;memcpy(&r.a[2],&px,4);memcpy(&r.a[3],&py,4);
        memcpy(g_recData.data()+r.a[4],raw,sizeof(raw));}
    double prepass=0,consume=0;
    for(unsigned frame=0;frame<33;++frame){
        StartPacketPreparation(fixture->params);Check(g_packetPreparation->ticket!=0);
        const auto wait2=g_packetPreparation->queue.Wait(g_packetPreparation->ticket);Check(wait2.complete&&!wait2.firstFailure);
        const auto start=std::chrono::steady_clock::now();
        for(unsigned i=0;i<packetCount;++i)Check(PreparedVertexPacket(i,true)!=nullptr);
        const auto end=std::chrono::steady_clock::now();
        if(frame){prepass+=g_packetPreparation->prepareMs;consume+=std::chrono::duration<double,std::milli>(end-start).count();}
        FinishPacketPreparation();
    }
    printf("OFFLINE actual owner snapshot+submit %.3f ms + ready consumer %.3f ms / %u packets (warmed mean, copies included; no driver/game timing)\n",prepass/32,consume/32,packetCount);
    }
    g_fullPacketPrepareWorker=false;StartPacketPreparation(fixture->params);Check(!g_packetPreparation->ticket);
}
void Configure(Job& job,unsigned preset) {
    auto& p=job.params;p.mode=2;p.transX=.031f;p.transY=-.009f;p.fovScaleX=.77f;p.fovScaleY=.81f;
    p.uiX=p.fovScaleX;p.uiY=p.fovScaleY;p.projectionFix=g_projectionFix;
#ifdef D2VR_MATCHED_CAPTURE
    NativeCameraUiScale(p.uiX,p.uiY);
#endif
    p.diagnosticSkipVs=g_diagnosticSkipVs;
    p.scale2DContent=g_scale2DContent;p.fixMotionVectorProjection=g_fixMotionVectorProjection;
    p.lightingPreset=g_lightingPreset=int(preset);p.headGeometry=preset%2==0;
    for(int k=0;k<16;++k)p.headClip[k]=(k%5==0)?1.f:0.f;
    p.headClip[12]=.013f;
}
void Fill(Job& job,unsigned count) {
    job.Reset(count);job.initial.Clear();job.packets.reserve(count);
    float values[68*4];
    for(unsigned i=0;i<count;++i) {
        Packet p;p.known=true;p.shader.analyzed=true;p.shader.wvpBase=p.shader.activeWvp=0;
        p.shader.stpBase=p.shader.activeStp=8;p.px=1.2f;p.py=1.6f;
        p.first=0;p.count=17;
        for(unsigned k=0;k<68*4;++k)values[k]=float(int((i*13+k*7)%57)-25)/41.f;
        for(unsigned b:{0u,8u}) {
            float* m=values+b*4;
            for(unsigned k=0;k<16;++k)m[k]=k%5==0?1.f:0.f;
            m[0]=1.2f;m[5]=1.6f;m[11]=1.f;m[15]=0.f;m[12]=float(i%31)/7.f;
        }
        if(i%11==1){p.first=1;p.count=2;}
        if(i%11==2){p.first=9;p.count=2;}
        if(i%11==3){p.shader.namedConstants=true;p.shader.reflected=true;p.shader.reflectedWvp=0;p.shader.reflectedStp=8;}
        if(i%11==4){p.shader.hash=0x73EBA724u;p.shader.reflectedPreviousWvp=4;}
        if(i%11==5){p.count=68;}
        if(i%11==6){p.known=false;}
        if(i%11==7){p.shader.analyzed=false;p.shader.wvpBase=p.shader.activeWvp=-1;p.shader.stpBase=p.shader.activeStp=-1;}
        if(i%11==8){values[3]=values[7]=values[11]=0;values[0]=values[5]=.001f;}
        if(i%11==9){p.clear=true;p.count=0;p.known=false;}
        if(i%23==0){const uint32_t qnan=0x7fc12345u;memcpy(values,&qnan,4);}
        if(i%23==1)values[0]=-0.f;
        if(i%17==1)d2vr_math::RebiasScreenTextureProjection(values+8*4);
        Check(job.Add(i,p,values));
    }
    job.Seal();
}
void Oracle(const Job& job,std::vector<Output>& result) {
    auto bank=job.initial;result.resize(job.packets.size());
    for(size_t i=0;i<job.packets.size();++i) {
        const auto& p=job.packets[i];auto& o=result[i];o.first=p.first;o.count=p.count;o.modified=false;o.effects={};
        if(p.clear){bank.Clear();continue;}
        const auto* src=job.input.data()+p.offset;
        bank.Write(p.first,p.count,src,size_t(p.count)*16);
        if(!p.known || !p.count || p.count>68)continue;
        const d2vr_shader_registers::Reference ref(bank,p.shader.activeWvp,p.shader.activeStp,p.first,p.count);
        VsInfo vi;static_cast<Shader&>(vi)=p.shader;
        auto sp=job.params;sp.projectionPx=p.px;sp.projectionPy=p.py;
        effects={};g_shiftCount=0;
        o.modified=::ApplyVertexStereoUpload(o.values,src,o.first,o.count,&vi,sp,&ref);
        o.effects=effects;o.effects.shiftCount=unsigned(g_shiftCount);
    }
}
int main() {
    Queue queue;
    unsigned originalFp=0,unused=0;_controlfp_s(&originalFp,0,0);
    for(unsigned rounding:{_RC_NEAR,_RC_DOWN,_RC_UP,_RC_CHOP}) {
    _controlfp_s(&unused,rounding,_MCW_RC);
    for(unsigned preset=0;preset<=8;++preset) {
        g_projectionFix=preset%4;g_scale2DContent=preset%2==0;g_diagnosticSkipVs=preset==4?0x73EBA724u:0u;
        auto job=std::make_shared<Job>();Configure(*job,preset);Fill(*job,4096);
        std::vector<Output> expected;Oracle(*job,expected);
        const auto ticket=queue.Submit(job);Check(ticket.accepted);Check(queue.Wait(ticket.ticket).complete);
        for(size_t i=0;i<expected.size();++i) {
            const auto& a=expected[i];const auto& b=job->output[i];
            Check(a.modified==b.modified && a.first==b.first && a.count==b.count);
            Check(a.effects.shiftCount==b.effects.shiftCount && a.effects.publish==b.effects.publish);
            if(a.effects.publish)Check(!memcmp(&a.effects.px,&b.effects.px,8));
            if(a.modified)Check(!memcmp(a.values,b.values,size_t(a.count)*16));
        }
        // Consumer never waits for an individual packet and rejects changed FP state.
        Check(job->Try(0)!=nullptr);Check(job->Try(4096)==nullptr);
        unsigned original=0;_controlfp_s(&original,0,0);unsigned ignored=0;
        _controlfp_s(&ignored,rounding==_RC_DOWN?_RC_UP:_RC_DOWN,_MCW_RC);
        Check(job->Try(0)==nullptr);_controlfp_s(&ignored,original,_MCW_RC);
        job->Reset(1);Packet invalid;invalid.count=257;Check(!job->Add(0,invalid,nullptr));
    }
    }
    _controlfp_s(&unused,originalFp,_MCW_RC);
    for(unsigned packetCount:{2048u,8192u}) {
    auto job=std::make_shared<Job>();Configure(*job,0);Fill(*job,packetCount);
    const auto t0=std::chrono::steady_clock::now();
    for(int i=0;i<80;++i)job->Run();
    const auto t1=std::chrono::steady_clock::now();
    for(int i=0;i<80;++i){job->ready.store(0);const auto t=queue.Submit(job);Check(queue.Wait(t.ticket).complete);}
    const auto t2=std::chrono::steady_clock::now();
    printf("PASS packet checks=%u; %u packet serial %.3f ms / worker+submit+join %.3f ms (owned input already assembled; not FPS)\n",checks,packetCount,
        std::chrono::duration<double,std::milli>(t1-t0).count()/80,
        std::chrono::duration<double,std::milli>(t2-t1).count()/80);
    }
    Lifecycle();printf("PASS actual packet lifecycle, ownership, fallback, unknown-state and config guards; checks=%u\n",checks);
}

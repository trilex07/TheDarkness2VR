#include "../../src/replay_prepare_worker.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <limits>
#define NOMINMAX
#include <windows.h>
unsigned checks=0,published=0;
void check(bool ok){++checks;if(!ok){printf("FAIL %u\n",checks);exit(1);}}
struct StereoParams : d2vr_stereo::EyeShiftParams {
    bool projectionSnapshot=true;float projectionPx=1.2f,projectionPy=1.6f;
};
bool g_cameraProjectionKnown=false;float g_cameraPx=0,g_cameraPy=0;
void PublishProjectionEstimate(float,float){++published;}
// Extracted verbatim from the production .inl by build-test.ps1.
#include "d2vr_prepare_wrapper.inl"
using UINT=unsigned int;
enum {OP_SetVertexShader=1,OP_ApplyStateBlock,OP_SetVSConstF};
struct Record{unsigned op=0;unsigned a[5]{};};
std::vector<Record> g_rec;std::vector<unsigned char> g_recData;
struct Shader{int wvp=0,stp=4;};
std::shared_ptr<Shader> g_activeVsOwner=std::make_shared<Shader>();
struct Registry{std::shared_ptr<Shader> Lookup(void*){return g_activeVsOwner;}};
Registry g_vsInfo;
int ActiveWvpRegister(const Shader* s){return s->wvp;}
int ActiveStpRegister(const Shader* s){return s->stp;}
bool g_recOverflow=false,g_replayPrepareWorker=true;
void Log(const char*,...){}
#include "d2vr_prepare_lifecycle.inl"
void testLifecycle(){
    StereoParams sp;g_recData.resize(256*128);g_rec.resize(256);
    for(unsigned i=0;i<256;++i){auto& r=g_rec[i];r.op=OP_SetVSConstF;r.a[0]=0;r.a[1]=8;r.a[4]=i*128;
        memcpy(&r.a[2],&sp.projectionPx,4);memcpy(&r.a[3],&sp.projectionPy,4);
        for(unsigned k=0;k<32;++k){float f=float(k)/7.f;memcpy(g_recData.data()+i*128+k*4,&f,4);}}
    StartReplayPreparation(sp);check(g_prepareCurrent!=nullptr);check(g_prepareCurrent->tasks.size()==768);
    check(g_prepareCurrent->tasks[2].record==0&&g_prepareCurrent->tasks[3].record==1);
    auto retained=g_prepareState->job;
    memset(g_recData.data(),0,g_recData.size()); // worker retains its own inputs
    check(retained->tasks[0].input[1]!=0.f);
    FinishReplayPreparation();check(!g_prepareCurrent&&!g_prepareState->ticket);
    check(g_prepareState->spare==retained); // completed buffer reused, never raced
    g_rec[0].op=OP_ApplyStateBlock;
    StartReplayPreparation(sp);check(!g_prepareCurrent); // unknown shader -> no speculation
    g_rec[1].op=OP_SetVertexShader;
    StartReplayPreparation(sp);check(g_prepareCurrent&&g_prepareCurrent->tasks.front().record==2);
    FinishReplayPreparation();
    for(auto& r:g_rec){r.op=OP_SetVSConstF;r.a[4]=UINT(g_recData.size());}
    StartReplayPreparation(sp);check(!g_prepareCurrent); // truncated payloads
    for(auto& r:g_rec){r.a[4]=0;r.a[1]=3;}
    StartReplayPreparation(sp);check(!g_prepareCurrent); // partial constants remain owner-only
    g_recOverflow=true;StartReplayPreparation(sp);check(!g_prepareCurrent);g_recOverflow=false;
    for(auto& r:g_rec){r.a[1]=8;r.a[4]=0;}
    g_rec.resize(10000,g_rec.back());StartReplayPreparation(sp);
    check(g_prepareCurrent&&g_prepareCurrent->tasks.size()<=d2vr_prepare::MaxTasks);
    FinishReplayPreparation();g_replayPrepareWorker=false;
    StartReplayPreparation(sp);check(!g_prepareCurrent);
}
int main(){
    d2vr_prepare::Queue queue;
    unsigned original=0;_controlfp_s(&original,0,0);
    for(unsigned rounding:{_RC_NEAR,_RC_DOWN,_RC_UP,_RC_CHOP}) {
        unsigned unused=0;_controlfp_s(&unused,rounding,_MCW_RC);
        auto job=std::make_shared<d2vr_prepare::Job>();job->CaptureFp();
        for(size_t i=0;i<4096;++i){
            d2vr_prepare::Task t;t.record=i;
            for(unsigned k=0;k<16;++k)t.input[k]=float(int((i*23+k*17)%147)-70)/19.0f;
            auto& p=t.params;p.mode=int(i%3);p.separation=.031f;p.convergence=2.7f;
            p.transX=.125f;p.transY=-.25f;p.fovScaleX=1.05f;p.fovScaleY=.92f;
            p.headGeometry=(i%2)!=0;
            for(unsigned k=0;k<16;++k)p.headClip[k]=(k%5==0)?1.f:.007f*float(k);
            t.cameraKnown=(i%5)!=0;t.px=1.2f;t.py=1.6f;job->tasks.push_back(t);
        }
        float before[16];memcpy(before,job->tasks[0].input,sizeof(before));
        check(!job->Try(0,before,job->tasks[0].params,false,1.2f,1.6f)); // not ready
        check(!memcmp(before,job->tasks[0].input,sizeof(before)));
        auto accepted=queue.Submit(job);check(accepted.accepted);
        auto done=queue.Wait(accepted.ticket);check(done.complete&&!done.firstFailure);
        check(job->ready==job->tasks.size());
        for(const auto& t:job->tasks){
            float serial[16],parallel[16];memcpy(serial,t.input,sizeof(serial));memcpy(parallel,t.input,sizeof(parallel));
            d2vr_stereo::ApplyEyeShiftToMatrix(serial,4,t.params,t.cameraKnown,t.px,t.py,t.params.headGeometry?t.params.headClip:nullptr);
            check(job->Try(t.record,parallel,t.params,t.cameraKnown,t.px,t.py));
            check(!memcmp(serial,parallel,sizeof(serial)));
            auto changed=t.params;changed.transX+=1.f;memcpy(parallel,t.input,sizeof(parallel));
            check(!job->Try(t.record,parallel,changed,t.cameraKnown,t.px,t.py));
            check(!memcmp(parallel,t.input,sizeof(parallel)));
        }
        unsigned current=0;_controlfp_s(&current,0,0);check((current&_MCW_RC)==rounding);
    }
    unsigned unused=0;_controlfp_s(&unused,original,_MCW_RC);
    // Actual owner wrapper: cached and serial paths publish the same projection.
    auto job=std::make_shared<d2vr_prepare::Job>();job->CaptureFp();
    StereoParams sp;sp.mode=1;sp.separation=.03f;sp.convergence=3.f;sp.headGeometry=true;
    for(unsigned i=0;i<16;++i)sp.headClip[i]=(i%5==0)?1.f:0.f;
    d2vr_prepare::Task t;t.record=47;t.params=sp;t.cameraKnown=true;t.px=sp.projectionPx;t.py=sp.projectionPy;
    for(unsigned i=0;i<16;++i)t.input[i]=float(i)/11.f;
    job->tasks.push_back(t);auto a=queue.Submit(job);check(queue.Wait(a.ticket).complete);
    float serial[16],cached[16];memcpy(serial,t.input,sizeof(serial));memcpy(cached,t.input,sizeof(cached));
    g_prepareCurrent=nullptr;ApplyEyeShiftToMatrix(serial,4,sp);
    g_prepareCurrent=job.get();g_prepareRecord=47;ApplyEyeShiftToMatrix(cached,4,sp);
    check(published==2&&g_prepareHits==1&&!memcmp(serial,cached,sizeof(serial)));
    g_prepareRecord=48;memcpy(cached,t.input,sizeof(cached));ApplyEyeShiftToMatrix(cached,4,sp);
    check(g_prepareMisses==1&&!memcmp(serial,cached,sizeof(serial)));
    g_prepareCurrent=nullptr;
    unsigned previousFp=0;_controlfp_s(&previousFp,0,0);
    unsigned changedFp=0;_controlfp_s(&changedFp,_RC_DOWN,_MCW_RC);
    memcpy(cached,t.input,sizeof(cached));check(!job->Try(47,cached,t.params,t.cameraKnown,t.px,t.py));
    _controlfp_s(&changedFp,previousFp,_MCW_RC);
    t.params.transX=0.f;auto changed=t.params;changed.transX=-0.f;
    check(!d2vr_prepare::Same(t,t.input,changed,t.cameraKnown,t.px,t.py));
    auto cancelled=std::make_shared<d2vr_prepare::Job>();cancelled->CaptureFp();cancelled->tasks.resize(d2vr_prepare::MaxTasks,t);cancelled->cancel=true;
    a=queue.Submit(cancelled);check(queue.Wait(a.ticket).complete&&cancelled->ready==0);
    // Queue accepts a fresh frame after a cancelled one, with no stale outputs.
    auto next=std::make_shared<d2vr_prepare::Job>();next->CaptureFp();next->tasks.push_back(t);
    a=queue.Submit(next);check(queue.Wait(a.ticket).complete&&next->ready==1);
    for(float special:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-0.f}){
        auto edge=std::make_shared<d2vr_prepare::Job>();edge->CaptureFp();auto input=t;input.input[0]=special;edge->tasks.push_back(input);
        a=queue.Submit(edge);check(queue.Wait(a.ticket).complete);
        float expected[16],actual[16];memcpy(expected,input.input,sizeof(expected));memcpy(actual,input.input,sizeof(actual));
        d2vr_stereo::ApplyEyeShiftToMatrix(expected,4,input.params,input.cameraKnown,input.px,input.py,input.params.headGeometry?input.params.headClip:nullptr);
        check(edge->Try(input.record,actual,input.params,input.cameraKnown,input.px,input.py));check(!memcmp(expected,actual,sizeof(actual)));
    }
    // Cost sanity check only: all outputs already ready, no D3D9/engine work.
    // This is NOT a frame-time/FPS benchmark and excludes the preparation pass.
    volatile float sink=0;
    auto begin=std::chrono::steady_clock::now();
    for(unsigned i=0;i<100000;++i){float m[16];memcpy(m,job->tasks[0].input,sizeof(m));g_prepareCurrent=nullptr;ApplyEyeShiftToMatrix(m,4,sp);sink=m[12];}
    auto middle=std::chrono::steady_clock::now();
    for(unsigned i=0;i<100000;++i){float m[16];memcpy(m,job->tasks[0].input,sizeof(m));g_prepareCurrent=job.get();g_prepareRecord=47;ApplyEyeShiftToMatrix(m,4,sp);sink=m[12];}
    auto end=std::chrono::steady_clock::now();g_prepareCurrent=nullptr;
    printf("OFFLINE ready-output consumption only: serial %.3f ns/op, cached %.3f ns/op (excludes prepass/scheduling; sink %.4f)\n",
        std::chrono::duration<double,std::nano>(middle-begin).count()/100000,
        std::chrono::duration<double,std::nano>(end-middle).count()/100000,float(sink));
    testLifecycle();
    printf("PASS %u x86 worker/actual wrapper/lifecycle checks; bitwise agreement in four FP rounding modes, exact keys, ownership, bounds, cancellation and fallback\n",checks);
}

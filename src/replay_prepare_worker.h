#pragma once
#include "stereo_geometry.h"
#include "render_command_worker.h"
#include <atomic>
#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>
#include <float.h>
#include <xmmintrin.h>

namespace d2vr_prepare {
constexpr size_t MaxTasks=8192;
constexpr unsigned FpMask=_MCW_RC|_MCW_PC|_MCW_EM|_MCW_IC;
inline bool Bits(float a,float b) {return !memcmp(&a,&b,sizeof(a));}
struct Task {
    size_t record=0;
    float input[16]{},output[16]{};
    d2vr_stereo::EyeShiftParams params;
    bool cameraKnown=false;
    float px=0,py=0;
};
inline bool Same(const Task& t,const float* m,const d2vr_stereo::EyeShiftParams& p,
                 bool known,float px,float py) {
    const auto& a=t.params;
    return t.cameraKnown==known && Bits(t.px,px) && Bits(t.py,py) &&
        Bits(a.separation,p.separation) && Bits(a.convergence,p.convergence) && a.mode==p.mode &&
        Bits(a.transX,p.transX) && Bits(a.transY,p.transY) && Bits(a.fovScaleX,p.fovScaleX) &&
        Bits(a.fovScaleY,p.fovScaleY) && a.headGeometry==p.headGeometry &&
        !memcmp(a.headClip,p.headClip,sizeof(a.headClip)) && !memcmp(t.input,m,sizeof(t.input));
}
struct Job {
    std::vector<Task> tasks;
    std::atomic<size_t> ready{0};
    std::atomic<bool> cancel{false};
    unsigned fp=0,mxcsr=0;
    // Only after the previous ticket completed: retain RAM capacity across frames.
    void Reset(){tasks.clear();ready.store(0,std::memory_order_relaxed);cancel.store(false,std::memory_order_relaxed);}
    void CaptureFp() {unsigned unused=0;_controlfp_s(&unused,0,0);fp=unused;mxcsr=_mm_getcsr();}
    void Run() {
        unsigned saved=0;_controlfp_s(&saved,0,0);const unsigned sse=_mm_getcsr();
        unsigned unused=0;_controlfp_s(&unused,fp,FpMask);_mm_setcsr(mxcsr);
        for(size_t i=0;i<tasks.size() && !cancel.load(std::memory_order_relaxed);++i) {
            auto& t=tasks[i];memcpy(t.output,t.input,sizeof(t.input));
            d2vr_stereo::ApplyEyeShiftToMatrix(t.output,4,t.params,t.cameraKnown,t.px,t.py,
                t.params.headGeometry?t.params.headClip:nullptr);
            ready.store(i+1,std::memory_order_release);
        }
        _controlfp_s(&unused,saved,FpMask);_mm_setcsr(sse);
    }
    bool Try(size_t record,float* matrix,const d2vr_stereo::EyeShiftParams& p,bool known,float px,float py) const {
        unsigned current=0;_controlfp_s(&current,0,0);
        if((current&FpMask)!=(fp&FpMask) || ((_mm_getcsr()^mxcsr)&~0x3fu))return false;
        // tasks sorted by record; immutable input even while output is computed.
        const auto begin=std::lower_bound(tasks.begin(),tasks.end(),record,
            [](const Task& t,size_t r){return t.record<r;});
        const size_t completed=ready.load(std::memory_order_acquire);
        for(auto it=begin;it!=tasks.end() && it->record==record;++it) {
            if(size_t(it-tasks.begin())>=completed)break;
            if(Same(*it,matrix,p,known,px,py)){memcpy(matrix,it->output,sizeof(it->output));return true;}
        }
        return false;
    }
};
struct Backend {bool Execute(const std::shared_ptr<Job>& job){job->Run();return true;}};
using Queue=d2vr_render_worker::Queue<Backend,std::shared_ptr<Job>,1>;
}

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../../src/projection_estimate_window.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>
#include <algorithm>

// The previous production function is extracted verbatim by the build script.
struct Previous {
    float s_estHistX[8]{},s_estHistY[8]{};
    int s_estHistN=0;unsigned s_estHistNext=0;
    float g_estPx=1.347f,g_estPy=1.347f;
    volatile LONG g_estValid=0;
#include "previous_projection_estimate.inl"
};
static unsigned checks=0;
static void Check(bool ok) {++checks;if(!ok){std::printf("FAIL check %u\n",checks);std::exit(1);}}
struct Input {float x,y;};
static std::vector<Input> Inputs(unsigned mode,unsigned count) {
    std::vector<Input> result;result.reserve(count);
    unsigned rng=193483;
    for(unsigned i=0;i<count;++i) {
        rng=rng*1664525u+1013904223u;
        const float a=float(rng%10001)/2000.f+0.25f;
        rng=rng*1664525u+1013904223u;
        const float b=float(rng%10001)/1000.f+0.25f;
        result.push_back(mode==0 ? Input{1.347f,2.1f} :
            mode==1 ? Input{float(i%9)+0.3f,float((i/9)%17)+0.3f} : Input{a,b});
    }
    return result;
}
static void Compare(const std::vector<Input>& input) {
    Previous old;d2vr_projection_estimate::Window next;
    float x=old.g_estPx,y=old.g_estPy;
    for(unsigned i=0;i<input.size();++i) {
        // Exact camera observations can overwrite published estimates between
        // uploads. An unchanged sliding window must still republish its median.
        if(i%11==0){x=old.g_estPx=7.0f;y=old.g_estPy=9.0f;}
        old.g_estValid=0;
        old.PublishProjectionEstimate(input[i].x,input[i].y);
        const bool ready=next.Push(input[i].x,input[i].y,x,y);
        Check(ready==(old.g_estValid!=0));
        Check(!std::memcmp(&x,&old.g_estPx,4) && !std::memcmp(&y,&old.g_estPy,4));
    }
}
static volatile float sink;
static double Measure(const std::vector<Input>& input,bool previous) {
    LARGE_INTEGER frequency,begin,end;QueryPerformanceFrequency(&frequency);
    double best=1e30;
    for(unsigned trial=0;trial<7;++trial) {
        Previous old;d2vr_projection_estimate::Window next;float x=0,y=0;
        QueryPerformanceCounter(&begin);
        for(const auto& p:input) {
            if(previous)old.PublishProjectionEstimate(p.x,p.y);
            else if(next.Push(p.x,p.y,x,y))InterlockedExchange(&old.g_estValid,1);
            sink=previous ? old.g_estPx+old.g_estPy : x+y;
        }
        QueryPerformanceCounter(&end);
        best=(std::min)(best,double(end.QuadPart-begin.QuadPart)*1e9/frequency.QuadPart/input.size());
    }
    return best;
}
int main() {
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    Compare({{1,2},{1,2},{1,2},{0.2f,2},{8,2},{1,0.2f},{1,16},{nan,1},{1,nan},{inf,1},{1,inf},
        {0,0},{-1,1},{1,-inf},{2,3},{3,4},{4,5},{5,6},{6,7},{7,8},{1,9},{2,10}});
    for(unsigned mode=0;mode<3;++mode) {
        const auto input=Inputs(mode,1048576);
        Compare(input);
        std::printf("synthetic mode=%u old=%.2f ns/upload new=%.2f ns/upload (best 7, x86 /O2)\n",
            mode,Measure(input,true),Measure(input,false));
    }
    std::printf("PASS projection estimator: %u comparisons; exact previous-source output\n",checks);
}

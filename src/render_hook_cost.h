#pragma once
#include <windows.h>
#include <cstdint>
namespace d2vr_hook_cost {
enum Stage { VertexConstants,PixelConstants,RecordEmit,RecordPayload,PreparedPacket,DrawTotal,DrawDriver,TextureBinding,RenderState,SamplerState,VertexShader,PixelShader,Count };
#ifdef D2VR_MATCHED_CAPTURE
inline bool enabled=false;
struct Entry {uint64_t calls=0,samples=0,ticks=0;};
inline thread_local Entry entries[Count];
inline thread_local uint32_t randomState=0x47b5d21;
class Scope {
    Stage stage_;LARGE_INTEGER start_{};bool sampled_=false;
public:
    explicit Scope(Stage stage):stage_(stage){
        if(!enabled)return;
        ++entries[stage].calls;
        // Sampling phase varies across stages/calls instead of aliasing every
        // 64th material. No thread counters or per-call logging on this path.
        randomState=randomState*1664525u+1013904223u;
        sampled_=(randomState>>26)==0 && QueryPerformanceCounter(&start_);
    }
    ~Scope(){if(sampled_){LARGE_INTEGER end{};if(QueryPerformanceCounter(&end) && end.QuadPart>=start_.QuadPart){auto& e=entries[stage_];++e.samples;e.ticks+=end.QuadPart-start_.QuadPart;}}}
};
template<class Log> void Report(Log log,uint64_t frame){
    if(!enabled)return;
    static thread_local ULONGLONG start=GetTickCount64();
    static thread_local uint64_t firstFrame=frame;
    auto now=GetTickCount64();if(now-start<5000)return;
    LARGE_INTEGER frequency{};if(!QueryPerformanceFrequency(&frequency))return;
    const char* names[]={"native VS constants","native PS constants","record command","record payload","prepared vertex lookup","draw total","draw driver","texture binding","render state","sampler state","vertex shader binding","pixel shader binding"};
    for(unsigned s=0;s<Count;++s){const auto& e=entries[s];if(e.samples){
        double mean=double(e.ticks)*1000.0/double(frequency.QuadPart)/double(e.samples);
        log("[HookCost] %s calls=%llu samples=%llu mean=%.6fms estimated=%.3fms frames=%llu elapsed=%llums; sampled wall, nested scopes NOT additive",names[s],e.calls,e.samples,mean,mean*double(e.calls),frame-firstFrame,now-start);
    }entries[s]={};}
    start=now;firstFrame=frame;
}
#else
class Scope {public:explicit Scope(Stage){}};
#endif
}

#pragma once
#include "replay_packet_core.h"
#include "replay_prepare_worker.h"

namespace d2vr_packet_prepare {
constexpr size_t MaxPackets=8192,MaxInputFloats=2*1024*1024;
struct Packet {
    unsigned first=0,count=0,offset=0;
    float px=0,py=0;
    Shader shader;
    bool known=false,clear=false;
};
struct Output {
    unsigned first=0,count=0;
    bool modified=false;
    Effects effects;
    float values[76*4]; // Written before release publication, not zeroed per frame.
    Output() noexcept {} // Avoid value-initializing the unused matrix storage.
};
struct Job {
    Params params;
    d2vr_shader_registers::Bank initial;
    std::vector<Packet> packets;
    std::vector<float> input;
    std::vector<Output> output;
    std::vector<unsigned> recordToPacket;
    std::atomic<size_t> ready{0};
    std::atomic<bool> cancel{false};
    unsigned fp=0,mxcsr=0;
    void Reset(size_t records) {
        packets.clear();input.clear();
        recordToPacket.assign(records,~0u);
        ready.store(0,std::memory_order_relaxed);cancel.store(false,std::memory_order_relaxed);
        unsigned current=0;_controlfp_s(&current,0,0);fp=current;mxcsr=_mm_getcsr();
    }
    bool Add(size_t record,const Packet& p,const float* values) {
        if(record>=recordToPacket.size() || packets.size()>=MaxPackets ||
           p.first>256 || p.count>256-p.first || input.size()>MaxInputFloats-size_t(p.count)*4 ||
           (p.count && !values))return false;
        Packet owned=p;owned.offset=unsigned(input.size());
        if(p.count)input.insert(input.end(),values,values+size_t(p.count)*4);
        recordToPacket[record]=unsigned(packets.size());packets.push_back(owned);return true;
    }
    // Called once after Add, before Submit. Capacity is reused only after Wait.
    void Seal(){output.resize(packets.size());}
    void Run() {
        unsigned saved=0;_controlfp_s(&saved,0,0);const unsigned sse=_mm_getcsr();
        unsigned unused=0;_controlfp_s(&unused,fp,d2vr_prepare::FpMask);_mm_setcsr(mxcsr);
        auto bank=initial;
        for(size_t i=0;i<packets.size() && !cancel.load(std::memory_order_relaxed);++i) {
            const auto& p=packets[i];auto& out=output[i];
            out.first=p.first;out.count=p.count;out.modified=false;out.effects={};
            if(p.clear)bank.Clear();
            else {
                const float* src=p.count?input.data()+p.offset:nullptr;
                bank.Write(p.first,p.count,src,size_t(p.count)*16);
                if(p.known && p.count && p.count<=68) {
                    const d2vr_shader_registers::Reference reference(bank,p.shader.activeWvp,
                        p.shader.activeStp,p.first,p.count);
                    auto local=params;local.projectionPx=p.px;local.projectionPy=p.py;local.effects=&out.effects;
                    // Core only observes its immutable shader snapshot.
                    auto shader=p.shader;
                    out.modified=ApplyVertexStereoUpload(out.values,src,out.first,out.count,&shader,local,&reference);
                }
            }
            if((i&15)==15 || i+1==packets.size())ready.store(i+1,std::memory_order_release);
        }
        _controlfp_s(&unused,saved,d2vr_prepare::FpMask);_mm_setcsr(sse);
    }
    const Output* Try(size_t record) const {
        unsigned current=0;_controlfp_s(&current,0,0);
        if((current&d2vr_prepare::FpMask)!=(fp&d2vr_prepare::FpMask) || ((_mm_getcsr()^mxcsr)&~0x3fu) ||
           record>=recordToPacket.size())return nullptr;
        const auto index=recordToPacket[record];
        if(index>=ready.load(std::memory_order_acquire) || !packets[index].known)return nullptr;
        return &output[index];
    }
};
struct Backend {bool Execute(const std::shared_ptr<Job>& job){job->Run();return true;}};
using Queue=d2vr_render_worker::Queue<Backend,std::shared_ptr<Job>,1>;
}

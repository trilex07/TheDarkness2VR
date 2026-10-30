#pragma once
#include <cstdint>
#include <cstddef>
// Single writer, fixed capacity, no COM ownership or renderer synchronization.
// Publication to the export worker is supplied by the integration's release/acquire.
namespace d2vr_command_history {
enum Op : uint32_t { Draw=1, DrawIndexed, DrawUP, DrawIndexedUP, CreateTexture,
    CreateVS, CreatePS, ProducerFlush, Blit, StateBlock, Clear, UpdateSurface, UpdateTexture, ColorFill, StateRead };
struct Pipeline {
    uint32_t vs=0, ps=0, declaration=0, fvf=0, rt[4]{}, depth=0;
    uint32_t textures[8]{}, render[16]{}, sampler[6]{};
    uint32_t known=0, renderKnown=0, samplerKnown=0;
};
struct Event {
    uint64_t sequence=0, begin=0, end=0;
    uint32_t frame=0, draw=0, tid=0, generation=0, device=0;
    uint32_t operation=0, eye=0, caller=0, args[6]{}, result=0, uncertain=0, resource=0;
    Pipeline pipeline{};
};
template<size_t N> struct Ring {
    Event entries[N]{};
    uint64_t written=0, overwritten=0, missingEnds=0, trigger=0;
    bool frozen=false;
    uint64_t Begin(Event event) {
        if(frozen)return 0;
        event.sequence=++written;
        if(written>N)++overwritten;
        entries[(written-1)%N]=event;
        return written;
    }
    void End(uint64_t ticket,uint64_t qpc,uint32_t result,uint64_t stallThreshold,bool uncertain=false) {
        if(!ticket || frozen)return;
        auto& e=entries[(ticket-1)%N];
        if(e.sequence!=ticket){++missingEnds;return;}
        e.end=qpc;e.result=result;e.uncertain=uncertain;
        if(qpc>=e.begin && qpc-e.begin>=stallThreshold){trigger=ticket;frozen=true;}
    }
    size_t Count()const{return written<N?size_t(written):N;}
    const Event& At(size_t chronological)const {
        return entries[((written> N?written-N:0)+chronological)%N];
    }
};
}

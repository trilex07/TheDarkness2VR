#pragma once
#include <cstdint>

// Incremental 64-bit signature of the live state a D3D9 driver specializes a
// shader pipeline on (the same fields pipeline_warmup::State records). Setter
// hooks update one slot in O(1); a draw combines it with the VS/PS identities.
//
// Measured need (pipeline-warmup.bin, 1530 records): 365 of 538 VS/PS pairs
// occur with several compile-relevant states, mostly sampler texture
// format/type/sRGB. Keying learning by the pointer PAIR captured only the
// first state per session, so every other variant compiled on first use in
// gameplay (enemy/effect spawns), surfacing as 21-55 ms producer-fence stalls.
//
// The signature only TRIGGERS a capture; the recorded Record always comes
// from the real device state. A stale slot can cost one redundant capture or
// delay a variant, never record wrong state. Pure: no Windows/D3D calls.
namespace d2vr_pipeline_warmup {
struct Signature {
    enum Slot : unsigned {
        Declaration=0, Target0=1, Depth=5, SrgbWrite=6, AlphaTest=7, AlphaFunc=8, Fog=9, ClipPlanes=10,
        Frequency0=11, Texture0=15, SamplerSrgb0=31, Count=47
    };
    static constexpr unsigned Targets=4, Streams=4, Samplers=16;
    uint64_t part[Count]{};
    uint64_t value=0;
    static uint64_t Mix(unsigned slot,uint64_t v) {
        uint64_t x=v+0x9e3779b97f4a7c15ull*(uint64_t(slot)+1); // splitmix64 finalizer
        x^=x>>30;x*=0xbf58476d1ce4e5b9ull;x^=x>>27;x*=0x94d049bb133111ebull;x^=x>>31;
        return x;
    }
    void Set(unsigned slot,uint64_t v) {
        if(slot>=Count) return;
        const uint64_t mixed=Mix(slot,v);
        value^=part[slot]^mixed;part[slot]=mixed;
    }
    void Reset() {for(auto& p:part)p=0;value=0;}
    uint64_t Key(uintptr_t vs,uintptr_t ps) const {
        return value^Mix(Count,uint64_t(vs))^Mix(Count+1,uint64_t(ps));
    }
};
// Capture work admitted per presented frame while discovering new keys.
// A deferred key is retried on its next draw; it is never marked seen.
constexpr unsigned CapturesPerFrame=16;
}

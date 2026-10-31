#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <random>
#include <unordered_set>
#include <vector>
#include "../src/pipeline_signature.h"
#define CHECK(x) do {if(!(x)){std::printf("FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using d2vr_pipeline_warmup::Signature;
int main() {
    unsigned checks=0;
    // Order independence: the same final state gives the same key whatever
    // sequence of setter calls produced it (incremental XOR bookkeeping).
    std::mt19937_64 rng(7);
    for(unsigned trial=0;trial<20000;++trial) {
        uint64_t target[Signature::Count];for(auto& v:target)v=rng()%5;
        Signature a,b;
        for(unsigned s=0;s<Signature::Count;++s)a.Set(s,target[s]);
        for(unsigned step=0;step<200;++step)b.Set(unsigned(rng()%Signature::Count),rng()%5); // noise
        for(int s=Signature::Count-1;s>=0;--s)b.Set(unsigned(s),target[s]);
        CHECK(a.value==b.value && a.Key(0x1000,0x2000)==b.Key(0x1000,0x2000));++checks;
    }
    // Every compile-relevant slot changes the key; VS and PS are distinct roles.
    {
        Signature base;for(unsigned s=0;s<Signature::Count;++s)base.Set(s,1);
        std::unordered_set<uint64_t> keys{base.Key(1,2)};
        for(unsigned s=0;s<Signature::Count;++s){Signature v=base;v.Set(s,2);CHECK(keys.insert(v.Key(1,2)).second);++checks;}
        CHECK(base.Key(1,2)!=base.Key(2,1) && base.Key(1,2)!=base.Key(1,3) && base.Key(1,2)!=base.Key(3,2));checks+=3;
        Signature same=base;same.Set(Signature::AlphaTest,1);CHECK(same.Key(1,2)==base.Key(1,2));++checks; // idempotent
        Signature out=base;out.Set(Signature::Count,9);CHECK(out.value==base.value);++checks;             // out of range ignored
    }
    // Model of the measured problem: one VS/PS pair drawn with V sampler/alpha
    // variants per session. Pair keying captures only the first variant seen;
    // signature keying captures each variant exactly once.
    for(unsigned variants:{1u,2u,4u,8u,16u}) {
        std::unordered_set<uint64_t> pairSeen,sigSeen;unsigned pairCaptures=0,sigCaptures=0;
        for(unsigned session=0;session<3;++session)for(unsigned draw=0;draw<5000;++draw) {
            const unsigned v=unsigned(rng()%variants);
            Signature s;s.Set(Signature::Texture0+5,0x31545844ull+v);s.Set(Signature::AlphaTest,v&1);
            if(pairSeen.insert((uint64_t(0x1000)<<32)|0x2000).second)++pairCaptures;
            if(sigSeen.insert(s.Key(0x1000,0x2000)).second)++sigCaptures;
        }
        CHECK(pairCaptures==1 && sigCaptures==variants);checks+=2;
    }
    // Collision sanity over 2M random distinct states (64-bit keys).
    {
        std::unordered_set<uint64_t> keys;keys.reserve(2100000);
        for(unsigned i=0;i<2000000;++i){Signature s;s.Set(Signature::Texture0,i);s.Set(Signature::Target0,i>>7);CHECK(keys.insert(s.Key(i&255,i>>3)).second);}
        checks+=2000000;
    }
    std::printf("PASS pipeline signature: %u checks (order independence, per-slot sensitivity, variant capture model, 2M collision-free keys)\n",checks);
    return 0;
}

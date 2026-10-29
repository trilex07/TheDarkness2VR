#pragma once
#include <cstdint>
#include <cstring>
namespace d2vr_replay_state {
// Replay-local knowledge only. Starts empty after frame restore; failed calls
// never establish state. No resource pointers, driver queries or allocations.
class Cache {
    uint32_t values_[744];
    uint64_t valid_[12]{};
public:
    static int Render(unsigned type){return type<256?int(type):-1;}
    static int Sampler(unsigned slot,unsigned type){return slot<16 && type>0 && type<14?int(256+slot*14+type):-1;}
    static int Stage(unsigned slot,unsigned type){return slot<8 && type>0 && type<33?int(480+slot*33+type):-1;}
    void Clear(){std::memset(valid_,0,sizeof(valid_));}
    void Invalidate(int key){if(key>=0)valid_[unsigned(key)/64]&=~(uint64_t(1)<<(unsigned(key)%64));}
    template<class Send> long Forward(int key,uint32_t value,Send&& send){
        if(key<0){Clear();return send();}
        const auto bit=uint64_t(1)<<(unsigned(key)%64);
        auto& mask=valid_[unsigned(key)/64];
        if((mask&bit) && values_[key]==value)return 0;
        const long result=send();
        if(result>=0){values_[key]=value;mask|=bit;}else mask&=~bit;
        return result;
    }
};
// Exact bindings inside one replay. The command ledger/snapshots already own
// every identity until replay ends. Never retain this knowledge across restores.
class Bindings {
    struct Entry {uintptr_t a=0,b=0,c=0;};
    Entry values_[57];
    uint64_t valid_=0;
public:
    static constexpr int VertexShader=53,PixelShader=54,Declaration=55;
    static int Texture(unsigned slot){return slot<16?int(slot):slot>=256 && slot<=260?int(16+slot-256):-1;}
    static int Stream(unsigned slot){return slot<16?int(21+slot):-1;}
    static int Frequency(unsigned slot){return slot<16?int(37+slot):-1;}
    static constexpr int Indices=56;
    void Clear(){valid_=0;}
    void Invalidate(int key){if(key>=0 && key<=Indices)valid_&=~(uint64_t(1)<<key);}
    template<class Send> long Forward(int key,uintptr_t a,uintptr_t b,uintptr_t c,Send&& send){
        if(key<0 || key>Indices){Clear();return send();}
        const uint64_t bit=uint64_t(1)<<key;
        auto& entry=values_[key];
        if((valid_&bit) && entry.a==a && entry.b==b && entry.c==c)return 0;
        const long result=send();
        if(result>=0){entry={a,b,c};valid_|=bit;}
        else Invalidate(key);
        return result;
    }
};

}

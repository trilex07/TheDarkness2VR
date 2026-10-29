#pragma once
#include <cstdint>
#include <cstring>
namespace d2vr_replay_state {
// Replay-local knowledge only. Starts empty after frame restore; failed calls
// never establish state. No resource pointers, driver queries or allocations.
class Cache {
    bool enabled_=true;
    uint32_t values_[744];
    uint64_t valid_[12]{};
public:
    static int Render(unsigned type){return type<256?int(type):-1;}
    static int Sampler(unsigned slot,unsigned type){return slot<16 && type>0 && type<14?int(256+slot*14+type):-1;}
    static int Stage(unsigned slot,unsigned type){return slot<8 && type>0 && type<33?int(480+slot*33+type):-1;}
    void Configure(bool enabled){enabled_=enabled;Clear();}
    void Clear(){std::memset(valid_,0,sizeof(valid_));}
    void Invalidate(int key){if(key>=0)valid_[unsigned(key)/64]&=~(uint64_t(1)<<(unsigned(key)%64));}
    template<class Send> long Forward(int key,uint32_t value,Send&& send){
        if(!enabled_)return send();
        if(key<0){Clear();return send();}
        const auto bit=uint64_t(1)<<(unsigned(key)%64);
        auto& mask=valid_[unsigned(key)/64];
        if((mask&bit) && values_[key]==value)return 0;
        const long result=send();
        if(result>=0){values_[key]=value;mask|=bit;}else mask&=~bit;
        return result;
    }
};
}

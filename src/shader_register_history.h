#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace d2vr_shader_registers {
// D3D9 float registers belong to the DEVICE, not the currently bound shader.
// Values here are raw recorded native payloads, never adjusted eye constants.
// Production accesses are serialized by the existing recorder lock.
struct Bank {
    float values[256*4]{};
    uint64_t valid[4]{};
    void Clear(){for(auto& word:valid)word=0;}
    void Invalidate(unsigned first,unsigned count) {
        if(first>256 || count>256-first){Clear();return;}
        for(unsigned i=first;i<first+count;++i)valid[i/64]&=~(uint64_t(1)<<(i%64));
    }
    bool Write(unsigned first,unsigned count,const void* owned,size_t bytes) {
        if(first>256 || count>256-first || bytes!=size_t(count)*16 || (!owned && count)) {
            Invalidate(first,count);return false;
        }
        if(count)std::memcpy(values+first*4,owned,bytes);
        for(unsigned i=first;i<first+count;++i)valid[i/64]|=uint64_t(1)<<(i%64);
        return true;
    }
    bool Matrix(int first,float out[16])const {
        if(first<0 || first>252)return false;
        for(unsigned i=unsigned(first);i<unsigned(first)+4;++i)
            if(!(valid[i/64]&(uint64_t(1)<<(i%64))))return false;
        std::memcpy(out,values+first*4,16*sizeof(float));return true;
    }
};
struct History {
    Bank live,begin,replay;
    void BeginRecord(){begin=live;}
    void BeginReplay(){replay=begin;}
    void Clear(){live.Clear();begin.Clear();replay.Clear();}
};
struct Reference {
    float values[16]{};
    bool valid=false;
    Reference()=default;
    Reference(const Bank& bank,int first):valid(bank.Matrix(first,values)){}
};
}

#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace d2vr_shader_registers {
// D3D9 float registers belong to the DEVICE, not the currently bound shader.
// History's live/begin/replay banks contain raw recorded native payloads;
// eyeCarry/eyeNext contain the replay eye's materialized device values.
// Production accesses are serialized by the existing recorder lock.
struct Bank {
    float values[256*4]{};
    uint64_t valid[4]{};
    void Clear(){for(auto& word:valid)word=0;}
private:
    // Caller validates the 256-register range. Touch at most four bitmap words,
    // rather than rebuilding one bit for every row of a bone-palette upload.
    void SetValidity(unsigned first,unsigned count,bool present) {
        while(count) {
            const unsigned bit=first%64;
            const unsigned rows=count<64-bit ? count : 64-bit;
            const uint64_t mask=(~uint64_t(0)>>(64-rows))<<bit;
            if(present)valid[first/64]|=mask;
            else valid[first/64]&=~mask;
            first+=rows;count-=rows;
        }
    }
public:
    void Invalidate(unsigned first,unsigned count) {
        if(first>256 || count>256-first){Clear();return;}
        SetValidity(first,count,false);
    }
    bool Write(unsigned first,unsigned count,const void* owned,size_t bytes) {
        if(first>256 || count>256-first || bytes!=size_t(count)*16 || (!owned && count)) {
            Invalidate(first,count);return false;
        }
        if(count)std::memcpy(values+first*4,owned,bytes);
        SetValidity(first,count,true);
        return true;
    }
    bool Matrix(int first,float out[16])const {
        if(first<0 || first>252)return false;
        for(unsigned i=unsigned(first);i<unsigned(first)+4;++i)
            if(!(valid[i/64]&(uint64_t(1)<<(i%64))))return false;
        std::memcpy(out,values+first*4,16*sizeof(float));return true;
    }
    template<class F> void ForEachValidRun(unsigned limit,F&& f)const {
        if(limit>256)limit=256;
        for(unsigned i=0;i<limit;) {
            if(!(valid[i/64]&(uint64_t(1)<<(i%64)))){++i;continue;}
            unsigned end=i+1;
            while(end<limit && (valid[end/64]&(uint64_t(1)<<(end%64))))++end;
            f(i,values+i*4,end-i);i=end;
        }
    }
};
struct History {
    Bank live,begin,replay;
    // Unlike the raw banks, eyeCarry/eyeNext hold values the REPLAY eye forwarded.
    // Frame-start state restores the live eye's adjusted registers; draws that
    // inherit an earlier frame's upload need the replay eye's own values.
    // Do not transform these values again with a new pose: both eyes inherit
    // their own device state until a native upload replaces the affected rows.
    Bank eyeCarry,eyeNext;
    bool recordOpen=false;
    void BeginRecord(){if(recordOpen)eyeCarry.Clear();recordOpen=true;begin=live;}
    void BeginReplay(){replay=begin;eyeNext=eyeCarry;}
    void ReplayStateBlock(){replay.Clear();eyeNext.Clear();}
    void EndReplay(bool complete){if(complete)eyeCarry=eyeNext;else eyeCarry.Clear();recordOpen=false;}
    // Native writes with no replay counterpart change the live device rows.
    void InvalidateUnrecorded(unsigned first,unsigned count){live.Invalidate(first,count);eyeCarry.Invalidate(first,count);}
    void Clear(){live.Clear();begin.Clear();replay.Clear();eyeCarry.Clear();eyeNext.Clear();recordOpen=false;}
};
struct Reference {
    float values[16]{};
    bool valid=false;
    // Only populated for an overlapping partial auxiliary projection. Normal
    // uploads do not read/copy another matrix from the raw register bank.
    float partialValues[16]{};
    int partialBase=-1;
    Reference()=default;
    Reference(const Bank& bank,int first,int auxiliary=-1,unsigned start=0,unsigned count=0)
        :valid(bank.Matrix(first,values)) {
        if(auxiliary>=0 && auxiliary<=252 && auxiliary!=first && count &&
           start<=256 && count<=256-start && start<unsigned(auxiliary)+4 && start+count>unsigned(auxiliary) &&
           !(start<=unsigned(auxiliary) && start+count>=unsigned(auxiliary)+4) &&
           bank.Matrix(auxiliary,partialValues))partialBase=auxiliary;
    }
};
}

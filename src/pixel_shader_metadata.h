#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace d2vr_shader_metadata {
template<class Word> bool ValidPixelCode(const Word* code,size_t words) {
    static_assert(sizeof(Word)==4,"D3D9 token size");
    if(!code || words<2 || words>8192 || (code[0]>>16)!=0xffff || ((code[0]>>8)&255)<2)return false;
    for(size_t i=1;i<words;) {
        const auto token=code[i];
        if((token&0xffff)==0xffff)return i+1==words;
        const size_t size=(token&0xffff)==0xfffe?(token>>16)&0x7fff:(token>>24)&15;
        if(size>=words-i)return false;
        i+=1+size;
    }
    return false;
}
struct Pixel {
    uint32_t hash=0;
    int inverse=-1,extents=-1;
};

// Creation may run on an engine worker while the render thread binds/replays.
// Publish one complete value; never expose independent maps or borrowed nodes.
// This lock is independent of the recorder and is held only for a map access,
// never a driver call, reflection, logging or command replay.
class Pixels {
    SRWLOCK lock_=SRWLOCK_INIT;
    std::unordered_map<const void*,Pixel> entries_;
    struct Write {
        SRWLOCK* lock;
        explicit Write(SRWLOCK& value):lock(&value){AcquireSRWLockExclusive(lock);}
        ~Write(){ReleaseSRWLockExclusive(lock);}
    };
public:
    Pixels()=default;
    Pixels(const Pixels&)=delete;
    Pixels& operator=(const Pixels&)=delete;
    void Publish(const void* shader,const Pixel& value) {
        if(!shader)return;
        Write guard(lock_);
        entries_[shader]=value;
    }
    Pixel Lookup(const void* shader) {
        if(!shader)return {};
        AcquireSRWLockShared(&lock_);
        const auto found=entries_.find(shader);
        const Pixel result=found==entries_.end()?Pixel{}:found->second;
        ReleaseSRWLockShared(&lock_);
        return result;
    }
};
}

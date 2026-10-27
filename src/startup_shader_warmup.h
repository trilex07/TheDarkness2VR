#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace d2vr_startup_shader {
constexpr uint32_t Magic = 0x31575344; // DSW1
constexpr size_t MaxBytes = 4u * 1024u * 1024u;
constexpr uint32_t MaxShaders = 256, MaxWords = 8192;
inline uint32_t Word(const unsigned char* p) { uint32_t v; std::memcpy(&v,p,4); return v; }
// Structural bounds only. D3D9 still validates instruction/register semantics.
inline bool ValidShader(const unsigned char* p, size_t words) {
    if(words<2 || words>MaxWords) return false;
    const auto version=Word(p);
    if(version!=0xfffe0300u && version!=0xffff0300u) return false;
    for(size_t i=1;i<words;) {
        const auto token=Word(p+4*i), op=token&0xffffu;
        if(op==0xffffu) return token==0x0000ffffu && i+1==words;
        const size_t n=op==0xfffeu ? ((token>>16)&0x7fffu) : ((token>>24)&15u);
        if(n>words-i-1) return false;
        i+=n+1;
    }
    return false;
}
inline bool Validate(const unsigned char* data, size_t bytes) {
    if(!data || bytes<8 || bytes>MaxBytes || Word(data)!=Magic) return false;
    const uint32_t count=Word(data+4);
    if(!count || count>MaxShaders) return false;
    size_t pos=8;
    for(uint32_t i=0;i<count;++i) {
        if(bytes-pos<4) return false;
        const uint32_t words=Word(data+pos);pos+=4;
        if(words>MaxWords || size_t(words)>(bytes-pos)/4 || !ValidShader(data+pos,words)) return false;
        pos+=size_t(words)*4;
    }
    return pos==bytes;
}
struct Result { uint32_t attempted=0, succeeded=0, failed=0; bool valid=false, timedOut=false; };
// Ops owns the valid device on its creation thread. It releases every created
// object immediately, never binds a shader and never touches native shader slots.
// A driver call itself cannot be preempted by this between-call time budget.
template<class Ops> Result Run(const unsigned char* data,size_t bytes,uint64_t budgetMs,Ops& ops) {
    Result r;
    if(!Validate(data,bytes)) return r; // whole package admitted before any calls
    r.valid=true;
    const auto start=ops.Now(); size_t pos=8;
    for(uint32_t i=0;i<Word(data+4);++i) {
        if(ops.Now()-start>=budgetMs) {r.timedOut=true;break;}
        const uint32_t words=Word(data+pos);pos+=4;
        ++r.attempted;
        if(ops.CreateAndRelease(data+pos,words)) ++r.succeeded; else ++r.failed;
        pos+=size_t(words)*4;
        if(r.failed) break; // no speculative retries after rejection/device loss
    }
    return r;
}
}

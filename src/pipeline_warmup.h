#pragma once
#include "startup_shader_warmup.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <type_traits>
#include <vector>

// Pipelines observed at their first live draw: exact VS/PS bytecode plus the
// public device state a D3D9 driver may specialize on. Replayed at startup
// with the SAME pooled shader objects the engine later receives.
namespace d2vr_pipeline_warmup {
constexpr uint32_t Magic=0x31575050; // PPW1
// A full 1024-record file stopped learning, so every later launch compiled the
// omitted pipelines on their first gameplay draw. Old PPW1 files still parse.
constexpr uint32_t MaxRecords=4096, MaxElements=32, Samplers=16, Streams=4, Targets=4;
constexpr size_t MaxBytes=32u*1024u*1024u;
constexpr uint32_t VertexVersion=0xfffe0300u, PixelVersion=0xffff0300u;

// D3DVERTEXELEMENT9 layout, D3DDECL_END excluded.
struct Element { uint16_t stream, offset; uint8_t type, method, usage, usageIndex; };
static_assert(sizeof(Element)==8,"D3DVERTEXELEMENT9 layout");
// type is D3DRESOURCETYPE: 0 none, 3 texture, 4 volume, 5 cube.
struct Texture { uint32_t type, format, usage, srgb; };
struct State {
    uint32_t targets[Targets];
    uint32_t depth;
    uint32_t srgbWrite, alphaTest, alphaFunc, fog, clipPlanes;
    uint32_t frequency[Streams];
    Texture textures[Samplers];
    uint32_t elementCount;
    Element elements[MaxElements];
};
static_assert(std::is_trivially_copyable<State>::value && sizeof(State)%4==0,"State is serialized as words");
constexpr uint32_t StateWords=uint32_t(sizeof(State)/4);

inline State EmptyState() {
    State s;std::memset(&s,0,sizeof(s));
    s.alphaFunc=8;for(auto& f:s.frequency)f=1;
    return s;
}
inline uint32_t TypeBytes(uint8_t type) {
    static const uint8_t bytes[17]={4,8,12,16,4,4,4,8,4,4,8,4,8,4,4,4,8};
    return type<17?bytes[type]:0;
}
inline uint32_t StreamStride(const State& s,uint32_t stream) {
    uint32_t stride=0;
    for(uint32_t i=0;i<s.elementCount;++i)
        if(s.elements[i].stream==stream)stride=std::max(stride,uint32_t(s.elements[i].offset)+TypeBytes(s.elements[i].type));
    return stride;
}
inline bool ValidFrequency(uint32_t v) {
    const uint32_t indexed=0x40000000u,instance=0x80000000u,count=v&0x3fffffffu;
    if(v==1) return true;
    if((v&indexed) && !(v&instance)) return count>0;
    if((v&instance) && !(v&indexed)) return count>0;
    return false;
}
inline bool ValidState(const State& s) {
    if(!s.targets[0] || !s.elementCount || s.elementCount>MaxElements) return false;
    if(s.srgbWrite>1 || s.alphaTest>1 || s.fog>1 || s.alphaFunc<1 || s.alphaFunc>8 || s.clipPlanes>0x3f) return false;
    for(uint32_t i=0;i<Streams;++i) if(!ValidFrequency(s.frequency[i])) return false;
    for(const auto& t:s.textures) {
        if(t.type!=0 && t.type!=3 && t.type!=4 && t.type!=5) return false;
        if(t.srgb>1 || (t.usage&~3u) || (!t.type && (t.format || t.usage))) return false;
        if(t.type==4 && t.usage) return false;
    }
    for(uint32_t i=0;i<MaxElements;++i) {
        const auto& e=s.elements[i];
        if(i>=s.elementCount) {
            if(e.stream || e.offset || e.type || e.method || e.usage || e.usageIndex) return false;
            continue;
        }
        if(e.stream>=Streams || e.type>16 || e.method>6 || e.usage>13 || e.offset>0x1000) return false;
    }
    return true;
}
struct Record {
    std::vector<uint32_t> vs, ps; // ps empty: null pixel shader binding
    State state;
    bool operator==(const Record& o) const {
        return vs==o.vs && ps==o.ps && !std::memcmp(&state,&o.state,sizeof(State));
    }
};
inline bool ValidCode(const std::vector<uint32_t>& code,uint32_t version) {
    return !code.empty() && code[0]==version && d2vr_startup_shader::ValidShader(
        reinterpret_cast<const unsigned char*>(code.data()),code.size());
}
inline bool ValidRecord(const Record& r) {
    return ValidCode(r.vs,VertexVersion) && (r.ps.empty() || ValidCode(r.ps,PixelVersion)) && ValidState(r.state);
}
inline size_t RecordWords(const Record& r) { return 2+r.vs.size()+r.ps.size()+StateWords; }

// Format: Magic, count, StateWords, then per record:
// vsWords, vs, psWords, ps, State words. The whole file is admitted or rejected.
inline bool Parse(const uint32_t* words,size_t count,std::vector<Record>& out) {
    out.clear();
    if(!words || count<3 || count>MaxBytes/4 || words[0]!=Magic || !words[1] ||
       words[1]>MaxRecords || words[2]!=StateWords) return false;
    size_t at=3;
    std::vector<Record> parsed;parsed.reserve(words[1]);
    for(uint32_t i=0;i<words[1];++i) {
        Record r;
        for(auto* code:{&r.vs,&r.ps}) {
            if(at>=count) return false;
            const uint32_t n=words[at++];
            if(n>d2vr_startup_shader::MaxWords || n>count-at) return false;
            code->assign(words+at,words+at+n);at+=n;
        }
        if(count-at<StateWords) return false;
        std::memcpy(&r.state,words+at,sizeof(State));at+=StateWords;
        if(!ValidRecord(r)) return false;
        parsed.push_back(std::move(r));
    }
    if(at!=count) return false;
    out=std::move(parsed);
    return true;
}

class Manifest {
    mutable std::mutex mutex_;
    std::vector<Record> records_;
    size_t words_=3;
    uint64_t version_=0;
public:
    enum class Admit { Added, Duplicate, Rejected };
    Admit Admit(const Record& r) {
        if(!ValidRecord(r)) return Admit::Rejected;
        std::lock_guard<std::mutex> lock(mutex_);
        for(const auto& existing:records_) if(existing==r) return Admit::Duplicate;
        if(records_.size()>=MaxRecords || (words_+RecordWords(r))*4>MaxBytes) return Admit::Rejected;
        records_.push_back(r);words_+=RecordWords(r);++version_;
        return Admit::Added;
    }
    bool Add(const Record& r) { return Admit(r)==Admit::Added; }
    size_t Size() const {std::lock_guard<std::mutex> lock(mutex_);return records_.size();}
    uint64_t Version() const {std::lock_guard<std::mutex> lock(mutex_);return version_;}
    std::vector<uint32_t> Serialize(uint64_t& version) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<uint32_t> out;out.reserve(words_);
        out.push_back(Magic);out.push_back(uint32_t(records_.size()));out.push_back(StateWords);
        for(const auto& r:records_) {
            out.push_back(uint32_t(r.vs.size()));out.insert(out.end(),r.vs.begin(),r.vs.end());
            out.push_back(uint32_t(r.ps.size()));out.insert(out.end(),r.ps.begin(),r.ps.end());
            const auto* s=reinterpret_cast<const uint32_t*>(&r.state);out.insert(out.end(),s,s+StateWords);
        }
        version=version_;return out;
    }
};

enum class Prepared { Ready, Unsupported, DeviceFailure };
struct Result { uint32_t records=0, warmed=0, missingObjects=0, unsupported=0, draws=0; bool timedOut=false, failed=false; };
// Ops supplies borrowed pool objects and executes on the device-creation
// thread. Every pooled copy of a recorded shader is drawn once per record,
// because the engine may receive any copy. No shader is created here.
template<class Ops> Result Warm(const std::vector<Record>& records,Ops& ops,uint64_t budgetMs) {
    Result r;r.records=uint32_t(records.size());
    const auto start=ops.Now();
    for(const auto& record:records) {
        if(ops.Now()-start>=budgetMs) {r.timedOut=true;break;}
        const auto vs=ops.Vertex(record.vs);
        auto ps=record.ps.empty()?decltype(ops.Pixel(record.ps)){nullptr}:ops.Pixel(record.ps);
        if(vs.empty() || ps.empty()) {++r.missingObjects;continue;}
        const auto prepared=ops.Prepare(record.state);
        if(prepared==Prepared::DeviceFailure) {r.failed=true;break;}
        if(prepared==Prepared::Unsupported) {++r.unsupported;continue;}
        const size_t n=std::max(vs.size(),ps.size());
        for(size_t i=0;i<n;++i) {
            if(!ops.Draw(vs[i%vs.size()],ps[i%ps.size()])) {r.failed=true;return r;}
            ++r.draws;
        }
        ++r.warmed;
    }
    return r;
}
}

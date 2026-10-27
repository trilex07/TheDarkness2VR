#pragma once
#include "startup_shader_warmup.h"
#include <array>
#include <mutex>
#include <vector>
#include <utility>

namespace d2vr_shader_preload {
constexpr size_t MaxShaders=1024;
inline bool ValidateArchive(const unsigned char* data,size_t bytes) {
    using namespace d2vr_startup_shader;
    if(!data || bytes<8 || bytes>MaxBytes || Word(data)!=Magic) return false;
    const uint32_t count=Word(data+4);if(!count || count>d2vr_shader_preload::MaxShaders)return false;
    size_t position=8;
    for(uint32_t i=0;i<count;++i) {
        if(bytes-position<4)return false;
        const uint32_t words=Word(data+position);position+=4;
        if(size_t(words)>(bytes-position)/4 || !ValidShader(data+position,words))return false;
        position+=size_t(words)*4;
    }
    return position==bytes;
}
// One precreated object can satisfy ONE creation request. This is an ownership
// transfer, not a shared shader-object cache. No hash-only identities.
template<class Object> class Pool {
    struct Entry {
        std::vector<uint32_t> code;
        uint64_t hash=0;
        Object* object=nullptr;
        int32_t result=0;
    };
    const void* device_;
    mutable std::mutex mutex_;
    std::vector<Entry> entries_;
    size_t bytes_=8;
    uint64_t version_=0;
    bool enabled_=true;
    static uint64_t Hash(const std::vector<uint32_t>& code) {
        uint64_t h=14695981039346656037ull;
        for(auto w:code) {h^=w;h*=1099511628211ull;} return h;
    }
    Entry* Find(const std::vector<uint32_t>& code) {
        const auto hash=Hash(code);
        for(auto& e:entries_) if(e.hash==hash && e.code==code) return &e;
        return nullptr;
    }
public:
    explicit Pool(const void* device):device_(device) {entries_.reserve(MaxShaders);}
    Pool(const Pool&)=delete;
    Pool& operator=(const Pool&)=delete;
    ~Pool() { ClearObjects(); }
    bool Remember(const void* device,const std::vector<uint32_t>& code) {
        if(device!=device_ || !d2vr_startup_shader::ValidShader(
            reinterpret_cast<const unsigned char*>(code.data()),code.size())) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(Find(code)) return true;
        const size_t bytes=4+code.size()*4;
        if(entries_.size()>=MaxShaders || bytes>d2vr_startup_shader::MaxBytes-bytes_) return false;
        entries_.push_back({code,Hash(code),nullptr,0}); bytes_+=bytes; ++version_;return true;
    }
    // Ownership moves only on true. Caller releases on rejection.
    bool Adopt(const void* device,const std::vector<uint32_t>& code,Object* object,int32_t result) {
        if(device!=device_ || !object || result<0) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        auto* e=Find(code);if(!e || e->object) return false;
        e->object=object;e->result=result;return true;
    }
    Object* Take(const void* device,const std::vector<uint32_t>& code,uint32_t version,int32_t& result) {
        if(device!=device_ || code.empty() || code[0]!=version) return nullptr;
        std::lock_guard<std::mutex> lock(mutex_);
        if(!enabled_) return nullptr;
        auto* e=Find(code);if(!e || !e->object) return nullptr;
        Object* object=e->object;e->object=nullptr;result=e->result;return object;
    }
    // No driver call under the pool mutex. Consumed objects belong to the game
    // and are never released here, including Reset and expiry.
    void ClearObjects() {
        std::array<Object*,MaxShaders> release{};size_t n=0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for(auto& e:entries_) if(e.object) {release[n++]=e.object;e.object=nullptr;}
        }
        for(size_t i=0;i<n;++i) release[i]->Release();
    }
    void ExpireOne() {
        Object* object=nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for(auto& e:entries_) if(e.object) {object=e.object;e.object=nullptr;break;}
        }
        if(object) object->Release();
    }
    size_t Size() const {std::lock_guard<std::mutex> lock(mutex_);return entries_.size();}
    size_t Pending() const {
        std::lock_guard<std::mutex> lock(mutex_);size_t count=0;
        for(const auto& entry:entries_)if(entry.object)++count;
        return count;
    }
    void Enable(bool enabled) {std::lock_guard<std::mutex> lock(mutex_);enabled_=enabled;}
    uint64_t Version() const {std::lock_guard<std::mutex> lock(mutex_);return version_;}
    std::vector<uint32_t> Archive(uint64_t& version) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<uint32_t> out;out.reserve(bytes_/4);
        out.push_back(d2vr_startup_shader::Magic);out.push_back(uint32_t(entries_.size()));
        for(const auto& e:entries_) {out.push_back(uint32_t(e.code.size()));out.insert(out.end(),e.code.begin(),e.code.end());}
        version=version_;return out;
    }
};
}

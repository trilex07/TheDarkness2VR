#pragma once
#include "startup_shader_warmup.h"
#include <array>
#include <algorithm>
#include <mutex>
#include <vector>
#include <utility>

namespace d2vr_shader_preload {
constexpr size_t MaxShaders=1024;
constexpr unsigned MaxCopies=8;
constexpr unsigned MaxObjects=2048;
constexpr uint32_t DemandMagic=0x32575344; // DSW2: quota, word count, exact code
enum class TakeStatus { Ineligible, Disabled, UnknownCode, Depleted, Transferred };
inline bool ValidateArchive(const unsigned char* data,size_t bytes) {
    using namespace d2vr_startup_shader;
    if(!data || bytes<8 || bytes>MaxBytes || (Word(data)!=Magic && Word(data)!=DemandMagic)) return false;
    const bool demand=Word(data)==DemandMagic;
    const uint32_t count=Word(data+4);if(!count || count>d2vr_shader_preload::MaxShaders)return false;
    size_t position=8;
    for(uint32_t i=0;i<count;++i) {
        if(demand) {
            if(bytes-position<4 || !Word(data+position) || Word(data+position)>MaxCopies)return false;
            position+=4;
        }
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
        std::array<Object*,MaxCopies> objects{};
        std::array<int32_t,MaxCopies> results{};
        unsigned count=0;
        unsigned quota=1,requests=0;
    };
    const void* device_;
    unsigned copies_;
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
    explicit Pool(const void* device,unsigned copies=1):device_(device),copies_(copies==2?2:1) {entries_.reserve(MaxShaders);}
    Pool(const Pool&)=delete;
    Pool& operator=(const Pool&)=delete;
    ~Pool() { ClearObjects(); }
    bool Remember(const void* device,const std::vector<uint32_t>& code,unsigned quota=1) {
        if(device!=device_ || !d2vr_startup_shader::ValidShader(
            reinterpret_cast<const unsigned char*>(code.data()),code.size())) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(Find(code)) return true;
        const size_t bytes=8+code.size()*4;
        if(entries_.size()>=MaxShaders || bytes>d2vr_startup_shader::MaxBytes-bytes_) return false;
        entries_.push_back({code,Hash(code)});
        entries_.back().quota=std::max(copies_,std::min(MaxCopies,std::max(1u,quota)));
        bytes_+=bytes; ++version_;return true;
    }
    // Ownership moves only on true. Caller releases on rejection.
    bool Adopt(const void* device,const std::vector<uint32_t>& code,Object* object,int32_t result) {
        if(device!=device_ || !object || result<0) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        auto* e=Find(code);if(!e || e->count>=e->quota) return false;
        e->objects[e->count]=object;e->results[e->count++]=result;return true;
    }
    Object* Take(const void* device,const std::vector<uint32_t>& code,uint32_t version,int32_t& result,
                 TakeStatus* status=nullptr) {
        if(status) *status=TakeStatus::Ineligible;
        if(device!=device_ || code.empty() || code[0]!=version) return nullptr;
        std::lock_guard<std::mutex> lock(mutex_);
        if(!enabled_) {if(status)*status=TakeStatus::Disabled;return nullptr;}
        auto* e=Find(code);
        if(!e) {if(status)*status=TakeStatus::UnknownCode;return nullptr;}
        if(!e->count) {if(status)*status=TakeStatus::Depleted;return nullptr;}
        const auto index=--e->count;
        Observe(*e);
        if(status)*status=TakeStatus::Transferred;
        Object* object=e->objects[index];e->objects[index]=nullptr;result=e->results[index];return object;
    }
    // Call only after a successful original creation, never after a failed API.
    // A session demand counter is independent of inventory and saturates safely.
    void ObserveCreation(const void* device,const std::vector<uint32_t>& code) {
        if(device!=device_)return;
        std::lock_guard<std::mutex> lock(mutex_);
        if(auto* e=Find(code))Observe(*e);
    }
private:
    void Observe(Entry& e) {
        if(e.requests<MaxCopies)++e.requests;
        if(e.requests>e.quota) {e.quota=e.requests;++version_;}
    }
public:
    // No driver call under the pool mutex. Consumed objects belong to the game
    // and are never released here, including Reset and expiry.
    void ClearObjects() {
        std::array<Object*,MaxShaders*MaxCopies> release{};size_t n=0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for(auto& e:entries_) while(e.count) {auto i=--e.count;release[n++]=e.objects[i];e.objects[i]=nullptr;}
        }
        for(size_t i=0;i<n;++i) release[i]->Release();
    }
    void ExpireOne() {
        Object* object=nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for(auto& e:entries_) if(e.count) {auto i=--e.count;object=e.objects[i];e.objects[i]=nullptr;break;}
        }
        if(object) object->Release();
    }
    size_t Size() const {std::lock_guard<std::mutex> lock(mutex_);return entries_.size();}
    size_t Pending() const {
        std::lock_guard<std::mutex> lock(mutex_);size_t count=0;
        for(const auto& entry:entries_)count+=entry.count;
        return count;
    }
    void Enable(bool enabled) {std::lock_guard<std::mutex> lock(mutex_);enabled_=enabled;}
    uint64_t Version() const {std::lock_guard<std::mutex> lock(mutex_);return version_;}
    std::vector<uint32_t> Archive(uint64_t& version) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<uint32_t> out;out.reserve(bytes_/4);
        out.push_back(DemandMagic);out.push_back(uint32_t(entries_.size()));
        for(const auto& e:entries_) {out.push_back(e.quota);out.push_back(uint32_t(e.code.size()));out.insert(out.end(),e.code.begin(),e.code.end());}
        version=version_;return out;
    }
};
}

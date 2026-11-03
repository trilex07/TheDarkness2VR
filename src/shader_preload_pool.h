#pragma once
#include "startup_shader_warmup.h"
#include <array>
#include <algorithm>
#include <mutex>
#include <vector>
#include <utility>
#include <unordered_map>
#include <numeric>

namespace d2vr_shader_preload {
constexpr size_t MaxShaders=4096;
constexpr size_t LegacyMaxShaders=1024;
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
// First cover each known code once. Then spend the bounded spare inventory
// on the highest observed demand, retaining recency order for equal demand.
inline std::vector<size_t> Inventory(const std::vector<unsigned>& quotas,bool optimized,unsigned budget=MaxObjects) {
    std::vector<size_t> out;out.reserve(budget);
    std::vector<size_t> order(quotas.size());std::iota(order.begin(),order.end(),size_t(0));
    for(auto i:order)if(quotas[i] && out.size()<budget)out.push_back(i);
    if(optimized) {
        std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){return quotas[a]>quotas[b];});
        for(auto i:order)for(unsigned n=1;n<std::min(MaxCopies,quotas[i]) && out.size()<budget;++n)out.push_back(i);
    } else for(unsigned n=1;n<MaxCopies;++n)
        for(auto i:order)if(n<quotas[i] && out.size()<budget)out.push_back(i);
    return out;
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
        uint64_t recent=0;
    };
    const void* device_;
    unsigned copies_;
    bool optimized_;
    uint64_t clock_=0;
    std::unordered_multimap<uint64_t,size_t> index_;
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
        if(optimized_) {
            const auto range=index_.equal_range(hash);
            for(auto at=range.first;at!=range.second;++at) {
                auto& e=entries_[at->second];if(e.code==code)return &e;
            }
        } else for(auto& e:entries_) if(e.hash==hash && e.code==code) return &e;
        return nullptr;
    }
public:
    explicit Pool(const void* device,unsigned copies=1,bool optimized=false):device_(device),copies_(copies==2?2:1),optimized_(optimized) {
        entries_.reserve(optimized?MaxShaders:LegacyMaxShaders);
        if(optimized)index_.reserve(MaxShaders);
    }
    Pool(const Pool&)=delete;
    Pool& operator=(const Pool&)=delete;
    ~Pool() { ClearObjects(); }
    bool Remember(const void* device,const std::vector<uint32_t>& code,unsigned quota=1) {
        if(device!=device_ || !d2vr_startup_shader::ValidShader(
            reinterpret_cast<const unsigned char*>(code.data()),code.size())) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(Find(code)) return true;
        const size_t bytes=8+code.size()*4;
        if(entries_.size()>=(optimized_?MaxShaders:LegacyMaxShaders) || bytes>d2vr_startup_shader::MaxBytes-bytes_) return false;
        entries_.push_back({code,Hash(code)});
        if(optimized_)try {index_.emplace(entries_.back().hash,entries_.size()-1);}
        catch(...) {entries_.pop_back();throw;}
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
        // Persist priority through archive ordering, retaining the DSW2 format.
        if(optimized_) {e.recent=++clock_;++version_;}
        if(e.requests<MaxCopies)++e.requests;
        if(e.requests>e.quota) {e.quota=e.requests;++version_;}
    }
public:
    // No driver call under the pool mutex. Consumed objects belong to the game
    // and are never released here, including Reset and expiry.
    void ClearObjects() {
        // Bounded stack, no allocating destructor and no driver call under lock.
        for(;;) {
            std::array<Object*,64> release{};size_t n=0;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for(auto& e:entries_) while(e.count && n<release.size()) {
                    auto i=--e.count;release[n++]=e.objects[i];e.objects[i]=nullptr;
                }
            }
            for(size_t i=0;i<n;++i)release[i]->Release();
            if(n<release.size())break;
        }
    }

    void ExpireOne() {
        Object* object=nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for(auto& e:entries_) if(e.count) {auto i=--e.count;object=e.objects[i];e.objects[i]=nullptr;break;}
        }
        if(object) object->Release();
    }
    // Borrowed pointers to still-pool-owned objects; ownership never moves.
    // Valid only while no Take/Clear can run, i.e. before device handoff.
    std::vector<Object*> PendingObjects(const void* device,const std::vector<uint32_t>& code) {
        std::vector<Object*> out;
        if(device!=device_) return out;
        std::lock_guard<std::mutex> lock(mutex_);
        if(auto* e=Find(code)) out.assign(e->objects.begin(),e->objects.begin()+e->count);
        return out;
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
        std::vector<size_t> order(entries_.size());std::iota(order.begin(),order.end(),size_t(0));
        if(optimized_)std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){
            return entries_[a].recent>entries_[b].recent;
        });
        for(auto i:order) {const auto& e=entries_[i];out.push_back(e.quota);out.push_back(uint32_t(e.code.size()));out.insert(out.end(),e.code.begin(),e.code.end());}
        version=version_;return out;
    }
};
}

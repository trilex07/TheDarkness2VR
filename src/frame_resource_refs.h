#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace d2vr_replay {
// One exact interface pointer needs one owned reference until replay finishes.
// This is a frame-lifetime ledger, NOT a resource/shader cache. Interface aliases
// remain distinct; no QueryInterface, hashing of shader bytes or cross-frame reuse.
// Caller serializes Retain/Clear with the record-stream lock.
template<class Resource>
class FrameResourceRefs {
public:
    FrameResourceRefs() = default;
    FrameResourceRefs(const FrameResourceRefs&) = delete;
    FrameResourceRefs& operator=(const FrameResourceRefs&) = delete;
    // Explicit Clear follows Reset/frame teardown; never call COM under DLL unload.
    ~FrameResourceRefs() = default;

    void Begin(bool unique) {
        Clear();
        unique_ = unique;
    }
    void Retain(Resource* resource) {
        if (!resource) return;
        ++requests_;
        if (unique_ && Contains(resource)) return;
        // Allocate bookkeeping before AddRef so allocation failure cannot leak
        // an owned COM reference. Insert grows transactionally before AddRef.
        owned_.push_back(resource);
        try { if(unique_) Insert(resource); }
        catch (...) {owned_.pop_back();throw;}
        resource->AddRef();
    }
    void Clear() {
        for (Resource* resource : owned_) resource->Release();
        owned_.clear();
        count_=0;
        if(++generation_==0) {for(auto& slot:seen_) slot.generation=0;generation_=1;}
        requests_ = 0;
    }
    std::size_t size() const { return owned_.size(); }
    std::size_t requests() const { return requests_; }
    bool unique() const { return unique_; }
private:
    bool unique_ = false;
    std::size_t requests_ = 0;
    std::vector<Resource*> owned_;
    struct Slot {Resource* key=nullptr;uint32_t generation=0;};
    std::vector<Slot> seen_; // retained flat storage; no per-bind node allocation
    uint32_t generation_=1;
    size_t count_=0;
    static size_t Hash(Resource* key) {
        return (reinterpret_cast<uintptr_t>(key)>>3)*size_t(2654435761u);
    }
    bool Contains(Resource* key) const {
        if(seen_.empty()) return false;
        size_t at=Hash(key)&(seen_.size()-1);
        for(size_t n=0;n<seen_.size();++n) {
            const auto& slot=seen_[at];
            if(slot.generation!=generation_) return false;
            if(slot.key==key) return true;
            at=(at+1)&(seen_.size()-1);
        }
        return false;
    }
    void Insert(Resource* key) {
        if(seen_.empty() || (count_*2>=seen_.size() && seen_.size()<65536)) {
            std::vector<Slot> grown(seen_.empty()?128:seen_.size()*2);
            for(Resource* object:owned_) {
                size_t at=Hash(object)&(grown.size()-1);
                while(grown[at].generation==generation_) at=(at+1)&(grown.size()-1);
                grown[at]={object,generation_};
            }
            seen_.swap(grown);count_=owned_.size();return;
        }
        // At the bounded 50% load limit, retain extra resources normally.
        // Missing dedup can add references but can never shorten their lifetime.
        if(count_*2>=seen_.size()) return;
        size_t at=Hash(key)&(seen_.size()-1);
        while(seen_[at].generation==generation_) at=(at+1)&(seen_.size()-1);
        seen_[at]={key,generation_};++count_;
    }
};
}

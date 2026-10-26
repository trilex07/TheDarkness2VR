#pragma once
#include <cstddef>
#include <unordered_set>
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
        if (unique_ && !seen_.insert(resource).second) return;
        // Allocate bookkeeping before AddRef so allocation failure cannot leak
        // an owned COM reference. Undo membership if vector growth fails.
        try { owned_.push_back(resource); }
        catch (...) { if (unique_) seen_.erase(resource); throw; }
        resource->AddRef();
    }
    void Clear() {
        for (Resource* resource : owned_) resource->Release();
        owned_.clear();
        seen_.clear();
        requests_ = 0;
    }
    std::size_t size() const { return owned_.size(); }
    std::size_t requests() const { return requests_; }
    bool unique() const { return unique_; }
private:
    bool unique_ = false;
    std::size_t requests_ = 0;
    std::vector<Resource*> owned_;
    std::unordered_set<Resource*> seen_;
};
}

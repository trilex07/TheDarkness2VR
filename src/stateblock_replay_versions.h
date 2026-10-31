#pragma once
#include "stateblock_lifetime.h"
#include <vector>
#include <unordered_map>

namespace d2vr_stateblock_lifetime {
// Immutable replay dependency set. A retained native reference prevents COM
// identity reuse; the captured version detects every intercepted Capture,
// including failed Capture and Capture from a synchronous driver callback.
// Does not make a mutable StateBlock into a snapshot: reject the entire pair
// whenever a dependency changes, BEFORE publishing an eye image.
// NativeScope releases recorder bookkeeping around all native COM calls.
// Caller must serialize this set on the render owner and clear it before Reset.
template<class NativeScope> class ReplayVersions {
    struct Lease { IDirect3DStateBlock9* block; Version version; };
    std::vector<Lease> leases_;
    std::unordered_map<Entry*,size_t> indices_;
    size_t capacity_;
    bool rejected_=false,busy_=false;
public:
    static constexpr size_t Invalid=size_t(-1);
    struct Result {HRESULT nativeResult=D3DERR_INVALIDCALL;bool executed=false,consistent=false;};
    explicit ReplayVersions(size_t capacity=kCapacity):capacity_(capacity<kCapacity?capacity:kCapacity){}
    ReplayVersions(const ReplayVersions&)=delete;
    ReplayVersions& operator=(const ReplayVersions&)=delete;
    ~ReplayVersions(){Clear();}
    size_t Size()const{return leases_.size();}
    bool Valid()const {
        if(rejected_ || busy_)return false;
        for(const auto& lease:leases_)if(!lease.version.Current())return false;
        return true;
    }
    size_t Retain(IDirect3DStateBlock9* block) {
        if(rejected_ || busy_){rejected_=true;return Invalid;}
        auto version=Observe(block);
        if(!version.Current()){rejected_=true;return Invalid;}
        const auto found=indices_.find(version.entry.get());
        if(found!=indices_.end()) {
            const auto& lease=leases_[found->second];
            if(lease.block==block && lease.version.generation==version.generation && Valid())return found->second;
            rejected_=true;return Invalid;
        }
        if(leases_.size()>=capacity_){rejected_=true;return Invalid;}
        busy_=true;
        {NativeScope native;block->AddRef();}
        busy_=false;
        if(rejected_ || !version.Current()) {
            rejected_=true;NativeScope native;block->Release();return Invalid;
        }
        const auto index=leases_.size();
        try {
            leases_.push_back({block,version});
            try {indices_.emplace(version.entry.get(),index);}
            catch(...) {leases_.pop_back();throw;}
        } catch(...) {
            rejected_=true;NativeScope native;block->Release();return Invalid;
        }
        return index;
    }
    Result Apply(size_t index) {
        if(index>=leases_.size() || !Valid()){rejected_=true;return {};}
        // No registry lock held across native Apply. Bypass the observer only
        // for replay: caller supplies the renderer mutation epoch/bypass scope.
        const auto& lease=leases_[index];
        const auto nativeApply=reinterpret_cast<Call>(lease.version.entry->original[5]);
        HRESULT result;busy_=true;
        {NativeScope native;result=nativeApply(lease.block);}
        busy_=false;
        const bool consistent=Valid();
        if(!consistent || FAILED(result))rejected_=true;
        // Keep native HRESULT distinct from the version proof. S_FALSE is
        // forwarded exactly, never replaced with a synthetic success result.
        return {result,true,consistent};
    }
    void Clear() {
        if(busy_){rejected_=true;return;}
        busy_=true;
        // Detach metadata BEFORE Release callbacks. A callback cannot observe
        // a half-retired dependency set or retain an address being destroyed.
        std::vector<Lease> retired;retired.swap(leases_);
        indices_.clear();rejected_=true;
        for(const auto& lease:retired){NativeScope native;lease.block->Release();}
        busy_=false;rejected_=false;
    }
};
}

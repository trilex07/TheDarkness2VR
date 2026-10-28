#pragma once
#include <windows.h>
#include <cstddef>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <utility>

namespace d2vr_shader_metadata {
// Immutable shader analysis plus render-thread-owned projection history.
// A replacement publishes a NEW node. An active binding retains its node,
// including its history, independently of map rehash/address recycling.
// Creation never changes an existing node's fields or projection history.
template<class Info> class Vertices {
    SRWLOCK lock_=SRWLOCK_INIT;
    std::unordered_map<const void*,std::shared_ptr<Info>> entries_;
    struct Write {
        SRWLOCK* lock;
        explicit Write(SRWLOCK& value):lock(&value){AcquireSRWLockExclusive(lock);}
        ~Write(){ReleaseSRWLockExclusive(lock);}
    };
    struct Read {
        SRWLOCK* lock;
        explicit Read(SRWLOCK& value):lock(&value){AcquireSRWLockShared(lock);}
        ~Read(){ReleaseSRWLockShared(lock);}
    };
public:
    Vertices()=default;
    Vertices(const Vertices&)=delete;
    Vertices& operator=(const Vertices&)=delete;
    std::shared_ptr<Info> Lookup(const void* shader) {
        if(!shader)return {};
        Read guard(lock_);
        const auto found=entries_.find(shader);
        auto result=found==entries_.end()?std::shared_ptr<Info>{}:found->second;
        return result;
    }
    void Bind(const void* shader,std::shared_ptr<Info>& binding) {
        std::shared_ptr<Info> selected;
        if(shader) {
            Read guard(lock_);
            const auto found=entries_.find(shader);
            if(found!=entries_.end()) {
                // Repeated bindings need neither allocation nor atomic refcount
                // changes. Check the CURRENT node, so address reuse still wins.
                if(binding.get()==found->second.get())return;
                selected=found->second;
            }
        }
        // Reclaim an old active node after releasing the registry lock.
        binding=std::move(selected);
    }
    template<class Word> bool Known(const void* shader,const Word* code,size_t words) {
        static_assert(sizeof(Word)==4,"D3D9 token size");
        if(!shader || !code || !words || words>8192)return false;
        const auto entry=Lookup(shader);
        return entry && entry->bytecode.size()==words &&
            !memcmp(entry->bytecode.data(),code,words*sizeof(Word));
    }
    bool Remember(const void* shader,Info& analyzed) {
        if(!shader)return false;
        auto replacement=std::make_shared<Info>(std::move(analyzed));
        Write guard(lock_);
        const auto previous=entries_.find(shader);
        // A still-live native deduplicated object must keep its ordered history.
        // Pointer/hash equality alone must never admit recycled shader metadata.
        if(previous!=entries_.end() && previous->second->bytecode==replacement->bytecode)return false;
        entries_[shader]=std::move(replacement);
        return true;
    }
    template<class Visit> void ForEach(Visit visit) {
        Read guard(lock_);
        for(auto& entry:entries_)visit(*entry.second);
    }
    size_t Size() {
        Read guard(lock_);
        const auto result=entries_.size();
        return result;
    }
};
}

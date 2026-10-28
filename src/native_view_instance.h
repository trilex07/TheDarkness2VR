#pragma once
#include <windows.h>
#include <cstring>
#include "native_image_guard.h"

namespace d2vr_native_view {
// An immutable process-lifetime copy of one SHA-pinned class table. Only the
// live instance's vptr is published; no executable/static table bytes change.
// No instance pointer is retained or dereferenced during destruction/unload.
class InstanceTable {
    void** source_=nullptr;
    void** clone_=nullptr;
    size_t count_=0,slot_=0;
    void* replacement_=nullptr;
public:
    bool Prepare(void** source,size_t count,size_t slot,void* replacement,
                 const d2vr_native_image::Guard& guard) {
        if(clone_)return source_==source && count_==count && slot_==slot && replacement_==replacement;
        if(!guard.enabled || !source || !replacement || slot>=count || count>256)return false;
        auto storage=static_cast<void**>(VirtualAlloc(nullptr,(count+1)*sizeof(void*),
                                                     MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        if(!storage)return false;
        if(!guard.Allows(reinterpret_cast<uintptr_t>(storage),(count+1)*sizeof(void*))) {
            VirtualFree(storage,0,MEM_RELEASE);return false;
        }
        // Preserve the word preceding the table too (RTTI/header convention).
        std::memcpy(storage,source-1,(count+1)*sizeof(void*));
        storage[slot+1]=replacement;
        DWORD old=0;
        if(!VirtualProtect(storage,(count+1)*sizeof(void*),PAGE_READONLY,&old)) {
            VirtualFree(storage,0,MEM_RELEASE);return false;
        }
        source_=source;clone_=storage+1;count_=count;slot_=slot;replacement_=replacement;return true;
    }
    bool Owns(void* instance)const {
        if(!instance || !clone_)return false;
        __try {return *static_cast<void***>(instance)==clone_;}
        __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    }
    bool Publish(void* instance,const d2vr_native_image::Guard& guard)const {
        if(!instance || !clone_ || reinterpret_cast<uintptr_t>(instance)%sizeof(void*) ||
           !guard.Allows(reinterpret_cast<uintptr_t>(instance),sizeof(void*)))return false;
        MEMORY_BASIC_INFORMATION memory{};
        if(!VirtualQuery(instance,&memory,sizeof(memory)) || memory.State!=MEM_COMMIT ||
           memory.Type!=MEM_PRIVATE || !(memory.Protect&(PAGE_READWRITE|PAGE_WRITECOPY)) ||
           (memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
        __try {
            auto previous=InterlockedCompareExchangePointer(static_cast<void*volatile*>(instance),clone_,source_);
            return previous==source_ || previous==clone_;
        } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    }
};
}

#pragma once
// A comparison-call adapter for owned code only. No native installer or patch.
#include <windows.h>
#include <cstdint>
#include <cstring>
#include "native_image_guard.h"
namespace d2vr_throw_compare {
using Compare=int(__cdecl*)(const float*,const float*,float);
using Resolve=bool(*)(void*,float*);
namespace detail {
inline Compare original=nullptr;
inline Resolve resolve=nullptr;
inline thread_local bool resolving=false;
static void __cdecl Apply(void* fire,float* velocity) {
    const auto error=GetLastError();
    if(!resolving) {resolving=true;resolve(fire,velocity);resolving=false;}
    SetLastError(error);
}
static __declspec(naked) int __cdecl Entry(const float*,const float*,float) {
    __asm {
        pushfd
        pushad
        mov esi,esp
        sub esp,528
        and esp,-16
        fxsave [esp]
        fninit
        push dword ptr [esi+40] // original first stack argument: velocity
        push dword ptr [esi]    // pinned release method's EDI: fire receiver
        call Apply
        add esp,8
        fxrstor [esp]
        mov esp,esi
        popad
        popfd
        jmp dword ptr [original] // original comparison once, original arguments
    }
}
}
class Bridge {
public:
    static bool Prepare(Compare original,Resolve resolve,const d2vr_native_image::Guard& guard) {
        static_assert(sizeof(void*)==4,"x86 only");
        const auto address=reinterpret_cast<uintptr_t>(original);
        if(!guard.enabled || !guard.base || guard.bytes<12 || !original || !resolve ||
           address<guard.base || address-guard.base>guard.bytes-12 ||
           !guard.Allows(reinterpret_cast<uintptr_t>(resolve),1) ||
           !guard.Allows(reinterpret_cast<uintptr_t>(&detail::Entry),1))return false;
        const unsigned char prologue[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x8b,0x45,0x08,0x0f,0x28,0x08};
        __try {if(std::memcmp(reinterpret_cast<const void*>(original),prologue,sizeof(prologue)))return false;}
        __except(EXCEPTION_EXECUTE_HANDLER){return false;}
        if(detail::original)return detail::original==original && detail::resolve==resolve;
        detail::original=original;detail::resolve=resolve;return true;
    }
    static void* Entry(){return reinterpret_cast<void*>(&detail::Entry);}
};
}

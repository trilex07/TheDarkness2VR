#pragma once
// An isolated replacement getter, not an installer. The host must prove the
// native owner and admit/pin a private instance table before publishing Entry.
#include <windows.h>
#include <cstdint>
#include <cstring>
#include "native_image_guard.h"

namespace d2vr_aim_getter {
using Getter=float*(__thiscall*)(void*);
using Resolve=bool(*)(void*,const float*,float*);
namespace detail {
inline Getter original=nullptr;
inline uintptr_t caller=0;
inline Resolve resolve=nullptr;
inline thread_local bool resolving=false;
inline __declspec(align(16)) thread_local float result[16];
static float* __cdecl Apply(float* base,void* self) {
    const auto error=GetLastError();
    float* output=base;
    if(!resolving) {
        resolving=true;
        if(resolve(self,base,result))output=result;
        resolving=false;
    }
    SetLastError(error);return output;
}
static __declspec(naked) float* __fastcall Entry(void*,void*) {
    __asm {
        // The native leaf changes only EAX. Even the caller filter's flags
        // must be undone on both paths. Unmatched callers tail-call it.
        pushfd
        mov eax,[esp+4]
        cmp eax,caller
        jne untouched
        popfd
        call dword ptr [original]
        pushfd
        pushad
        mov esi,esp
        sub esp,528
        and esp,-16
        fxsave [esp]
        fninit
        push dword ptr [esi+24] // original ECX receiver
        push dword ptr [esi+28] // native EAX matrix pointer
        call Apply
        add esp,8
        mov [esi+28],eax        // only EAX may differ from the native leaf
        fxrstor [esp]
        mov esp,esi
        popad
        popfd
        ret
    untouched:
        popfd
        jmp dword ptr [original]
    }
}
}
class Bridge {
public:
    // Single process-lifetime bridge. Host pin/digest and owner admission are
    // separate requirements; this method never writes native code or objects.
    static bool Prepare(Getter original,uintptr_t caller,Resolve resolve,
                        const d2vr_native_image::Guard& guard) {
        static_assert(sizeof(void*)==4,"x86 only");
        const auto address=reinterpret_cast<uintptr_t>(original);
        if(!guard.enabled || !guard.base || guard.bytes<7 || !original || !resolve ||
           address<guard.base || address-guard.base>guard.bytes-7 ||
           caller<guard.base || caller-guard.base>=guard.bytes ||
           !guard.Allows(reinterpret_cast<uintptr_t>(resolve),1) ||
           !guard.Allows(reinterpret_cast<uintptr_t>(&detail::Entry),1))return false;
        const unsigned char leaf[]={0x8d,0x81,0x80,0,0,0,0xc3};
        __try {if(std::memcmp(reinterpret_cast<const void*>(original),leaf,sizeof(leaf)))return false;}
        __except(EXCEPTION_EXECUTE_HANDLER){return false;}
        if(detail::original)return detail::original==original && detail::caller==caller && detail::resolve==resolve;
        detail::original=original;detail::caller=caller;detail::resolve=resolve;return true;
    }
    static void* Entry(){return reinterpret_cast<void*>(&detail::Entry);}
};
}

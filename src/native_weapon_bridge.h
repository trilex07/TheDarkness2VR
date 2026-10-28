#pragma once
// Owned-code adapters only. The host pins the complete native code/table
// contracts before publishing either entry on an equipped instance.
#include <windows.h>
#include <cstdint>
#include <cstring>
#include "native_image_guard.h"
namespace d2vr_weapon_bridge {
using Compare=int(__cdecl*)(const float*,const float*,float);
// CFBBA0 is void thiscall with five stack arguments (RET 14h), not a bool
// C47570 shortcut. Calling it preserves native reference/ammo/query handling.
using Fire=void(__thiscall*)(void*,void*,const float*,uintptr_t,uintptr_t,uintptr_t);
using ModelResolve=bool(*)(const float*,float*);
using FireResolve=bool(*)(void*,void*,float*);
namespace detail {
inline Compare compare=nullptr;
inline Fire fire=nullptr;
inline ModelResolve modelResolve=nullptr;
inline FireResolve fireResolve=nullptr;
inline thread_local bool resolving=false;
static void __cdecl ApplyModel(const float* current,float* candidate) {
    const DWORD error=GetLastError();
    if(!resolving) {
        resolving=true;
        __try {modelResolve(current,candidate);}
        __finally {resolving=false;SetLastError(error);}
    }
}
static bool __cdecl ApplyFire(void* self,void* weaponRef,float* matrix) {
    const DWORD error=GetLastError();bool applied=false;
    if(!resolving) {
        resolving=true;
        __try {applied=fireResolve(self,weaponRef,matrix);}
        __finally {resolving=false;SetLastError(error);}
    }
    return applied;
}
static __declspec(naked) int __cdecl ModelEntry(const float*,const float*,float) {
    __asm {
        pushfd
        pushad
        mov esi,esp
        sub esp,528
        and esp,-16
        fxsave [esp]
        fninit
        push dword ptr [esi+44] // candidate
        push dword ptr [esi+40] // current
        call ApplyModel
        add esp,8
        fxrstor [esp]
        mov esp,esi
        popad
        popfd
        jmp dword ptr [compare]
    }
}
static __declspec(naked) void __fastcall FireEntry(void*,void*,void*,const float*,uintptr_t,uintptr_t,uintptr_t) {
    __asm {
        push ebp
        mov ebp,esp
        pushfd
        push ebx
        push esi
        push edi
        mov esi,ecx
        sub esp,592
        and esp,-16
        mov edi,esp
        fxsave [edi]
        fninit
        lea eax,[edi+512] // invocation-local world matrix, lives through fire
        push eax
        push dword ptr [ebp+8] // unchanged weapon smart-reference argument
        push esi
        call ApplyFire
        add esp,12
        mov ebx,dword ptr [ebp+12]
        test al,al
        jz stockMatrix
        lea ebx,[edi+512]
    stockMatrix:
        fxrstor [edi]
        push dword ptr [ebp-4]
        popfd
        push dword ptr [ebp+24]
        push dword ptr [ebp+20]
        push dword ptr [ebp+16]
        push ebx
        push dword ptr [ebp+8]
        mov ecx,esi
        call dword ptr [fire] // original CFBBA0 once; callee removes 20 bytes
        // Do not touch original outgoing FP state, volatile registers or flags.
        lea esp,[ebp-16]
        pop edi
        pop esi
        pop ebx
        lea esp,[ebp]
        pop ebp
        ret 20
    }
}
}
class Bridge {
public:
    static bool Prepare(Compare compare,Fire fire,ModelResolve model,FireResolve shot,
                        const d2vr_native_image::Guard& guard) {
        static_assert(sizeof(void*)==4,"x86 only");
        if(!guard.enabled || !guard.base || guard.bytes<9 || !compare || !fire || !model || !shot ||
           reinterpret_cast<uintptr_t>(compare)<guard.base ||
           reinterpret_cast<uintptr_t>(compare)-guard.base>guard.bytes-9 ||
           reinterpret_cast<uintptr_t>(fire)<guard.base ||
           reinterpret_cast<uintptr_t>(fire)-guard.base>=guard.bytes ||
           !guard.Allows(reinterpret_cast<uintptr_t>(model),1) ||
           !guard.Allows(reinterpret_cast<uintptr_t>(shot),1) ||
           !guard.Allows(reinterpret_cast<uintptr_t>(&detail::ModelEntry),1) ||
           !guard.Allows(reinterpret_cast<uintptr_t>(&detail::FireEntry),1))return false;
        const unsigned char expected[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x83,0xec,0x50};
        __try {if(std::memcmp(reinterpret_cast<void*>(compare),expected,sizeof(expected)))return false;}
        __except(EXCEPTION_EXECUTE_HANDLER){return false;}
        if(detail::compare)return detail::compare==compare && detail::fire==fire &&
            detail::modelResolve==model && detail::fireResolve==shot;
        detail::compare=compare;detail::fire=fire;detail::modelResolve=model;detail::fireResolve=shot;
        return true;
    }
    static void* ModelEntry(){return reinterpret_cast<void*>(&detail::ModelEntry);}
    static void* FireEntry(){return reinterpret_cast<void*>(&detail::FireEntry);}
};
}

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
inline Resolve scopedResolve=nullptr;
inline Resolve activeGrabResolve=nullptr;
inline uintptr_t scopedCaller=0;
inline thread_local bool resolving=false;
inline __declspec(align(16)) thread_local float result[16];
static bool IsActiveGrabSelector(uintptr_t from,const uintptr_t* frame) {
    if(!frame)return false;
    __try {
        if(from!=0xCB0300) {
            const uintptr_t helperReturn=from==0x64FDAC ? 0xCB0091 : from==0xBF2769 ? 0xCB00AE : 0;
            // The active callers also prepare the native selection basis
            // before entering CE9A00. Keep both helper reads on the same ray.
            if(from==0x64FDAC && (frame[1]==0xCC86AA || frame[1]==0xC0BEAD))return true;
            if(from==0xBF2769 && (frame[1]==0xCC86C6 || frame[1]==0xC0BECB))return true;
            if(!helperReturn || frame[1]!=helperReturn)return false;
            const auto parent=frame[0];
            const auto address=reinterpret_cast<uintptr_t>(frame);
            if(parent<=address || parent-address>0x4000 || parent%sizeof(uintptr_t))return false;
            frame=reinterpret_cast<const uintptr_t*>(parent);
        }
        // CE9A00 saves EBP and uses 1 as a constant. The validated stack
        // prologue leaves its native caller 0x48 bytes above the CB0040 frame.
        if(frame[0]!=1 || frame[1]!=0xCE9A61)return false;
        const auto activeCaller=frame[0x48/sizeof(uintptr_t)];
        return activeCaller==0xCC86DF || activeCaller==0xC0BEE6;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static float* __cdecl Apply(float* base,void* self,uintptr_t from,const uintptr_t* frame) {
    const auto error=GetLastError();
    float* output=base;
    if(!resolving) {
        resolving=true;
        Resolve selected=nullptr;
        if(from==caller)selected=resolve;
        else if(activeGrabResolve && IsActiveGrabSelector(from,frame))selected=activeGrabResolve;
        else if(from==scopedCaller || from==0x64FDAC || from==0xBF2769)selected=scopedResolve;
        if(selected && selected(self,base,result))output=result;
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
        je matched
        cmp eax,64FDACh
        je matched
        cmp eax,0BF2769h
        je matched
        cmp eax,0CB0300h
        je matched
        cmp eax,scopedCaller
        jne untouched
    matched:
        popfd
        call dword ptr [original]
        pushfd
        pushad
        mov esi,esp
        sub esp,528
        and esp,-16
        fxsave [esp]
        fninit
        push dword ptr [esi+8]  // original EBP
        push dword ptr [esi+36] // original caller return address
        push dword ptr [esi+24] // original ECX receiver
        push dword ptr [esi+28] // native EAX matrix pointer
        call Apply
        add esp,16
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
    static bool ActiveGrab(Resolve resolve,const d2vr_native_image::Guard& guard) {
        if(!resolve || !guard.enabled || !guard.Allows(reinterpret_cast<uintptr_t>(resolve),1))return false;
        if(detail::activeGrabResolve)return detail::activeGrabResolve==resolve;
        detail::activeGrabResolve=resolve;return true;
    }
    static bool Scoped(Resolve resolve,uintptr_t caller,const d2vr_native_image::Guard& guard) {
        if(!resolve || !caller || !guard.enabled || !guard.Allows(caller,1) ||
           !guard.Allows(reinterpret_cast<uintptr_t>(resolve),1))return false;
        if(detail::scopedResolve)return detail::scopedResolve==resolve && detail::scopedCaller==caller;
        detail::scopedResolve=resolve;detail::scopedCaller=caller;return true;
    }
};
}

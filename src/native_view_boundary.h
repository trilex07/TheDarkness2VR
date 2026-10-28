#pragma once
// One process-local Win32 implementation interception. The game's code, IAT,
// static class vtables, timing result and API arguments stay unchanged.
#include <windows.h>
#include <intrin.h>
#include <atomic>
#include "native_image_guard.h"
#include "../third_party/minhook/include/MinHook.h"

namespace d2vr_native_view {
namespace detail {
    using Query=BOOL(WINAPI*)(LARGE_INTEGER*);
    using Callback=void(*)(void*);
    inline Query next_=nullptr;
    inline uintptr_t caller_=0;
    inline Callback callback_=nullptr;
    inline std::atomic<bool> reserved_{false};
    static BOOL __stdcall Dispatch(LARGE_INTEGER* output,void* owner) {
        const BOOL ok=next_(output);
        const DWORD error=GetLastError();
        if(ok && callback_) {
            // The native camera setter uses x87/SSE. Its invocation must not
            // perturb the caller's floating-point stack/control environment.
            __declspec(align(16)) unsigned char floating[512];
            _fxsave(floating);
            __asm fninit
            callback_(owner);
            _fxrstor(floating);
        }
        SetLastError(error);
        return ok;
    }
#if defined(_M_IX86)
    static BOOL __declspec(naked) WINAPI Hook(LARGE_INTEGER*) {
        __asm mov eax,[esp]
        __asm cmp eax,dword ptr [caller_]
        __asm jne forward
        // ESI is the owning view's this at the SHA-pinned native call site.
        // Capture it before C++ can reuse any registers.
        __asm push esi
        __asm push dword ptr [esp+8]
        __asm call Dispatch
        __asm ret 4
    forward:
        __asm jmp dword ptr [next_]
    }
#endif
}
class Boundary {
    bool installed_=false;
public:
    using Query=detail::Query;
    using Callback=detail::Callback;
    bool Start(void* target,uintptr_t caller,Callback callback,const d2vr_native_image::Guard& guard) {
#if !defined(_M_IX86)
        return false;
#else
        using namespace detail;
        if(installed_)return true;
        if(!target || !caller || !callback || !guard.enabled)return false;
        const auto address=reinterpret_cast<uintptr_t>(target);
        if(address<5 || !guard.Allows(address-5,80))return false;
        MEMORY_BASIC_INFORMATION first{},last{};
        if(!VirtualQuery(reinterpret_cast<void*>(address-5),&first,sizeof(first)) ||
           !VirtualQuery(reinterpret_cast<void*>(address+74),&last,sizeof(last)))return false;
        constexpr DWORD execute=PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
        if(first.Type!=MEM_IMAGE || last.Type!=MEM_IMAGE || first.State!=MEM_COMMIT || last.State!=MEM_COMMIT ||
           first.AllocationBase!=last.AllocationBase || !(first.Protect&execute) || !(last.Protect&execute) ||
           ((first.Protect|last.Protect)&(PAGE_NOACCESS|PAGE_GUARD)))return false;
        HMODULE self=nullptr,api=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCSTR>(&Hook),&self) || first.AllocationBase==self ||
           !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCSTR>(target),&api))return false;
        bool expected=false;
        if(!reserved_.compare_exchange_strong(expected,true))return false;
        const auto status=MH_Initialize();
        if(status!=MH_OK && status!=MH_ERROR_ALREADY_INITIALIZED){reserved_=false;return false;}
        void* trampoline=nullptr;
        if(MH_CreateHook(target,reinterpret_cast<void*>(&Hook),&trampoline)!=MH_OK){reserved_=false;return false;}
        next_=reinterpret_cast<Query>(trampoline);caller_=caller;callback_=callback;
        // Published once before enable, retained for process lifetime. Never
        // dismantle a trampoline while another thread can be entering it.
        if(MH_EnableHook(target)!=MH_OK)return false;
        installed_=true;return true;
#endif
    }
    bool Installed()const{return installed_;}
};
}

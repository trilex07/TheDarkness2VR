#pragma once
// Protected input observes the real XInput implementation. It never writes an
// import table, EXE byte, native class vtable or game-owned object.
#include <windows.h>
#include <xinput.h>
#include <atomic>
#include "native_image_guard.h"
#include "../third_party/minhook/include/MinHook.h"

namespace d2vr_protected_input {
using GetState = DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
inline bool Requested(bool imageObserver,bool guard,bool input,bool enabled) {
    return imageObserver && guard && input && enabled;
}
// Analog squeeze maps to native shoulder-button actions only. The bridge does
// not supply controller-aligned native targeting, aim, or throws.
struct Grips {
    bool held[2]{};
    WORD Update(float left,float right) {
        const float values[2]={left,right};
        for(unsigned h=0;h<2;++h) held[h]=values[h]>(held[h] ? .35f:.65f);
        return WORD((held[0] ? XINPUT_GAMEPAD_LEFT_SHOULDER:0) |
                    (held[1] ? XINPUT_GAMEPAD_RIGHT_SHOULDER:0));
    }
};
class ExternalXInput {
    std::atomic<GetState> next_{nullptr};
    std::atomic<bool> ready_{false};
    static bool PinExternal(void* target,void* replacement,const d2vr_native_image::Guard& guard) {
        const auto p=reinterpret_cast<uintptr_t>(target);
        if(!guard.enabled || p<5 || !guard.Allows(p-5,80)) return false;
        MEMORY_BASIC_INFORMATION a{},b{};
        if(!VirtualQuery(reinterpret_cast<void*>(p-5),&a,sizeof(a)) ||
           !VirtualQuery(reinterpret_cast<void*>(p+74),&b,sizeof(b))) return false;
        constexpr DWORD execute=PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
        if(a.Type!=MEM_IMAGE || b.Type!=MEM_IMAGE || a.State!=MEM_COMMIT || b.State!=MEM_COMMIT ||
           a.AllocationBase!=b.AllocationBase || !(a.Protect&execute) || !(b.Protect&execute) ||
           ((a.Protect|b.Protect)&(PAGE_NOACCESS|PAGE_GUARD))) return false;
        HMODULE self=nullptr,owner=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCSTR>(replacement),&self) || a.AllocationBase==self) return false;
        return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCSTR>(target),&owner)!=FALSE;
    }
public:
    // Name and ordinal must identify the same documented stdcall ABI. Absolute
    // system path prevents admitting an arbitrary game-local XInput wrapper.
    bool Start(GetState replacement,const d2vr_native_image::Guard& guard) {
        if(ready_.load(std::memory_order_acquire)) return true;
        if(!replacement || !guard.enabled) return false;
        WCHAR path[MAX_PATH]{};
        const UINT count=GetSystemDirectoryW(path,MAX_PATH);
        if(!count || count+sizeof(L"\\xinput1_3.dll")/sizeof(WCHAR)>=MAX_PATH) return false;
        wcscat_s(path,L"\\xinput1_3.dll");
        HMODULE system=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!system) return false;
        auto target=reinterpret_cast<GetState>(GetProcAddress(system,"XInputGetState"));
        const auto byOrdinal=reinterpret_cast<GetState>(GetProcAddress(system,MAKEINTRESOURCEA(2)));
        const bool allowed=target && target==byOrdinal &&
            PinExternal(reinterpret_cast<void*>(target),reinterpret_cast<void*>(replacement),guard);
        if(!allowed) {FreeLibrary(system);return false;}
        const auto initialized=MH_Initialize();
        if(initialized!=MH_OK && initialized!=MH_ERROR_ALREADY_INITIALIZED) {FreeLibrary(system);return false;}
        void* trampoline=nullptr;
        const auto created=MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(replacement),&trampoline);
        if(created!=MH_OK) {FreeLibrary(system);return false;}
        next_.store(reinterpret_cast<GetState>(trampoline),std::memory_order_release);
        const auto enabled=MH_EnableHook(reinterpret_cast<void*>(target));
        FreeLibrary(system); // target and replacement have both been pinned
        if(enabled!=MH_OK) return false; // never fall back to EXE import writes
        ready_.store(true,std::memory_order_release);
        return true;
    }
    GetState Native() const {return next_.load(std::memory_order_acquire);}
    bool Ready() const {return ready_.load(std::memory_order_acquire);}
};
}

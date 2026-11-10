#pragma once
#include <windows.h>
#include <atomic>
#include <cstring>
#include "native_image_guard.h"
#include "native_view_code_clone.h"
#include "../third_party/minhook/include/MinHook.h"

namespace d2vr_menu_external {
using GetData=HRESULT(STDMETHODCALLTYPE*)(void*,uint32_t,void*,uint32_t*,uint32_t);
// MAIN's actual READ_ONLY_EQUAL_PAIR identifies the Steam wrapper. Normalize
// ONLY its two PE HIGHLOW operands, not arbitrary differing code bytes.
inline bool Normalize(unsigned char (&bytes)[96],uintptr_t base) {
    if(!base || base>UINT32_MAX-0x18C000)return false;
    const unsigned offsets[]={5,21},rvas[]={0x147D5C,0x147D5D};
    for(unsigned i=0;i<2;++i) {
        uint32_t value=0;std::memcpy(&value,bytes+offsets[i],4);
        if(value!=base+rvas[i])return false;
        value=0x10000000+rvas[i];std::memcpy(bytes+offsets[i],&value,4);
    }
    return true;
}
inline bool MethodMatches(const void* code,uintptr_t base) {
    unsigned char bytes[96]{};
    __try {std::memcpy(bytes,code,sizeof(bytes));}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    return Normalize(bytes,base) && d2vr_native_clone::MatchesDigest(bytes,sizeof(bytes),
        "d6eb3d69833b60c950b03096c7cebc040e8051aeaddd33f015ae37bd29f4dbc6");
}
class ExternalMouse {
    std::atomic<GetData> next_{nullptr};
    std::atomic<bool> ready_{false};
public:
    GetData Native()const{return next_.load(std::memory_order_acquire);}
    bool Ready()const{return ready_.load(std::memory_order_acquire);}
    bool Start(GetData target,GetData replacement,const d2vr_native_image::Guard& guard) {
        if(Ready())return true;
        const auto p=reinterpret_cast<uintptr_t>(target);
        if(!target || !replacement || !guard.enabled || p<5 || !guard.Allows(p-5,101))return false;
        MEMORY_BASIC_INFORMATION a{},b{};
        if(!VirtualQuery(reinterpret_cast<void*>(p-5),&a,sizeof(a)) ||
           !VirtualQuery(reinterpret_cast<void*>(p+95),&b,sizeof(b)))return false;
        const DWORD execute=PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
        if(a.Type!=MEM_IMAGE || b.Type!=MEM_IMAGE || a.State!=MEM_COMMIT || b.State!=MEM_COMMIT ||
           a.AllocationBase!=b.AllocationBase || !(a.Protect&execute) || !(b.Protect&execute) ||
           ((a.Protect|b.Protect)&(PAGE_NOACCESS|PAGE_GUARD)))return false;
        const auto base=reinterpret_cast<uintptr_t>(a.AllocationBase);
        if(p!=base+0x823D0 || GetModuleHandleW(L"GameOverlayRenderer.dll")!=a.AllocationBase)return false;
        bool valid=false;
        __try {
            const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            if(dos->e_magic==IMAGE_DOS_SIGNATURE && dos->e_lfanew==0x138) {
                const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
                valid=nt->Signature==IMAGE_NT_SIGNATURE && nt->FileHeader.Machine==IMAGE_FILE_MACHINE_I386 &&
                    nt->FileHeader.TimeDateStamp==0x6AD92A09 && nt->OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
                    nt->OptionalHeader.SizeOfImage==0x18C000 && MethodMatches(target,base);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER){valid=false;}
        if(!valid)return false;
        HMODULE owner=nullptr,self=nullptr;
        const auto pin=GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN;
        if(!GetModuleHandleExA(pin,reinterpret_cast<LPCSTR>(target),&owner) ||
           !GetModuleHandleExA(pin,reinterpret_cast<LPCSTR>(replacement),&self) || owner==self)return false;
        const auto initialized=MH_Initialize();
        if(initialized!=MH_OK && initialized!=MH_ERROR_ALREADY_INITIALIZED)return false;
        void* trampoline=nullptr;
        if(MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(replacement),&trampoline)!=MH_OK)return false;
        next_.store(reinterpret_cast<GetData>(trampoline),std::memory_order_release);
        if(MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)return false;
        ready_.store(true,std::memory_order_release);return true;
    }
};
}

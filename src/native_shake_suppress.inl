// Pinned stock EXE: 00628420 generates a procedural shake sample. Only these
// four calls in the selected first-person update are redirected. The original
// generator still advances its phase; the computed offset is cleared before
// the caller combines it with view and first-person entity transforms.
#ifdef D2VR_MATCHED_CAPTURE
namespace d2vr_native_shake {
struct Site { DWORD address; BYTE original[5]; BYTE patched[5]; bool active; };
static Site sites[] = {
    {0x63187E,{0xE8,0x9D,0x6B,0xFF,0xFF},{},false},
    {0x6318AC,{0xE8,0x6F,0x6B,0xFF,0xFF},{},false},
    {0x6318CA,{0xE8,0x51,0x6B,0xFF,0xFF},{},false},
    {0x63197B,{0xE8,0xA0,0x6A,0xFF,0xFF},{},false},
};
static bool installed=false;
static void __fastcall SampleWithoutShake(void* self,void*,float dt,float scale) {
    using Generator=void(__thiscall*)(void*,float,float);
    reinterpret_cast<Generator>(0x628420)(self,dt,scale);
    if(!self)return;
    __try {
        // 00628473..88 and 00628634..70 establish these as output fields.
        // Preserve phase, amplitudes, timing and all other native parameters.
        std::memset(self,0,16);
        std::memset(static_cast<BYTE*>(self)+0x38,0,12);
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
}
static bool Restore() {
    bool clean=true;
    for(auto& site:sites) {
        if(!site.active)continue;
        auto target=reinterpret_cast<BYTE*>(site.address);
        if(std::memcmp(target,site.patched,5)!=0){clean=false;continue;}
        DWORD old=0;
        if(!VirtualProtect(target,5,PAGE_EXECUTE_READWRITE,&old)){clean=false;continue;}
        std::memcpy(target,site.original,5);
        DWORD ignored=0;VirtualProtect(target,5,old,&ignored);
        FlushInstructionCache(GetCurrentProcess(),target,5);
        site.active=false;
    }
    installed=false;
    return clean;
}
static bool Install() {
    if(!g_cameraShakeSuppress)return true;
    if(installed)return true;
    if(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))!=0x400000)return false;
    const BYTE entry[12]={0x55,0x8B,0xEC,0x83,0xE4,0xF0,0x83,0xEC,0x1C,0x56,0x8B,0xF1};
    if(std::memcmp(reinterpret_cast<void*>(0x628420),entry,sizeof(entry))!=0)return false;
    for(const auto& site:sites) {
        if(!NativeImageWriteAllowed(site.address,5) ||
           std::memcmp(reinterpret_cast<void*>(site.address),site.original,5)!=0)return false;
    }
    for(auto& site:sites) {
        auto target=reinterpret_cast<BYTE*>(site.address);
        DWORD old=0;
        if(!VirtualProtect(target,5,PAGE_EXECUTE_READWRITE,&old)){Restore();return false;}
        const intptr_t delta=reinterpret_cast<intptr_t>(&SampleWithoutShake)-intptr_t(site.address+5);
        site.patched[0]=0xE8;
        const DWORD relative=static_cast<DWORD>(delta);
        std::memcpy(site.patched+1,&relative,4);
        std::memcpy(target,site.patched,5);
        DWORD ignored=0;VirtualProtect(target,5,old,&ignored);
        FlushInstructionCache(GetCurrentProcess(),target,5);
        site.active=true;
        if(std::memcmp(target,site.patched,5)!=0){Restore();return false;}
    }
    installed=true;
    Log("[CameraShakeSuppress] active=1 sites=4 generator=00628420; native phase retained, procedural offsets cleared");
    return true;
}
}
#endif

#include "native_hud.h"
static void UpdateNativeHudForView(void* renderer,void* reference,void* manager) {
#ifdef D2VR_MATCHED_CAPTURE
    static d2vr_hud::Override state;
    if((!g_hideNativeHud && !state.held) || !g_nativePlayerOwnerAdmitted ||
       !g_nativeImageGuard.enabled || !g_imagePreservingPresent ||
       GetCurrentThreadId()!=g_renderThreadId || !reference)return;
    // Pin the two getters, setter/visibility predicate and relevant owner/constructor instructions.
    // Admission has already pinned the stock EXE SHA256 and the callback ABI.
    static int contract=0;
    if(!contract) {
        struct Pin {uintptr_t address;const unsigned char* bytes;size_t size;};
        static const unsigned char player[]={0x8b,0x89,0xe4,0x04,0,0,0x8b,0x44,0x24,0x04,0xff,0x41,0x04,0x89,0x08,0xc2,0x04,0};
        static const unsigned char avatar[]={0x8b,0x49,0x74,0x8b,0x44,0x24,0x04,0xff,0x41,0x04,0x89,0x08,0xc2,0x04,0};
        static const unsigned char getter[]={0x8b,0x47,0x60,0x83,0x38,0,0x8d,0x5f,0x60};
        static const unsigned char setter[]={0x8a,0x44,0x24,0x04,0x88,0x81,0x3c,0x01,0,0,0xc2,0x04,0};
        static const unsigned char visible[]={0x80,0xb9,0x3c,0x01,0,0,0,0x74,0x15,0x80,0x3d,0x54,0xdc,0x0f,0x01,0,0x74,0x0c,0xb8,1,0,0,0,0x88,0x81,0x3d,1,0,0,0xc3,0x33,0xc0,0x88,0x81,0x3d,1,0,0,0xc3};
        static const unsigned char ctor[]={0xc7,0x06,0x88,0x3b,0xfe,0};
        const Pin pins[]={{0x6b0cd0,player,sizeof(player)},{0x43bcb0,avatar,sizeof(avatar)},
            {0x7928d2,getter,sizeof(getter)},{0xc8b380,setter,sizeof(setter)},
            {0x819f60,visible,sizeof(visible)},{0xbfa14c,ctor,sizeof(ctor)}};
        contract=1;
        for(const auto& pin:pins) {
            unsigned char bytes[64];SIZE_T copied=0;
            if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(pin.address),bytes,pin.size,&copied) ||
               copied!=pin.size || memcmp(bytes,pin.bytes,pin.size)) {contract=-1;break;}
        }
        Log("[NativeHUD] contract=%s; native HudStatus heap flag only, shared UI shaders unchanged",contract==1?"admitted":"REFUSED");
    }
    if(contract!=1)return;
    const auto read=[](uint32_t address,uint32_t& value) {
        __try {value=*reinterpret_cast<const uint32_t*>(address);return true;}
        __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    };
    const auto readByte=[](uint32_t address,uint8_t& value) {
        __try {value=*reinterpret_cast<const uint8_t*>(address);return true;}
        __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    };
    const auto writeByte=[](uint32_t address,uint8_t value) {
        if(!g_nativeImageGuard.Allows(address,1))return false;
        __try {*reinterpret_cast<uint8_t*>(address)=value;return true;}
        __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    };
    const bool guidance=d2vr_hud::GuidanceActive(GetTickCount64(),
        uint64_t(InterlockedCompareExchange64(&g_objectiveHudUntil,0,0)));
    const bool hide=g_hideNativeHud && !guidance && !g_disableOpenXR && g_submitMode==4 && g_stereoMode==1 &&
        g_haveViews && g_trackedFrame.open;
    const auto consume=[&](const d2vr_player_camera::Owner& selected) {
        d2vr_hud::Owner hud;
        if(!d2vr_hud::Resolve(selected.avatar,read,hud))return;
        const bool wasHeld=state.held;
        if(state.Update(hud,hide,readByte,writeByte) && wasHeld!=state.held)
            Log("[NativeHUD] hidden=%d avatar=%08X player=%08X hud=%08X; native visibility consumer pending HMD verification",
                int(state.held),hud.avatar,hud.player,hud.hud);
    };
    d2vr_player_camera::WithSelectedLocalPlayer(uint32_t(reinterpret_cast<uintptr_t>(renderer)),
        uint32_t(reinterpret_cast<uintptr_t>(manager)),uint32_t(reinterpret_cast<uintptr_t>(reference)),
        read,ReadSelectedLocalPlayer,ReleaseSelectedLocalPlayer,consume,ProtectedAimCameraTable());
#else
    (void)renderer;(void)reference;(void)manager;
#endif
}

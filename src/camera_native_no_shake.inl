// Selected live producer, BEFORE native camera composition. No temporal filter.
#ifdef D2VR_MATCHED_CAPTURE
#include "camera_event_feedback.h"
#include "camera_source_clone_plan.h"
static d2vr_native_view::InstanceTable g_cameraSourceTable;
static d2vr_native_clone::Code* g_cameraSourceCode=nullptr;
static std::atomic<DWORD> g_cameraSourceAvatar{0};
static std::atomic<DWORD> g_cameraSourcePublishedTable{0};
static std::atomic<unsigned long long> g_cameraSourceCalls{0},g_cameraSourceNoAnimationCalls{0};
static bool CameraSourceRead(uint32_t address,uint32_t& value) {
    __try {value=*reinterpret_cast<const uint32_t*>(address);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool IsCameraSourceTable(DWORD table) {
    const auto clone=g_cameraSourcePublishedTable.load(std::memory_order_acquire);
    return table==0xfdf170 || (clone && table==clone);
}
static bool CameraSourceOwnerMatches(DWORD owner,DWORD avatar) {
    uint32_t table=0;if(!CameraSourceRead(owner,table))return false;
    return IsCameraSourceTable(table) &&
        d2vr_camera_event::Matches(owner,avatar,CameraSourceRead,table);
}
// Replaces ONLY cmp byte ptr [esp+17],0 at 8098FA in the owned copy.
// JE 809A0E then skips the animated camera-channel bone contribution.
// cmp esp,esp has the same defined compare flags as comparing zero with zero;
// registers, SSE/x87 and the caller's stack are preserved.
static __declspec(naked) void CameraSourceSkipBone() {
    __asm {
        cmp esp,esp
        ret
    }
}
using CameraSourceDispatch=void(__thiscall*)(void*,void*);
using CameraSourceCompose=void(__thiscall*)(void*,void*,void*,const float*,float,float,float);
static CameraSourceDispatch g_cameraSourceOriginalDispatch=reinterpret_cast<CameraSourceDispatch>(0x730890);
static CameraSourceDispatch g_cameraSourceOwnedDispatch=nullptr;
static CameraSourceCompose g_cameraSourceOwnedCompose=nullptr;
// ABI witness: stock 730FCF..730FFB constructs exactly these six stack words.
// Native 7255E0 consumes the avatar and camera smart references (ret 18h).
static void __fastcall CameraSourceNoAnimation(void* self,void*,void* cameraReference) {
    auto owner=static_cast<BYTE*>(self);
    auto controller=**reinterpret_cast<BYTE***>(owner+0x828);
    auto avatarReference=*reinterpret_cast<DWORD**>(controller+0x74);
    ++avatarReference[1];
    alignas(16) const float offset[4]={0,0,0,0};
    g_cameraSourceOwnedCompose(
        self,avatarReference,cameraReference,offset,0.f,0.f,0.f);
    // 5E1164..5E1192: retain the ordinary native first-person matrix snapshot.
    std::memcpy(owner+0x100,owner+0x70,64);
    ++g_cameraSourceNoAnimationCalls;
}
struct CameraSourceSaved {float translation[4],rotation[3];BYTE channel,apply;};
static void __fastcall CameraSourceDispatchBridge(void* self,void*,void* cameraReference) {
    const auto avatar=g_cameraSourceAvatar.load(std::memory_order_acquire);
    const auto owner=static_cast<BYTE*>(self);
    bool admit=false;
    __try {
        // Preserve alternate/cinematic camera branches. The owned dispatch keeps
        // all their native calls; only ordinary FP camera-channel composition differs.
        admit=CameraSourceOwnerMatches(DWORD(reinterpret_cast<uintptr_t>(self)),avatar) &&
            !owner[0x10AF] && !owner[0x8A4] && !owner[0x10AD] && !owner[0x10AE];
    } __except(EXCEPTION_EXECUTE_HANDLER){}
    if(!admit) {
        g_cameraSourceOriginalDispatch(self,cameraReference);return;
    }
    CameraSourceSaved saved{};
    std::memcpy(saved.translation,owner+0x1F0,16);
    std::memcpy(saved.rotation,owner+0x20C,12);
    saved.channel=owner[0x9BC];saved.apply=owner[0x1118];
    // Dynamic procedural/impact outputs remain intact for haptics. Only the
    // native composition inputs are masked synchronously, then restored.
    std::memset(owner+0x1F0,0,16);std::memset(owner+0x20C,0,12);
    owner[0x9BC]=0;owner[0x1118]=0;
    __try {
        g_cameraSourceOwnedDispatch(self,cameraReference);
    } __finally {
        std::memcpy(owner+0x1F0,saved.translation,16);
        std::memcpy(owner+0x20C,saved.rotation,12);
        owner[0x9BC]=saved.channel;owner[0x1118]=saved.apply;
    }
    const auto calls=++g_cameraSourceCalls;
    if(calls<=3 || calls%1800==0)
        Log("[CameraNativeNoShake] dispatch=%llu animationBypass=%llu owner=%p avatar=%08lX; native same-update FP path, raw impact/environment preserved",calls,g_cameraSourceNoAnimationCalls.load(),self,avatar);
}
static bool PrepareCameraNativeNoShake(DWORD owner,DWORD avatar) {
    if(!g_cameraNativeNoShake || !g_nativeImageGuard.enabled ||
       !CameraSourceOwnerMatches(owner,avatar))return false;
    g_cameraSourceAvatar.store(avatar,std::memory_order_release);
    if(g_cameraSourceTable.Owns(reinterpret_cast<void*>(owner)))return true;
    if(!g_cameraSourceCode) {
        // 90 function entries followed by a literal string at FDF2D8. Preserve
        // and pin the preceding RTTI/header word, as for other protected tables.
        if(!d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0xFDF16C),91*4,
            "d13a6d5ccdfff5871c8583600a0ac2bcf48cdbda46932c4fa8ee0a460b671f66"))return false;
        auto code=new d2vr_native_clone::Code;
        if(!d2vr_camera_source::Prepare(*code,reinterpret_cast<void*>(CameraSourceNoAnimation),
            reinterpret_cast<void*>(CameraSourceSkipBone),g_nativeImageGuard)) {
            Log("[CameraNativeNoShake] REFUSED plan=%s",code->Status());delete code;return false;
        }
        HMODULE module=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCSTR>(CameraSourceDispatchBridge),&module) ||
           !g_cameraSourceTable.Prepare(reinterpret_cast<void**>(0xFDF170),90,25,
               reinterpret_cast<void*>(CameraSourceDispatchBridge),g_nativeImageGuard)) {
            delete code;return false;
        }
        g_cameraSourceOwnedDispatch=reinterpret_cast<CameraSourceDispatch>(code->Entry(0));
        g_cameraSourceOwnedCompose=reinterpret_cast<CameraSourceCompose>(code->Entry(1));
        g_cameraSourcePublishedTable.store(DWORD(reinterpret_cast<uintptr_t>(g_cameraSourceTable.Table())),std::memory_order_release);
        g_cameraSourceCode=code; // Process lifetime after heap vptr publication.
    }
    const bool published=g_cameraSourceTable.Publish(reinterpret_cast<void*>(owner),g_nativeImageGuard);
    if(published)Log("[CameraNativeNoShake] published owner=%08lX avatar=%08lX slot=25; EXE unchanged",owner,avatar);
    return published;
}
#else
static bool IsCameraSourceTable(DWORD table){return table==0xfdf170;}
static bool CameraSourceOwnerMatches(DWORD,DWORD){return false;}
static bool PrepareCameraNativeNoShake(DWORD,DWORD){return false;}
#endif

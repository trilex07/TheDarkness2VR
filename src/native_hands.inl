#include "native_hands_math.h"
// Reuse the accepted pistol WORLD-pose callback. Native parent/socket resolution
// has already consumed the animated model-space palette at that point. No new
// engine code patch: both failed palette/renderer sites remain strictly stock.
static bool g_nativeHandsInstalled=false;
static bool NativeHandsAvailable() { return g_nativeHandsEnabled && g_nativeHandsInstalled; }
static SRWLOCK g_nativeHandsLock=SRWLOCK_INIT;
struct NativeHandCalibration {
    void* parent=nullptr; void* skeleton=nullptr;
    float right[16]{},left[16]{},support[16]{};
};
static NativeHandCalibration g_nativeHandCalibration;
struct NativeHandsPaletteCache {
    void* renderer=nullptr; void* skeleton=nullptr; void* palette=nullptr;
    float original[44][16]{},applied[44][16]{};
};
static NativeHandsPaletteCache g_nativeHandsCache[3];
static bool ApplyNativeHandsChecked(void* receiver,DWORD cameraVtable) {
    if(!g_nativeHandsEnabled || !g_nativeHandsInstalled) return false;
    NativeGameplaySample sample;
    if(!CopyNativeGameplay(sample) || !sample.entities[0]) return false;
    // Grab/attach animations rewrite the same palette; defer human IK until empty.
    if(sample.grab==d2vr_input::GrabFeedback::Busy) return false;
    float gunWorld[16];
    if(!TryNativeWeaponMatrix((void*)sample.entities[0],gunWorld,cameraVtable)) return false;
    NativeWeaponPose left; bool support;
    if(!TryAcquireSRWLockShared(&g_nativeWeaponLock)) return false;
    left=g_nativeWeaponPoses[0]; support=g_nativeSupport;
    ReleaseSRWLockShared(&g_nativeWeaponLock);
    bool locked=false;
    __try {
        auto parent=**(BYTE***)((BYTE*)sample.entities[0]+0x3C4);
        if(!parent || *(DWORD*)parent!=0xFFC300) return false;
        auto actor=**(BYTE***)(parent+0xF8);
        if(!actor || *(DWORD*)actor!=0xFC5F30) return false;
        auto renderer=**(BYTE***)(actor+8);
        const DWORD index=*(DWORD*)(renderer+0x424);
        const DWORD skinIndex=*(DWORD*)(renderer+0x448);
        if(renderer!=receiver || index>=3 || skinIndex>=2) return false;
        auto outputVector=renderer+0x3F4+16*index;
        auto resource=**(BYTE***)(renderer+0x5BC);
        if(!resource || *(DWORD*)resource!=0xFC2578) return false;
        auto skeleton=**(BYTE***)(resource+0x5E0);
        if(!skeleton || *(DWORD*)skeleton!=0xFC2338) return false;
        auto records=*(BYTE**)(skeleton+0x90);
        if(*(BYTE**)(skeleton+0x94)-records!=130*12) return false;
        if(strcmp(*(char**)(records+12*5),"GAME_L1_ARM3") ||
           strcmp(*(char**)(records+12*27),"GAME_R1_ARM3") ||
           strcmp(*(char**)(records+12*21),"GAME_L1_WEAPON1") ||
           strcmp(*(char**)(records+12*43),"GAME_R1_WEAPON1")) return false;
        auto palette=*(float**)outputVector;
        if(*(float**)((BYTE*)outputVector+4)-palette!=130*16) return false;
        alignas(16) float bones[130][16]; memcpy(bones,palette,sizeof(bones));
        for(int i:{5,21,27,43}) if(!d2vr_hands::Valid(bones[i])) return false;
        float inverseParent[16],rightTarget[16],leftTarget[16];
        if(!d2vr_math::Invert((float*)(parent+0x140),inverseParent)) return false;
        d2vr_math::Multiply(gunWorld,inverseParent,rightTarget);
        const bool leftValid=left.valid && GetTickCount64()-left.time<100;
        if(leftValid) {
            float lw[16];
            if(!d2vr_aim::BuildWeaponMatrix((float*)(sample.camera+0x80),left.local,lw)) return false;
            d2vr_math::Multiply(lw,inverseParent,leftTarget);
        }
        if(!TryAcquireSRWLockExclusive(&g_nativeHandsLock)) return false;
        locked=true;
        auto& cache=g_nativeHandsCache[index];
        const bool same=cache.renderer==renderer && cache.skeleton==skeleton && cache.palette==palette;
        // Cached native poses can be queried more than once before animation
        // runs again. Solve from their pristine human bones, never our last IK.
        if(same && memcmp(palette+2*16,cache.applied,sizeof(cache.applied))==0)
            memcpy(bones+2,cache.original,sizeof(cache.original));
        else {
            cache.renderer=renderer; cache.skeleton=skeleton; cache.palette=palette;
            memcpy(cache.original,bones+2,sizeof(cache.original));
        }
        NativeHandCalibration cal=g_nativeHandCalibration;
        if(cal.parent!=parent || cal.skeleton!=skeleton) {
            float ir[16],il[16];
            if(!d2vr_math::Invert(bones[43],ir) || !d2vr_math::Invert(bones[21],il)) {
                ReleaseSRWLockExclusive(&g_nativeHandsLock); return false;
            }
            cal.parent=parent; cal.skeleton=skeleton;
            d2vr_math::Multiply(bones[27],ir,cal.right);
            d2vr_math::Multiply(bones[5],il,cal.left);
            d2vr_math::Multiply(bones[5],ir,cal.support);
            g_nativeHandCalibration=cal;
        }
        float wrist[16]; d2vr_math::Multiply(cal.right,rightTarget,wrist);
        if(!d2vr_hands::PoseArm(bones,25,26,27,44,45,wrist)) { ReleaseSRWLockExclusive(&g_nativeHandsLock); return false; }
        if(leftValid) {
            d2vr_math::Multiply(support ? cal.support:cal.left,support ? rightTarget:leftTarget,wrist);
            if(!d2vr_hands::PoseArm(bones,3,4,5,22,23,wrist)) { ReleaseSRWLockExclusive(&g_nativeHandsLock); return false; }
        }
        const bool rightChanged=memcmp(bones+24,palette+24*16,22*64)!=0;
        const bool leftChanged=leftValid && memcmp(bones+2,palette+2*16,22*64)!=0;
        if(!rightChanged && !leftChanged) {
            ReleaseSRWLockExclusive(&g_nativeHandsLock);
            locked=false;
            return true;
        }
        // Keep the camera root and demon tentacles unchanged.
        memcpy(palette+24*16,bones[24],22*64);
        if(leftValid) memcpy(palette+2*16,bones[2],22*64);
        // Native pose setter 79927D marks this packed-skin buffer dirty after
        // editing model-space bones. 6C3C71 otherwise skips rebuilding the
        // 48-byte GPU matrices in renderer+428, leaving the visible arms stale.
        // Skin buffers (448, two slots) rotate independently of pose buffers
        // (424, three slots). Do not use the pose index for this byte.
        renderer[0x580+skinIndex]=1;
        // Preserve the actual unchanged left range on tracking loss.
        memcpy(cache.applied,palette+2*16,sizeof(cache.applied));
        ReleaseSRWLockExclusive(&g_nativeHandsLock);
        locked=false;
        static ULONGLONG last=0; auto now=GetTickCount64();
        if(now-last>1000) { last=now; Log("[NativeHands] wrists/fingers + arm IK applied; skin palette invalidated slot=%lu; pose slot=%lu; left=%s",skinIndex,index,leftValid ? (support ? "support":"free"):"tracking unavailable"); }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { if(locked) ReleaseSRWLockExclusive(&g_nativeHandsLock); return false; }
}
static bool NativeHandsFromWeapon(void* model,DWORD cameraVtable) {
    if(!NativeHandsAvailable()) return false;
    NativeGameplaySample sample;
    if(!CopyNativeGameplay(sample) || sample.entities[0]!=(DWORD)(uintptr_t)model) return false;
    __try {
        if(*(DWORD*)model!=0xFD6BC0) return false;
        auto parent=**(BYTE***)((BYTE*)model+0x3C4);
        if(!parent || *(DWORD*)parent!=0xFFC300) return false;
        auto actor=**(BYTE***)(parent+0xF8);
        if(!actor || *(DWORD*)actor!=0xFC5F30) return false;
        auto renderer=**(BYTE***)(actor+8);
        return ApplyNativeHandsChecked(renderer,cameraVtable);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool InstallNativeHandsHook() {
    if(!g_nativeHandsEnabled || !g_nativeWeaponInstalled || g_nativeHandsInstalled) return g_nativeHandsInstalled;
    g_nativeHandsInstalled=true;
    Log("[NativeHands] human bone adapter enabled on accepted pistol-pose hook; NO additional engine patch; both failed sites untouched; in-level application pending");
    return true;
}
static void RemoveNativeHandsHook() {
    g_nativeHandsInstalled=false;
}

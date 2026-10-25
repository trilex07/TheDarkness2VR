#include "native_hands_math.h"
// Current trigger: the accepted pistol WORLD-pose callback. Its order relative
// to the avatar skin-pack and visible draw is still unproven; do not treat a
// successful CPU palette write as evidence that the rendered hand moved.
// No additional engine code patch: both failed palette/renderer sites stay stock.
static bool g_nativeHandsInstalled=false;
#ifdef D2VR_MATCHED_CAPTURE
static volatile LONG g_nativeHandsWriteSeq=0; // completed CPU palette edits, diagnostic only
#endif
static bool NativeHandsAvailable() { return g_nativeHandsEnabled && g_nativeHandsInstalled; }
static SRWLOCK g_nativeHandsLock=SRWLOCK_INIT;
#ifdef D2VR_MATCHED_CAPTURE
struct NativeHandsWriteStamp {
    LONG sequence=0;
    ULONGLONG frame=0,time=0;
    void* renderer=nullptr;
    void* palette=nullptr;
    DWORD poseSlot=0,skinSlot=0;
    bool rightWritten=false,leftWritten=false;
    float rightWrist[16]{},leftWrist[16]{};
};
static NativeHandsWriteStamp g_nativeHandsLastWrite;
#endif
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
        // The pistol model's +3C4 parent is not proof of ownership of the
        // visible first-person arms. The recenter observer established the
        // avatar's strong model reference at +1F8C and its 130-bone skeleton.
        // Resolve that same renderer here; refuse any missing/type-mismatched
        // link instead of writing an unrelated skin palette.
        auto camera=(BYTE*)sample.camera;
        if(!camera || !NativeAimOwnerMatches(camera,cameraVtable)) return false;
        auto avatar=**(BYTE***)(camera+0x30);
        if(!avatar || **(BYTE***)(avatar+0x630)!=(BYTE*)sample.inventory) return false;
        auto parent=**(BYTE***)(avatar+0x1F8C);
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
        if(g_nativeHandsDirtyBoth) {
            // The renderer repacks both 48-byte skin buffers from the selected
            // pose (6C3BC1..6C3C04), but the current dirty flag follows only
            // one alternating slot. Mark its partner as well so a later draw
            // cannot consume the previous packed pose after this CPU edit.
            renderer[0x580+(1-skinIndex)]=1;
        }
        // Preserve the actual unchanged left range on tracking loss.
        memcpy(cache.applied,palette+2*16,sizeof(cache.applied));
#ifdef D2VR_MATCHED_CAPTURE
        auto& stamp=g_nativeHandsLastWrite;
        stamp.frame=g_trackedFrame.id; stamp.time=GetTickCount64();
        stamp.renderer=renderer; stamp.palette=palette;
        stamp.poseSlot=index; stamp.skinSlot=skinIndex;
        stamp.rightWritten=rightChanged; stamp.leftWritten=leftChanged;
        memcpy(stamp.rightWrist,palette+27*16,sizeof(stamp.rightWrist));
        if(leftChanged) memcpy(stamp.leftWrist,palette+5*16,sizeof(stamp.leftWrist));
        stamp.sequence=InterlockedIncrement(&g_nativeHandsWriteSeq);
#endif
        ReleaseSRWLockExclusive(&g_nativeHandsLock);
        locked=false;
        static ULONGLONG last=0; auto now=GetTickCount64();
        if(now-last>1000) { last=now; Log("[NativeHands] wrists/fingers + arm IK applied; dirty=%s skin slot=%lu pose slot=%lu left=%s",g_nativeHandsDirtyBoth?"both":"current",skinIndex,index,leftValid ? (support ? "support":"free"):"tracking unavailable"); }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { if(locked) ReleaseSRWLockExclusive(&g_nativeHandsLock); return false; }
}
static bool NativeHandsFromWeapon(void* model,DWORD cameraVtable) {
    if(!NativeHandsAvailable()) return false;
    NativeGameplaySample sample;
    if(!CopyNativeGameplay(sample) || sample.entities[0]!=(DWORD)(uintptr_t)model) return false;
    __try {
        if(*(DWORD*)model!=0xFD6BC0) return false;
        auto camera=(BYTE*)sample.camera;
        if(!camera || !NativeAimOwnerMatches(camera,cameraVtable)) return false;
        auto avatar=**(BYTE***)(camera+0x30);
        if(!avatar || **(BYTE***)(avatar+0x630)!=(BYTE*)sample.inventory) return false;
        auto parent=**(BYTE***)(avatar+0x1F8C);
        if(!parent || *(DWORD*)parent!=0xFFC300) return false;
        auto actor=**(BYTE***)(parent+0xF8);
        if(!actor || *(DWORD*)actor!=0xFC5F30) return false;
        auto renderer=**(BYTE***)(actor+8);
        static void* reportedRenderer=nullptr;
        if(renderer && renderer!=reportedRenderer) {
            reportedRenderer=renderer;
            Log("[NativeHands] selected avatar skin renderer=%p parent=%p (weapon model=%p)",
                renderer,parent,model);
        }
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

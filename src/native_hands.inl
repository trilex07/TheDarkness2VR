#include "native_hands_math.h"
static bool ProtectedHandsAvailable();
static bool ProtectedHandsResourceOwned(void* resource);
static bool CopyProtectedHandPoses(DWORD camera,NativeWeaponPose (&poses)[2]);
static bool CopyProtectedWeaponHandPose(DWORD camera,unsigned hand,XrPosef& pose);
// Shared guarded IK. Legacy triggers remain separate from the protected
// private-instance packer in protected_hands.inl; neither is implicitly enabled.
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
    void* parent=nullptr; void* skeleton=nullptr; void* resource=nullptr;
    bool rightValid=false,leftValid=false,supportValid=false;
    float right[16]{},left[16]{},support[16]{};
};
static NativeHandCalibration g_nativeHandCalibration;
struct NativeHandsPaletteCache {
    void* renderer=nullptr; void* skeleton=nullptr; void* palette=nullptr; void* resource=nullptr;
    bool appliedValid[2]{};
    float original[44][16]{},applied[44][16]{};
};
static NativeHandsPaletteCache g_nativeHandsCache[3];
static bool ApplyNativeHandsChecked(void* receiver,DWORD cameraVtable,void* expectedResource=nullptr,bool protectedRoute=false) {
    if(protectedRoute ? !ProtectedHandsAvailable() : (!g_nativeHandsEnabled || !g_nativeHandsInstalled)) return false;
    NativeGameplaySample sample;
    if(!CopyNativeGameplay(sample) || (!expectedResource && !sample.entities[0])) return false;
    // The protected receiver is the selected avatar's packer, after animation
    // and before skin packing. Its owned human bones 2..45 can follow the
    // controllers during a demon hold; tentacle/attachment bones stay native.
    // The older pistol-triggered route has no such ordering guarantee.
    if(sample.grab==d2vr_input::GrabFeedback::Unknown ||
       (!protectedRoute && (sample.grab==d2vr_input::GrabFeedback::Busy || sample.alternateHeldTarget)))return false;
    float gunWorld[16]{};
    if(!expectedResource &&
       !TryNativeWeaponMatrix((void*)sample.entities[0],gunWorld,cameraVtable)) return false;
    NativeWeaponPose left,right; bool support=false;
    if(protectedRoute) {
        NativeWeaponPose poses[2];
        if(!expectedResource || !CopyProtectedHandPoses(sample.camera,poses))return false;
        left=poses[0];right=poses[1];
    } else {
        if(!TryAcquireSRWLockShared(&g_nativeWeaponLock)) return false;
        left=g_nativeWeaponPoses[0]; right=g_nativeWeaponPoses[1]; support=g_nativeSupport;
        ReleaseSRWLockShared(&g_nativeWeaponLock);
    }
    const auto poseTime=GetTickCount64();
    bool rightValid=!expectedResource || (right.valid && poseTime>=right.time && poseTime-right.time<100);
    bool leftValid=left.valid && poseTime>=left.time && poseTime-left.time<100;
    if(!rightValid && !leftValid)return false;
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
        if(renderer!=receiver) return false;
        const DWORD index=*(DWORD*)(renderer+0x424);
        const DWORD skinIndex=*(DWORD*)(renderer+0x448);
        if(index>=3 || skinIndex>=2) return false;
        auto outputVector=renderer+0x3F4+16*index;
        auto resource=**(BYTE***)(renderer+0x5BC);
        if(!resource || (expectedResource && resource!=expectedResource) ||
           (*(DWORD*)resource!=0xFC2578 && !(protectedRoute && ProtectedHandsResourceOwned(resource)))) return false;
        const float handUnits=protectedRoute ? g_gameUnitsPerMeter:1.f;
        if(expectedResource && rightValid && !d2vr_aim::BuildScaledWeaponMatrix((float*)(camera+0x80),right.local,handUnits,gunWorld))
            rightValid=false;
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
        float inverseParent[16],rightTarget[16],leftTarget[16];
        if(!d2vr_math::Invert((float*)(parent+0x140),inverseParent)) return false;
        if(rightValid)d2vr_math::Multiply(gunWorld,inverseParent,rightTarget);
        if(leftValid) {
            float lw[16];
            if(!d2vr_aim::BuildScaledWeaponMatrix((float*)(sample.camera+0x80),left.local,handUnits,lw))leftValid=false;
            else d2vr_math::Multiply(lw,inverseParent,leftTarget);
        }
        if(!TryAcquireSRWLockExclusive(&g_nativeHandsLock)) return false;
        locked=true;
        auto& cache=g_nativeHandsCache[index];
        const bool same=cache.renderer==renderer && cache.skeleton==skeleton && cache.palette==palette && cache.resource==resource;
        // Cached native poses can be queried more than once before animation
        // runs again. Solve from their pristine human bones, never our last IK.
        if(!same) {
            cache.renderer=renderer; cache.skeleton=skeleton; cache.palette=palette;
            cache.resource=resource;cache.appliedValid[0]=cache.appliedValid[1]=false;
        }
        // Animation can update just one arm between pack calls. Its change
        // must not turn our already-solved peer into a new pristine baseline.
        for(unsigned hand=0;hand<2;++hand) {
            const unsigned offset=hand*22;
            if(cache.appliedValid[hand] && !memcmp(palette+(2+offset)*16,cache.applied+offset,22*64))
                memcpy(bones+2+offset,cache.original+offset,22*64);
            else {
                memcpy(cache.original+offset,bones+2+offset,22*64);
                cache.appliedValid[hand]=false;
            }
        }
        NativeHandCalibration cal=g_nativeHandCalibration;
        if(cal.parent!=parent || cal.skeleton!=skeleton || cal.resource!=resource) {
            cal={};cal.parent=parent;cal.skeleton=skeleton;cal.resource=resource;
        }
        rightValid=rightValid && d2vr_hands::Valid(bones[27]) && d2vr_hands::Valid(bones[43]);
        leftValid=leftValid && d2vr_hands::Valid(bones[5]) && d2vr_hands::Valid(bones[21]);
        if(rightValid && !cal.rightValid) {
            float inverse[16];rightValid=d2vr_math::Invert(bones[43],inverse);
            if(rightValid){d2vr_math::Multiply(bones[27],inverse,cal.right);cal.rightValid=true;}
        }
        if(leftValid && !cal.leftValid) {
            float inverse[16];leftValid=d2vr_math::Invert(bones[21],inverse);
            if(leftValid){d2vr_math::Multiply(bones[5],inverse,cal.left);cal.leftValid=true;}
        }
        // Legacy two-hand support is optional. Protected grip poses never use
        // this cross-hand calibration; failure cannot suppress the free hand.
        if(support && rightValid && leftValid) {
            if(!cal.supportValid) {
                float inverse[16];support=d2vr_math::Invert(bones[43],inverse);
                if(support){d2vr_math::Multiply(bones[5],inverse,cal.support);cal.supportValid=true;}
            }
        } else support=false;
        g_nativeHandCalibration=cal;
        float wrist[16];bool rightSolved=false,leftSolved=false;
        if(rightValid) {
            d2vr_math::Multiply(cal.right,rightTarget,wrist);
            rightSolved=d2vr_hands::PoseArm(bones,25,26,27,44,45,wrist);
        }
        if(leftValid) {
            const bool useSupport=support && rightSolved;
            d2vr_math::Multiply(useSupport ? cal.support:cal.left,useSupport ? rightTarget:leftTarget,wrist);
            leftSolved=d2vr_hands::PoseArm(bones,3,4,5,22,23,wrist);
        }
        if(!rightSolved && !leftSolved) {
            ReleaseSRWLockExclusive(&g_nativeHandsLock);locked=false;return false;
        }
        const bool rightChanged=rightSolved && memcmp(bones+24,palette+24*16,22*64)!=0;
        const bool leftChanged=leftSolved && memcmp(bones+2,palette+2*16,22*64)!=0;
        if(!rightChanged && !leftChanged) {
            ReleaseSRWLockExclusive(&g_nativeHandsLock);
            locked=false;
            return true;
        }
        // Keep the camera root and demon tentacles unchanged.
        if(rightSolved)memcpy(palette+24*16,bones[24],22*64);
        if(leftSolved)memcpy(palette+2*16,bones[2],22*64);
        // Native pose setter 79927D marks this packed-skin buffer dirty after
        // editing model-space bones. 6C3C71 otherwise skips rebuilding the
        // 48-byte GPU matrices in renderer+428, leaving the visible arms stale.
        // Skin buffers (448, two slots) rotate independently of pose buffers
        // (424, three slots). Do not use the pose index for this byte.
        renderer[0x580+skinIndex]=1;
        if(g_nativeHandsDirtyBoth || protectedRoute) {
            // The renderer repacks both 48-byte skin buffers from the selected
            // pose (6C3BC1..6C3C04), but the current dirty flag follows only
            // one alternating slot. Mark its partner as well so a later draw
            // cannot consume the previous packed pose after this CPU edit.
            renderer[0x580+(1-skinIndex)]=1;
        }
        if(leftSolved){memcpy(cache.applied,palette+2*16,22*64);cache.appliedValid[0]=true;}
        if(rightSolved){memcpy(cache.applied+22,palette+24*16,22*64);cache.appliedValid[1]=true;}
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
        if(now-last>1000) { last=now; Log("[NativeHands] route=%s right=%d left=%d wrists/fingers + arm IK; dirty=%s skin slot=%lu pose slot=%lu",protectedRoute?"private-pack":"legacy-pistol",int(rightSolved),int(leftSolved),(g_nativeHandsDirtyBoth || protectedRoute)?"both":"current",skinIndex,index); }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { if(locked) ReleaseSRWLockExclusive(&g_nativeHandsLock); return false; }
}
static bool NativeHandsFromWeapon(void* model,DWORD cameraVtable) {
    if(!NativeHandsAvailable() || g_nativeHandsPackInstalled) return false;
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
    Log("[NativeHands] human bone adapter enabled; legacy pistol-pose trigger active unless verified packer opt-in is installed");
    return true;
}
static void RemoveNativeHandsHook() {
    g_nativeHandsInstalled=false;
}

#ifdef D2VR_MATCHED_CAPTURE
// Current-EXE resource vtable 00FC2578+EC -> 006C3890. The method takes one
// renderer smart-reference argument (ret 4); forwarding once preserves its
// native reference release and all renderer side effects.
using NativeHandsPackMethod=void(__thiscall*)(void*,void*);
static NativeHandsPackMethod g_nativeHandsPackOriginal=(NativeHandsPackMethod)0x006C3890;
static thread_local bool g_insideNativeHandsPack=false;
static void __fastcall NativeHandsPackHook(void* resource,void*,void* rendererRef) {
    const bool outermost=!g_insideNativeHandsPack;
    if(outermost) g_insideNativeHandsPack=true;
    if(outermost && g_nativeHandsPackInstalled && g_restoredTrackedGameplay &&
       resource==g_nativeHandsSelectedResource) {
        void* renderer=nullptr;
        __try { if(rendererRef) renderer=*(void**)rendererRef; }
        __except(EXCEPTION_EXECUTE_HANDLER) { renderer=nullptr; }
        if(renderer) ApplyNativeHandsChecked(renderer,0xFFC7C0,resource);
    }
    g_nativeHandsPackOriginal(resource,rendererRef);
    if(outermost) g_insideNativeHandsPack=false;
}
static bool InstallNativeHandsPackHook() {
    if(!g_nativeHandsInstalled || g_nativeHandsPackInstalled) return g_nativeHandsPackInstalled;
    auto slot=(void* volatile*)0x00FC2664;
    const void* expected=(void*)0x006C3890;
    __try {
        const BYTE prolog[]={0x55,0x8B,0xEC,0x83,0xE4,0xF0,0x83,0xEC,0x74};
        if(*slot!=expected || memcmp((void*)0x006C3890,prolog,sizeof(prolog))!=0) {
            Log("[NativeHandsPack] REFUSED resource vtable or packer bytes mismatch"); return false;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    if (!NativeImageWriteAllowed(reinterpret_cast<uintptr_t>(slot),sizeof(void*))) return false;
    DWORD old=0;
    if(!VirtualProtect((void*)slot,sizeof(void*),PAGE_READWRITE,&old)) return false;
    void* prior=InterlockedCompareExchangePointer(slot,(void*)&NativeHandsPackHook,(void*)expected);
    DWORD ignored=0;
    const bool restored=VirtualProtect((void*)slot,sizeof(void*),old,&ignored)!=FALSE;
    if(prior!=expected || !restored) {
        if(prior==expected) {
            DWORD writable=0;
            if(VirtualProtect((void*)slot,sizeof(void*),PAGE_READWRITE,&writable)) {
                InterlockedCompareExchangePointer(slot,(void*)expected,(void*)&NativeHandsPackHook);
                VirtualProtect((void*)slot,sizeof(void*),writable,&ignored);
            }
        }
        if(*slot!=(void*)expected) RaiseFailFastException(nullptr,nullptr,0);
        return false;
    }
    g_nativeHandsPackInstalled=true;
    Log("[NativeHandsPack] active: guarded controller IK before selected avatar skin pack; resource vtable slot=%p",slot);
    return true;
}
static bool RemoveNativeHandsPackHook() {
    if(!g_nativeHandsPackInstalled) return true;
    auto slot=(void* volatile*)0x00FC2664;
    DWORD old=0;
    if(!VirtualProtect((void*)slot,sizeof(void*),PAGE_READWRITE,&old)) return false;
    void* prior=InterlockedCompareExchangePointer(slot,(void*)g_nativeHandsPackOriginal,(void*)&NativeHandsPackHook);
    DWORD ignored=0;
    const bool restored=VirtualProtect((void*)slot,sizeof(void*),old,&ignored)!=FALSE;
    if(prior!=(void*)&NativeHandsPackHook || !restored || *slot!=(void*)g_nativeHandsPackOriginal)
        return false;
    g_nativeHandsPackInstalled=false;
    return true;
}
#endif

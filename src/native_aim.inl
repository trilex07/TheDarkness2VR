// Pinned stock EXE, x86. C62F50 builds inventory aim; C6300F calls the camera
// matrix getter (thiscall, no stack args, EAX pointer), then reads forward row2
// and origin row3. Replacing ONLY that getter leaves native query filters,
// ignored entities, async callback lifetime and hit smart references intact.
struct NativeAimSnapshot {
    bool valid=false;
    ULONGLONG time=0;
    float origin[3]{},direction[3]{0,0,1};
};
static SRWLOCK g_nativeAimLock=SRWLOCK_INIT;
static NativeAimSnapshot g_nativeAim;
static NativeAimSnapshot g_nativeThrowAim;
static ULONGLONG g_nativeThrowAimUntil=0;
static bool g_nativeAimHookInstalled=false;
static volatile LONG g_nativeAimAppliedAt=0;
static const BYTE kNativeAimOriginal[8]={0x8B,0x82,0x9C,0,0,0,0xFF,0xD0};
static BYTE g_nativeAimPatch[6]{};
struct NativeGameplaySample {
    ULONGLONG time=0;
    d2vr_input::GrabFeedback grab=d2vr_input::GrabFeedback::Unknown;
    DWORD camera=0,inventory=0,entities[2]{}; // primary/right, secondary/left
    bool offHandEquipped=false;
    DWORD grabFire=0,grabEntity=0;
#ifdef D2VR_MATCHED_CAPTURE
    DWORD avatarSkinResource=0; // fast hint only; full ownership checked at the packer
#endif
    // Character/weapon holds do not use the ordinary prop throw path.
    bool alternateHeldTarget=false;
};
static SRWLOCK g_nativeGameplayLock=SRWLOCK_INIT;
static NativeGameplaySample g_nativeGameplay;
static bool CopyNativeGameplay(NativeGameplaySample& out) {
    if(!TryAcquireSRWLockShared(&g_nativeGameplayLock)) return false;
    out=g_nativeGameplay; ReleaseSRWLockShared(&g_nativeGameplayLock);
    return out.camera && GetTickCount64()-out.time<150;
}
static d2vr_input::GrabFeedback ReadNativeGrabFeedback() {
    NativeGameplaySample s;
    return CopyNativeGameplay(s) ? s.grab:d2vr_input::GrabFeedback::Unknown;
}

static void ResetNativeAim() {
    ResetNativeThrow();
    AcquireSRWLockExclusive(&g_nativeAimLock);
    g_nativeAim={};
    g_nativeThrowAim={}; g_nativeThrowAimUntil=0;
    ReleaseSRWLockExclusive(&g_nativeAimLock);
    InterlockedExchange(&g_nativeAimAppliedAt,0);
    AcquireSRWLockExclusive(&g_nativeGameplayLock); g_nativeGameplay={}; ReleaseSRWLockExclusive(&g_nativeGameplayLock);
#ifdef D2VR_MATCHED_CAPTURE
    InterlockedExchangePointer(&g_nativeHandsSelectedResource,nullptr);
#endif
    ResetNativeWeaponPoses();
}
static bool CopyNativeAim(NativeAimSnapshot& out) {
    if(!TryAcquireSRWLockShared(&g_nativeAimLock)) return false;
    out=g_nativeAim;
    ReleaseSRWLockShared(&g_nativeAimLock);
    return out.valid && GetTickCount64()-out.time<100;
}
static bool ReadNativeAimDirection(float out[3]) {
    NativeAimSnapshot s;
    if(!CopyNativeAim(s) || DWORD(GetTickCount()-DWORD(InterlockedCompareExchange(&g_nativeAimAppliedAt,0,0)))>=100) return false;
    memcpy(out,s.direction,sizeof(s.direction)); return true;
}
static void PublishNativeAim(bool context,const XINPUT_GAMEPAD& gp,
    bool throwRequested=false,bool active=true,bool newGrab=false) {
    NativeAimSnapshot s;
    NativeAimSnapshot previous,throwAim;
    ULONGLONG throwUntil;
    AcquireSRWLockShared(&g_nativeAimLock);
    previous=g_nativeAim; throwAim=g_nativeThrowAim; throwUntil=g_nativeThrowAimUntil;
    ReleaseSRWLockShared(&g_nativeAimLock);
    const auto now=GetTickCount64();
    if(newGrab) { throwAim={}; throwUntil=0; }
    NativeGameplaySample held;
    const bool stockAlternateHold=CopyNativeGameplay(held) && held.alternateHeldTarget;
    if(g_nativeLeftAim && g_nativeAimHookInstalled && context && g_headGeometry &&
       g_headGeometryReferenceValid && g_hands[0].valid && !stockAlternateHold &&
       gp.bLeftTrigger<30 && gp.bRightTrigger<30) {
        const auto& ref=g_headGeometryReference;
        const auto& aim=g_hands[0].aim;
        const XrQuaternionf inverse={-ref.orientation.x,-ref.orientation.y,-ref.orientation.z,ref.orientation.w};
        const auto origin=RotVec(inverse,aim.position.x-ref.position.x,
            aim.position.y-ref.position.y,aim.position.z-ref.position.z);
        const auto forward=RotVec(aim.orientation,0,0,-1);
        const auto direction=RotVec(inverse,forward.x,forward.y,forward.z);
        s.origin[0]=origin.x; s.origin[1]=origin.y; s.origin[2]=-origin.z;
        s.direction[0]=direction.x; s.direction[1]=direction.y; s.direction[2]=-direction.z;
        s.time=now; s.valid=true;
        if(throwRequested) {
            // Preserve the last held-grip ray, before the release wrist motion.
            // Engine query/animation may consume it after the 120ms LB pulse.
            throwAim=previous.valid && now-previous.time<100 ? previous:s;
            throwUntil=now+450;
        }
        if(throwAim.valid && now<throwUntil) { s=throwAim; s.time=now; }
        else { throwAim={}; throwUntil=0; if(!active) s={}; }
    } else {
        // Tracking loss, pause, recenter and trigger ownership cancel the lock.
        throwAim={}; throwUntil=0;
    }
    AcquireSRWLockExclusive(&g_nativeAimLock);
    g_nativeAim=s; g_nativeThrowAim=throwAim; g_nativeThrowAimUntil=throwUntil;
    ReleaseSRWLockExclusive(&g_nativeAimLock);
    if(!s.valid) InterlockedExchange(&g_nativeAimAppliedAt,0);
    if(throwRequested) Log("[NativeAim] throw ray locked=%d for 450ms direction=(%.3f,%.3f,%.3f) triggers=(%u,%u)",
        (int)s.valid,s.direction[0],s.direction[1],s.direction[2],gp.bLeftTrigger,gp.bRightTrigger);
}

static bool NativeAimOwnerMatches(void* self,DWORD cameraVtable=0xFFC7C0) {
    __try {
        // Runtime verified bidirectionally; a shared method address alone is not a type check.
        if(*(DWORD*)self!=cameraVtable) return false;
        auto avatar=**(BYTE***)((BYTE*)self+0x30);
        if(!avatar || *(DWORD*)avatar!=0x1001630) return false;
        if(**(void***)(avatar+0x62C)!=self) return false;
        auto inventory=**(BYTE***)(avatar+0x630);
        return inventory && *(DWORD*)inventory==0xFFCA10 &&
            **(BYTE***)(inventory+0x2A8)==avatar;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static void CaptureNativeGameplay(void* self) {
    NativeGameplaySample s;
    __try {
        auto avatar=**(BYTE***)((BYTE*)self+0x30);
        auto inventory=**(BYTE***)(avatar+0x630);
        s.camera=(DWORD)(uintptr_t)self; s.inventory=(DWORD)(uintptr_t)inventory;
        s.offHandEquipped=**(DWORD**)(inventory+0xBC)!=0;
        // Slot 6 is the left demon arm, confirmed by native BA66E0 and live RTTI.
        auto weapon=**(BYTE***)(inventory+0x20C+6*4);
        if(weapon && *(DWORD*)weapon==0xFD6E00 && **(BYTE***)(weapon+0x980)==avatar) {
            const DWORD mode=*(DWORD*)(weapon+0xA78);
            if(mode<4) {
                auto parts=*(BYTE**)(weapon+0xA64);
                auto fire=**(BYTE***)(parts+48*mode+0xC);
                if(fire && *(DWORD*)fire==0x1004FE8 && *(DWORD*)(fire+4)==0x11126F0 &&
                   **(BYTE***)(fire+0x4C)==weapon) {
                    const bool busy=**(DWORD**)(fire+0x558) || **(DWORD**)(fire+0x580) || **(DWORD**)(fire+0x590);
                    s.grab=busy ? d2vr_input::GrabFeedback::Busy:d2vr_input::GrabFeedback::Empty;
                    s.grabFire=(DWORD)(uintptr_t)fire;
                    s.grabEntity=**(DWORD**)(fire+0x580);
                    const auto alternateCharacter=*(DWORD**)(fire+0x584);
                    const auto retainedTarget=*(DWORD**)(fire+0x590);
                    s.alternateHeldTarget=(alternateCharacter && *alternateCharacter) ||
                        (retainedTarget && *retainedTarget) ||
                        (s.grabEntity && *((BYTE*)s.grabEntity+0x24));
                }
            }
        }
        for(int hand=0;hand<2;++hand) {
            auto gun=**(BYTE***)(inventory+0xB0+12*hand);
            if(!gun || *(DWORD*)gun!=0x1010E30 || **(BYTE***)(gun+0x980)!=avatar) continue;
            auto entity=**(BYTE***)(gun+0xE0+0x4C); // native A1DA90(0,0)
            if(entity && *(DWORD*)entity==0xFD6BC0) s.entities[hand]=(DWORD)(uintptr_t)entity;
        }
#ifdef D2VR_MATCHED_CAPTURE
        __try {
            auto parent=**(BYTE***)(avatar+0x1F8C);
            auto actor=parent && *(DWORD*)parent==0xFFC300 ? **(BYTE***)(parent+0xF8):nullptr;
            auto renderer=actor && *(DWORD*)actor==0xFC5F30 ? **(BYTE***)(actor+8):nullptr;
            auto resource=renderer ? **(BYTE***)(renderer+0x5BC):nullptr;
            if(resource && *(DWORD*)resource==0xFC2578)
                s.avatarSkinResource=(DWORD)(uintptr_t)resource;
        } __except(EXCEPTION_EXECUTE_HANDLER) { s.avatarSkinResource=0; }
#endif
        s.time=GetTickCount64();
    } __except(EXCEPTION_EXECUTE_HANDLER) { s={}; }
    if(TryAcquireSRWLockExclusive(&g_nativeGameplayLock)) {
        g_nativeGameplay=s;
#ifdef D2VR_MATCHED_CAPTURE
        InterlockedExchangePointer(&g_nativeHandsSelectedResource,(void*)(uintptr_t)s.avatarSkinResource);
#endif
        ReleaseSRWLockExclusive(&g_nativeGameplayLock);
    }
    static int last=-1;
    const int state=int(s.grab);
    if(s.camera && state!=last) {
        last=state; Log("[NativeGrab] state=%s (558 acquisition / 580 attachment / 590 retained target)",
            state==0 ? "unknown":state==1 ? "empty":"busy");
    }
    static bool lastAlternate=false;
    if(s.camera && s.alternateHeldTarget!=lastAlternate) {
        lastAlternate=s.alternateHeldTarget;
        Log("[NativeGrab] alternate character/weapon hold=%d; native aim/velocity override suppressed",
            (int)s.alternateHeldTarget);
    }
    static DWORD lastEntities[2]{};
    if(s.camera && (s.entities[0]!=lastEntities[0] || s.entities[1]!=lastEntities[1])) {
        memcpy(lastEntities,s.entities,sizeof(lastEntities));
        Log("[NativeWeapon] active pistol model primary/right=%p secondary/left=%p",(void*)s.entities[0],(void*)s.entities[1]);
    }
}
static float* NativeAimMatrixChecked(void* self,DWORD cameraVtable) {
    using Getter=float*(__thiscall*)(void*);
    // Same receiver and same verified ABI as the original direct virtual call.
    const auto getter=*(Getter*)((BYTE*)*(void**)self+0x9C);
    float* original=getter(self);
    const bool owner=NativeAimOwnerMatches(self,cameraVtable);
    if(owner) CaptureNativeGameplay(self);
    NativeGameplaySample held;
    // A failed or stale branch read must not apply a controller ray to an
    // unidentified native target (notably during enemy acquisition).
    if(owner && (!CopyNativeGameplay(held) || held.camera!=(DWORD)(uintptr_t)self ||
                 held.alternateHeldTarget)) return original;
    NativeAimSnapshot snapshot;
    if(!original || !CopyNativeAim(snapshot) || !owner) return original;
    __declspec(align(16)) static thread_local float result[16];
    if(!d2vr_aim::BuildRayMatrix(original,snapshot.origin,snapshot.direction,result)) return original;
    InterlockedExchange(&g_nativeAimAppliedAt,LONG(GetTickCount()));
    static ULONGLONG lastLog=0;
    const auto now=GetTickCount64();
    if(now-lastLog>1000) {
        lastLog=now;
        Log("[NativeAim] left ray applied origin=(%.3f,%.3f,%.3f) forward=(%.3f,%.3f,%.3f); engine collision query retained",
            result[12],result[13],result[14],result[8],result[9],result[10]);
    }
    return result;
}
static float* __fastcall NativeAimMatrix(void* self,void*) {
    return NativeAimMatrixChecked(self,0xFFC7C0);
}
static bool InstallNativeAimHook() {
    if((!g_nativeLeftAim && !g_nativeWeaponPose) || g_nativeAimHookInstalled) return g_nativeAimHookInstalled;
    BYTE* site=(BYTE*)0xC63009;
    __try {
        if((uintptr_t)GetModuleHandleW(nullptr)!=0x400000 ||
           memcmp(site,kNativeAimOriginal,8)!=0 ||
           *(DWORD*)(0xFFC7C0+0x9C)!=0x504810 ||
           memcmp((void*)0x504810,"\x8D\x81\x80\0\0\0\xC3",7)!=0) {
            Log("[NativeAim] unsupported image/call site; disabled"); return false;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    DWORD old=0;
    if(!VirtualProtect(site,6,PAGE_EXECUTE_READWRITE,&old)) return false;
    g_nativeAimPatch[0]=0xB8; g_nativeAimPatch[5]=0x90;
    const DWORD fn=(DWORD)(uintptr_t)&NativeAimMatrix;
    memcpy(g_nativeAimPatch+1,&fn,4); memcpy(site,g_nativeAimPatch,6);
    DWORD ignored; VirtualProtect(site,6,old,&ignored); FlushInstructionCache(GetCurrentProcess(),site,6);
    g_nativeAimHookInstalled=true;
    Log("[NativeAim] experimental left-hand ray hook installed; requires tracking/reference, tracked left hand and grip/holding intent; triggers restore stock aim");
    return true;
}
static void RemoveNativeAimHook() {
    if(!g_nativeAimHookInstalled) return;
    BYTE* site=(BYTE*)0xC63009; DWORD old;
    if(memcmp(site,g_nativeAimPatch,6)==0 && VirtualProtect(site,6,PAGE_EXECUTE_READWRITE,&old)) {
        memcpy(site,kNativeAimOriginal,6); DWORD ignored;
        VirtualProtect(site,6,old,&ignored); FlushInstructionCache(GetCurrentProcess(),site,6);
    }
    g_nativeAimHookInstalled=false;
}

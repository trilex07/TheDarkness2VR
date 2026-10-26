// AD9240 resolves the skeleton/parent first. AD94A2 compares its resulting
// stack matrix before native dirty-transform/bounds/physics updates. Change
// that stack value only: local +180 and world +140 are different spaces.
struct NativeWeaponPose {
    bool valid=false; ULONGLONG time=0;
    float local[16]{};
};
static SRWLOCK g_nativeWeaponLock=SRWLOCK_INIT;
static NativeWeaponPose g_nativeWeaponPoses[2]; // OpenXR left, right
static d2vr_input::HandSupport g_handSupport;
static bool g_nativeSupport=false;
static volatile LONG g_nativeSupportReset=0;
using NativeMatrixCompare=int(__cdecl*)(const float*,const float*,float);
static NativeMatrixCompare g_nativeMatrixCompare=(NativeMatrixCompare)0x481F50;
static BYTE g_nativeWeaponPatch[5]{};
static const BYTE kNativeWeaponOriginal[5]={0xE8,0xA9,0x8A,0x9A,0xFF};
static bool g_nativeWeaponInstalled=false;

static void ResetNativeWeaponPoses() {
    AcquireSRWLockExclusive(&g_nativeWeaponLock);
    g_nativeWeaponPoses[0]={}; g_nativeWeaponPoses[1]={};
    g_nativeSupport=false;
    InterlockedExchange(&g_nativeSupportReset,1);
    ReleaseSRWLockExclusive(&g_nativeWeaponLock);
}
static void PublishNativeWeaponPoses(bool context) {
    NativeWeaponPose poses[2]{};
    if(InterlockedExchange(&g_nativeSupportReset,0)) g_handSupport.Reset();
    if(g_nativeWeaponPose && g_nativeWeaponInstalled && context && g_headGeometry && g_headGeometryReferenceValid) {
        const auto& ref=g_headGeometryReference;
        const float rq[4]={ref.orientation.x,ref.orientation.y,ref.orientation.z,ref.orientation.w};
        const float rp[3]={ref.position.x,ref.position.y,ref.position.z};
        float reference[16],inverse[16];
        if(d2vr_math::TrackingPose(rq,rp,reference) && d2vr_math::Invert(reference,inverse)) {
            for(int hand=0;hand<2;++hand) if(g_hands[hand].valid) {
                const auto& p=g_hands[hand].aim;
                const float q[4]={p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w};
                const float pos[3]={p.position.x,p.position.y,p.position.z};
                float tracking[16];
                if(!d2vr_math::TrackingPose(q,pos,tracking)) continue;
                d2vr_math::Multiply(tracking,inverse,poses[hand].local);
                poses[hand].time=GetTickCount64(); poses[hand].valid=true;
            }
        }
    }
    NativeGameplaySample sample;
    const bool both=NativeHandsAvailable() && poses[0].valid && poses[1].valid && CopyNativeGameplay(sample) && sample.entities[0];
    const auto& l=g_hands[0].grip.position;
    const auto& r=g_hands[1].grip.position;
    const float dx=l.x-r.x,dy=l.y-r.y,dz=l.z-r.z;
    const bool oldSupport=g_handSupport.active;
    // A left grip belongs to the demon arm. Drop the optional pistol support
    // before its grab pulse: the native skin packer deliberately leaves human
    // IK alone while an object is held, so a support pose would otherwise stay
    // visible until the object is released.
    const bool supportEligible=both && sample.grab!=d2vr_input::GrabFeedback::Busy &&
        !g_tentacles.holdingIntent && g_hands[0].squeeze<.35f;
    const bool support=g_handSupport.Update(GetTickCount64()/1000.0,supportEligible,
        both && sample.offHandEquipped,std::sqrt(dx*dx+dy*dy+dz*dz));
    if(support!=oldSupport) Log("[HandSupport] %s; left near right, no grip required",support ? "two-handed":"free left");
    AcquireSRWLockExclusive(&g_nativeWeaponLock);
    memcpy(g_nativeWeaponPoses,poses,sizeof(poses));
    g_nativeSupport=support;
    ReleaseSRWLockExclusive(&g_nativeWeaponLock);
}
static bool TryNativeWeaponMatrix(void* self,float world[16],DWORD cameraVtable=0xFFC7C0) {
    // An unequipped model is null and otherwise matches an empty entities[]
    // slot, reaching the vtable read below. Reject it before matching the hand.
    if(!self || !world || !g_nativeWeaponPose || !g_nativeWeaponInstalled) return false;
    NativeGameplaySample sample;
    if(!CopyNativeGameplay(sample)) return false;
    int hand=-1;
    for(int i=0;i<2;++i) if(sample.entities[i]==(DWORD)(uintptr_t)self) hand=1-i;
    if(hand<0) return false;
    NativeWeaponPose pose;
    if(!TryAcquireSRWLockShared(&g_nativeWeaponLock)) return false;
    pose=g_nativeWeaponPoses[hand]; ReleaseSRWLockShared(&g_nativeWeaponLock);
    if(!pose.valid || GetTickCount64()-pose.time>=100) return false;
    __try {
        if(!NativeAimOwnerMatches((void*)sample.camera,cameraVtable) || *(DWORD*)self!=0xFD6BC0) return false;
        auto inventory=(BYTE*)sample.inventory;
        auto camera=(BYTE*)sample.camera;
        auto avatar=**(BYTE***)(camera+0x30);
        if(**(BYTE***)(avatar+0x630)!=inventory) return false;
        auto gun=**(BYTE***)(inventory+0xB0+12*(1-hand));
        if(!gun || *(DWORD*)gun!=0x1010E30 || **(BYTE***)(gun+0x980)!=avatar ||
           **(void***)(gun+0x12C)!=self) return false;
        if(!d2vr_aim::BuildWeaponMatrix((float*)(camera+0x80),pose.local,world)) return false;
        // Refuse unrelated/dropped models or implausible transforms.
        float distance=0;
        for(int c=0;c<3;++c) {
            const float d=((float*)((BYTE*)self+0x140))[12+c]-((float*)(camera+0x80))[12+c];
            distance+=d*d;
        }
        if(!std::isfinite(distance) || distance>9) return false;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static int NativeWeaponCompareChecked(const float* current,float* candidate,float tolerance,DWORD cameraVtable) {
    __declspec(align(16)) float world[16];
    void* self=(BYTE*)current-0x140;
    if(TryNativeWeaponMatrix(self,world,cameraVtable)) {
        memcpy(candidate,world,sizeof(world));
        NativeHandsFromWeapon(self,cameraVtable);
        static ULONGLONG last=0; const auto now=GetTickCount64();
        if(now-last>1000) { last=now; Log("[NativeWeapon] pistol model pose applied entity=%p position=(%.3f,%.3f,%.3f); tracked shot hook reported separately",self,world[12],world[13],world[14]); }
    }
    return g_nativeMatrixCompare(current,candidate,tolerance);
}
static int __cdecl NativeWeaponCompare(const float* current,float* candidate,float tolerance) {
    return NativeWeaponCompareChecked(current,candidate,tolerance,0xFFC7C0);
}
static bool InstallNativeWeaponHook() {
    if(!g_nativeWeaponPose || g_nativeWeaponInstalled || !g_nativeAimHookInstalled) return g_nativeWeaponInstalled;
    BYTE* site=(BYTE*)0xAD94A2;
    __try {
        const BYTE before[]={0x52,0x8D,0xBE,0x40,0x01,0,0,0x57};
        if(memcmp(site,kNativeWeaponOriginal,5)!=0 || memcmp(site-8,before,sizeof(before))!=0 ||
           *(DWORD*)(0xFD6BC0+0xF0)!=0xAD9240 ||
           memcmp((void*)0x481F50,"\x55\x8B\xEC\x83\xE4\xF0\x83\xEC\x50",9)!=0) {
            Log("[NativeWeapon] unsupported world-transform call site; disabled"); return false;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    if (!NativeImageWriteAllowed(reinterpret_cast<uintptr_t>(site),5)) return false;
    DWORD old;
    if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old)) return false;
    g_nativeWeaponPatch[0]=0xE8;
    const DWORD jump=(DWORD)(uintptr_t)&NativeWeaponCompare-(DWORD)(uintptr_t)(site+5);
    memcpy(g_nativeWeaponPatch+1,&jump,4); memcpy(site,g_nativeWeaponPatch,5);
    DWORD ignored; VirtualProtect(site,5,old,&ignored); FlushInstructionCache(GetCurrentProcess(),site,5);
    g_nativeWeaponInstalled=true;
    Log("[NativeWeapon] tracked pistol MODEL enabled; primary=right, secondary=left; shot hook reported separately");
    return true;
}
static void RemoveNativeWeaponHook() {
    if(!g_nativeWeaponInstalled) return;
    BYTE* site=(BYTE*)0xAD94A2; DWORD old;
    if(memcmp(site,g_nativeWeaponPatch,5)==0 && VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old)) {
        memcpy(site,kNativeWeaponOriginal,5); DWORD ignored;
        VirtualProtect(site,5,old,&ignored); FlushInstructionCache(GetCurrentProcess(),site,5);
    }
    g_nativeWeaponInstalled=false;
}

// CFBBA0 supplies the pistol fire matrix to C47570. The latter retains ammo,
// spread (B27AA0), collision (AE9B50), damage and return AL. Its five stack
// arguments are callee-cleaned. Never patch the shared camera/inventory ray
// to aim a gun: it is also used by the demon arm.
using NativePistolFire=bool(__thiscall*)(void*,void*,const float*,uintptr_t,uintptr_t,uintptr_t);
static NativePistolFire g_nativePistolFire=(NativePistolFire)0xC47570;
static bool g_nativePistolInstalled=false;
static BYTE g_nativePistolPatch[5]{};
static const BYTE kNativePistolOriginal[5]={0xE8,0x0B,0xB8,0xF4,0xFF};
static bool TryNativePistolMatrix(void* self,void* weaponRef,float world[16],DWORD cameraVtable) {
    if(!g_nativePistolInstalled) return false;
    void* entity=nullptr;
    __try {
        auto fire=(BYTE*)self;
        if(*(DWORD*)fire!=0x1010AE0 || *(DWORD*)(fire+4)!=0x11193F0) return false;
        auto weapon=**(BYTE***)weaponRef;
        if(!weapon || *(DWORD*)weapon!=0x1010E30 || **(BYTE***)(fire+0x4C)!=weapon) return false;
        const DWORD mode=*(DWORD*)(weapon+0xA78);
        if(mode>=4) return false;
        auto parts=*(BYTE**)(weapon+0xA64);
        if(**(void***)(parts+48*mode+0xC)!=self) return false;
        entity=**(void***)(weapon+0x12C);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    return TryNativeWeaponMatrix(entity,world,cameraVtable);
}
static bool NativePistolFireChecked(void* self,void* weaponRef,const float* matrix,
    uintptr_t a3,uintptr_t a4,uintptr_t a5,DWORD cameraVtable) {
    __declspec(align(16)) float world[16];
    const bool tracked=TryNativePistolMatrix(self,weaponRef,world,cameraVtable);
    if(tracked) {
        Log("[NativeWeapon] tracked SHOT origin=(%.3f,%.3f,%.3f) direction=(%.3f,%.3f,%.3f); native spread/collision retained",
            world[12],world[13],world[14],world[8],world[9],world[10]);
    }
    return g_nativePistolFire(self,weaponRef,tracked ? world:matrix,a3,a4,a5);
}
static bool __fastcall NativePistolFireHook(void* self,void*,void* weaponRef,const float* matrix,
    uintptr_t a3,uintptr_t a4,uintptr_t a5) {
    return NativePistolFireChecked(self,weaponRef,matrix,a3,a4,a5,0xFFC7C0);
}
static bool InstallNativePistolHook() {
    if(!g_nativeWeaponInstalled || g_nativePistolInstalled) return g_nativePistolInstalled;
    BYTE* site=(BYTE*)0xCFBD60;
    __try {
        if(memcmp(site,kNativePistolOriginal,5)!=0 || *(DWORD*)(0x1010AE0+0x64)!=0xCFBBA0 ||
           memcmp((void*)0xC47570,"\x55\x8B\xEC\x83\xE4\xF0\x83\xEC\x64",9)!=0) {
            Log("[NativeWeapon] unsupported pistol fire call site; shot override disabled"); return false;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    if (!NativeImageWriteAllowed(reinterpret_cast<uintptr_t>(site),5)) return false;
    DWORD old;
    if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old)) return false;
    g_nativePistolPatch[0]=0xE8;
    const DWORD jump=(DWORD)(uintptr_t)&NativePistolFireHook-(DWORD)(uintptr_t)(site+5);
    memcpy(g_nativePistolPatch+1,&jump,4); memcpy(site,g_nativePistolPatch,5);
    DWORD ignored; VirtualProtect(site,5,old,&ignored); FlushInstructionCache(GetCurrentProcess(),site,5);
    g_nativePistolInstalled=true;
    Log("[NativeWeapon] tracked pistol SHOT hook enabled; live impact alignment awaiting playtest");
    return true;
}
static void RemoveNativePistolHook() {
    if(!g_nativePistolInstalled) return;
    BYTE* site=(BYTE*)0xCFBD60; DWORD old;
    if(memcmp(site,g_nativePistolPatch,5)==0 && VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old)) {
        memcpy(site,kNativePistolOriginal,5); DWORD ignored;
        VirtualProtect(site,5,old,&ignored); FlushInstructionCache(GetCurrentProcess(),site,5);
    }
    g_nativePistolInstalled=false;
}

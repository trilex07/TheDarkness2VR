// Stock DemonArmGrabFireBehavior::event D23972 -> 878930 releases a held
// object. 878930 computes a camera/auto-aim velocity, copies it to stack +60,
// then 879C88 calls cdecl 6DA050(velocity,zero,tolerance). The same stack +60
// reaches physics VT+C4 at 879CEA (mode 2). Redirect only this one comparison,
// after native auto-aim and before physics; retain magnitude, spin and ownership.
// EDI remains the fire receiver throughout this pinned function. The naked
// ABI bridge adds EDI explicitly; no inline C++ assumption about register use.
struct NativeThrowToken {
    ULONGLONG until=0,sequence=0;
    DWORD camera=0,inventory=0,fire=0,entity=0;
    float direction[3]{}; // reference-local LH unit direction, independent of gaze
};
static SRWLOCK g_nativeThrowLock=SRWLOCK_INIT;
static NativeThrowToken g_nativeThrowToken;
static ULONGLONG g_nativeThrowSequence=0;
static bool g_nativeThrowInstalled=false;
static BYTE g_nativeThrowPatch[5]{};
static const BYTE kNativeThrowOriginal[]={0xE8,0xC3,0x03,0xE6,0xFF};
using NativeThrowCompare=int(__cdecl*)(const float*,const float*,float);
static NativeThrowCompare g_nativeThrowCompare=(NativeThrowCompare)0x6DA050;
static void ResetNativeThrow() {
    AcquireSRWLockExclusive(&g_nativeThrowLock);
    g_nativeThrowToken={};
    ReleaseSRWLockExclusive(&g_nativeThrowLock);
}
static void PublishNativeThrow(bool context,bool newGrab,bool requested,d2vr_input::Vec3 velocity,d2vr_input::Vec3 aim={}) {
    if(!context || newGrab || !g_hands[0].valid || !g_headGeometry || !g_headGeometryReferenceValid) {
        ResetNativeThrow(); return;
    }
    if(!requested) return;
    NativeThrowToken token;
    NativeGameplaySample sample;
    d2vr_input::Vec3 direction{};
    if(g_nativeLeftAim && g_nativeThrowInstalled && d2vr_input::ThrowDirection(velocity,aim,g_throwAimAssist,direction) &&
       CopyNativeGameplay(sample) && sample.grabFire && sample.grabEntity) {
        const auto& q=g_headGeometryReference.orientation;
        const auto v=RotVec({-q.x,-q.y,-q.z,q.w},direction.x,direction.y,direction.z);
        token.direction[0]=v.x; token.direction[1]=v.y; token.direction[2]=-v.z;
        token.camera=sample.camera; token.inventory=sample.inventory;
        token.fire=sample.grabFire; token.entity=sample.grabEntity;
        token.until=GetTickCount64()+650;
    }
    AcquireSRWLockExclusive(&g_nativeThrowLock);
    token.sequence=++g_nativeThrowSequence;
    g_nativeThrowToken=token;
    ReleaseSRWLockExclusive(&g_nativeThrowLock);
    Log("[NativeThrow] request=%llu armed=%d chosenDirection=(%.3f,%.3f,%.3f) peak=(%.3f,%.3f,%.3f) controllerAim=(%.3f,%.3f,%.3f) assist=%.2f fire=%p entity=%p lifetime=650ms",
        token.sequence,(int)(token.until!=0),token.direction[0],token.direction[1],token.direction[2],velocity.x,velocity.y,velocity.z,aim.x,aim.y,aim.z,g_throwAimAssist,(void*)token.fire,(void*)token.entity);
}
static bool TryNativeThrowVelocity(void* self,float velocity[4],DWORD cameraVT=0xFFC7C0) {
    if(!g_nativeLeftAim || !g_nativeThrowInstalled) return false;
    NativeThrowToken token;
    if(!TryAcquireSRWLockShared(&g_nativeThrowLock)) return false;
    token=g_nativeThrowToken; ReleaseSRWLockShared(&g_nativeThrowLock);
    if(!token.until || GetTickCount64()>=token.until || token.fire!=(DWORD)(uintptr_t)self) return false;
    NativeGameplaySample sample;
    if(!CopyNativeGameplay(sample) || sample.camera!=token.camera || sample.inventory!=token.inventory) return false;
    __declspec(align(16)) float ray[16];
    float original[4],speed=0;
    __try {
        if(!NativeAimOwnerMatches((void*)token.camera,cameraVT)) return false;
        auto avatar=**(BYTE***)((BYTE*)token.camera+0x30);
        auto inventory=(BYTE*)token.inventory;
        auto weapon=**(BYTE***)(inventory+0x20C+6*4);
        if(!weapon || *(DWORD*)weapon!=0xFD6E00 || **(BYTE***)(weapon+0x980)!=avatar) return false;
        const DWORD mode=*(DWORD*)(weapon+0xA78);
        if(mode>=4) return false;
        auto parts=*(BYTE**)(weapon+0xA64);
        if(!parts || **(void***)(parts+48*mode+0xC)!=self) return false;
        auto fire=(BYTE*)self;
        if(*(DWORD*)fire!=0x1004FE8 || *(DWORD*)(fire+4)!=0x11126F0 ||
           **(BYTE***)(fire+0x4C)!=weapon || **(DWORD**)(fire+0x580)!=token.entity) return false;
        // The alternate held character/weapon branches have different native
        // physics semantics and are deliberately left to the engine.
        if(**(DWORD**)(fire+0x584) || **(DWORD**)(fire+0x590) || *((BYTE*)token.entity+0x24)) return false;
        const float zero[3]{};
        if(!d2vr_aim::BuildRayMatrix((float*)((BYTE*)token.camera+0x80),zero,token.direction,ray)) return false;
        memcpy(original,velocity,sizeof(original));
        for(int c=0;c<4;++c) if(!std::isfinite(original[c])) return false;
        for(int c=0;c<3;++c) speed+=original[c]*original[c];
        if(!std::isfinite(speed) || speed<1e-10f || speed>1e12f) return false;
        speed=std::sqrt(speed);
        AcquireSRWLockExclusive(&g_nativeThrowLock);
        const bool consume=g_nativeThrowToken.sequence==token.sequence && g_nativeThrowToken.until &&
            GetTickCount64()<g_nativeThrowToken.until;
        if(consume) g_nativeThrowToken={}; // one gesture cannot steer a later object
        ReleaseSRWLockExclusive(&g_nativeThrowLock);
        if(!consume) return false;
        for(int c=0;c<3;++c) velocity[c]=ray[8+c]*speed;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    Log("[NativeThrow] APPLIED request=%llu entity=%p stockVelocity=(%.3f,%.3f,%.3f) swingVelocity=(%.3f,%.3f,%.3f) nativeSpeed=%.3f",
        token.sequence,(void*)token.entity,original[0],original[1],original[2],velocity[0],velocity[1],velocity[2],speed);
    return true;
}
static int NativeThrowCompareChecked(void* fire,float* velocity,const float* zero,float tolerance,DWORD cameraVT) {
    TryNativeThrowVelocity(fire,velocity,cameraVT);
    return g_nativeThrowCompare(velocity,zero,tolerance);
}
static int __cdecl NativeThrowCompareBridge(void* fire,float* velocity,const float* zero,float tolerance) {
    return NativeThrowCompareChecked(fire,velocity,zero,tolerance,0xFFC7C0);
}
static __declspec(naked) int __cdecl NativeThrowCompareHook(const float*,const float*,float) {
    __asm {
        push dword ptr [esp+12] // tolerance
        push dword ptr [esp+12] // zero
        push dword ptr [esp+12] // velocity
        push edi               // pinned fire receiver, preserved by cdecl bridge
        call NativeThrowCompareBridge
        add esp,16
        ret                    // original caller cleans its original three args
    }
}
static bool InstallNativeThrowHook() {
    if(!g_nativeLeftAim || !g_nativeAimHookInstalled || g_nativeThrowInstalled) return g_nativeThrowInstalled;
    BYTE* site=(BYTE*)0x879C88;
    __try {
        const BYTE before[]={0x52,0x8D,0x44,0x24,0x68,0x50,0x0F,0x29,0x84,0x24,0x8C,0,0,0};
        const BYTE after[]={0x83,0xC4,0x0C,0x84,0xC0};
        if(memcmp(site,kNativeThrowOriginal,5) || memcmp(site-sizeof(before),before,sizeof(before)) ||
           memcmp(site+5,after,sizeof(after)) || *(DWORD*)(0x1004FE8+0x74)!=0xD23220 ||
           memcmp((void*)0x878955,"\x8B\xF9\x8B\x47\x4C",5) ||
           memcmp((void*)0x6DA050,"\x55\x8B\xEC\x83\xE4\xF0\x8B\x45\x08\x0F\x28\x08",12)) {
            Log("[NativeThrow] unsupported throw velocity site; disabled"); return false;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    DWORD old;
    if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old)) return false;
    g_nativeThrowPatch[0]=0xE8;
    const DWORD displacement=(DWORD)(uintptr_t)&NativeThrowCompareHook-(DWORD)(uintptr_t)(site+5);
    memcpy(g_nativeThrowPatch+1,&displacement,4); memcpy(site,g_nativeThrowPatch,5);
    DWORD ignored; VirtualProtect(site,5,old,&ignored); FlushInstructionCache(GetCurrentProcess(),site,5);
    g_nativeThrowInstalled=true;
    Log("[NativeThrow] ordinary held-object physics direction enabled; left swing, native speed/spin; headset trajectory unverified");
    return true;
}
static void RemoveNativeThrowHook() {
    if(!g_nativeThrowInstalled) return;
    BYTE* site=(BYTE*)0x879C88; DWORD old;
    if(memcmp(site,g_nativeThrowPatch,5)==0 && VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old)) {
        memcpy(site,kNativeThrowOriginal,5); DWORD ignored;
        VirtualProtect(site,5,old,&ignored); FlushInstructionCache(GetCurrentProcess(),site,5);
    }
    g_nativeThrowInstalled=false; ResetNativeThrow();
}

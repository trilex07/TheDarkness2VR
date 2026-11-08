// Included after native_throw.inl, before the protected release installer.
#ifdef D2VR_MATCHED_CAPTURE
#include "throw_target_math.h"
static d2vr_native_clone::Code g_throwTargetCode;
struct ThrowTargetScope {
    DWORD camera=0;
    ULONGLONG sequence=0;
    __declspec(align(16)) float ray[16]{};
};
static thread_local ThrowTargetScope* g_throwTargetScope=nullptr;
using ThrowTargetSort=void(__cdecl*)(BYTE*,BYTE*,int,unsigned);
static ThrowTargetSort g_throwTargetSort=reinterpret_cast<ThrowTargetSort>(0x4E4320);
using ThrowTargetQuery=void*(__thiscall*)(void*,void*,void*,float,float,unsigned,void*,unsigned);
static ThrowTargetQuery g_throwTargetQuery=reinterpret_cast<ThrowTargetQuery>(0xCB0040);
using ThrowTargetTypeCheck=bool(__thiscall*)(void*,const void*);
static ThrowTargetTypeCheck g_throwTargetTypeCheck=reinterpret_cast<ThrowTargetTypeCheck>(0x4BF720);
using ThrowTargetFilter=bool(__thiscall*)(void*,void*,void*,int,int,unsigned,const float*);
static ThrowTargetFilter g_throwTargetFilter=reinterpret_cast<ThrowTargetFilter>(0x7BE900);
static bool ThrowTargetNpc(void* reference) {
    __try {
        auto object=reference ? *static_cast<void**>(reference) : nullptr;
        // EC5AC5 registers D2NpcAvatar at1110508; class/registration bytes
        // and all query call sites are pinned by the offline contract.
        return object && g_throwTargetTypeCheck(object,reinterpret_cast<void*>(0x1110508));
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static bool __fastcall FilterThrowTarget(void* fire,void*,void* source,void* target,
                                        int group,int excluded,unsigned flags,const float* origin) {
    const bool npc=!g_throwTargetScope || ThrowTargetNpc(target);
    // Always call stock: it owns/destructs both by-value smart references.
    const bool admitted=g_throwTargetFilter(fire,source,target,group,excluded,flags,origin);
    return npc && admitted;
}
static bool __fastcall SkipThrowProjectile(void* object,void*,const void* type) {
    // BraggProjectile's special early-return would bypass angular sorting.
    return !g_throwTargetScope && g_throwTargetTypeCheck(object,type);
}
static bool ResolveThrowTargetRay(void* camera,const float*,float* output) {
    const auto scope=g_throwTargetScope;
    if(!scope || scope->camera!=DWORD(reinterpret_cast<uintptr_t>(camera)) ||
       GetCurrentThreadId()!=g_renderThreadId)return false;
    std::memcpy(output,scope->ray,sizeof(scope->ray));return true;
}
// CB0FC5 sorts native 0x30-byte candidate records by ascending float at+0.
// +10 holds the native aim point, +20 a retained reference. Only replace
// the score; stock sorting/collision/visibility and all reference ops remain.
static void __cdecl SortThrowTargets(BYTE* first,BYTE* last,int depth,unsigned predicate) {
    const auto scope=g_throwTargetScope;
    if(scope && last>=first && (last-first)%0x30==0 && (last-first)/0x30<=4096) {
        for(auto p=first;p<last;p+=0x30)
            *reinterpret_cast<float*>(p)=d2vr_throw_target::Score(scope->ray+12,scope->ray+8,
                                                               reinterpret_cast<float*>(p+0x10));
    }
    g_throwTargetSort(first,last,depth,predicate);
}
static bool BuildThrowTargetRay(const NativeThrowToken& token,float (&ray)[16]) {
    if(!std::isfinite(g_gameUnitsPerMeter) || g_gameUnitsPerMeter<.01f || g_gameUnitsPerMeter>5000.f)return false;
    __try {
        float stable[16];
        const auto base=StabilizedCameraWorld(reinterpret_cast<void*>(token.camera),stable);
        if(!d2vr_aim::BuildRayMatrix(base,token.origin,token.direction,ray))return false;
        for(unsigned c=0;c<3;++c) {
            double position=base[12+c];
            for(unsigned r=0;r<3;++r)position+=double(token.origin[r])*g_gameUnitsPerMeter*base[r*4+c];
            ray[12+c]=float(position);if(!std::isfinite(ray[12+c]))return false;
        }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static void* __fastcall QueryThrowTarget(void* fire,void*,void* result,void* avatarReference,
                                       float cone,float range,unsigned flags,void* detail,unsigned mode) {
    NativeThrowToken token;
    AcquireSRWLockShared(&g_nativeThrowLock);token=g_nativeThrowToken;ReleaseSRWLockShared(&g_nativeThrowLock);
    ThrowTargetScope scope;
    const bool scoped=g_throwTargetAssist && !g_throwTargetScope && token.until>GetTickCount64() &&
        token.fire==DWORD(reinterpret_cast<uintptr_t>(fire)) && ProtectedThrowFireOwned(fire) &&
        GetCurrentThreadId()==g_renderThreadId &&
        NativeThrowFireOwnerMatches(reinterpret_cast<void*>(token.camera),token.inventory,fire,0xFFC7C0,true) &&
        BuildThrowTargetRay(token,scope.ray);
    auto query=scoped ? reinterpret_cast<ThrowTargetQuery>(g_throwTargetCode.Entry(0)) : g_throwTargetQuery;
    if(scoped) {scope.camera=token.camera;scope.sequence=token.sequence;}
    auto previous=g_throwTargetScope;
    if(scoped)g_throwTargetScope=&scope;
    void* answer=nullptr;
    __try {answer=query(fire,result,avatarReference,scoped ? 12.f : cone,range,flags,detail,mode);}
    __finally {g_throwTargetScope=previous;}
    if(scoped) {
        bool selected=false;
        selected=ThrowTargetNpc(answer);
        AcquireSRWLockExclusive(&g_nativeThrowLock);
        if(g_nativeThrowToken.sequence==scope.sequence)g_nativeThrowToken.targetSelected=selected;
        ReleaseSRWLockExclusive(&g_nativeThrowLock);
        Log("[ThrowTarget] request=%llu nativeTarget=%d cone=12deg angular priority; native visibility/trajectory retained",scope.sequence,int(selected));
    }
    return answer;
}
static void __cdecl MarkThrowTargetTrajectory(void* fire,unsigned success) {
    const DWORD error=GetLastError();
    if(GetCurrentThreadId()!=g_renderThreadId)return;
    AcquireSRWLockExclusive(&g_nativeThrowLock);
    if(g_nativeThrowToken.fire==DWORD(reinterpret_cast<uintptr_t>(fire)) &&
       g_nativeThrowToken.until>GetTickCount64() && g_nativeThrowToken.targetSelected)
        g_nativeThrowToken.targetApplied=(success&0xff)!=0;
    ReleaseSRWLockExclusive(&g_nativeThrowLock);
    SetLastError(error);
}
static void* g_throwTargetTrajectory=reinterpret_cast<void*>(0xE0BCA0);
// E0BCA0 uses ECX/ESI and three caller-cleaned stack arguments. Replay exactly
// those arguments, then observe AL while preserving the returned CPU state.
static __declspec(naked) void ThrowTargetTrajectory() {
    __asm {
        push dword ptr [esp+12]
        push dword ptr [esp+12]
        push dword ptr [esp+12]
        call dword ptr [g_throwTargetTrajectory]
        lea esp,[esp+12]
        pushfd
        pushad
        mov esi,esp
        sub esp,528
        and esp,-16
        fxsave [esp]
        fninit
        push dword ptr [esi+28]
        push dword ptr [esi]
        call MarkThrowTargetTrajectory
        add esp,8
        fxrstor [esp]
        mov esp,esi
        popad
        popfd
        ret
    }
}
static bool PrepareThrowTarget() {
    const d2vr_native_clone::Override replacements[]={
        {0,0xCB0FC5-0xCB0040,reinterpret_cast<void*>(SortThrowTargets)},
        {0,0xCB0534-0xCB0040,reinterpret_cast<void*>(FilterThrowTarget)},
        {0,0xCB0351-0xCB0040,reinterpret_cast<void*>(SkipThrowProjectile)}};
    if(!g_throwTargetCode.Entry(0) && !g_throwTargetCode.Prepare(d2vr_native_clone::targetBlocks,1,d2vr_native_clone::targetCalls,
        sizeof(d2vr_native_clone::targetCalls)/sizeof(d2vr_native_clone::targetCalls[0]),replacements,
        g_nativeImageGuard,nullptr,nullptr,3))return false;
    return d2vr_aim_getter::Bridge::Scoped(ResolveThrowTargetRay,
        reinterpret_cast<uintptr_t>(g_throwTargetCode.Entry(0))+(0xCB0300-0xCB0040),g_nativeImageGuard);
}
#endif

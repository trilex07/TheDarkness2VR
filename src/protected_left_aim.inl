// Image-preserving opt-in. NativeLeftAim remains OFF: no legacy EXE installer.
#ifdef D2VR_MATCHED_CAPTURE
#include "native_aim_getter.h"
static d2vr_native_view::InstanceTable g_protectedAimTable;
static bool g_protectedAimPrivateReady=false;
struct ProtectedLeftRay {NativeAimSnapshot ray;DWORD camera=0;};
static ProtectedLeftRay g_protectedAimRay;
static SRWLOCK g_protectedAimRayLock=SRWLOCK_INIT;
static void ResetProtectedLeftAim() {
    AcquireSRWLockExclusive(&g_protectedAimRayLock);g_protectedAimRay={};
    ReleaseSRWLockExclusive(&g_protectedAimRayLock);
}
static DWORD ProtectedAimCameraTable() {
    return DWORD(reinterpret_cast<uintptr_t>(g_protectedAimTable.Table()));
}
static bool ProtectedAimCameraOwned(void* camera) {
    return g_protectedAimPrivateReady && g_protectedAimTable.Owns(camera);
}
static bool ResolveProtectedLeftAim(void* self,const float* base,float* output) {
    const XrViewStateFlags tracked=XR_VIEW_STATE_POSITION_TRACKED_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
    if(!g_protectedLeftAim || !g_imagePreservingInput || !g_protectedGestureActions ||
       !g_nativeImageGuard.enabled || GetCurrentThreadId()!=g_renderThreadId ||
       !g_trackedFrame.open || !g_haveViews || !g_in3DGameplayMode || g_protectedRightTentacle.paused ||
       (g_trackedFrame.flags&tracked)!=tracked || !g_hands[0].valid ||
       !g_headGeometry || !g_headGeometryReferenceValid ||
       !std::isfinite(g_gameUnitsPerMeter) || g_gameUnitsPerMeter<.01f || g_gameUnitsPerMeter>5000.f ||
       !ProtectedAimCameraOwned(self) || !NativeAimOwnerMatches(self))return false;
    ProtectedLeftRay sample;
    if(!TryAcquireSRWLockShared(&g_protectedAimRayLock))return false;
    sample=g_protectedAimRay;ReleaseSRWLockShared(&g_protectedAimRayLock);
    if(!sample.ray.valid || sample.camera!=DWORD(reinterpret_cast<uintptr_t>(self)) ||
       GetTickCount64()-sample.ray.time>=100)return false;
    // Refresh the branch state at consumption: a previous frame's ordinary
    // prop cannot authorize a newly acquired character/weapon target.
    if(!CaptureNativeGameplay(self))return false;
    NativeGameplaySample held;
    if(!CopyNativeGameplay(held) || held.camera!=sample.camera || !held.grabFire || held.alternateHeldTarget)return false;
    float matrix[16];
    const bool applied=d2vr_aim::BuildRayMatrix(base,sample.ray.origin,sample.ray.direction,matrix);
    if(applied) {
        // Validate the controller's physical reach in metres first, then use
        // the same metres-to-native scale as head translation and stereo IPD.
        for(int c=0;c<3;++c) {
            double position=base[12+c];
            for(int r=0;r<3;++r)
                position+=double(sample.ray.origin[r])*g_gameUnitsPerMeter*base[r*4+c];
            matrix[12+c]=float(position);
            if(!std::isfinite(matrix[12+c]))return false;
        }
        std::memcpy(output,matrix,sizeof(matrix));
    }
    static unsigned reports=0;
    if(applied && (++reports<=3 || reports%600==0))
        Log("[ProtectedLeftAim] controller ray applied camera=%p; exact native getter consumer, ordinary target branch; release physics handled separately",self);
    return applied;
}
static void PublishProtectedLeftAim(const XINPUT_GAMEPAD& gp) {
    ProtectedLeftRay sample;
    const XrViewStateFlags tracked=XR_VIEW_STATE_POSITION_TRACKED_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
    NativeGameplaySample owner;
    if(g_protectedLeftAim && g_protectedAimPrivateReady && g_imagePreservingInput && g_protectedGestureActions &&
       g_nativeImageGuard.enabled && g_haveViews && g_in3DGameplayMode &&
       (g_trackedFrame.flags&tracked)==tracked && !g_protectedRightTentacle.paused &&
       g_headGeometry && g_headGeometryReferenceValid && g_hands[0].valid &&
       ((gp.wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER) || ProtectedLeftAimHeld()) && gp.bLeftTrigger<30 && gp.bRightTrigger<30 &&
       CopyNativeGameplay(owner) && owner.grabFire && !owner.alternateHeldTarget &&
       ProtectedAimCameraOwned(reinterpret_cast<void*>(owner.camera))) {
        const auto& ref=g_headGeometryReference;
        const auto& aim=g_hands[0].aim;
        const XrQuaternionf inverse={-ref.orientation.x,-ref.orientation.y,-ref.orientation.z,ref.orientation.w};
        const auto origin=RotVec(inverse,aim.position.x-ref.position.x,aim.position.y-ref.position.y,aim.position.z-ref.position.z);
        const auto forward=RotVec(aim.orientation,0,0,-1);
        const auto direction=RotVec(inverse,forward.x,forward.y,forward.z);
        sample.ray.valid=true;sample.ray.time=GetTickCount64();sample.camera=owner.camera;
        sample.ray.origin[0]=origin.x;sample.ray.origin[1]=origin.y;sample.ray.origin[2]=-origin.z;
        sample.ray.direction[0]=direction.x;sample.ray.direction[1]=direction.y;sample.ray.direction[2]=-direction.z;
    }
    AcquireSRWLockExclusive(&g_protectedAimRayLock);g_protectedAimRay=sample;
    ReleaseSRWLockExclusive(&g_protectedAimRayLock);
}
static bool PrepareProtectedAimCamera(void* camera) {
    if(!g_protectedLeftAim || !g_nativePlayerOwnerAdmitted || !g_nativeImageGuard.enabled)return false;
    if(ProtectedAimCameraOwned(camera))return true;
    if(!NativeAimOwnerMatches(camera))return false;
    if(!g_protectedAimPrivateReady) {
        // Every byte of the native consumer and all71 original methods/header
        // are pinned. Only the heap instance's getter slot may be redirected.
        if(!d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0xC62F50),0x2be,
            "f0be0d6ab8a610b998561d68ff4f004efdad6709bc455247f93b9064e17705c8") ||
           !d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0xFFC7BC),72*4,
            "b359543d54ab6c337fadf529c733d42628acffcdf8dd848a9980c4aa926fae0b"))return false;
        HMODULE self=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCSTR>(d2vr_aim_getter::Bridge::Entry()),&self) ||
           !d2vr_aim_getter::Bridge::Prepare(reinterpret_cast<d2vr_aim_getter::Getter>(0x504810),
               0xC63011,ResolveProtectedLeftAim,g_nativeImageGuard) ||
           !g_protectedAimTable.Prepare(reinterpret_cast<void**>(0xFFC7C0),71,0x9c/4,
               d2vr_aim_getter::Bridge::Entry(),g_nativeImageGuard))return false;
        g_protectedAimPrivateReady=true;
    }
    const bool published=g_protectedAimTable.Publish(camera,g_nativeImageGuard);
    if(published)Log("[ProtectedLeftAim] private camera getter published camera=%p exactCaller=00C63011; native target/query lifetime retained, release physics handled separately",camera);
    return published;
}
#else
static bool ProtectedAimCameraOwned(void*) {return false;}
static DWORD ProtectedAimCameraTable() {return 0;}
static bool PrepareProtectedAimCamera(void*) {return false;}
static void ResetProtectedLeftAim() {}
#endif

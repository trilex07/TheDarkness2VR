// Image-preserving opt-in. NativeLeftAim remains OFF: no legacy EXE installer.
#ifdef D2VR_MATCHED_CAPTURE
#include "native_aim_getter.h"
static d2vr_native_view::InstanceTable g_protectedAimTable;
static bool g_protectedAimPrivateReady=false;
#include "dialogue_camera.inl"
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
    if(CinemaSceneThisFrame() || !NativeJackieInputAllowed() || NativeActionMovies() ||
       !g_protectedLeftAim || !g_imagePreservingInput || !g_protectedGestureActions ||
       !g_nativeImageGuard.enabled || GetCurrentThreadId()!=g_renderThreadId ||
       !g_trackedFrame.open || !g_haveViews || !g_in3DGameplayMode || g_protectedRightTentacle.paused ||
       (g_trackedFrame.flags&tracked)!=tracked ||
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
    if(!CinemaSceneThisFrame() && NativeJackieInputAllowed() && !NativeActionMovies() &&
       g_protectedLeftAim && g_protectedAimPrivateReady && g_imagePreservingInput && g_protectedGestureActions &&
       g_nativeImageGuard.enabled && g_haveViews && g_in3DGameplayMode &&
       (g_trackedFrame.flags&tracked)==tracked && !g_protectedRightTentacle.paused &&
       g_headGeometry && g_headGeometryReferenceValid &&
       gp.bLeftTrigger<30 && gp.bRightTrigger<30 &&
       CopyNativeGameplay(owner) && owner.grabFire && !owner.alternateHeldTarget &&
       ProtectedAimCameraOwned(reinterpret_cast<void*>(owner.camera))) {
        const auto& ref=g_headGeometryReference;
        const auto& aim=g_hands[0].aim;
        const XrQuaternionf inverse={-ref.orientation.x,-ref.orientation.y,-ref.orientation.z,ref.orientation.w};
        // [EyeTracking] GazeGrab: the tentacle acquires what you look at (head
        // origin + gaze); controller ray when the gaze is unavailable.
        XrVector3f gazeOrigin{},gazeDir{};
        const bool gaze=g_gazeGrab && TentacleGazeWorldRay(gazeOrigin,gazeDir);
        // Publish the acquisition ray before the grip edge. The native query
        // can run before the input pulse; waiting for LB leaves it head-aimed
        // on that first update. A usable gaze does not require a tracked hand.
        // Without gaze, require the actual aim pose (grip-only is insufficient).
        if(!gaze && !g_hands[0].aimValid) {
            ResetProtectedLeftAim();return;
        }
        const XrVector3f from=gaze ? gazeOrigin : aim.position;
        const auto origin=RotVec(inverse,from.x-ref.position.x,from.y-ref.position.y,from.z-ref.position.z);
        const auto forward=gaze ? gazeDir : RotVec(aim.orientation,0,0,-1);
        static int lastGaze=-1;
        if(int(gaze)!=lastGaze) { lastGaze=int(gaze); Log("[GazeGrab] tentacle grab ray from %s",gaze ? "gaze" : "left controller"); }
        const auto direction=RotVec(inverse,forward.x,forward.y,forward.z);
        sample.ray.valid=true;sample.ray.time=GetTickCount64();sample.camera=owner.camera;
        sample.ray.origin[0]=origin.x;sample.ray.origin[1]=origin.y;sample.ray.origin[2]=-origin.z;
        sample.ray.direction[0]=direction.x;sample.ray.direction[1]=direction.y;sample.ray.direction[2]=-direction.z;
    }
    AcquireSRWLockExclusive(&g_protectedAimRayLock);g_protectedAimRay=sample;
    ReleaseSRWLockExclusive(&g_protectedAimRayLock);
}
static bool ResolveProtectedActiveGrabAim(void* self,const float* base,float* output) {
    if(!ResolveProtectedLeftAim(self,base,output))return false;
    static unsigned reports=0;
    if(++reports<=3 || reports%600==0)
        Log("[ProtectedActiveGrab] selected camera=%p direction=%.5f,%.5f,%.5f; native grab selection",
            self,output[8],output[9],output[10]);
    return true;
}
static bool PrepareProtectedActiveGrabAim() {
    const d2vr_native_clone::Block callers[]={
        {reinterpret_cast<void*>(0xCC78D0),0x27a6,"c707848ff2a34c02c9d8bbecc3fe01361a548ddb3958e0e5b81d9f174aa4330c"},
        {reinterpret_cast<void*>(0xC0BE93),0x53,"4d842f9a320cc446995aa67c22e543fa869d9142fdeeb27777a6c7a37799ae59"},
        {reinterpret_cast<void*>(0xCE9A00),0x398,"f6f9386486e3db688198d0d2d8a5aa00be2944e71a130691236b6797939fe1f7"},
        {reinterpret_cast<void*>(0xCB0040),0x18c1,"31561db14cd1d134a743f1b2777043ba39db984f05281196e7ed2a6e4501eb37"},
        {reinterpret_cast<void*>(0x64FD90),0x6c,"6595addaf9deffa9ab30d8f63f0a5b33ca26b26e42f087032b1e7b4755ce0d58"},
        {reinterpret_cast<void*>(0xBF2750),0x47,"650214ec7b36b1f3d16f5c42e79f6aa79789d8e534992deea3f20dee418e399b"},
        {reinterpret_cast<void*>(0x7F9870),0x16,"8f64cce3250fbfc93882e98b1e1abfda346eab84dda0ca9271e1795aa2b7b55d"},
    };
    for(const auto& block:callers)
        if(!d2vr_native_clone::MatchesDigest(block.source,block.bytes,block.sha256))return false;
    __try {
        if(*reinterpret_cast<const DWORD*>(0x1001630+0x240)!=0x7F9870 ||
           *reinterpret_cast<const DWORD*>(0x1001630+0x2F8)!=0xC7C710 ||
           *reinterpret_cast<const DWORD*>(0x1004FE8+0x68)!=0xCC78D0)return false;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
    return d2vr_aim_getter::Bridge::ActiveGrab(ResolveProtectedActiveGrabAim,g_nativeImageGuard);
}
static bool PrepareProtectedAimCamera(void* camera) {
    if(!g_protectedLeftAim || !g_nativePlayerOwnerAdmitted || !g_nativeImageGuard.enabled)return false;
    if(ProtectedAimCameraOwned(camera))return true;
    if(!NativeAimOwnerMatches(camera))return false;
    if(!g_protectedAimPrivateReady) {
        // Every byte of the native consumer and all71 original methods/header
        // are pinned. Only the heap instance's getter slot may be redirected.
        if(!PrepareDialogueCamera() || !d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0xC62F50),0x2be,
            "f0be0d6ab8a610b998561d68ff4f004efdad6709bc455247f93b9064e17705c8") ||
           !d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0xFFC7BC),72*4,
            "b359543d54ab6c337fadf529c733d42628acffcdf8dd848a9980c4aa926fae0b"))return false;
        HMODULE self=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCSTR>(d2vr_aim_getter::Bridge::Entry()),&self) ||
           !d2vr_aim_getter::Bridge::Prepare(reinterpret_cast<d2vr_aim_getter::Getter>(0x504810),
               0xC63011,ResolveProtectedLeftAim,g_nativeImageGuard) ||
           !g_protectedAimTable.Prepare(reinterpret_cast<void**>(0xFFC7C0),71,0x9c/4,
               d2vr_aim_getter::Bridge::Entry(),g_nativeImageGuard,0x8c/4,
               g_dialogueCameraComfort?reinterpret_cast<void*>(DialogueCameraAngles):nullptr))return false;
        const bool activeGrab=PrepareProtectedActiveGrabAim();
        Log("[ProtectedActiveGrab] ready=%d; exact native input acquisition path",int(activeGrab));
        g_protectedAimPrivateReady=true;
    }
    const bool published=g_protectedAimTable.Publish(camera,g_nativeImageGuard);
    if(published)g_dialogueCamera.store(DWORD(reinterpret_cast<uintptr_t>(camera)),std::memory_order_release);
    if(published)Log("[ProtectedLeftAim] private camera getter published camera=%p exactCaller=00C63011; native target/query lifetime retained, release physics handled separately",camera);
    return published;
}
#else
static bool ProtectedAimCameraOwned(void*) {return false;}
static DWORD ProtectedAimCameraTable() {return 0;}
static bool PrepareProtectedAimCamera(void*) {return false;}
static void ResetProtectedLeftAim() {}
#endif

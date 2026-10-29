#ifdef D2VR_MATCHED_CAPTURE
static d2vr_native_view::InstanceTable g_protectedHandsTable;
static bool g_protectedHandsPrivateReady=false;
static SRWLOCK g_protectedHandsPoseLock=SRWLOCK_INIT;
static NativeWeaponPose g_protectedHandPoses[2];
static DWORD g_protectedHandsPoseCamera=0;
static bool ProtectedHandsResourceOwned(void* resource) {
    return g_protectedHandsPrivateReady && g_protectedHandsTable.Owns(resource);
}
static bool ProtectedHandsAvailable() {
    const XrViewStateFlags tracked=XR_VIEW_STATE_POSITION_TRACKED_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
    return g_protectedTrackedHands && g_protectedHandsPrivateReady && g_nativeImageGuard.enabled &&
        g_imagePreservingInput && g_protectedGestureActions && g_haveViews && g_in3DGameplayMode &&
        g_trackedFrame.open && g_trackedFrame.id && GetCurrentThreadId()==g_renderThreadId &&
        (g_trackedFrame.flags&tracked)==tracked && !g_protectedRightTentacle.paused &&
        !g_protectedLeftTentacle.paused;
}
static void ResetProtectedHandPoses() {
    AcquireSRWLockExclusive(&g_protectedHandsPoseLock);
    g_protectedHandsPoseCamera=0;g_protectedHandPoses[0]={};g_protectedHandPoses[1]={};
    ReleaseSRWLockExclusive(&g_protectedHandsPoseLock);
}
static bool CopyProtectedHandPoses(DWORD camera,NativeWeaponPose (&poses)[2]) {
    if(!TryAcquireSRWLockShared(&g_protectedHandsPoseLock))return false;
    const bool same=camera && camera==g_protectedHandsPoseCamera;
    if(same)std::memcpy(poses,g_protectedHandPoses,sizeof(poses));
    ReleaseSRWLockShared(&g_protectedHandsPoseLock);
    return same;
}
static void PublishProtectedHandPoses() {
    NativeWeaponPose poses[2]{};DWORD camera=0;
    NativeGameplaySample owner;
    if(ProtectedHandsAvailable() && g_headGeometry && g_headGeometryReferenceValid &&
       CopyNativeGameplay(owner) && ProtectedAimCameraOwned(reinterpret_cast<void*>(owner.camera))) {
        const auto& ref=g_headGeometryReference;
        const float q[4]={ref.orientation.x,ref.orientation.y,ref.orientation.z,ref.orientation.w};
        const float p[3]={ref.position.x,ref.position.y,ref.position.z};
        float reference[16],inverse[16];
        if(d2vr_math::TrackingPose(q,p,reference) && d2vr_math::Invert(reference,inverse)) {
            camera=owner.camera;
            // Free hands follow grip. An equipped, independently owned pistol
            // uses the SAME aim-space transform as its model/fire resolver.
            // Mixing grip-space human sockets with aim-space guns separates
            // the visible grip even when both controller poses are valid.
            for(unsigned hand=0;hand<2;++hand)if(g_hands[hand].gripValid) {
                XrPosef grip=g_hands[hand].grip;
                CopyProtectedWeaponHandPose(owner.camera,hand,grip);
                const float hq[4]={grip.orientation.x,grip.orientation.y,grip.orientation.z,grip.orientation.w};
                const float hp[3]={grip.position.x,grip.position.y,grip.position.z};
                float tracking[16];
                if(!d2vr_math::TrackingPose(hq,hp,tracking))continue;
                d2vr_math::Multiply(tracking,inverse,poses[hand].local);
                poses[hand].time=GetTickCount64();poses[hand].valid=true;
            }
        }
    }
    AcquireSRWLockExclusive(&g_protectedHandsPoseLock);
    g_protectedHandsPoseCamera=camera;std::memcpy(g_protectedHandPoses,poses,sizeof(poses));
    ReleaseSRWLockExclusive(&g_protectedHandsPoseLock);
}
using ProtectedHandsPackMethod=void(__thiscall*)(void*,void*);
static ProtectedHandsPackMethod g_protectedHandsPackOriginal=reinterpret_cast<ProtectedHandsPackMethod>(0x6C3890);
static thread_local bool g_insideProtectedHandsPack=false;
static void __fastcall ProtectedHandsPackHook(void* resource,void*,void* rendererRef) {
    const bool outermost=!g_insideProtectedHandsPack;
    if(outermost)g_insideProtectedHandsPack=true;
    __try {
        if(outermost && ProtectedHandsAvailable() && ProtectedHandsResourceOwned(resource) &&
           resource==g_nativeHandsSelectedResource) {
            // Preserve the native packer's incoming floating environment and
            // LastError around our IK. The native reference is consumed once,
            // exclusively by the original thiscall(rendererRef), never by us.
            const DWORD error=GetLastError();
            __declspec(align(16)) unsigned char floating[512];_fxsave(floating);
            __asm fninit
            void* renderer=nullptr;
            __try {if(rendererRef)renderer=*static_cast<void**>(rendererRef);}
            __except(EXCEPTION_EXECUTE_HANDLER){renderer=nullptr;}
            if(renderer) {
                if(NativeCinematicPlaying())RestoreNativeHandsPalette(renderer,resource);
                else ApplyNativeHandsChecked(renderer,0xFFC7C0,resource,true);
            }
            _fxrstor(floating);SetLastError(error);
        }
        g_protectedHandsPackOriginal(resource,rendererRef);
    } __finally {if(outermost)g_insideProtectedHandsPack=false;}
}
static void PrepareProtectedHandsForCamera(void* camera) {
    if(!g_protectedTrackedHands || !g_nativeImageGuard.enabled || !g_nativePlayerOwnerAdmitted ||
       GetCurrentThreadId()!=g_renderThreadId || !ProtectedAimCameraOwned(camera) || !NativeAimOwnerMatches(camera))return;
    NativeGameplaySample sample;
    if(!CopyNativeGameplay(sample) || sample.camera!=DWORD(reinterpret_cast<uintptr_t>(camera)) ||
       !sample.avatarSkinResource)return;
    void* resource=reinterpret_cast<void*>(sample.avatarSkinResource);
    if(ProtectedHandsResourceOwned(resource))return;
    // Repeat the complete avatar -> model -> actor -> renderer -> resource
    // ownership traversal while the native selected-view reference is live.
    __try {
        auto avatar=**reinterpret_cast<unsigned char***>(static_cast<unsigned char*>(camera)+0x30);
        auto parent=**reinterpret_cast<unsigned char***>(avatar+0x1F8C);
        if(!parent || *reinterpret_cast<DWORD*>(parent)!=0xFFC300)return;
        auto actor=**reinterpret_cast<unsigned char***>(parent+0xF8);
        if(!actor || *reinterpret_cast<DWORD*>(actor)!=0xFC5F30)return;
        auto renderer=**reinterpret_cast<unsigned char***>(actor+8);
        if(!renderer || **reinterpret_cast<void***>(renderer+0x5BC)!=resource ||
           *static_cast<DWORD*>(resource)!=0xFC2578)return;
    } __except(EXCEPTION_EXECUTE_HANDLER){return;}
    if(!g_protectedHandsPrivateReady) {
        // All96 methods and the preceding word are pinned. The next dword
        // (FC26F8) is scalar data, outside this concrete resource's table.
        if(!d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0xFC2574),97*4,
            "dd72119cdb1e161f1c90cd8d7a2163a1d88113f1e16f2a5ca926fdbc153be85d") ||
           !d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0x6C3890),0x64c,
            "3fa6d60482c9fd3b8c7ef73d161e74561cebb6dd7c0677e9803fdbee6e2f4223"))return;
        HMODULE self=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCSTR>(&ProtectedHandsPackHook),&self) ||
           !g_protectedHandsTable.Prepare(reinterpret_cast<void**>(0xFC2578),96,0xEC/4,
               reinterpret_cast<void*>(&ProtectedHandsPackHook),g_nativeImageGuard))return;
        g_protectedHandsPrivateReady=true;
    }
    if(g_protectedHandsTable.Publish(resource,g_nativeImageGuard))
        Log("[ProtectedHands] private resource packer published resource=%p; independent grip poses, no pistol gate, original thiscall/ref release once; visible GPU consumption unverified",resource);
}
#else
static bool ProtectedHandsResourceOwned(void*) {return false;}
static bool ProtectedHandsAvailable() {return false;}
static bool CopyProtectedHandPoses(DWORD,NativeWeaponPose (&)[2]) {return false;}
static void ResetProtectedHandPoses() {}
static void PrepareProtectedHandsForCamera(void*) {}
#endif

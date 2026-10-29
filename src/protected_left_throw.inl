#ifdef D2VR_MATCHED_CAPTURE
static d2vr_native_clone::Code g_protectedThrowCode;
static d2vr_native_view::InstanceTable g_protectedThrowTable;
static bool g_protectedThrowPrivateReady=false;
static d2vr_input::Tentacles g_protectedLeftTentacle;
static DWORD g_protectedThrowInputOwner[3]{};
static bool ProtectedThrowFireOwned(void* fire) {
    return g_protectedThrowPrivateReady && g_protectedThrowTable.Owns(fire);
}
static bool ProtectedThrowAvailable() {
    return g_protectedLeftThrow && g_protectedLeftAim && g_protectedGestureActions &&
        g_imagePreservingInput && g_nativeImageGuard.enabled && g_protectedThrowPrivateReady;
}
static bool ProtectedLeftAimHeld() {
    return ProtectedThrowAvailable() && g_protectedLeftTentacle.leftDown && !g_protectedLeftTentacle.paused;
}
static void ResetProtectedLeftThrow() {
    g_protectedLeftTentacle={};std::memset(g_protectedThrowInputOwner,0,sizeof(g_protectedThrowInputOwner));
}
static bool ResolveProtectedThrowVelocity(void* fire,float* velocity,const void* nativeCharacterReference=nullptr) {
    const XrViewStateFlags tracked=XR_VIEW_STATE_POSITION_TRACKED_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
    if(!ProtectedThrowAvailable() || !ProtectedThrowFireOwned(fire) ||
       GetCurrentThreadId()!=g_renderThreadId || !g_trackedFrame.open || !g_haveViews ||
       !g_in3DGameplayMode || (g_trackedFrame.flags&tracked)!=tracked ||
       g_protectedRightTentacle.paused || g_protectedLeftTentacle.paused ||
       !g_hands[0].valid || !g_headGeometry || !g_headGeometryReferenceValid)return false;
    return TryNativeThrowVelocity(fire,velocity,0xFFC7C0,true,nativeCharacterReference);
}
static void PrepareProtectedThrowForCamera(void* camera) {
    if(!g_protectedLeftThrow || !g_protectedLeftAim || !g_nativePlayerOwnerAdmitted ||
       !g_nativeImageGuard.enabled || !ProtectedAimCameraOwned(camera) || !NativeAimOwnerMatches(camera))return;
    NativeGameplaySample sample;
    if(!CopyNativeGameplay(sample) || sample.camera!=DWORD(reinterpret_cast<uintptr_t>(camera)) || !sample.grabFire)return;
    void* fire=reinterpret_cast<void*>(sample.grabFire);
    if(ProtectedThrowFireOwned(fire))return;
    if(!NativeThrowFireOwnerMatches(camera,sample.inventory,fire,0xFFC7C0,true))return;
    __try {if(*static_cast<DWORD*>(fire)!=0x1004FE8)return;}
    __except(EXCEPTION_EXECUTE_HANDLER){return;}
    if(!g_protectedThrowPrivateReady) {
        if(!d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0x1004FE4),
            (d2vr_native_clone::throwTableCount+1)*4,d2vr_native_clone::throwTableDigest))return;
        HMODULE self=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCSTR>(d2vr_throw_compare::Bridge::Entry()),&self) ||
           !d2vr_throw_compare::Bridge::Prepare(reinterpret_cast<d2vr_throw_compare::Compare>(0x6DA050),
               ResolveProtectedThrowVelocity,g_nativeImageGuard))return;
        const d2vr_native_clone::Override replacement={1,0x879C88-0x878930,d2vr_throw_compare::Bridge::Entry()};
        if(!g_protectedThrowCode.Entry(0) && !g_protectedThrowCode.Prepare(
            d2vr_native_clone::throwBlocks,2,d2vr_native_clone::throwCalls,
            sizeof(d2vr_native_clone::throwCalls)/sizeof(d2vr_native_clone::throwCalls[0]),&replacement,g_nativeImageGuard)) {
            Log("[ProtectedLeftThrow] private release preparation refused: %s",g_protectedThrowCode.Status());return;
        }
        if(!g_protectedThrowTable.Prepare(reinterpret_cast<void**>(0x1004FE8),d2vr_native_clone::throwTableCount,
            0x74/4,g_protectedThrowCode.Entry(0),g_nativeImageGuard))return;
        g_protectedThrowPrivateReady=true;
    }
    if(g_protectedThrowTable.Publish(fire,g_nativeImageGuard))
        Log("[ProtectedLeftThrow] owned fire event published fire=%p; private D23220->878930, native magnitude/spin/physics retained; EXE/static tables unchanged",fire);
}
static void ApplyProtectedLeftThrow(XINPUT_GAMEPAD& gp) {
    if(!g_protectedLeftThrow)return;
    if(!ProtectedThrowAvailable()) {ResetProtectedLeftThrow();ResetNativeThrow();return;}
    NativeGameplaySample owner;
    const XrViewStateFlags tracked=XR_VIEW_STATE_POSITION_TRACKED_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
    const bool context=g_haveViews && g_in3DGameplayMode && !g_protectedRightTentacle.paused && (g_trackedFrame.flags&tracked)==tracked &&
        g_headGeometry && g_headGeometryReferenceValid && gp.bLeftTrigger<30 && gp.bRightTrigger<30 &&
        CopyNativeGameplay(owner) &&
        ProtectedAimCameraOwned(reinterpret_cast<void*>(owner.camera)) &&
        ProtectedThrowFireOwned(reinterpret_cast<void*>(owner.grabFire));
    // The stock LB action handles both props and the native enemy/weapon
    // branches. Entering one must not erase the held grip/stroke or suppress
    // its release pulse. Aim and physics steering have separate branch gates
    // in PublishProtectedLeftAim/PublishNativeThrow/TryNativeThrowVelocity.
    if(context && (g_protectedThrowInputOwner[0]!=owner.camera ||
       g_protectedThrowInputOwner[1]!=owner.inventory || g_protectedThrowInputOwner[2]!=owner.grabFire)) {
        // A different native owner cannot inherit a held grip or powered stroke.
        const bool paused=g_protectedLeftTentacle.paused,pauseDown=g_protectedLeftTentacle.pauseDown;
        ResetProtectedLeftThrow();ResetNativeThrow();
        g_protectedLeftTentacle.paused=paused;g_protectedLeftTentacle.pauseDown=pauseDown;
        g_protectedThrowInputOwner[0]=owner.camera;g_protectedThrowInputOwner[1]=owner.inventory;
        g_protectedThrowInputOwner[2]=owner.grabFire;
    }
    const auto& p=g_hands[0].grip.position;
    const auto forward=RotVec(g_hands[0].aim.orientation,0,0,-1);
    const auto out=g_protectedLeftTentacle.Update(context,(gp.wButtons&XINPUT_GAMEPAD_START)!=0,
        g_hands[0].valid,false,g_hands[0].squeeze,0,{p.x,p.y,p.z},g_hands[0].motion.velocity,{},
        g_trackedFrame.state.predictedDisplayTime*1e-9,context?owner.grab:d2vr_input::GrabFeedback::Unknown,
        {forward.x,forward.y,forward.z});
    // Only LB belongs to this recognizer. Right slash and stick stay untouched.
    gp.wButtons&=~XINPUT_GAMEPAD_LEFT_SHOULDER;
    if(out.leftButton)gp.wButtons|=XINPUT_GAMEPAD_LEFT_SHOULDER;
    PublishNativeThrow(context && !g_protectedLeftTentacle.paused,out.grab,out.throwObject,
        out.throwVelocity,out.throwAim,true);
    if(out.grab || out.throwObject)
        Log("[ProtectedLeftThrow] stock LB pulse grab=%d throw=%d; left aim/swing only, right controller independent",int(out.grab),int(out.throwObject));
}
#else
static bool ProtectedThrowFireOwned(void*) {return false;}
static bool ProtectedThrowAvailable() {return false;}
static bool ProtectedLeftAimHeld() {return false;}
static void PrepareProtectedThrowForCamera(void*) {}
static void ResetProtectedLeftThrow() {}
#endif

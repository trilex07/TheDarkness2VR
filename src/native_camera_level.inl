// One-shot native input-angle correction, consumed BEFORE selected FP composition.
#ifdef D2VR_MATCHED_CAPTURE
static std::atomic<uint64_t> g_nativeLevelRequest{0};
static void RequestNativeCameraLevel() {
    NativeGameplaySample owner;
    if(!g_dialogueCameraComfort || !g_cameraNativeNoShake || !g_headGeometry ||
       !g_in3DGameplayMode || !CopyNativeGameplay(owner))return;
    const uint64_t request=(uint64_t(DWORD(GetTickCount()+500))<<32)|owner.camera;
    g_nativeLevelRequest.store(request,std::memory_order_release);
}
static void ApplyNativeCameraLevel(DWORD avatar) {
    const auto request=g_nativeLevelRequest.load(std::memory_order_acquire);
    if(!request)return;
    if(LONG(DWORD(request>>32)-GetTickCount())<0) {
        auto expected=request;g_nativeLevelRequest.compare_exchange_strong(expected,0);return;
    }
    void* camera=nullptr;
    __try {camera=**reinterpret_cast<void***>(avatar+0x62c);}
    __except(EXCEPTION_EXECUTE_HANDLER){return;}
    if(DWORD(request)!=DWORD(reinterpret_cast<uintptr_t>(camera)) ||
       !g_nativeImageGuard.enabled || !NativeAimOwnerMatches(camera))return;
    // Pinned native camera getter +84 returns +110: yaw, pitch, roll in degrees.
    // +8C setter updates both input and interpolation angles (+120).
    if(!d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0x9D9120),0x9a,
        "9fc2b97094c608861c2672f4c73aa517fa70438f4e353b8d73ff98f8d3945a58"))return;
    float angles[3];
    __try {std::memcpy(angles,static_cast<BYTE*>(camera)+0x110,sizeof(angles));}
    __except(EXCEPTION_EXECUTE_HANDLER){return;}
    for(float a:angles)if(!std::isfinite(a))return;
    const float pitch=angles[1],roll=angles[2];angles[1]=angles[2]=0;
    auto expected=request;
    if(!g_nativeLevelRequest.compare_exchange_strong(expected,0))return;
    reinterpret_cast<void(__thiscall*)(void*,const float*)>(0x9D9120)(camera,angles);
    Log("[NativeCameraLevel] camera=%p yaw=%.4f pitch=%.4f->0 roll=%.4f->0; before FP composition",camera,angles[0],pitch,roll);
}
#else
static void RequestNativeCameraLevel() {}
#endif

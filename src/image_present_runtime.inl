// Included after the existing frame/capture helpers. Only the diagnostic build
// opts in; the protected profile can NEVER open an EXE-write gate.
static bool ImagePresentCaller(d2vr_image_present::Method method,uintptr_t caller) {
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if(method==d2vr_image_present::Method::Create) return caller==image+0x564cc1;
    return method==d2vr_image_present::Method::Present &&
        (caller==image+0x694057 || caller==image+0x694074);
}
struct ImagePresentScope {
    bool previousInside=false,opened=false;
    IDirect3DDevice9* owner=nullptr;
    LARGE_INTEGER forwardStart{};
    bool forwardActive=false;
    uint64_t adaptiveSerial=0;
};
static thread_local ImagePresentScope g_imagePresentScope;
static void ImagePresentBefore(IDirect3DSwapChain9* sc,uintptr_t caller,bool qualified,bool ready) {
    auto& state=g_imagePresentScope;
    state={g_insidePresentation,false,nullptr};g_insidePresentation=true;
    if(!qualified || g_captureResetting) return;
    if(!ready) {
        // An occluded desktop Present closes frame admission. Keep draining
        // lifecycle/focus events so wearing the HMD can still wake the session.
        // No device calls, replay, or XR frame APIs belong on this path. The
        // next D3D_OK only warms the observer; its following frame uses the
        // ordinary BeginTrackedFrame pump, with no duplicate polling here.
        if(g_imagePreservingPresent && g_nativeImageGuard.enabled &&
            g_imagePresent.OwnsCurrentNativePresent() && !g_disableOpenXR &&
            g_xrReady && g_xrInstance!=XR_NULL_HANDLE && g_xrSession!=XR_NULL_HANDLE)
            PumpXR();
        return;
    }
    const HRESULT hr=sc->GetDevice(&state.owner);
    if(FAILED(hr) || !state.owner) {g_imagePresent.Fail("GetDevice failed at native Present");return;}
    state.adaptiveSerial=BeginAdaptivePresentation();
    g_presentCaller=caller;
    const auto decision=g_frameBoundary.OnEvent(MakePipelineEvent(d2vr_frame::EventType::SwapChainPresent));
    {
        d2vr_cpu_sync::Scope cpuBoundary(d2vr_cpu_sync::BoundaryWork);
        ApplyBoundaryDecision(state.owner,decision);
    }
    state.opened=true;
}
static void ImagePresentForwardBegin() {
    auto& state=g_imagePresentScope;
    state.forwardActive=QueryPerformanceCounter(&state.forwardStart)!=FALSE;
}
static void ImagePresentForwardEnd() {
    auto& state=g_imagePresentScope;
    if(state.forwardActive) d2vr_cpu_sync::timing.Add(d2vr_cpu_sync::PresentForward,state.forwardStart);
    state.forwardActive=false;
}
static void ImagePresentAfter(IDirect3DSwapChain9* sc,uintptr_t caller,bool qualified,bool ready,HRESULT hr) {
    auto& state=g_imagePresentScope;
    if(state.adaptiveSerial)
        EndAdaptivePresentation(state.adaptiveSerial,hr,state.opened && qualified && ready && !g_imagePresent.Failed());
    if(state.opened) {
        if(g_imagePresent.Failed()) matched_capture::Reject("external Present observer fault");
        matched_capture::PresentReturned(hr,true);
        g_capturePresent.Leave();
    }
    if(state.owner) state.owner->Release();
    if(qualified) {
        // The protected observer bypasses EngineSwapChainPresent, so it must
        // publish the existing stage accumulator itself. Scopes overlap: these
        // are wall-time API spans, never CPU busy time or additive frame cost.
        d2vr_cpu_sync::timing.Report(g_srcW,g_srcH,g_trackedFrame.id);
        InterlockedIncrement(&g_swapChainPresentCount);InterlockedExchange(&g_presentSeen,1);
        // First real successful forwarding opens admission for the NEXT game
        // frame. Never replay commands accumulated across that warm-up frame.
        if(!ready || hr!=D3D_OK || g_imagePresent.Failed()) {
            d2vr_replay::ReleaseRecording();g_gpuTiming.Release();
            InterlockedExchange(&g_needRecBegin,1);
        }
        const auto n=g_imagePresent.nativePresents.load();
        if(n<=3 || n%1000==0 || FAILED(hr))
            Log("[ImagePresent] native=%llu entries=%llu forwarded=%llu returned=%llu recursive=%llu hr=%08lX ready=%d ok=%llu errors=%llu statuses=%llu sc=%p caller=%p tid=%lu fault=%d EXE_WRITES=FORBIDDEN",
                n,g_imagePresent.entries.load(),g_imagePresent.forwarded.load(),g_imagePresent.returned.load(),
                g_imagePresent.recursive.load(),hr,g_imagePresent.Ready(),g_imagePresent.ok.load(),g_imagePresent.errors.load(),
                g_imagePresent.statuses.load(),sc,reinterpret_cast<void*>(caller),GetCurrentThreadId(),g_imagePresent.Failed());
    }
    g_insidePresentation=state.previousInside;
}
static void InstallImagePresent(IDirect3DDevice9* device) {
    if(!g_imagePreservingPresent) return;
    char path[MAX_PATH]{};
    if(!g_nativeImageGuard.enabled || !GetModuleFileNameA(nullptr,path,MAX_PATH)) {
        g_imagePresent.Fail("image-write guard required");return;
    }
    const auto sha=matched_capture::HashFile(path);
    const auto image=reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS32*>(image+dos->e_lfanew);
    const unsigned char original[]={0x8b,0x41,0x0c,0xff,0xd0};
    const unsigned char alternate[]={0x6a,0,0x50,0xff,0xd2};
    const unsigned char acquisition[]={0xff,0xd0};
    if(!d2vr_present_patch::Identity(sha.c_str(),pe->FileHeader.Machine,pe->FileHeader.TimeDateStamp,
        pe->OptionalHeader.SizeOfImage,pe->OptionalHeader.ImageBase,pe->OptionalHeader.Magic) ||
        reinterpret_cast<uintptr_t>(image)!=0x400000 ||
        memcmp(image+0x694052,original,sizeof(original))!=0 ||
        memcmp(image+0x69406f,alternate,sizeof(alternate))!=0 ||
        memcmp(image+0x564cbf,acquisition,sizeof(acquisition))!=0) {
        g_imagePresent.Fail("SHA/PE/original presentation dispatch mismatch");return;
    }
    const bool nativeCopy=d2vr_native_constants::AdmitImage(sha.c_str(),image);
    Log("[NativeOwnedConstantCopy] requested=%d admitted=%d sha=%s; exact native local frames only; no EXE writes",
        int(d2vr_native_constants::enabled.load()),nativeCopy,sha.c_str());
    Log("[NativeOwnedMaterialCopy] requested=%d admitted=%d; exact EBX owner, fixed field, call site and extent required",
        int(d2vr_native_constants::materialEnabled.load()),
        int(nativeCopy && d2vr_native_constants::materialEnabled.load()));
    d2vr_image_present::Callbacks callbacks{
        ImagePresentCaller,ImagePresentBefore,ImagePresentAfter,Log};
    if(d2vr_cpu_sync::timing.enabled) {
        callbacks.forwardBegin=ImagePresentForwardBegin;
        callbacks.forwardEnd=ImagePresentForwardEnd;
    }
    const bool started=g_imagePresent.Start(device,g_nativeImageGuard,callbacks);
    Log("[ImagePresent] acquisitionObserver=%d sha=%s; XR pending typed swapchain/QI and successful native Present; no table clone or EXE write",started,sha.c_str());
}

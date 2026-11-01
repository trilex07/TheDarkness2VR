// Opt-in read-only observation of the actual native LOD dispatcher. The
// protected image profile forbids EXE code/IAT/vtable writes, so this uses one
// hardware execution breakpoint on the render thread and never changes LOD.
#ifdef D2VR_MATCHED_CAPTURE
static d2vr_native_execution::Boundary g_gazeLodBoundary;
static volatile LONG g_gazeLodCalls=0;
static volatile LONG g_gazeLodSampleState=0; // 0 empty, 1 being copied, 2 ready
static volatile LONG g_gazeLodStopped=0;
struct GazeLodEntrySample {
    DWORD thread=0,renderer=0,list=0,view=0,node=0,object=0;
    DWORD objectTable=0,resource=0,selected=0;
    bool readable=false;
};
static GazeLodEntrySample g_gazeLodEntrySample;

static bool ReadNativeGazeLodEntry(DWORD esp,GazeLodEntrySample* sample) {
    __try {
        // 8513A0: thiscall ECX=renderer, [ESP+4]=view-list. 8513BA reads
        // [view-list] and 8513D0/851443 pass view-list+8 to selectors.
        sample->list=*reinterpret_cast<const DWORD*>(esp+4);
        if(sample->list) {
            sample->view=*reinterpret_cast<const DWORD*>(sample->list);
            sample->node=*reinterpret_cast<const DWORD*>(sample->list+8);
            if(sample->node && sample->node!=*reinterpret_cast<const DWORD*>(sample->node+0x200)) {
                // AE30F0/AE312E read the first node and its object; +1C4 and
                // +2B0 are the resource pointer and selected LOD index.
                sample->object=*reinterpret_cast<const DWORD*>(sample->node);
                if(sample->object) {
                    sample->objectTable=*reinterpret_cast<const DWORD*>(sample->object);
                    sample->resource=*reinterpret_cast<const DWORD*>(sample->object+0x1c4);
                    sample->selected=*reinterpret_cast<const DWORD*>(sample->object+0x2b0);
                }
            }
        }
        return sample->list!=0;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}

static void ObserveNativeGazeLodEntry(const CONTEXT& context) {
    InterlockedIncrement(&g_gazeLodCalls);
    if(!g_in3DGameplayMode)return;
    if(InterlockedCompareExchange(&g_gazeLodSampleState,1,0)!=0)return;
    GazeLodEntrySample sample{};
    sample.thread=GetCurrentThreadId();sample.renderer=context.Ecx;
    sample.readable=ReadNativeGazeLodEntry(context.Esp,&sample);
    g_gazeLodEntrySample=sample;
    InterlockedExchange(&g_gazeLodSampleState,2);
}

static bool GazeLodEntryBytesMatch() {
    static const unsigned char entry[]={0x83,0xec,0x08,0x80,0x3d,0x54,0xde,0x10,0x01,0x00};
    __try {return memcmp(reinterpret_cast<const void*>(0x8513a0),entry,sizeof(entry))==0;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}

static void InstallGazeLodRuntimeObserver() {
    if(!GetPrivateProfileIntA("Render","GazeLodRuntimeObserver",0,
        g_activeIni[0]?g_activeIni:".\\Darkness2VR.ini"))return;
    if(!g_nativeImageGuard.enabled || !g_imagePreservingPresent ||
       g_nativeViewBoundaryMode==2 || g_gazeLodBoundary.Installed()) {
        Log("[GazeLOD] runtime observer refused: protected profile or debug slot unavailable");return;
    }
    char path[MAX_PATH]{};
    if(!GetModuleFileNameA(nullptr,path,MAX_PATH) ||
       matched_capture::HashFile(path)!="d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1" ||
       reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))!=0x400000) {
        Log("[GazeLOD] runtime observer refused: EXE identity");return;
    }
    if(!GazeLodEntryBytesMatch()){Log("[GazeLOD] runtime observer refused: selector bytes");return;}
    const bool installed=g_gazeLodBoundary.Start(reinterpret_cast<void*>(0x8513a0),
        ObserveNativeGazeLodEntry,g_nativeImageGuard);
    Log("[GazeLOD] runtime observer installed=%d status=%s thread=%lu DR0=%08lX DR1=%08lX DR2=%08lX DR3=%08lX DR7=%08lX; selector=008513A0; read-only, native quality unchanged",
        int(installed),g_gazeLodBoundary.Status(),GetCurrentThreadId(),
        g_gazeLodBoundary.Debug(0),g_gazeLodBoundary.Debug(1),g_gazeLodBoundary.Debug(2),
        g_gazeLodBoundary.Debug(3),g_gazeLodBoundary.Debug(4));
}

static void StopGazeLodRuntimeObserverAfterSample() {
    if(InterlockedCompareExchange(&g_gazeLodSampleState,0,0)!=2 ||
       GetCurrentThreadId()!=g_gazeLodEntrySample.thread ||
       !g_gazeLodBoundary.Installed())return;
    if(g_gazeLodBoundary.Stop())InterlockedExchange(&g_gazeLodStopped,1);
}

static void ReportGazeLodRuntimeObserver() {
    if(!g_gazeLodBoundary.Installed() && InterlockedCompareExchange(&g_gazeLodSampleState,0,0)!=2)return;
    static ULONGLONG next=0;
    static bool reported=false;
    const auto now=GetTickCount64();
    if(now<next || reported)return;
    next=now+1000;
    const LONG calls=InterlockedCompareExchange(&g_gazeLodCalls,0,0);
    if(InterlockedCompareExchange(&g_gazeLodSampleState,0,0)!=2) {
        Log("[GazeLOD] selector calls=%ld sample=none; execution may use another thread; quality unchanged",calls);
        return;
    }
    const auto sample=g_gazeLodEntrySample;
    const bool stopped=InterlockedCompareExchange(&g_gazeLodStopped,0,0)!=0;
    Log("[GazeLOD] selector calls=%ld thread=%lu renderer=%08lX list=%08lX view=%08lX node=%08lX object=%08lX table=%08lX resource=%08lX priorIndex=%lu readable=%d stopped=%d; entry sample only, quality unchanged",
        calls,sample.thread,sample.renderer,sample.list,sample.view,sample.node,sample.object,
        sample.objectTable,sample.resource,sample.selected,int(sample.readable),int(stopped));
    reported=true;
}
#else
static void ReportGazeLodRuntimeObserver() {}
static void StopGazeLodRuntimeObserverAfterSample() {}
#endif

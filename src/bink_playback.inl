// Actual installed x86 delay imports: _BinkWait@4 and _BinkClose@4.
// Observe playback polling, not Open (which may be preload). No EXE/IAT writes.
static SRWLOCK g_binkPlaybackLock=SRWLOCK_INIT;
static d2vr_cinema::Playback g_binkPlaying;
static std::atomic<bool> g_binkVideo{false};
static void ObserveBinkHandle(void* handle,bool playing) {
    if(!handle)return;
    AcquireSRWLockExclusive(&g_binkPlaybackLock);
    g_binkPlaying.Observe(reinterpret_cast<uintptr_t>(handle),playing);
    g_binkVideo.store(g_binkPlaying.Active(),std::memory_order_release);
    ReleaseSRWLockExclusive(&g_binkPlaybackLock);
}
#ifdef D2VR_MATCHED_CAPTURE
using BinkWaitFn=int(WINAPI*)(void*);
using BinkCloseFn=void(WINAPI*)(void*);
static BinkWaitFn g_binkWaitNext=nullptr;
static BinkCloseFn g_binkCloseNext=nullptr;
static int WINAPI CinemaBinkWait(void* handle) {
    const DWORD saved=GetLastError();ObserveBinkHandle(handle,true);SetLastError(saved);
    return g_binkWaitNext(handle);
}
static void WINAPI CinemaBinkClose(void* handle) {
    const DWORD saved=GetLastError();ObserveBinkHandle(handle,false);SetLastError(saved);
    g_binkCloseNext(handle);
}
static void TryInstallCinemaBink() {
    static bool attempted=false;
    if(!g_cinematicTheater || attempted || !g_nativeImageGuard.enabled || !g_imagePresent.Ready())return;
    HMODULE module=GetModuleHandleW(L"binkw32.dll");if(!module)return;
    attempted=true;
    char path[MAX_PATH]{};
    if(!GetModuleFileNameA(module,path,MAX_PATH) || matched_capture::HashFile(path)!=
       "1a8e98434afd92412add96dc7c24885c88067f368a21c6a28445ef049ff09fe9") {
        Log("[CinemaBink] refused codec identity; observation UNKNOWN");return;
    }
    void* wait=reinterpret_cast<void*>(GetProcAddress(module,"_BinkWait@4"));
    void* close=reinterpret_cast<void*>(GetProcAddress(module,"_BinkClose@4"));
    HMODULE pinned=nullptr,self=nullptr;
    if(!wait || !close || !g_nativeImageGuard.Allows(reinterpret_cast<uintptr_t>(wait)-5,80) ||
       !g_nativeImageGuard.Allows(reinterpret_cast<uintptr_t>(close)-5,80) ||
       !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
           reinterpret_cast<LPCSTR>(wait),&pinned) ||
       !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
           reinterpret_cast<LPCSTR>(&CinemaBinkWait),&self))return;
    const auto init=MH_Initialize();if(init!=MH_OK && init!=MH_ERROR_ALREADY_INITIALIZED)return;
    void* w=nullptr;void* c=nullptr;
    if(MH_CreateHook(wait,reinterpret_cast<void*>(&CinemaBinkWait),&w)!=MH_OK)return;
    if(MH_CreateHook(close,reinterpret_cast<void*>(&CinemaBinkClose),&c)!=MH_OK) {MH_RemoveHook(wait);return;}
    g_binkWaitNext=reinterpret_cast<BinkWaitFn>(w);g_binkCloseNext=reinterpret_cast<BinkCloseFn>(c);
    // Close first; only publish playback once cleanup can be observed.
    if(MH_EnableHook(close)!=MH_OK) {MH_RemoveHook(wait);MH_RemoveHook(close);return;}
    if(MH_EnableHook(wait)!=MH_OK) {MH_DisableHook(close);MH_RemoveHook(wait);MH_RemoveHook(close);return;}
    Log("[CinemaBink] external Wait/Close observers ready; codec SHA pinned, EXE/IAT unchanged");
}
#else
static void TryInstallCinemaBink() {}
#endif

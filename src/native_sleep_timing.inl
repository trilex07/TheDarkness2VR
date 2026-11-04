// Explicit diagnostic observer of the SHA/site-admitted engine SleepEx wrapper.
// No EXE/IAT patch, wait removal, timer-resolution or scheduler change.
#ifdef D2VR_MATCHED_CAPTURE
using NativeSleepExFn=DWORD(WINAPI*)(DWORD,BOOL);
static NativeSleepExFn g_sleepTimingNext=nullptr;
struct NativeSleepWindow {
    unsigned count=0,deadline=0,zero=0,other=0;
    ULONGLONG requested=0,ticks=0,maximum=0;
    uintptr_t otherCaller=0;
};
static NativeSleepWindow g_sleepWindow;
static DWORD WINAPI ObserveNativeSleepEx(DWORD milliseconds,BOOL alertable) {
    if(reinterpret_cast<uintptr_t>(_ReturnAddress())!=0x99813Du ||
       GetCurrentThreadId()!=g_renderThreadId)
        return g_sleepTimingNext(milliseconds,alertable);
    // Verified wrapper 00998130 pushes alertable and milliseconds, then calls
    // SleepEx at00998137. Its caller return is after the two stdcall arguments.
    uintptr_t parent=0;
    __try { parent=reinterpret_cast<const uintptr_t*>(_AddressOfReturnAddress())[3]; }
    __except(EXCEPTION_EXECUTE_HANDLER) {}
    const DWORD before=GetLastError();LARGE_INTEGER begin{},end{};QueryPerformanceCounter(&begin);
    SetLastError(before);
    const DWORD result=g_sleepTimingNext(milliseconds,alertable);
    const DWORD after=GetLastError();QueryPerformanceCounter(&end);
    auto& s=g_sleepWindow;++s.count;s.deadline+=parent==0x938B67u;
    if(parent!=0x938B67u){++s.other;s.otherCaller=parent;}
    s.zero+=milliseconds==0;s.requested+=milliseconds;
    const ULONGLONG ticks=ULONGLONG(end.QuadPart-begin.QuadPart);
    s.ticks+=ticks;if(ticks>s.maximum)s.maximum=ticks;
    SetLastError(after);return result;
}
static void PollNativeSleepTiming() {
    static bool requested=GetPrivateProfileIntA("Diagnostics","NativeSleepTiming",0,".\\Darkness2VR-diagnostic.ini")!=0;
    static bool attempted=false,installed=false;static ULONGLONG next=0;
    if(!requested || !g_nativeImageGuard.enabled || !g_imagePresent.Ready() ||
       GetCurrentThreadId()!=g_renderThreadId)return;
    if(!attempted) {
        attempted=true;
        if(!VerifyEngineFrameCap())return;
        void* target=reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"SleepEx"));
        void* imported=nullptr;
        __try { imported=*reinterpret_cast<void**>(0xEEB358); }
        __except(EXCEPTION_EXECUTE_HANDLER) { return; }
        HMODULE api=nullptr,self=nullptr;
        if(!target || imported!=target || !g_nativeImageGuard.Allows(reinterpret_cast<uintptr_t>(target)-5,80) ||
           !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCSTR>(target),&api) ||
           !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCSTR>(&ObserveNativeSleepEx),&self)) {
            Log("[NativeSleepTiming] refused external API/import identity");return;
        }
        const auto init=MH_Initialize();if(init!=MH_OK && init!=MH_ERROR_ALREADY_INITIALIZED)return;
        void* original=nullptr;
        if(MH_CreateHook(target,reinterpret_cast<void*>(&ObserveNativeSleepEx),&original)!=MH_OK)return;
        g_sleepTimingNext=reinterpret_cast<NativeSleepExFn>(original);
        if(MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);return;}
        installed=true;next=GetTickCount64()+5000;
        Log("[NativeSleepTiming] external observer active: wrapper0099813D, deadline caller00938B67; arguments/results unchanged");
    }
    if(!installed || GetTickCount64()<next)return;
    next=GetTickCount64()+5000;
    LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
    const auto s=g_sleepWindow;g_sleepWindow={};
    const double scale=1000./double(frequency.QuadPart);
    Log("[NativeSleepTiming] calls=%u deadline=%u other=%u otherCaller=%p zero=%u requested=%llums wall=%.3fms max=%.3fms tid=%lu; waits, NOT CPU execution",
        s.count,s.deadline,s.other,reinterpret_cast<void*>(s.otherCaller),s.zero,s.requested,s.ticks*scale,s.maximum*scale,g_renderThreadId);
}
#else
static void PollNativeSleepTiming() {}
#endif

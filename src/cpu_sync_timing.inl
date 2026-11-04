// Diagnostic-only render-thread timing. CPUSync reports wall spans; CPUExecution
// separately reports scheduled kernel+user time. Neither is GPU busy time.
// Scopes overlap; quantized per-thread CPU counters are not per-draw profiling.
namespace d2vr_cpu_sync {
struct ThreadStamp {
    ULONGLONG ticks=0;
    DWORD thread=0;
    bool Read() {
        FILETIME created{},exited{},kernel{},user{};
        if(!GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user)) return false;
        ticks=((ULONGLONG(kernel.dwHighDateTime)<<32)|kernel.dwLowDateTime)+
              ((ULONGLONG(user.dwHighDateTime)<<32)|user.dwLowDateTime);
        thread=GetCurrentThreadId();return true;
    }
};
enum Stage { XrFrameWait, SharedReader, D3D9Fence, SharedCapture, D3D11Eye,
             XrSwapWait, XrSwapchainTotal, XrEnd, BoundaryWork, PresentForward, Count };
inline constexpr const char* names[Count]={"xrWaitFrame","D3D11 reader fence",
    "D3D9 producer fence","shared capture","D3D11 eye draw",
    "xrWaitSwapchainImage","XR swapchain total","xrEndFrame",
    "engine Present boundary work","original SwapChain Present"};
struct Entry {
    double sum=0,max=0;
    ULONGLONG cpuTicks=0;
    unsigned cpuCount=0;
    unsigned count=0,over5=0,over11=0,over22=0;
};
struct Timing {
    bool enabled=false;
    LARGE_INTEGER frequency{};
    ULONGLONG windowStart=0;
    SRWLOCK lock=SRWLOCK_INIT;
    Entry entries[Count]{};
    ThreadStamp windowCpu{};
    void Enable(bool on) {
        enabled=on && QueryPerformanceFrequency(&frequency) && frequency.QuadPart>0;
        windowStart=GetTickCount64();
        if(enabled) Log("[CPUSync] enabled=1; wall spans include blocking; separate CPUExecution uses quantized thread counters");
    }
    void MarkFrame(unsigned long long frame) {
        // QPC is the ETW/PresentMon clock on Windows. A sparse marker connects
        // the game's XR frame ID to their timelines without per-frame file I/O.
        if(!enabled || frame%120) return;
        LARGE_INTEGER qpc{};
        if(QueryPerformanceCounter(&qpc))
            Log("[PerfClock] frame=%llu qpc=%lld frequency=%lld pid=%lu tid=%lu",
                frame,qpc.QuadPart,frequency.QuadPart,GetCurrentProcessId(),GetCurrentThreadId());
    }
    void Add(Stage stage,const LARGE_INTEGER& start,const ThreadStamp* cpuStart=nullptr) {
        LARGE_INTEGER end{};
        if(!QueryPerformanceCounter(&end) || end.QuadPart<start.QuadPart) return;
        ThreadStamp cpuEnd{};
        const bool cpuValid=cpuStart && cpuStart->thread && cpuEnd.Read() &&
            cpuStart->thread==cpuEnd.thread && cpuEnd.ticks>=cpuStart->ticks;
        const double ms=(end.QuadPart-start.QuadPart)*1000.0/frequency.QuadPart;
        AcquireSRWLockExclusive(&lock);
        auto& e=entries[stage];
        e.sum+=ms; e.max=std::max(e.max,ms); ++e.count;
        e.over5+=ms>=5; e.over11+=ms>=11.111; e.over22+=ms>=22.222;
        if(cpuValid){e.cpuTicks+=cpuEnd.ticks-cpuStart->ticks;++e.cpuCount;}
        ReleaseSRWLockExclusive(&lock);
    }
    void Report(unsigned width,unsigned height,unsigned long long frame) {
        if(!enabled) return;
        ThreadStamp cpuNow{};const bool cpuValid=cpuNow.Read();
        if(!windowCpu.thread && cpuValid) windowCpu=cpuNow;
        const ULONGLONG now=GetTickCount64();
        if(now-windowStart<5000) return;
        Log("[CPUSync] window frame=%llu source=%ux%u elapsed=%llums",frame,width,height,now-windowStart);
        Entry snapshot[Count];
        AcquireSRWLockExclusive(&lock);
        for(int stage=0;stage<Count;++stage) {
            snapshot[stage]=entries[stage];
            entries[stage]={};
        }
        ReleaseSRWLockExclusive(&lock);
        if(cpuValid && windowCpu.thread==cpuNow.thread && cpuNow.ticks>=windowCpu.ticks)
            Log("[CPUExecution] render-thread cpu=%.3fms elapsed=%llums frames=%u tid=%lu; scheduled kernel+user time, includes driver spinning",
                (cpuNow.ticks-windowCpu.ticks)/10000.0,now-windowStart,snapshot[BoundaryWork].count,cpuNow.thread);
        windowCpu=cpuValid?cpuNow:ThreadStamp{};
        for(int stage=0;stage<Count;++stage) {
            const auto& e=snapshot[stage];
            if(e.count) Log("[CPUSync] %s mean=%.3fms max=%.3fms n=%u over5=%u over11=%u over22=%u",
                names[stage],e.sum/e.count,e.max,e.count,e.over5,e.over11,e.over22);
            if(e.cpuCount) Log("[CPUExecution] %s cpu=%.3fms n=%u; overlapping scopes, quantized thread accounting",
                names[stage],e.cpuTicks/10000.0,e.cpuCount);
        }
        windowStart=now;
    }
};
static Timing timing;
struct Scope {
    Stage stage;
    LARGE_INTEGER start{};
    ThreadStamp cpuStart{};
    bool active=false;
    explicit Scope(Stage value):stage(value) {
        active=timing.enabled && QueryPerformanceCounter(&start);
        if(active)cpuStart.Read();
    }
    ~Scope() { if(active) timing.Add(stage,start,&cpuStart); }
};
}

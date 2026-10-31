// Diagnostic-only, render-thread CPU wall timing. These are waits and API-call
// durations, not CPU execution time or GPU timestamps. Ordinary builds omit it.
namespace d2vr_cpu_sync {
enum Stage { XrFrameWait, SharedReader, D3D9Fence, SharedCapture, D3D11Eye,
             XrSwapWait, XrSwapchainTotal, XrEnd, BoundaryWork, PresentForward, Count };
inline constexpr const char* names[Count]={"xrWaitFrame","D3D11 reader fence",
    "D3D9 producer fence","shared capture","D3D11 eye draw",
    "xrWaitSwapchainImage","XR swapchain total","xrEndFrame",
    "engine Present boundary work","original SwapChain Present"};
struct Entry {
    double sum=0,max=0;
    unsigned count=0,over5=0,over11=0,over22=0;
};
struct Timing {
    bool enabled=false;
    LARGE_INTEGER frequency{};
    ULONGLONG windowStart=0;
    SRWLOCK lock=SRWLOCK_INIT;
    Entry entries[Count]{};
    void Enable(bool on) {
        enabled=on && QueryPerformanceFrequency(&frequency) && frequency.QuadPart>0;
        windowStart=GetTickCount64();
        if(enabled) Log("[CPUSync] enabled=1; wall time includes driver/runtime blocking, not CPU busy time");
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
    void Add(Stage stage,const LARGE_INTEGER& start) {
        LARGE_INTEGER end{};
        if(!QueryPerformanceCounter(&end) || end.QuadPart<start.QuadPart) return;
        const double ms=(end.QuadPart-start.QuadPart)*1000.0/frequency.QuadPart;
        AcquireSRWLockExclusive(&lock);
        auto& e=entries[stage];
        e.sum+=ms; e.max=std::max(e.max,ms); ++e.count;
        e.over5+=ms>=5; e.over11+=ms>=11.111; e.over22+=ms>=22.222;
        ReleaseSRWLockExclusive(&lock);
    }
    // Per-call identity for rare producer-fence outliers only. The window
    // aggregate cannot prove which call owns a stall, nor relate it to one
    // PresentMon interval. Each interval brackets exactly one driver call:
    // issue=[Issue(D3DISSUE_END)], flush=[GetData(D3DGETDATA_FLUSH)]. Command
    // history construction/Finish lie outside both. QPC is the PresentMon/ETW
    // clock. Logging happens only after an already >=11.111 ms stall (cap 256),
    // so ordinary frames add four QPC reads and no file I/O.
    struct ProducerCall {
        LARGE_INTEGER issueBegin{},issueEnd{},flushBegin{},flushEnd{};
        HRESULT issueHr=E_PENDING,flushHr=E_PENDING;
        bool timed=false;
    };
    static constexpr LONG producerOutlierCap=256;
    static constexpr double producerOutlierMs=11.111;
    LONG producerOutliers=0;
    double Ms(const LARGE_INTEGER& a,const LARGE_INTEGER& b) const {
        return b.QuadPart>=a.QuadPart ? (b.QuadPart-a.QuadPart)*1000.0/frequency.QuadPart : 0.0;
    }
    // Returns true when an event was logged.
    bool ProducerOutlier(const ProducerCall& c,int mode,unsigned capture,unsigned long long frame) {
        if(!enabled || !c.timed) return false;
        const double issueMs=Ms(c.issueBegin,c.issueEnd);
        const double flushMs=c.flushEnd.QuadPart ? Ms(c.flushBegin,c.flushEnd) : 0.0;
        const double totalMs=issueMs+flushMs;
        if(totalMs<producerOutlierMs || InterlockedIncrement(&producerOutliers)>producerOutlierCap) return false;
        Log("[CPUSync] producer outlier mode=%d capture=%u frame=%llu issueQpc=%lld..%lld issueMs=%.3f issueHr=0x%08lX flushQpc=%lld..%lld flushMs=%.3f flushHr=0x%08lX totalMs=%.3f tid=%lu",
            mode,capture,frame,c.issueBegin.QuadPart,c.issueEnd.QuadPart,issueMs,c.issueHr,
            c.flushBegin.QuadPart,c.flushEnd.QuadPart,flushMs,c.flushHr,totalMs,GetCurrentThreadId());
        return true;
    }
    void Report(unsigned width,unsigned height,unsigned long long frame) {
        if(!enabled) return;
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
        for(int stage=0;stage<Count;++stage) {
            const auto& e=snapshot[stage];
            if(e.count) Log("[CPUSync] %s mean=%.3fms max=%.3fms n=%u over5=%u over11=%u over22=%u",
                names[stage],e.sum/e.count,e.max,e.count,e.over5,e.over11,e.over22);
        }
        windowStart=now;
    }
};
static Timing timing;
struct Scope {
    Stage stage;
    LARGE_INTEGER start{};
    bool active=false;
    explicit Scope(Stage value):stage(value) {
        active=timing.enabled && QueryPerformanceCounter(&start);
    }
    ~Scope() { if(active) timing.Add(stage,start); }
};
}

// PSVR2 eye tracking reader. The x64 bridge publishes the Toolkit gaze sample
// into "Local\Darkness2VR.Gaze.<pid>" (src/psvr2/GazeShare.h). Read-only here;
// stale (>100 ms), invalid or blinking samples mean "no gaze" and every
// consumer keeps its controller behaviour. Recon: docs/research/psvr2-eye-tracking-recon.md.
// psvr2/GazeShare.h is included at the top of d3d9.cpp (outside the anonymous namespace).

struct GazeReader {
    HANDLE mapping=nullptr;
    const d2vr::psvr2::GazeShared* view=nullptr;
    ULONGLONG nextTry=0;
    bool announced=false;
    bool Open() {
        if(view) return true;
        const ULONGLONG now=GetTickCount64();
        if(now<nextTry) return false;
        nextTry=now+2000; // the bridge may start later; retry cheaply
        wchar_t name[64];
        swprintf_s(name,L"Local\\Darkness2VR.Gaze.%lu",GetCurrentProcessId());
        mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,name);
        if(!mapping) return false;
        view=static_cast<const d2vr::psvr2::GazeShared*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,sizeof(d2vr::psvr2::GazeShared)));
        if(!view) { CloseHandle(mapping); mapping=nullptr; return false; }
        if(!announced) { announced=true; Log("[EyeTracking] bridge gaze section opened"); }
        return true;
    }
    bool Read(d2vr::psvr2::GazeSample& out) {
        if(!Open()) return false;
        bool ok=false;
        __try { ok=d2vr::psvr2::GazeRead(*view,out); } __except(EXCEPTION_EXECUTE_HANDLER) { ok=false; }
        return ok;
    }
    void Close() {
        if(view) UnmapViewOfFile(view);
        if(mapping) CloseHandle(mapping);
        view=nullptr; mapping=nullptr;
    }
};
static GazeReader g_gazeReader;
static SRWLOCK g_gazeLock=SRWLOCK_INIT;

// [EyeTracking] GazeAxes maps raw Toolkit axes to OpenXR view space
// (+X right, +Y up, -Z forward). Format "x,y,-z": each output axis takes a
// signed input axis. Re-read every 2 s so it can be fixed while playing.
struct GazeAxes { int src[3]{0,1,2}; float sign[3]{1,1,-1}; };
static bool ParseGazeAxes(const char* text,GazeAxes& out) {
    GazeAxes a; int n=0;
    for(const char* p=text; *p && n<3;) {
        while(*p==' ' || *p==',') ++p;
        float sign=1; if(*p=='-') { sign=-1; ++p; } else if(*p=='+') ++p;
        const char c=char(*p|0x20);
        if(c<'x' || c>'z') return false;
        a.src[n]=c-'x'; a.sign[n]=sign; ++n; ++p;
    }
    if(n!=3 || a.src[0]==a.src[1] || a.src[0]==a.src[2] || a.src[1]==a.src[2]) return false;
    out=a; return true;
}
static GazeAxes CurrentGazeAxes() {
    static GazeAxes axes; static ULONGLONG next=0; static char last[32]="";
    const ULONGLONG now=GetTickCount64();
    if(now>=next) {
        next=now+2000;
        char value[32]{};
        GetPrivateProfileStringA("EyeTracking","GazeAxes","x,y,-z",value,sizeof(value),g_activeIni[0] ? g_activeIni:".\\Darkness2VR.ini");
        if(strcmp(value,last)) {
            strcpy_s(last,value);
            GazeAxes parsed;
            if(ParseGazeAxes(value,parsed)) { axes=parsed; Log("[EyeTracking] GazeAxes=%s",value); }
            else Log("[EyeTracking] invalid GazeAxes=%s; keeping previous",value);
        }
    }
    return axes;
}

// Latest usable gaze direction in OpenXR VIEW space (unit vector).
static bool LatestGaze(XrVector3f& dir) {
    if(!g_eyeTrackingEnabled) return false;
    d2vr::psvr2::GazeSample s;
    AcquireSRWLockExclusive(&g_gazeLock);
    const bool read=g_gazeReader.Read(s);
    ReleaseSRWLockExclusive(&g_gazeLock);
    const ULONGLONG now=GetTickCount64();
    // Quality report every 5 s: rate, validity and the raw combined direction
    // (look straight ahead once to see which raw axis is forward).
    static ULONGLONG reportAt=0; static uint32_t lastFrame=0, frames=0, valid=0, polls=0;
    ++polls;
    if(read && s.frameCounter!=lastFrame) { lastFrame=s.frameCounter; ++frames; if(s.flags & d2vr::psvr2::GazeCombinedValid) ++valid; }
    if(now>=reportAt) {
        if(reportAt) Log("[EyeTracking] %s samples/5s=%u valid=%u polls=%u age=%llums raw=(%.3f,%.3f,%.3f) flags=%02X tracking=%u",
            g_gazeReader.view ? "bridge" : "NO BRIDGE SECTION",frames,valid,polls,
            read ? (unsigned long long)(now-s.publishMs) : 0ull,s.dirCombined[0],s.dirCombined[1],s.dirCombined[2],s.flags,s.trackingState);
        frames=valid=polls=0; reportAt=now+5000;
    }
    if(!read || now-s.publishMs>100) return false;
    using namespace d2vr::psvr2;
    if(!(s.flags & GazeCombinedValid) || ((s.flags & GazeBlinkLeft) && (s.flags & GazeBlinkRight))) return false;
    const GazeAxes a=CurrentGazeAxes();
    float v[3];
    for(int i=0;i<3;++i) v[i]=a.sign[i]*s.dirCombined[a.src[i]];
    const float n=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if(!(n>.5f && n<1.5f)) return false;
    dir={v[0]/n,v[1]/n,v[2]/n};
    return true;
}

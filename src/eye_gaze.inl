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
    static GazeAxes axes{{0,1,2},{-1,1,-1}}; static ULONGLONG next=0; static char last[32]="";
    const ULONGLONG now=GetTickCount64();
    if(now>=next) {
        next=now+2000;
        char value[32]{};
        GetPrivateProfileStringA("EyeTracking","GazeAxes","-x,y,-z",value,sizeof(value),g_activeIni[0] ? g_activeIni:".\\Darkness2VR.ini");
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

// Gaze pixel on one submitted eye image (foveated NIS). The image was rendered
// with this frame's submit FOV (+ the shared horizontal offset) at the eye pose.
static bool FoveaGazePixel(int eye,uint32_t width,uint32_t height,float& px,float& py) {
    if(eye<0 || eye>1 || !g_haveViews) return false;
    XrVector3f d{};
    if(!LatestGaze(d)) return false;
    const XrPosef head=HeadCenterPose(g_eyePose[0],g_eyePose[1]);
    const XrVector3f world=RotVec(head.orientation,d.x,d.y,d.z);
    const auto& q=g_eyePose[eye].orientation;
    const XrQuaternionf inv{-q.x,-q.y,-q.z,q.w};
    const XrVector3f e=RotVec(inv,world.x,world.y,world.z);
    XrFovf fov=g_pairSubmitMetaValid ? g_pairSubmitFov[eye] : g_eyeFov[eye];
    fov.angleLeft+=g_trackedFrame.offsetX; fov.angleRight+=g_trackedFrame.offsetX;
    return d2vr_fovea::GazePixel(e.x,e.y,e.z,fov.angleLeft,fov.angleRight,fov.angleUp,fov.angleDown,width,height,px,py);
}

// Gaze in tracking space (g_xrSpace): head centre + rotated gaze direction.
static bool GazeWorldDirection(XrVector3f& direction) {
    if(!g_haveViews) return false;
    XrVector3f d{};
    if(!LatestGaze(d)) return false;
    const XrPosef head=HeadCenterPose(g_eyePose[0],g_eyePose[1]);
    direction=RotVec(head.orientation,d.x,d.y,d.z);
    return true;
}
static bool GazeWorldRay(XrVector3f& origin,XrVector3f& direction) {
    if(!GazeWorldDirection(direction)) return false;
    origin=HeadCenterPose(g_eyePose[0],g_eyePose[1]).position;
    return true;
}

// ---------------------------------------------------------------- menu gaze
// [EyeTracking] GazeMenu: in menus (cinema quad shown) the native menu cursor
// follows the gaze; A/trigger then confirms the hovered item through the
// game's own pad handling. Cursor setter MouseDeviceWin vtable FC00C8 +0x70 ->
// C4B690 (thiscall x,y client pixels: ClientToScreen, SetCursorPos when
// acquired, stores +21C/+220). Runs only on the native window thread.
static volatile bool g_gazeMenuActive=false; // window thread writes, input thread reads
struct MenuSurface { bool valid=false; XrPosef pose{}; float width=0,height=0; ULONGLONG time=0; XrSpace space=XR_NULL_HANDLE; };
static SRWLOCK g_menuSurfaceLock=SRWLOCK_INIT;
static MenuSurface g_menuSurface;
static bool GazeMenuActive() { return g_gazeMenuActive; }
static void RecordMenuSurface(const XrCompositionLayerQuad& quad) {
    MenuSurface s; s.valid=true; s.pose=quad.pose; s.width=quad.size.width; s.height=quad.size.height;
    s.time=GetTickCount64(); s.space=quad.space;
    AcquireSRWLockExclusive(&g_menuSurfaceLock); g_menuSurface=s; ReleaseSRWLockExclusive(&g_menuSurfaceLock);
}
static bool g_menuClampSaved=false;
static uint32_t g_menuClampMouse=0;
static int g_menuClampW=0,g_menuClampH=0;
static void RestoreMenuClamp() {
    if(!g_menuClampSaved) return;
    __try {
        if(*(DWORD*)g_menuClampMouse==0xFC00C8 && g_menuClampW>0 && g_menuClampH>0) {
            *(int*)(g_menuClampMouse+0x224)=g_menuClampW; *(int*)(g_menuClampMouse+0x228)=g_menuClampH;
            Log("[GazeMenu] cursor clamp restored %dx%d",g_menuClampW,g_menuClampH);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    g_menuClampSaved=false; g_menuClampMouse=0;
}
static void ApplyGazeMenuCursor() {
#ifdef D2VR_MATCHED_CAPTURE
    static d2vr_gaze_menu::CursorFilter filter;
    static ULONGLONG nextObserve=0; static uint32_t mouse=0; static bool contractOk=false, contractChecked=false;
    if(!g_gazeMenu || !g_eyeTrackingEnabled) { filter.Reset(); g_gazeMenuActive=false; RestoreMenuClamp(); return; }
    const HWND window=g_renderSizeHwnd;
    DWORD pid=0;
    if(!window || GetWindowThreadProcessId(window,&pid)!=GetCurrentThreadId() || pid!=GetCurrentProcessId()) return;
    // Menu gate (headset 00:43: pause menus keep the 3D world rendering, so
    // "no world" alone never fired). Native pause predicate A4D340 or the top
    // Flash movie asking for a cursor (movie+F4), re-read every 100 ms.
    static ULONGLONG nextGate=0; static bool paused=false, cursorWanted=false, gateLogged=false, topWantsCursor=false; static int lastGate=-1;
    static uint32_t topMovie=0, forcedMovie=0;
    const ULONGLONG nowGate=GetTickCount64();
    if(nowGate>=nextGate) {
        nextGate=nowGate+100;
        static int predicateOk=-1;
        if(predicateOk<0) {
            static const BYTE predicate[]={0x80,0xb9,0x98,0x01,0x00,0x00,0x00,0x75,0x11,0x8b,0x81,0xec,0x00,0x00,0x00,0x83,0x38,0x00,0x74,0x06};
            __try { predicateOk=!memcmp((void*)0xA4D340,predicate,sizeof(predicate)); } __except(EXCEPTION_EXECUTE_HANDLER) { predicateOk=0; }
        }
        paused=false;
        const auto manager=uint32_t(g_nativeRegionManager);
        if(predicateOk==1 && manager) {
            __try { paused=*(BYTE*)(manager+0x198)==0 && **(DWORD**)(manager+0xEC)!=0; } __except(EXCEPTION_EXECUTE_HANDLER) { paused=false; }
        }
        const auto readMovie=[](uint32_t address,uint32_t& value) {
            MEMORY_BASIC_INFORMATION region{};
            if(VirtualQuery(reinterpret_cast<const void*>(address),&region,sizeof(region))!=sizeof(region) ||
               region.State!=MEM_COMMIT || (region.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
            SIZE_T copied=0;
            return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),&value,4,&copied) && copied==4;
        };
        d2vr_menu_native::TopMovieSnapshot movie;
        const bool observed=d2vr_menu_native::ObserveTopMovie(readMovie,movie) && movie.hasMovie && movie.movie.visible;
        if(forcedMovie && (!observed || movie.topMovie!=forcedMovie)) forcedMovie=0; // replaced: never touch it again
        // Our own forced flag must not hold the gate open.
        cursorWanted=observed && movie.cursorRequested && movie.topMovie!=forcedMovie;
        topMovie=observed ? movie.topMovie : 0; topWantsCursor=observed && movie.cursorRequested;
        (void)gateLogged;
    }
    const bool gate=paused || cursorWanted || !g_in3DGameplayMode;
    // Headset 00:53: relative mouse moves reached the native cursor exactly,
    // yet nothing highlighted: the top movie had cursorRequested=0 (pad mode,
    // mouse ignored). Request the cursor the way the game's own setter B072E0
    // does (movie+F4 = 1) while the gaze menu is active; restore on exit.
    if(g_gazeMenuForceCursor) {
        static int setterOk=-1;
        if(setterOk<0) {
            static const BYTE setterBytes[]={0x8A,0x44,0x24,0x04,0x88,0x81,0xF4,0x00,0x00,0x00,0xC2,0x04,0x00};
            __try { setterOk=!memcmp((void*)0xB072E0,setterBytes,sizeof(setterBytes)); } __except(EXCEPTION_EXECUTE_HANDLER) { setterOk=0; }
            Log("[GazeMenu] cursor-request flag movie+F4 %s",setterOk==1 ? "verified (setter B072E0)" : "REFUSED: unexpected EXE bytes");
        }
        if(setterOk==1 && gate && topMovie && !topWantsCursor && (!forcedMovie || forcedMovie==topMovie)) {
            // Re-request if the game switched back to pad mode (logged, rate-limited).
            static unsigned reapplied=0; static ULONGLONG lastReapply=0;
            const bool again=forcedMovie==topMovie;
            __try { *(BYTE*)(topMovie+0xF4)=1; forcedMovie=topMovie; topWantsCursor=true; } __except(EXCEPTION_EXECUTE_HANDLER) { forcedMovie=0; }
            if(forcedMovie && !again) Log("[GazeMenu] cursor requested on top movie %08X (was pad mode)",forcedMovie);
            else if(forcedMovie && (++reapplied, nowGate-lastReapply>2000)) {
                lastReapply=nowGate; Log("[GazeMenu] game cleared the cursor request again (%u times); re-requested",reapplied);
            }
        } else if(!gate && forcedMovie) {
            if(topMovie==forcedMovie) { __try { *(BYTE*)(forcedMovie+0xF4)=0; } __except(EXCEPTION_EXECUTE_HANDLER) {} }
            Log("[GazeMenu] cursor request released on movie %08X",forcedMovie);
            forcedMovie=0;
        }
    }
    if(int(gate)!=lastGate) {
        lastGate=int(gate);
        Log("[GazeMenu] menu cursor %s (pause=%d flashCursor=%d world=%d manager=%08X)",gate ? "ON" : "off",
            int(paused),int(cursorWanted),int(g_in3DGameplayMode),unsigned(g_nativeRegionManager));
    }
    g_gazeMenuActive=gate;
    if(!gate) { filter.Reset(); RestoreMenuClamp(); return; }
    if(!contractChecked) {
        contractChecked=true;
        static const BYTE setter[]={0x83,0xEC,0x18,0x55,0x8B,0x6C,0x24,0x24,0x56,0x57,0x8B,0x7C,0x24,0x28,0x8B,0xF1,0x8B,0x8E,0xE8,0x01,0,0};
        __try {
            contractOk=*(DWORD*)(0xFC00C8+0x70)==0xC4B690 && !memcmp((void*)0xC4B690,setter,sizeof(setter));
        } __except(EXCEPTION_EXECUTE_HANDLER) { contractOk=false; }
        Log("[GazeMenu] native cursor setter %s",contractOk ? "verified (FC00C8+70 -> C4B690)" : "REFUSED: unexpected EXE bytes; menu gaze off");
    }
    if(!contractOk) return;
    MenuSurface surface;
    AcquireSRWLockShared(&g_menuSurfaceLock); surface=g_menuSurface; ReleaseSRWLockShared(&g_menuSurfaceLock);
    const ULONGLONG now=GetTickCount64();
    float u=0,v=0;
    if(surface.valid && now-surface.time<=250 && surface.space==g_xrSpace) {
        // Menu on the cinema quad (no 3D world): gaze ray against the quad.
        XrVector3f o{},d{};
        if(!GazeWorldRay(o,d)) return;
        const auto& q=surface.pose.orientation; const auto& c=surface.pose.position;
        if(!d2vr_gaze_menu::HitQuad({o.x,o.y,o.z},{d.x,d.y,d.z},{c.x,c.y,c.z},{q.x,q.y,q.z,q.w},surface.width,surface.height,u,v)) return;
    } else {
        // Menu drawn into the stereo eye images (pause over the world): the 2D
        // UI spans each eye image, so the gaze pixel (mean of both eyes) is
        // the client position.
        if(!g_eyeCacheW || !g_eyeCacheH) return;
        float px[2]{},py[2]{}; int n=0;
        for(int eye=0;eye<2;++eye) if(FoveaGazePixel(eye,g_eyeCacheW,g_eyeCacheH,px[n],py[n])) ++n;
        if(!n) return;
        u=(n==2 ? (px[0]+px[1])*.5f : px[0])/float(g_eyeCacheW);
        v=(n==2 ? (py[0]+py[1])*.5f : py[0])/float(g_eyeCacheH);
    }
    RECT client{};
    if(!GetClientRect(window,&client) || client.right<=0 || client.bottom<=0) return;
    // The menu draws the cursor in backbuffer pixels (2676x2728 per eye) while
    // the window client is only 2676x1228 and 787DF0 clamps the native cursor
    // to [mouse+224]x[mouse+228] (headset 00:57: cursor stuck in the top band).
    // Target the backbuffer and widen the clamp while the gaze menu is active.
    const int surfW=g_eyeCacheW ? int(g_eyeCacheW) : int(client.right);
    const int surfH=g_eyeCacheH ? int(g_eyeCacheH) : int(client.bottom);
    client.right=surfW; client.bottom=surfH; // logged size = cursor space
    const int x=int(u*float(surfW-1)+.5f), y=int(v*float(surfH-1)+.5f);
    if(!filter.Changed(x,y,3)) return;
    // Resolve the mouse device through the verified receiver graph (cached 2 s).
    if(!mouse || now>=nextObserve) {
        nextObserve=now+2000; mouse=0;
        const auto read=[](uint32_t address,uint32_t& value) {
            MEMORY_BASIC_INFORMATION region{};
            if(VirtualQuery(reinterpret_cast<const void*>(address),&region,sizeof(region))!=sizeof(region) ||
               region.State!=MEM_COMMIT || (region.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
            SIZE_T copied=0;
            return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),&value,4,&copied) && copied==4;
        };
        d2vr_menu_native::Snapshot snap;
        const auto framework=uint32_t(GetWindowLongPtrW(window,GWLP_USERDATA));
        const auto owned=uint32_t(reinterpret_cast<uintptr_t>(g_nativeViewTables[0].Table()));
        if(d2vr_menu_native::Observe(uint32_t(reinterpret_cast<uintptr_t>(window)),framework,read,snap,owned)) mouse=snap.mouse;
        static uint32_t loggedMouse=1;
        if(mouse!=loggedMouse) { loggedMouse=mouse; Log("[GazeMenu] mouse device %08X%s",mouse,mouse ? "" : " unavailable; cursor not moved"); }
    }
    if(!mouse) return;
    // Mode 2 (default): Flash highlights only on decoded mouse MOVE events
    // (headset 00:49: native position matched the gaze, nothing highlighted).
    // Send real relative mouse input and let the game's own DirectInput
    // decoder (8A1470) move its cursor; closed loop on the native position.
    // Mode 1: direct native setter (position only, no events).
    // Clamp bounds: widen to the backbuffer (restored when the menu closes).
    __try {
        if(*(DWORD*)mouse==0xFC00C8) {
            int& bw=*(int*)(mouse+0x224); int& bh=*(int*)(mouse+0x228);
            if(bw!=surfW || bh!=surfH) {
                if(!g_menuClampSaved || g_menuClampMouse!=mouse) {
                    g_menuClampMouse=mouse; g_menuClampW=bw; g_menuClampH=bh; g_menuClampSaved=true;
                    Log("[GazeMenu] cursor clamp %dx%d -> backbuffer %dx%d (restored on exit)",bw,bh,surfW,surfH);
                }
                bw=surfW; bh=surfH;
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { mouse=0; return; }
    int nativeX=0,nativeY=0; BYTE acquiredNow=0;
    __try {
        if(*(DWORD*)mouse!=0xFC00C8) { mouse=0; return; }
        nativeX=*(int*)(mouse+0x21C); nativeY=*(int*)(mouse+0x220); acquiredNow=*(BYTE*)(mouse+7);
    } __except(EXCEPTION_EXECUTE_HANDLER) { mouse=0; return; }
    if(g_gazeMenuMode==1) {
        using SetPos=void(__thiscall*)(void*,int,int);
        __try { reinterpret_cast<SetPos>(0xC4B690)(reinterpret_cast<void*>(mouse),x,y); }
        __except(EXCEPTION_EXECUTE_HANDLER) { mouse=0; Log("[GazeMenu] cursor setter faulted; device dropped"); return; }
    } else {
        // Only while the game owns the foreground and its mouse is acquired, so
        // the desktop cursor of another application is never moved.
        DWORD fgPid=0; GetWindowThreadProcessId(GetForegroundWindow(),&fgPid);
        if(fgPid!=GetCurrentProcessId() || !acquiredNow) {
            static ULONGLONG warned=0;
            if(now-warned>5000) { warned=now; Log("[GazeMenu] relative input skipped: foreground pid=%lu acquired=%d",fgPid,int(acquiredNow)); }
            filter.Reset(); return;
        }
        int dx=x-nativeX, dy=y-nativeY;
        if(std::abs(dx)<3 && std::abs(dy)<3) return;
        // Half the error per tick: converges even if the game scales deltas.
        dx=std::clamp(dx/2+(dx>0)-(dx<0),-150,150); dy=std::clamp(dy/2+(dy>0)-(dy<0),-150,150);
        INPUT in{}; in.type=INPUT_MOUSE; in.mi.dx=dx; in.mi.dy=dy; in.mi.dwFlags=MOUSEEVENTF_MOVE;
        SendInput(1,&in,sizeof(in));
        filter.Reset(); // keep correcting every tick until the native cursor arrives
    }
    static ULONGLONG lastLog=0;
    if(now-lastLog>1000) {
        lastLog=now;
        int nx=-1,ny=-1,acquired=-1;
        __try { nx=*(int*)(mouse+0x21C); ny=*(int*)(mouse+0x220); acquired=*(BYTE*)(mouse+7); } __except(EXCEPTION_EXECUTE_HANDLER) {}
        Log("[GazeMenu] mode=%d gaze uv=(%.3f,%.3f) -> client (%d,%d) of %ldx%ld; native cursor now (%d,%d) acquired=%d",
            g_gazeMenuMode,u,v,x,y,client.right,client.bottom,nx,ny,acquired);
    }
#endif
}

// Read-only root-to-top-movie observation. No singleton factory call, native
// object retention, input ownership or inferred menu plane follows from this.
static void ObserveNativeTopMovie() {
#ifdef D2VR_MATCHED_CAPTURE
    static unsigned attempts=0;
    static bool complete=false,contracts=false;
    static ULONGLONG next=0;
    if(complete || attempts>=8 || !g_nativePlayerOwnerAdmitted ||
       !g_nativeImageGuard.enabled || !g_imagePreservingPresent)return;
    const auto now=GetTickCount64();if(now<next)return;next=now+1000;++attempts;
    DWORD process=0;
    const auto thread=GetWindowThreadProcessId(g_renderSizeHwnd,&process);
    if(thread!=GetCurrentThreadId() || process!=GetCurrentProcessId()) {
        if(attempts==8)Log("[NativeTopMovie] native window owner thread unavailable; observation=UNKNOWN injection=OFF");
        return;
    }
    if(!contracts) {
        struct Range {uintptr_t address;size_t size;uint64_t hash;};
        static const Range pins[]={
        {0xC98BD0,257,0x775E9CC657A3ABD4ull},
        {0xBB87C0,411,0x744830DEB35C14FFull},
        {0xB64910,7,0x59A67108C1D2E8B9ull},
        {0x76D5C0,4,0x586C2AFCD8DE981Cull},
        {0x6E4250,114,0x15BD1818B66D731Cull},
        {0xA0B120,19,0x2BA6035F85972C56ull},
        {0xA0DC30,9,0xA56588CB2DA4B470ull},
        {0xB0FB20,63,0xF94302B056FB32A8ull},
        };
        for(const auto& pin:pins) {
            unsigned char bytes[512];SIZE_T copied=0;
            if(pin.size>sizeof(bytes) || !ReadProcessMemory(GetCurrentProcess(),
                reinterpret_cast<const void*>(pin.address),bytes,pin.size,&copied) || copied!=pin.size) {
                complete=true;Log("[NativeTopMovie] instruction contract unreadable; observation=UNKNOWN injection=OFF");return;
            }
            uint64_t hash=14695981039346656037ull;
            for(size_t i=0;i<pin.size;++i)hash=(hash^bytes[i])*1099511628211ull;
            if(hash!=pin.hash) {
                complete=true;Log("[NativeTopMovie] instruction contract differs at %08X; observation=UNKNOWN injection=OFF",unsigned(pin.address));return;
            }
        }
        contracts=true;
    }
    const auto read=[](uint32_t address,uint32_t& value) {
        MEMORY_BASIC_INFORMATION region{};
        if(VirtualQuery(reinterpret_cast<const void*>(address),&region,sizeof(region))!=sizeof(region) ||
           region.State!=MEM_COMMIT || (region.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
        const auto end=reinterpret_cast<uintptr_t>(region.BaseAddress)+region.RegionSize;
        if(end<address || end-address<4)return false;
        SIZE_T copied=0;
        return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),
            &value,sizeof(value),&copied) && copied==sizeof(value);
    };
    d2vr_menu_native::TopMovieSnapshot first,second;
    const bool observed=d2vr_menu_native::ObserveTopMovie(read,first) &&
        d2vr_menu_native::ObserveTopMovie(read,second) && first==second;
    if(observed && first.hasMovie && first.movie.visible && first.movie.topmost && first.cursorRequested)
        complete=true;
    if(complete || attempts==8) {
        complete=true;
        if(observed)
            Log("[NativeTopMovie] managerRef=%08X manager=%08X begin=%08X end=%08X topRef=%08X movie=%08X hasMovie=%d visible=%d topmost=%d cursorRequested=%d; read-only root snapshot lifetimeLease=0 surface=UNKNOWN injection=OFF",
                first.managerRef,first.manager,first.begin,first.end,first.topRef,first.topMovie,
                int(first.hasMovie),int(first.movie.visible),int(first.movie.topmost),int(first.cursorRequested));
        else Log("[NativeTopMovie] no supported stable top movie after eight observations; lifetimeLease=0 surface=UNKNOWN injection=OFF");
    }
#endif
}

// Bounded one-shot receiver observation for the next consolidated manual test.
// No native calls, writes, factories, cursor movement or process scanning.
static void ObserveNativePauseState(void* manager) {
#ifdef D2VR_MATCHED_CAPTURE
    static unsigned attempts=0;
    static bool complete=false;
    static ULONGLONG next=0;
    if(complete || attempts>=8)return;
    const auto now=GetTickCount64();if(now<next)return;next=now+1000;++attempts;
    static const unsigned char predicate[]={0x80,0xb9,0x98,0x01,0x00,0x00,0x00,0x75,0x11,
        0x8b,0x81,0xec,0x00,0x00,0x00,0x83,0x38,0x00,0x74,0x06,0xb8,0x01,0x00,0x00,
        0x00,0xc3,0x33,0xc0,0xc3};
    unsigned char loaded[sizeof(predicate)]{};SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(0xA4D340),
        loaded,sizeof(loaded),&copied) || copied!=sizeof(loaded) || memcmp(loaded,predicate,sizeof(loaded))) {
        complete=true;
        Log("[NativePauseState] loaded predicate differs; state=UNKNOWN injection=OFF");return;
    }
    const auto read=[](uint32_t address,uint32_t& value) {
        SIZE_T copied=0;
        return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),
            &value,sizeof(value),&copied) && copied==sizeof(value);
    };
    d2vr_menu_native::PauseSnapshot first,second;
    const auto receiver=uint32_t(reinterpret_cast<uintptr_t>(manager));
    if(d2vr_menu_native::ObservePause(receiver,read,first) &&
       d2vr_menu_native::ObservePause(receiver,read,second) && first==second) {
        complete=true;
        Log("[NativePauseState] manager=%08X rulesRef=%08X rules=%08X table=%08X suppressed=%u menuRef=%08X menu=%08X showing=%d; read-only predicate snapshot; lifetimeLease=0 surface=UNKNOWN injection=OFF",
            first.manager,first.rulesRef,first.rules,first.table,unsigned(first.suppressed),
            first.menuRef,first.menu,int(first.showing));
    } else if(attempts==8)
        Log("[NativePauseState] no stable supported predicate after eight observations; state=UNKNOWN injection=OFF");
#else
    (void)manager;
#endif
}
static void ObserveNativeMenuReceiver() {
    ObserveNativeTopMovie();
#ifdef D2VR_MATCHED_CAPTURE
    static unsigned attempts=0;
    static bool complete=false;
    static ULONGLONG next=0;
    if(complete || attempts>=8 || !g_nativePlayerOwnerAdmitted ||
       !g_nativeImageGuard.enabled || !g_imagePreservingPresent)return;
    const auto now=GetTickCount64();if(now<next)return;next=now+1000;++attempts;
    const HWND window=g_renderSizeHwnd;
    DWORD pid=0;const DWORD thread=GetWindowThreadProcessId(window,&pid);
    const auto framework=uint32_t(GetWindowLongPtrW(window,GWLP_USERDATA));
    // FrameworkWin is also the selected-view receiver. Mode3 legitimately
    // installs our immutable private table before this observer runs. Accept
    // only the exact clone prepared from FC8930, never the other view class.
    const auto ownedFrameworkTable=uint32_t(reinterpret_cast<uintptr_t>(g_nativeViewTables[0].Table()));
    const auto read=[](uint32_t address,uint32_t& value) {
        MEMORY_BASIC_INFORMATION region{};
        if(VirtualQuery(reinterpret_cast<const void*>(address),&region,sizeof(region))!=sizeof(region) ||
           region.State!=MEM_COMMIT || (region.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
        const auto end=reinterpret_cast<uintptr_t>(region.BaseAddress)+region.RegionSize;
        if(end<address || end-address<4)return false;
        SIZE_T copied=0;
        return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),&value,4,&copied) && copied==4;
    };
    d2vr_menu_native::Snapshot first,second;
    d2vr_menu_native::Refusal refusal;
    DWORD afterPid=0;
    if(thread && pid==GetCurrentProcessId() &&
       d2vr_menu_native::Observe(uint32_t(reinterpret_cast<uintptr_t>(window)),framework,read,first,ownedFrameworkTable,&refusal) &&
       d2vr_menu_native::Observe(uint32_t(reinterpret_cast<uintptr_t>(window)),framework,read,second,ownedFrameworkTable,&refusal) && first==second &&
       GetWindowThreadProcessId(window,&afterPid)==thread && afterPid==pid &&
       uint32_t(GetWindowLongPtrW(window,GWLP_USERDATA))==framework) {
        complete=true;
        Log("[NativeMenuReceiver] hwnd=%p thread=%lu framework=%08X client=%08X input=%08X inputTable=%08X mouse=%08X directInput=%08X GetDeviceState=%08X GetDeviceData=%08X; read-only stable snapshot; lifetimeLease=0 menuState=UNKNOWN surface=UNKNOWN injection=OFF",
            window,thread,first.framework,first.client,first.input,first.inputTable,first.mouse,first.device,first.getState,first.getData);
    } else if(attempts==8)
        Log("[NativeMenuReceiver] no stable receiver after eight bounded observations; stage=%s address=%08X readable=%d observed=%08X expected=%08X hwnd=%p thread=%lu pid=%lu framework=%08X ownedTable=%08X; injection=OFF",
            refusal.stage,refusal.address,int(refusal.readable),refusal.observed,refusal.expected,
            window,thread,pid,framework,ownedFrameworkTable);
#endif
}

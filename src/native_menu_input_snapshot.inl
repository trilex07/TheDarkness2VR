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
    DWORD afterPid=0;
    if(thread && pid==GetCurrentProcessId() &&
       d2vr_menu_native::Observe(uint32_t(reinterpret_cast<uintptr_t>(window)),framework,read,first,ownedFrameworkTable) &&
       d2vr_menu_native::Observe(uint32_t(reinterpret_cast<uintptr_t>(window)),framework,read,second,ownedFrameworkTable) && first==second &&
       GetWindowThreadProcessId(window,&afterPid)==thread && afterPid==pid &&
       uint32_t(GetWindowLongPtrW(window,GWLP_USERDATA))==framework) {
        complete=true;
        Log("[NativeMenuReceiver] hwnd=%p thread=%lu framework=%08X client=%08X input=%08X mouse=%08X directInput=%08X GetDeviceState=%08X GetDeviceData=%08X; read-only stable snapshot; lifetimeLease=0 menuState=UNKNOWN surface=UNKNOWN injection=OFF",
            window,thread,first.framework,first.client,first.input,first.mouse,first.device,first.getState,first.getData);
    } else if(attempts==8)
        Log("[NativeMenuReceiver] no stable receiver after eight bounded observations; injection=OFF");
#endif
}

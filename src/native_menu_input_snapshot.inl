// Bounded one-shot receiver observation for the next consolidated manual test.
// No native calls, writes, factories, cursor movement or process scanning.
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
       d2vr_menu_native::Observe(uint32_t(reinterpret_cast<uintptr_t>(window)),framework,read,first) &&
       d2vr_menu_native::Observe(uint32_t(reinterpret_cast<uintptr_t>(window)),framework,read,second) && first==second &&
       GetWindowThreadProcessId(window,&afterPid)==thread && afterPid==pid &&
       uint32_t(GetWindowLongPtrW(window,GWLP_USERDATA))==framework) {
        complete=true;
        Log("[NativeMenuReceiver] hwnd=%p thread=%lu framework=%08X client=%08X input=%08X mouse=%08X directInput=%08X GetDeviceState=%08X GetDeviceData=%08X; read-only stable snapshot; lifetimeLease=0 menuState=UNKNOWN surface=UNKNOWN injection=OFF",
            window,thread,first.framework,first.client,first.input,first.mouse,first.device,first.getState,first.getData);
    } else if(attempts==8)
        Log("[NativeMenuReceiver] no stable receiver after eight bounded observations; injection=OFF");
#endif
}

// Opt-in Main-menu coordinate correction at the borrowed native movie's event
// transform. No OS cursor/clamp, input decoder, button or viewport mutation.
#include "native_view_code_clone.h"
static std::atomic<bool> g_nativeFlashCursorInstalled{false};
static std::atomic<uint32_t> g_flashCursorMovie{0};
static std::atomic<uint64_t> g_flashCursorXY{0},g_flashCursorTime{0};
using FlashTransform=void(__thiscall*)(void*,int*,int*);
static bool FlashRead(uint32_t address,uint32_t& value) {
    SIZE_T bytes=0;
    return d2vr_menu_native::Pointer(address) &&
        ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,4,&bytes) && bytes==4;
}
static bool FlashRayPoint(uintptr_t movie,d2vr_flash_cursor::Point& point) {
    if(!g_nativeFlashRayCursor || !g_nativeFlashCursorInstalled.load(std::memory_order_acquire) || !ControllerMenuEnabled() ||
       !(NativeVrMenuFlags()&d2vr_action_movies::Main) || GetForegroundWindow()!=g_renderSizeHwnd)return false;
    DWORD pid=0;
    if(GetWindowThreadProcessId(g_renderSizeHwnd,&pid)!=GetCurrentThreadId() || pid!=GetCurrentProcessId())return false;
    d2vr_menu_native::TopMovieSnapshot top;
    if(!d2vr_menu_native::ObserveTopMovie(FlashRead,top) || !top.hasMovie ||
       top.topMovie!=movie || !top.movie.visible || !top.movie.topmost || !top.cursorRequested)return false;
    d2vr_menu_native::Snapshot input;
    const auto framework=uint32_t(GetWindowLongPtrW(g_renderSizeHwnd,GWLP_USERDATA));
    const auto owned=uint32_t(reinterpret_cast<uintptr_t>(g_nativeViewTables[0].Table()));
    if(!d2vr_menu_native::Observe(uint32_t(reinterpret_cast<uintptr_t>(g_renderSizeHwnd)),framework,
                                FlashRead,input,owned))return false;
    uint32_t ref=0,driverRef=0,actual=0,acquired=0,player=0;
    if(!FlashRead(uint32_t(movie)+0x7C,ref) || ref!=input.inputRef ||
       !FlashRead(top.movie.container+0x48,driverRef) || driverRef!=ref ||
       !FlashRead(uint32_t(movie)+0x30,player) || !d2vr_menu_native::Pointer(player) ||
       !FlashRead(input.mouse+4,acquired) || !uint8_t(acquired>>24))return false; // mouse+7
    MenuSurface source;
    AcquireSRWLockShared(&g_menuSurfaceLock);source=g_menuSurface;ReleaseSRWLockShared(&g_menuSurfaceLock);
    if(!source.flashTexture)return false; // no invented projected pause/HUD mapping
    d2vr_menu::Surface surface;d2vr_menu::Hit hit;
    if(!ControllerMenuHit(source,surface,hit))return false;
    uint32_t fields[10]{};
    for(unsigned i=0;i<10;++i)if(!FlashRead(uint32_t(movie)+0x48+4*i,fields[i]))return false;
    d2vr_flash_cursor::Movie dimensions;
    dimensions.width=int(fields[0]);dimensions.height=int(fields[1]);
    dimensions.offsetX=int(fields[2]);dimensions.offsetY=int(fields[3]);
    dimensions.viewW=int(fields[4]);dimensions.viewH=int(fields[5]);
    memcpy(&dimensions.scaleX,&fields[6],4);memcpy(&dimensions.scaleY,&fields[7],4);
    if(fields[8]!=fields[4] || fields[9]!=fields[5] ||
       !FlashRead(top.manager+0x3D4,actual) || actual!=fields[4] ||
       !FlashRead(top.manager+0x3D8,actual) || actual!=fields[5])return false;
    return d2vr_flash_cursor::Map(source.flashImage,dimensions,hit.u,hit.v,point);
}
static bool NativeFlashCursorReady() {
    const auto time=g_flashCursorTime.load(std::memory_order_acquire),now=GetTickCount64();
    if(!time || now<time || now-time>150)return false;
    d2vr_flash_cursor::Point point;
    if(!FlashRayPoint(g_flashCursorMovie.load(std::memory_order_relaxed),point))return false;
    const auto xy=g_flashCursorXY.load(std::memory_order_relaxed);
    return std::abs(int64_t(point.x)-int(uint32_t(xy)))<=2 &&
        std::abs(int64_t(point.y)-int(uint32_t(xy>>32)))<=2;
}
static __declspec(noinline) void __fastcall NativeFlashCursorHook(void* movie,void*,int* x,int* y) {
    const auto original=[](uintptr_t object,int* px,int* py) {
        reinterpret_cast<FlashTransform>(0x6D56B0)(reinterpret_cast<void*>(object),px,py);
    };
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const bool applied=d2vr_flash_cursor::Transform(g_nativeFlashRayCursor,
        caller,reinterpret_cast<uintptr_t>(movie),x,y,original,FlashRayPoint);
    if(applied) {
        g_flashCursorMovie.store(uint32_t(reinterpret_cast<uintptr_t>(movie)),std::memory_order_relaxed);
        g_flashCursorXY.store(uint64_t(uint32_t(*x))|(uint64_t(uint32_t(*y))<<32),std::memory_order_relaxed);
        g_flashCursorTime.store(GetTickCount64(),std::memory_order_release);
        static ULONGLONG next=0;const auto now=GetTickCount64();
        if(now>=next){next=now+1000;Log("[NativeFlashRayCursor] event movie=%p flash=(%d,%d); submitted texture ray; HWND clamp untouched",movie,*x,*y);}
    } else if(caller==0xCB727C && uint32_t(reinterpret_cast<uintptr_t>(movie))==g_flashCursorMovie.load()) {
        g_flashCursorTime.store(0,std::memory_order_release);
    }
}
static bool InstallNativeFlashCursor() {
    if(g_nativeFlashCursorInstalled.load(std::memory_order_acquire))return true;
    static bool attempted=false;if(!g_nativeFlashRayCursor || attempted)return false;attempted=true;
    auto slot=reinterpret_cast<void* volatile*>(0xFDA6E8+0x148);
    bool valid=false;
    __try {
        valid=*slot==reinterpret_cast<void*>(0x6D56B0) && *(uint32_t*)(0xFDA6E8+0x80)==0xCB70C0 &&
            d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0x6D56B0),0xB9,"a9e058beb6f77c70f5a36ad6b7e32c6030f63c02230d70f2f94874954be1c792") &&
            d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0xCB70C0),0x291,"e17dff6e0f5ab7232943a73943c1a3588e8ebd735e95fe040d50c2a26b7cebcd") &&
            d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0x591650),0x58,"6ae74f091f5ad4fcbbd483fef25dadc06726d5541e827d4fd17751f251f88e51") &&
            d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0xCEDC30),0x5D,"a0ef759f22b528a5d641546324725454c025e3742b4424d6b07dee21b90bd5ef");
    } __except(EXCEPTION_EXECUTE_HANDLER) {valid=false;}
    if(!valid || !NativeImageWriteAllowed(reinterpret_cast<uintptr_t>(slot),4)) {
        Log("[NativeFlashRayCursor] REFUSED: transform/updater/event/viewport contract");return false;
    }
    HMODULE self=nullptr;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCSTR>(&NativeFlashCursorHook),&self))return false;
    DWORD old=0;if(!VirtualProtect(const_cast<void**>(slot),4,PAGE_READWRITE,&old))return false;
    void* prior=InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(&NativeFlashCursorHook),reinterpret_cast<void*>(0x6D56B0));
    DWORD ignored=0;const bool restored=VirtualProtect(const_cast<void**>(slot),4,old,&ignored)!=FALSE;
    if(prior!=reinterpret_cast<void*>(0x6D56B0) || !restored) {
        if(prior==reinterpret_cast<void*>(0x6D56B0)) {
            DWORD writable=0;
            if(VirtualProtect(const_cast<void**>(slot),4,PAGE_READWRITE,&writable)) {
                InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(0x6D56B0),reinterpret_cast<void*>(&NativeFlashCursorHook));
                VirtualProtect(const_cast<void**>(slot),4,old,&ignored);
            }
            if(*slot==reinterpret_cast<void*>(&NativeFlashCursorHook))RaiseFailFastException(nullptr,nullptr,0);
        }
        Log("[NativeFlashRayCursor] REFUSED: slot publication/protection");return false;
    }
    // Atomic pointer publication; callback/module and forwarding target remain
    // valid for process lifetime. No loader-time join, allocation or unhook.
    g_nativeFlashCursorInstalled.store(true,std::memory_order_release);
    Log("[NativeFlashRayCursor] active: Main quad -> CB727C event transform; no OS motion/clamp writes");
    return true;
}

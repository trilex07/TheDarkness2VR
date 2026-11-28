// Opt-in Main-menu coordinate correction at the native movie's raw input
// recipient, using a mouse instance published under its current native poll.
// No EXE/static-vtable, OS cursor/clamp, button or viewport mutation.
#include "native_view_code_clone.h"
#include "native_view_instance.h"
#include "native_menu_mouse_poll.h"
#include "native_menu_external_mouse.h"
static std::atomic<bool> g_nativeFlashCursorInstalled{false};
static d2vr_native_view::InstanceTable g_nativeFlashMouseTable;
static d2vr_menu_external::ExternalMouse g_nativeFlashMouseAPI;
static std::atomic<uint32_t> g_flashCursorMovie{0};
static std::atomic<uint64_t> g_flashCursorXY{0},g_flashCursorTime{0};
static bool NativeFlashCursorInstalled() {
    return g_nativeFlashCursorInstalled.load(std::memory_order_acquire);
}
using FlashTransform=void(__thiscall*)(void*,int*,int*);
static bool FlashRead(uint32_t address,uint32_t& value) {
    SIZE_T bytes=0;
    return d2vr_menu_native::Pointer(address) &&
        ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,4,&bytes) && bytes==4;
}
static bool FlashRayPoint(uintptr_t movie,d2vr_flash_cursor::Point& point,
                          d2vr_flash_cursor::Point* raw=nullptr,uint32_t borrowedMouse=0) {
    if(!g_nativeFlashRayCursor || !ControllerMenuEnabled() ||
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
                                FlashRead,input,owned,nullptr,
                                uint32_t(reinterpret_cast<uintptr_t>(g_nativeFlashMouseTable.Table()))) ||
       (borrowedMouse && input.mouse!=borrowedMouse))return false;
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
    if(!d2vr_flash_cursor::Map(source.flashImage,dimensions,hit.u,hit.v,point))return false;
    return !raw || d2vr_flash_cursor::MapRaw(source.flashImage,dimensions,hit.u,hit.v,*raw);
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
        caller,reinterpret_cast<uintptr_t>(movie),x,y,original,
        [](uintptr_t object,d2vr_flash_cursor::Point& point){return FlashRayPoint(object,point);});
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
// 43E160 tail-jumps this getter: the native caller remains CB7259 and its
// outputs are exactly the active movie's E0/E4. Native6D56B0 still transforms
// those raw viewport coordinates once afterwards. Other callers are untouched.
static __declspec(noinline) void __fastcall NativeFlashMousePosition(void* mouse,void*,int* x,int* y) {
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    using Getter=void(__thiscall*)(void*,int*,int*);
    reinterpret_cast<Getter>(0x603190)(mouse,x,y);
    const auto error=GetLastError();
    if(caller==0xCB7259 && x && y && reinterpret_cast<uintptr_t>(x)>=0x100E0 &&
       reinterpret_cast<uintptr_t>(y)==reinterpret_cast<uintptr_t>(x)+4 &&
       g_nativeFlashMouseTable.Owns(mouse)) {
        const auto movie=reinterpret_cast<uintptr_t>(x)-0xE0;
        d2vr_flash_cursor::Point point,raw;
        if(FlashRayPoint(movie,point,&raw,uint32_t(reinterpret_cast<uintptr_t>(mouse)))) {
            *x=raw.x;*y=raw.y;
            g_flashCursorMovie.store(uint32_t(movie),std::memory_order_relaxed);
            g_flashCursorXY.store(uint64_t(uint32_t(point.x))|(uint64_t(uint32_t(point.y))<<32),std::memory_order_relaxed);
            g_flashCursorTime.store(GetTickCount64(),std::memory_order_release);
            static ULONGLONG nextLog=0;const auto now=GetTickCount64();
            if(now>=nextLog) {
                nextLog=now+1000;
                Log("[NativeFlashRayCursor] CB7259 mouse=%p movie=%p raw=(%d,%d) flash=(%d,%d); native transform follows",mouse,reinterpret_cast<void*>(movie),raw.x,raw.y,point.x,point.y);
            }
        } else g_flashCursorTime.store(0,std::memory_order_release);
    }
    SetLastError(error);
}
static HRESULT STDMETHODCALLTYPE NativeFlashMouseDataImpl(void* device,uint32_t bytes,void* events,
        uint32_t* count,uint32_t flags,uint32_t caller,uint32_t owner) {
    const auto entryError=GetLastError();
    const auto next=g_nativeFlashMouseAPI.Native();
    // Published before hook enable; an unreachable missing-trampoline case
    // refuses without making any guessed COM/native call.
    if(!next)return E_UNEXPECTED;
    d2vr_menu_native::MousePollCall call{caller,owner,uint32_t(reinterpret_cast<uintptr_t>(device)),bytes,
        uint32_t(reinterpret_cast<uintptr_t>(events)),uint32_t(reinterpret_cast<uintptr_t>(count)),flags};
    const auto hwnd=uint32_t(reinterpret_cast<uintptr_t>(g_renderSizeHwnd));
    const auto ownTable=uint32_t(reinterpret_cast<uintptr_t>(g_nativeFlashMouseTable.Table()));
    DWORD pid=0;
    const bool ownerThread=g_renderSizeHwnd && GetWindowThreadProcessId(g_renderSizeHwnd,&pid)==GetCurrentThreadId() &&
        pid==GetCurrentProcessId();
    DWORD error=entryError;
    // The owner is borrowed only on this native poll stack. Publish an immutable
    // process-lifetime table to THIS heap instance, never retain its address.
    // No coordinates, event buffers, counts, button bits or native tables change.
    const auto result=d2vr_menu_native::ForwardBorrowedMouse(call,hwnd,ownTable,ownerThread,FlashRead,
        [&](){SetLastError(entryError);const auto r=next(device,bytes,events,count,flags);error=GetLastError();return r;},
        [&](uint32_t borrowed){
            if(g_nativeFlashMouseTable.Publish(reinterpret_cast<void*>(borrowed),g_nativeImageGuard)) {
                static bool reported=false;
                if(!reported){reported=true;Log("[NativeFlashRayCursor] native9A5E70 owner lease published private mouse getter; no retained instance/static table write");}
            }
        });
    SetLastError(error);return result;
}
D2VR_MOUSE_DATA_GATE(NativeFlashMouseData,NativeFlashMouseDataImpl)

static bool InstallNativeFlashCursor() {
    if(NativeFlashCursorInstalled())return true;
    static bool attempted=false;if(!g_nativeFlashRayCursor || attempted)return false;attempted=true;
    bool valid=false;
    __try {
        valid=*(uint32_t*)(0xFDA6E8+0x148)==0x6D56B0 && *(uint32_t*)(0xFDA6E8+0x80)==0xCB70C0 &&
            d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0x6D56B0),0xB9,"a9e058beb6f77c70f5a36ad6b7e32c6030f63c02230d70f2f94874954be1c792") &&
            d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0xCB70C0),0x291,"e17dff6e0f5ab7232943a73943c1a3588e8ebd735e95fe040d50c2a26b7cebcd") &&
            d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0x591650),0x58,"6ae74f091f5ad4fcbbd483fef25dadc06726d5541e827d4fd17751f251f88e51") &&
            d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0xCEDC30),0x5D,"a0ef759f22b528a5d641546324725454c025e3742b4424d6b07dee21b90bd5ef") &&
            d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0xFC00C4),124,"6f3f8082e34befa5937ca3ba6855ebcd4804f00302611276480265065609d0c7") &&
            d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0x603190),0x1B,"e9c127f7d17c85159bc87d51c7aec652cb00fec8ab9114a7072931508bb045f2") &&
            d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0x43E160),0x17,"ace6e45967527155cce8426c6b949ac6dbd9e4e37d4eeb0b6dbc7b27e6418764") &&
            d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0x9A5D90),0x17A,"67e02e71a4e562cd5c8ce6e55708d4e1c3f15e65495187ffe9be3724cc170515") &&
            d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0x821400),0x79,"abd98e7652244d6eb61ebac0a5f879044829789a86845f257631fca3f07511a5");
    } __except(EXCEPTION_EXECUTE_HANDLER) {valid=false;}
    if(!valid) {
        Log("[NativeFlashRayCursor] REFUSED: transform/updater/event/viewport contract");return false;
    }
    d2vr_menu_native::Snapshot input;
    const auto framework=uint32_t(GetWindowLongPtrW(g_renderSizeHwnd,GWLP_USERDATA));
    const auto owned=uint32_t(reinterpret_cast<uintptr_t>(g_nativeViewTables[0].Table()));
    if(!d2vr_menu_native::Observe(uint32_t(reinterpret_cast<uintptr_t>(g_renderSizeHwnd)),framework,
        FlashRead,input,owned) || !g_nativeFlashMouseTable.Prepare(reinterpret_cast<void**>(0xFC00C8),30,27,
        reinterpret_cast<void*>(&NativeFlashMousePosition),g_nativeImageGuard) ||
        !g_nativeFlashMouseAPI.Start(reinterpret_cast<d2vr_menu_external::GetData>(input.getData),
        NativeFlashMouseData,g_nativeImageGuard)) {
        Log("[NativeFlashRayCursor] REFUSED: external Steam COM method/private owner table admission");return false;
    }
    g_nativeFlashCursorInstalled.store(true,std::memory_order_release);
    Log("[NativeFlashRayCursor] external Steam COM route ready: borrowed9A5E70 -> private mouse getter -> CB7259 raw event; native transforms/buttons/OS clamp unchanged");
    return true;
}

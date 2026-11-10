// CPU-only production context/callback + actual local x86 native bodies.
#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <limits>
#include "../src/controller_menu_pointer.h"
#include "../src/native_menu_input_snapshot.h"
#include "../src/native_flash_cursor.h"
#include "../src/native_view_code_clone.h"
#include "fixture_contract.h"
static unsigned checks=0,events=0;
static void Check(bool condition,const char* what) {
    ++checks;if(!condition){std::fprintf(stderr,"FAIL %s check=%u\n",what,checks);ExitProcess(2);}
}
static void __fastcall QueueEvent(void*,void*,const void* event) {
    Check(*static_cast<const unsigned char*>(event)==15,"native MOVE event type15");++events;
}
static std::vector<unsigned char> Read(const char* file) {
    std::ifstream input(file,std::ios::binary);Check(bool(input),"fixture exists");
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
static void* Code(const char* file,bool transform) {
    auto bytes=Read(file);Check(bytes.size()==(transform?0xB9u:0x58u),"fixture length");
    Check(d2vr_native_clone::MatchesDigest(bytes.data(),bytes.size(),transform?
        "a9e058beb6f77c70f5a36ad6b7e32c6030f63c02230d70f2f94874954be1c792":
        "6ae74f091f5ad4fcbbd483fef25dadc06726d5541e827d4fd17751f251f88e51"),"actual body digest");
    auto storage=static_cast<unsigned char*>(VirtualAlloc(nullptr,
        65536,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));Check(storage!=nullptr,"CPU RX storage");
    auto code=storage;std::memcpy(code,bytes.data(),bytes.size());
    if(transform) {
        for(auto& c:constants) {
            const auto address=uint32_t(reinterpret_cast<uintptr_t>(&c.bits));
            std::memcpy(code+c.offset,&address,4);
        }
    } else {
        const auto displacement=uint32_t(reinterpret_cast<uintptr_t>(&QueueEvent))-uint32_t(reinterpret_cast<uintptr_t>(code+0x52));
        std::memcpy(code+0x4e,&displacement,4);
    }
    DWORD previous=0;Check(VirtualProtect(storage,65536,PAGE_EXECUTE_READ,&previous)!=FALSE,"CPU seal RX");
    FlushInstructionCache(GetCurrentProcess(),code,bytes.size());return code;
}
static bool g_nativeFlashRayCursor=true,controllerEnabled=true;
using FixtureTransform=void(__thiscall*)(void*,int*,int*);
static FixtureTransform g_fixtureTransform=nullptr;
static HWND g_renderSizeHwnd=reinterpret_cast<HWND>(0x9998),foreground=g_renderSizeHwnd;
static XrSpace g_xrSpace=(XrSpace)0x7770;
static unsigned g_eyeCacheW=1280,g_eyeCacheH=1400;
struct Hand {XrPosef aim{};};static Hand g_hands[2];
struct OwnedTable {void* Table(){return reinterpret_cast<void*>(0x44440);}};static OwnedTable g_nativeViewTables[1];
static bool ControllerMenuEnabled(){return controllerEnabled;}
namespace d2vr_action_movies {enum {Main=1};}
static unsigned phase=1;static unsigned NativeVrMenuFlags(){return phase;}
static uint64_t tick=10000;
static bool ownThread=true;
static uintptr_t testCaller=0xCB727C;
static std::unordered_map<uint32_t,uint32_t> memory;
static BOOL TestRead(HANDLE,LPCVOID source,LPVOID target,SIZE_T n,SIZE_T* copied) {
    const auto it=memory.find(uint32_t(reinterpret_cast<uintptr_t>(source)));
    if(n!=4 || it==memory.end()){if(copied)*copied=0;return FALSE;}
    std::memcpy(target,&it->second,4);if(copied)*copied=4;return TRUE;
}
static HWND TestForeground(){return foreground;}
static DWORD TestThread(HWND hwnd,DWORD* pid) {
    if(pid)*pid=hwnd==g_renderSizeHwnd?GetCurrentProcessId():0;
    return ownThread?GetCurrentThreadId():GetCurrentThreadId()+1;
}
static LONG_PTR TestWindowData(HWND,int){return 0x1d000;}
static uint64_t TestTick(){return tick;}
static void* TestCaller(){return reinterpret_cast<void*>(testCaller);}
static void Log(const char*,...){}
#include "actual_menu_surface.h"
static MenuSurface g_menuSurface;
static SRWLOCK g_menuSurfaceLock=SRWLOCK_INIT;
static unsigned g_srcW=3345,g_srcH=3410,g_scW=1280,g_scH=1400;
struct Frame {uint64_t id=1;};static Frame g_trackedFrame;
struct Settings {void AnchorMenu(const XrPosef&,float,float,XrSpace){}};static Settings g_vrSettings;
#define ReadProcessMemory TestRead
#define GetForegroundWindow TestForeground
#define GetWindowThreadProcessId TestThread
#undef GetWindowLongPtrW
#define GetWindowLongPtrW TestWindowData
#define GetTickCount64 TestTick
#define _ReturnAddress TestCaller
#include "actual_record_menu_surface.inl"
#include "actual_controller_hit.inl"
#include "actual_flash_callback.inl"
#undef ReadProcessMemory
#undef GetForegroundWindow
#undef GetWindowThreadProcessId
#undef GetWindowLongPtrW
#undef GetTickCount64
#undef _ReturnAddress
alignas(16) static unsigned char movieStorage[0x200]{},playerStorage[0x200]{};
static uint32_t MovieAddress(){return uint32_t(reinterpret_cast<uintptr_t>(movieStorage));}
static void Set(uint32_t address,uint32_t value){memory[address]=value;}
static void Dimensions(d2vr_flash_cursor::Movie d) {
    uint32_t fields[]={uint32_t(d.width),uint32_t(d.height),uint32_t(d.offsetX),uint32_t(d.offsetY),
        uint32_t(d.viewW),uint32_t(d.viewH),0,0,uint32_t(d.viewW),uint32_t(d.viewH)};
    std::memcpy(fields+6,&d.scaleX,4);std::memcpy(fields+7,&d.scaleY,4);
    std::memcpy(movieStorage+0x48,fields,sizeof(fields));
    for(unsigned i=0;i<10;++i)Set(MovieAddress()+0x48+4*i,fields[i]);
    Set(0x10040+0x3D4,d.viewW);Set(0x10040+0x3D8,d.viewH);
}
static d2vr_flash_cursor::Movie Measured(){return {1120,584,3345,3410,0,0,1120.f/3345,584.f/3410};}
static void Ray(double u,double v) {
    g_hands[1].aim.orientation={0,0,0,1};
    g_hands[1].aim.position={float(2*(u-.5)),float(2*(.5-v)),0};
}
static void Setup() {
    memory.clear();g_nativeFlashRayCursor=controllerEnabled=ownThread=true;
    g_nativeFlashCursorInstalled=true;foreground=g_renderSizeHwnd;phase=1;testCaller=0xCB727C;
    g_flashCursorMovie=0;g_flashCursorTime=0;
    const auto m=MovieAddress();
    Set(0x10B389C,0x10000);Set(0x10000,0x10040);Set(0x10040,0xFDA948);Set(0xFDA948+0xA8,0xA0B120);
    Set(0x10040+0x58,0x12000);Set(0x10040+0x5C,0x12004);Set(0x12000,0x13000);Set(0x13000,m);
    Set(m,0xFDA6E8);Set(0xFDA6E8+0xFC,0x76D5C0);Set(0xFDA6E8+0x104,0x6E4250);Set(0xFDA6E8+0x130,0xB64910);
    Set(m+0x78,0x100);Set(m+0xC,0x13000);Set(m+0x44,0x14000);Set(0x14000+0x44,0x10000);
    Set(m+0xF4,1);Set(m+0x7C,0x15000);Set(0x14000+0x48,0x15000);
    Set(m+0x30,uint32_t(reinterpret_cast<uintptr_t>(playerStorage)));
    Set(0x15000,0x16000);Set(0x16000,0xFBFCD0);Set(0x16000+0xA44,0x17000);Set(0x17000,0x18000);
    Set(0x18000,0xFC00C8);Set(0x18000+4,0x01000000);Set(0x18000+0x1A4,0x19000);Set(0x19000,0x1A000);
    Set(0x1A000+0x24,0x1B000);Set(0x1A000+0x28,0x1C000);
    Set(0x1D000,0x44440);Set(0x1D000+0xF8,0x9998);Set(0x1D000+0xA8,0x1E000);Set(0x1E000,0x1F000);Set(0x1F000+0x374,0x15000);
    Dimensions(Measured());
    g_menuSurface={};g_menuSurface.valid=g_menuSurface.submitted=g_menuSurface.flashTexture=true;
    g_menuSurface.frame=1;g_menuSurface.space=g_xrSpace;g_menuSurface.time=tick;
    g_menuSurface.pose.orientation={0,0,0,1};g_menuSurface.pose.position={0,0,-1.5f};
    g_menuSurface.width=g_menuSurface.height=2;g_menuSurface.flashImage={3345,3410,0,0,1,1};
    Ray(.595,.871);
}
static d2vr_flash_cursor::Point Hook() {
    int x=761,y=1071;NativeFlashCursorHook(movieStorage,nullptr,&x,&y);return {x,y};
}
static void Refuses(const char* what) {
    int x=761,y=1071;g_fixtureTransform(movieStorage,&x,&y);
    const auto p=Hook();Check(p.x==x && p.y==y,what);Check(!NativeFlashCursorReady(),"no confirm after refusal");
}
int main(int argc,char** argv) {
    Check(argc>=3,"usage transform.bin position.bin [--legacy]");
    const d2vr_native_image::Guard protectedProfile{true,0x400000,0xDEC000};
    Check(!protectedProfile.Allows(0xFDA6E8+0x148,4),"production guard rejects prototype static table publication");
    Check(!protectedProfile.Allows(0x6D56B0,5),"production guard rejects transform code patch too");
    // Exact method-byte pins do not admit writes inside the main image.
    // Guard is interval-based; it does not inspect a matched prologue.
    Check(!protectedProfile.Allows(0x603190,5),"pinned603190 detour remains forbidden");
    Check(!protectedProfile.Allows(0x603190,0x1b),"full pinned603190 body remains main image");
    Check(!protectedProfile.Allows(0x603190-5,80),"external interceptor span rejects603190");
    Check(!protectedProfile.Allows(0x43E160,5),"input getter detour also forbidden");
    Check(!protectedProfile.Allows(0xCB7259,5),"caller detour also forbidden");
    Check(protectedProfile.Allows(0x20000000,80),"external span admitted; not an always-refuse mock");
    const d2vr_native_image::Guard legacyProfile{false,0x400000,0xDEC000};
    Check(legacyProfile.Allows(0x603190,5),"only guard OFF admits native span; not authorized");
    g_fixtureTransform=reinterpret_cast<FixtureTransform>(Code(argv[1],true));using Position=void(__thiscall*)(void*,int,int,int);
    const auto position=reinterpret_cast<Position>(Code(argv[2],false));
    Setup();d2vr_flash_cursor::Point correct;
    Check(d2vr_flash_cursor::Map(g_menuSurface.flashImage,Measured(),.595,.871,correct),"measured map");
    Check(correct.x==666 && correct.y==509,"render extent -> measured movie extent");
    if(argc==4) {
        int x=761,y=1071;g_fixtureTransform(movieStorage,&x,&y);
        std::printf("RED legacy measured=(%d,%d) wanted=(%d,%d)\n",x,y,correct.x,correct.y);
        return x==correct.x && y==correct.y?0:1;
    }
    auto p=Hook();Check(p.x==666 && p.y==509,"actual production callback overrides Windows-clamped input");
    Check(NativeFlashCursorReady(),"actual production confirm ready");
    std::memset(playerStorage,0,sizeof(playerStorage));events=0;
    position(playerStorage,p.x,p.y,0x35);Check(events==1,"actual consumer queues MOVE");
    Check(*reinterpret_cast<int*>(playerStorage+0x48)==666 && *reinterpret_cast<int*>(playerStorage+0x4c)==509,"actual consumer receives ray point");
    Check(*reinterpret_cast<int*>(playerStorage+0x50)==0x35,"native buttons preserved");
    position(playerStorage,p.x,p.y,0xA6);Check(events==1,"unchanged point no extra MOVE");
    Check(*reinterpret_cast<int*>(playerStorage+0x50)==0xA6,"native button-only transition preserved");
    g_nativeFlashRayCursor=false;Refuses("default OFF preserves native result");Setup();
    controllerEnabled=false;Refuses("tracking/context refusal");Setup();
    phase=2;Refuses("Title not Main");Setup();
    phase=0;Refuses("world not Main");Setup();
    testCaller=0xCB727D;Refuses("other transform call passthrough");Setup();
    foreground=reinterpret_cast<HWND>(0x8898);Refuses("foreground refusal");Setup();
    ownThread=false;Refuses("owner thread refusal");Setup();
    Set(MovieAddress()+0x78,0);Refuses("invisible movie refusal");Setup();
    Set(MovieAddress()+0xF4,0);Refuses("pad movie refusal");Setup();
    Set(0x13000,0x20000);Refuses("replaced movie refusal");Setup();
    Set(MovieAddress()+0x7C,0x20000);Refuses("wrong input owner refusal");Setup();
    Set(0x14000+0x48,0x20000);Refuses("wrong driver owner refusal");Setup();
    Set(0x18000+4,0);Refuses("unacquired mouse refusal");Setup();
    g_menuSurface.submitted=false;Refuses("not submitted refusal");Setup();
    g_menuSurface.flashTexture=false;Refuses("projected pause/HUD refusal");Setup();
    g_menuSurface.time=tick-151;Refuses("stale surface refusal");Setup();
    g_menuSurface.space=(XrSpace)0x7780;Refuses("recentered space refusal");Setup();
    g_menuSurface.flashImage.height=3411;Refuses("resize waits for matching submitted texture");Setup();
    Set(0x10040+0x3D8,1000);Refuses("manager viewport disagreement");Setup();
    Set(MovieAddress()+0x68,1000);Refuses("viewport copy disagreement");Setup();
    auto d=Measured();d.offsetY=20;Dimensions(d);
    // Original fallback itself now includes that native offset.
    p=Hook();Check(p.x==255 && p.y==185,"unproved letterbox preserves native conversion");Setup();
    // Compare the SAME production float math against the complete native SSE
    // body over resize, endpoints and nontrivial crop. No game/runtime loaded.
    unsigned comparisons=0;
    for(const auto size:std::vector<POINT>{{3345,3410},{2676,2728},{1280,720},{1920,1080},{3840,2160}}) {
        d=Measured();d.viewW=size.x;d.viewH=size.y;d.scaleX=float(d.width)/size.x;d.scaleY=float(d.height)/size.y;Dimensions(d);
        for(int y=0;y<=100;++y)for(int x=0;x<=100;++x) {
            d2vr_flash_cursor::Point raw{x*(size.x-1)/100,y*(size.y-1)/100},expected=raw;
            g_fixtureTransform(movieStorage,&expected.x,&expected.y);
            const auto mapped=d2vr_flash_cursor::Convert(raw,d);
            Check(mapped.x==expected.x && mapped.y==expected.y,"production/native SSE rounding equality");++comparisons;
        }
    }
    Setup();XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
    quad.pose=g_menuSurface.pose;quad.size={2,2};quad.space=g_xrSpace;
    quad.subImage.imageRect={{128,280},{1024,840}};
    RecordMenuSurface(quad,true);g_menuSurface.submitted=true;Ray(.25,.75);p=Hook();
    Check(p.x==336 && p.y==380,"actual cropped quad UV restored to full movie UV");
    Check(g_menuSurface.flashImage.width==3345 && g_menuSurface.flashImage.height==3410,"actual renderer source extent not eye output");
    RecordMenuSurface(quad);Check(!g_menuSurface.flashTexture,"actual projected plane has no texture claim");
    quad.subImage.imageRect.offset.x=-1;RecordMenuSurface(quad,true);Check(!g_menuSurface.flashTexture,"actual producer rejects bad crop");
    Setup();d=Measured();d.viewW=2676;d.viewH=2728;d.scaleX=1120.f/2676;d.scaleY=584.f/2728;Dimensions(d);
    Refuses("actual resized movie before resized texture refuses");
    g_srcW=2676;g_srcH=2728;quad.subImage.imageRect={{0,0},{1280,1400}};
    RecordMenuSurface(quad,true);g_menuSurface.submitted=true;p=Hook();
    Check(p.x==666 && p.y==509,"actual new submitted size restores correct event point");
    g_srcW=3345;g_srcH=3410;Setup();
    d2vr_flash_cursor::Point unchanged{123,456};auto image=g_menuSurface.flashImage;
    Check(!d2vr_flash_cursor::Map(image,Measured(),-0.01,.5,unchanged) && unchanged.x==123,"outside ray untouched");
    image.x=.5;image.w=.6;Check(!d2vr_flash_cursor::Map(image,Measured(),.5,.5,unchanged),"invalid crop refusal");
    image=g_menuSurface.flashImage;d=Measured();d.scaleY=std::numeric_limits<float>::quiet_NaN();
    Check(!d2vr_flash_cursor::Map(image,d,.5,.5,unchanged),"NaN scale refusal");
    // Physical window/clip has NO parameter in the event-space mapper. Exercise
    // the production callback with the same render content at output resizes.
    Setup();for(auto width:{640u,1280u,1934u,2676u})for(auto height:{584u,720u,1071u,1400u}) {
        g_eyeCacheW=width;g_eyeCacheH=height;p=Hook();Check(p.x==666 && p.y==509,"XR output/window independent");
    }
    std::printf("PASS %u checks; %u actual native transform comparisons; actual event consumer/buttons; production callback ownership refusals\n",checks,comparisons);
    return 0;
}

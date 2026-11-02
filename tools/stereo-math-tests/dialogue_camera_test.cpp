#include <windows.h>
#include <intrin.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "../../src/native_view_instance.h"
#define D2VR_MATCHED_CAPTURE
static bool g_dialogueCameraComfort=true;
static void* admitted=nullptr;
static bool NativeAimOwnerMatches(void* p){return p==admitted;}
static void Log(const char*,...){}
namespace d2vr_native_clone {static bool MatchesDigest(void*,size_t,const char*){return true;}}
#include "../../src/dialogue_camera.inl"
static unsigned forwarded=0;
static void __fastcall Original(void*,void*,const float*){++forwarded;}
static void* camera=nullptr;
static void* __fastcall GetCamera(void*,void*){return camera;}
static void Check(bool ok,const char* message){if(!ok){std::printf("FAIL %s\n",message);std::exit(1);}}
static void Map(uintptr_t address){Check(VirtualAlloc(reinterpret_cast<void*>(address),0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE)!=nullptr,"fixture mapping");}
static void Jump(uintptr_t from,void* target){auto b=reinterpret_cast<unsigned char*>(from);b[0]=0xe9;*reinterpret_cast<int32_t*>(b+1)=int32_t(reinterpret_cast<uintptr_t>(target)-from-5);}
static void Site(uintptr_t address,uintptr_t wrapper){
    auto b=reinterpret_cast<unsigned char*>(address-9);
    const unsigned char code[]={0xff,0x74,0x24,0x04,0xe8,0,0,0,0,0xc2,4,0};
    std::memcpy(b,code,sizeof(code));*reinterpret_cast<int32_t*>(b+5)=int32_t(wrapper-address);
}
int main(){
    Map(0xaf7000);Map(0x9f2000);Map(0x9d9000);
    Jump(0x9d9120,reinterpret_cast<void*>(Original));
    // Exact original 9F2770..9F278D instructions, including both native pushes.
    const unsigned char wrapper[]={0x56,0x8b,0xf1,0x8b,0x06,0x8b,0x90,0xf4,2,0,0,0xff,0xd2,
      0x8b,0x4c,0x24,8,0x8b,0x10,0x8b,0x92,0x8c,0,0,0,0x51,0x8b,0xc8,0xff,0xd2,
      0x5e,0xc2,4,0};
    std::memcpy(reinterpret_cast<void*>(0x9f2770),wrapper,sizeof(wrapper));
    Site(0xaf7a2b,0x9f2770);Site(0xaf7afb,0x9f2770);Site(0xaf7d09,0x9f2770);
    void* cameraTable[71]{};cameraTable[0x8c/4]=reinterpret_cast<void*>(DialogueCameraAngles);
    void* cameraObject=cameraTable;camera=&cameraObject;admitted=camera;
    void* avatarTable[190]{};avatarTable[0x2f4/4]=reinterpret_cast<void*>(GetCamera);
    void* avatar=&avatarTable;
    g_dialogueCamera.store(DWORD(reinterpret_cast<uintptr_t>(camera)));
    float angles[3]={.5f,1.f,.1f};
    using Call=void(__thiscall*)(void*,const float*);
    auto first=reinterpret_cast<Call>(0xaf7a2b-9),second=reinterpret_cast<Call>(0xaf7afb-9);
    first(&avatar,angles);second(&avatar,angles);
    Check(forwarded==0 && g_dialogueBlocks==2,"both conversation branches blocked by actual compiled hook");
    reinterpret_cast<Call>(0xaf7d09-9)(&avatar,angles);
    Check(forwarded==1,"unmatched caller forwarded");
    DialogueCameraAngles(camera,nullptr,angles);
    Check(forwarded==2,"direct native setter caller forwarded");
    g_dialogueCameraComfort=false;first(&avatar,angles);
    Check(forwarded==3,"disabled feature forwarded");g_dialogueCameraComfort=true;
    g_dialogueCamera.store(0);first(&avatar,angles);
    Check(forwarded==4,"previous selected camera forwarded");
    g_dialogueCamera.store(DWORD(reinterpret_cast<uintptr_t>(camera)));admitted=nullptr;
    first(&avatar,angles);Check(forwarded==5,"invalid owner forwarded");
    d2vr_native_image::Guard guard{true,0x400000,0xdc0000};
    void* source[73]{};for(unsigned i=0;i<73;++i)source[i]=reinterpret_cast<void*>(0x500000+i*16);
    d2vr_native_view::InstanceTable table;
    Check(table.Prepare(source+1,71,39,reinterpret_cast<void*>(0x20000000),guard,35,reinterpret_cast<void*>(0x20000100)),"two-slot immutable table prepared");
    Check(table.Table()[-1]==source[0],"native table header retained");
    for(unsigned i=0;i<71;++i)Check(table.Table()[i]==(i==39?reinterpret_cast<void*>(0x20000000):i==35?reinterpret_cast<void*>(0x20000100):source[i+1]),"all other native methods unchanged");
    Check(!table.Prepare(source+1,71,39,reinterpret_cast<void*>(0x20000000),guard,35,reinterpret_cast<void*>(0x20000200)),"different immutable plan rejected");
    d2vr_native_view::InstanceTable invalid;
    Check(!invalid.Prepare(source+1,71,39,reinterpret_cast<void*>(1),guard,71,reinterpret_cast<void*>(2)),"out of bounds second slot rejected");
    Check(!invalid.Prepare(source+1,71,39,reinterpret_cast<void*>(1),guard,39,reinterpret_cast<void*>(2)),"duplicate second slot rejected");
    std::puts("PASS dialogue actual x86 caller chain, forwarding, selection and immutable two-slot table");
}

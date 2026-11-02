#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <cstdio>
#include "../../src/native_view_instance.h"
using WorldMarkerProjectMethod=float*(__thiscall*)(void*,float*,const float*);
static WorldMarkerProjectMethod g_worldMarkerProjectOriginal=nullptr;
static std::atomic<uint64_t> g_worldMarkerCorrected{0},g_worldMarkerRefused{0};
static struct {uint64_t id=1;bool open=true;}g_trackedFrame;
static uint64_t g_nativeCoveredFrame=1;
static DWORD g_renderThreadId=0;
static bool g_frameEyeParamsValid=true;
static struct {bool headGeometry=true;}g_frameEyeParams[2];
static bool correct=false,nullResult=false;
static unsigned calls=0,corrections=0;
static void Log(const char*,...){}
static bool CorrectWorldMarkerProjection(void*,const float*,float* out) {
    ++corrections;if(correct)out[0]+=100;return correct;
}
#include "world_marker_hook_actual.inl"
struct Movie {void** table;};
static float* __fastcall Original(void* self,void*,float* output,const float* world) {
    assert(self && world && world[0]==1 && world[1]==2 && world[2]==3);
    ++calls;if(nullResult)return nullptr;
    output[0]=12;output[1]=34;output[2]=56;return output;
}
int main() {
    static_assert(sizeof(void*)==4,"actual x86 ABI required");
    void* original[90];for(auto& p:original)p=reinterpret_cast<void*>(&Original);
    original[0]=reinterpret_cast<void*>(0x12345678);
    void* before[90];memcpy(before,original,sizeof(before));
    d2vr_native_image::Guard guard;guard.enabled=true;guard.base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(guard.base);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS*>(guard.base+dos->e_lfanew);
    guard.bytes=nt->OptionalHeader.SizeOfImage;
    d2vr_native_view::InstanceTable table;
    assert(table.Prepare(original+1,89,0x110/4,reinterpret_cast<void*>(&WorldMarkerProjectHook),guard));
    Movie* movie=static_cast<Movie*>(VirtualAlloc(nullptr,sizeof(Movie),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    assert(movie);movie->table=original+1;assert(table.Publish(movie,guard));
    assert(!memcmp(before,original,sizeof(before)));
    assert(movie->table[-1]==original[0] && movie->table[0]==original[1] && movie->table[88]==original[89]);
    g_worldMarkerProjectOriginal=reinterpret_cast<WorldMarkerProjectMethod>(&Original);
    auto method=reinterpret_cast<WorldMarkerProjectMethod>(movie->table[0x110/4]);
    const float world[3]={1,2,3};float out[3]{};
    assert(method(movie,out,world)==out && calls==1 && corrections==1);
    assert(out[0]==12 && out[1]==34 && out[2]==56);
    correct=true;
    assert(method(movie,out,world)==out && calls==2 && corrections==2);
    assert(out[0]==112 && out[1]==34 && out[2]==56);
    nullResult=true;
    assert(method(movie,out,world)==nullptr && calls==3 && corrections==2);
    assert(g_worldMarkerCorrected==1 && g_worldMarkerRefused==2);
    VirtualFree(movie,0,MEM_RELEASE);
    puts("PASS actual x86 movie vtable ABI/return/native-once/null/depth forwarding; source table and destructor unchanged");
}

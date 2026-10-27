#include <windows.h>
#include <intrin.h>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "../src/native_constant_source.h"
#include "../src/small_record_copy.h"
using IDirect3DDevice9=void;
using namespace d2vr_native_constants;
static uintptr_t apiReturn,callbackReturn,parentReturn;
static const float* overrideSource;
static unsigned firstRegister=0,vectorCount=4;
static float recorded[16],forwarded[16];
static bool copied=false;
static unsigned checks=0;
static void Check(bool condition,const char* name) {
    ++checks;if(!condition){std::printf("FAIL %s\n",name);std::exit(1);}
}
static void ClassifierFailures() {
    auto* page=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Check(page!=nullptr,"fixture allocation");
    const uintptr_t low=reinterpret_cast<uintptr_t>(page),high=low+4096,frame=low+0x900;
    auto* slot=reinterpret_cast<uintptr_t*>(frame-0x794-48);
    auto* source=reinterpret_cast<float*>(frame-0x794+64);
    Sites sites{{11,12},{21,22},{31,32},{{41,0,0},{42,0,0}}};
    slot[0]=12;slot[1]=0;slot[2]=0;slot[3]=reinterpret_cast<uintptr_t>(source);slot[4]=4;
    slot[5]=22;slot[6]=0;slot[7]=slot[3];slot[8]=4;
    reinterpret_cast<uintptr_t*>(frame)[1]=42;
    auto inspect=[&] {return Proof::Inspect(slot,frame,source,64,Stage::Pixel,low,high,sites);};
    Check(inspect().Matches(source,64),"classifier exact frame");
    for(unsigned field: {0u,2u,3u,4u,5u,6u,7u,8u}) {
        const auto saved=slot[field];slot[field]=~uintptr_t(0);
        Check(!inspect().Matches(source,64),"each dispatch/argument mismatch rejects");slot[field]=saved;
    }
    reinterpret_cast<uintptr_t*>(frame)[1]=43;
    Check(!inspect().Matches(source,64),"unknown native parent rejects");
    reinterpret_cast<uintptr_t*>(frame)[1]=42;
    Check(!Proof::Inspect(slot,frame+16,source,64,Stage::Pixel,low,high,sites).Matches(source,64),"shifted frame rejects");
    Check(!Proof::Inspect(slot,frame,source,63,Stage::Pixel,low,high,sites).Matches(source,63),"non-vector extent rejects");
    auto proof=inspect();Check(!proof.Matches(source+1,64)&&!proof.Matches(source,48),"proof binds exact payload");
    DWORD old=0;Check(VirtualProtect(page,4096,PAGE_NOACCESS,&old)!=FALSE,"fault fixture protection");
    Check(!Copy(proof,recorded,source,64)&&!admitted.load()&&faulted.load()&&counts.faults==1,"violated contract disables admission and returns fallback");
    admitted=true;Check(!proof.Matches(source,64),"fault latch survives attempted readmission");
    VirtualProtect(page,4096,old,&old);VirtualFree(page,0,MEM_RELEASE);faulted=false;admitted=true;
    auto* guard=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    Check(guard!=nullptr,"guard fixture allocation");
    VirtualProtect(guard,4096,PAGE_READWRITE|PAGE_GUARD,&old);
    Proof none;Check(!Copy(none,recorded,guard,64),"unproven guard never direct copied");
    d2vr_record_copy::Copy(recorded,guard,64,true,&ReadProcessMemory);
    MEMORY_BASIC_INFORMATION mbi{};VirtualQuery(guard,&mbi,sizeof(mbi));
    Check((mbi.Protect&PAGE_GUARD)!=0,"fallback preserves foreign source guard");
    VirtualFree(guard,0,MEM_RELEASE);
}
static Proof InspectFixture(const uintptr_t* slot,uintptr_t frame,const void* p,size_t n,Stage stage) {
    Sites sites{};
    for(unsigned b=0;b<2;++b) {
        sites.apiReturn[b]=apiReturn;sites.builderFirst[b]=callbackReturn;
        sites.builderEnd[b]=callbackReturn+1;sites.parentReturn[b][0]=parentReturn;
    }
    const auto* tib=reinterpret_cast<NT_TIB*>(NtCurrentTeb());
    return Proof::Inspect(slot,frame,p,n,stage,reinterpret_cast<uintptr_t>(tib->StackLimit),
        reinterpret_cast<uintptr_t>(tib->StackBase),sites);
}
static bool g_recActive=true;
static int g_replayEye=-1,g_activePsInverse=-1,g_activePsViewExtents=-1,g_submitMode=0;
static int OP_SetPSConstF=1;
static void ObserveInverseProjection(const float*,UINT,UINT,int){}
static int MakeEyeParams(int){return 0;}
static bool CompensatePsConstants(float*,const float*,UINT,UINT,int,int,int){return false;}
static void RecEmitData(int,const void* data,size_t bytes,UINT,UINT,int,int,const Proof& proof) {
    copied=Copy(proof,recorded,data,bytes);
    if(!copied)d2vr_record_copy::Copy(recorded,data,bytes,true,&ReadProcessMemory);
}
static HRESULT STDMETHODCALLTYPE Forward(void*,UINT,const float* data,UINT count) {
    // The original API is a separate consumer, after recorder capture.
    if(data && count==4)std::memcpy(forwarded,data,sizeof(forwarded));
    return S_OK;
}
static struct {decltype(&Forward) SetPixelShaderConstantF=Forward;} g_orig;
#define D3D_OK S_OK
// Generated directly from the production PS hook. Only the policy-site lookup
// is redirected to fixture addresses; EBP/return-slot capture is unchanged.
#include "actual_native_constant_hook.inl"

__declspec(naked) static void Wrapper() {
    __asm {
        mov edx,[esp+12]
        push edx
        mov edx,[esp+12]
        push edx
        mov edx,[esp+12]
        push edx
        push ecx
        lea eax,afterApi
        mov apiReturn,eax
        call Hook_SetPSConstF
    afterApi:
        ret 12
    }
}
__declspec(naked) static void Builder() {
    __asm {
        push ebp
        mov ebp,esp
        and esp,0xfffffff0
        sub esp,0x794
        push ebx
        push esi
        push edi
        lea edi,[esp+12+0x100]
        mov ecx,16
        mov eax,0x3f800000
    fill:
        stosd
        add eax,1
        loop fill
        lea eax,[esp+12+0x100]
        cmp overrideSource,0
        je useLocal
        mov eax,overrideSource
    useLocal:
        push vectorCount
        push eax
        push firstRegister
        xor ecx,ecx
        lea eax,afterCallback
        mov callbackReturn,eax
        call Wrapper
    afterCallback:
        pop edi
        pop esi
        pop ebx
        mov esp,ebp
        pop ebp
        ret
    }
}
__declspec(naked) static void Parent() {
    __asm {
        lea eax,afterBuilder
        mov parentReturn,eax
        call Builder
    afterBuilder:
        ret
    }
}
int main() {
    enabled=true;admitted=true;
    for(unsigned i=0;i<1000;++i) {
        copied=false;Parent();Check(copied,"actual hook accepts active native-shaped local frame");
        Check(std::memcmp(recorded,forwarded,sizeof(recorded))==0,"recorder and forwarded payload bits match");
    }
    float heapLike[16]{};overrideSource=heapLike;Parent();
    Check(!copied,"foreign frame source rejects fast copy");overrideSource=nullptr;
    firstRegister=223;Parent();Check(!copied,"invalid register extent rejects");firstRegister=0;
    enabled=false;Parent();Check(!copied,"disabled rejects");enabled=true;
    admitted=false;Parent();Check(!copied,"unadmitted image rejects");admitted=true;
    Check(!Proof::Inspect(nullptr,0,nullptr,64,Stage::Pixel,1,2).Matches(nullptr,64),"invalid bounds reject");
    Proof invalid;Check(!Copy(invalid,recorded,forwarded,sizeof(recorded)),"default proof rejects");
    ClassifierFailures();
    Check(!AdmitImage("wrong",nullptr),"wrong image SHA rejects");
    std::printf("PASS actual x86 hook/native-shaped producer ABI: %u checks; copies=%llu injectedFaults=%llu\n",checks,counts.copies,counts.faults);
}

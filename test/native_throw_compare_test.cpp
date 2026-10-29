#define NOMINMAX
#include <windows.h>
#include <xmmintrin.h>
#include <cstdio>
#include <cstdlib>
#include "../src/native_throw_compare.h"
struct __declspec(align(16)) State {DWORD regs[9];DWORD padding[3];BYTE floating[512];};
struct __declspec(align(16)) Fire {BYTE padding[128];float velocity[4];};
using Run=uintptr_t(__cdecl*)(void*,void*,State*,State*,uintptr_t*);
static unsigned checks=0,calls=0;
static bool modify=false;
static Fire fire;
static void Check(bool v){++checks;if(!v){printf("FAIL %u\n",checks);exit(1);}}
static bool Resolve(void* self,float* velocity,const void* characterReference) {
    ++calls;Check(self==&fire && velocity==fire.velocity);
    Check(characterReference==reinterpret_cast<const void*>(0x45678901));
    if(modify){velocity[0]=4;velocity[1]=0;velocity[2]=3;}
    SetLastError(99);_mm_setcsr(_mm_getcsr()^0x6000);
    __asm fld1
    __asm pxor xmm0,xmm0
    __asm pxor xmm1,xmm1
    __asm pxor xmm2,xmm2
    __asm pxor xmm3,xmm3
    __asm pxor xmm4,xmm4
    __asm pxor xmm5,xmm5
    __asm pxor xmm6,xmm6
    __asm pxor xmm7,xmm7
    return modify;
}
int main(int argc,char** argv) {
    Check(argc==2);auto module=LoadLibraryA(argv[1]);Check(module!=nullptr);
    auto original=reinterpret_cast<d2vr_throw_compare::Compare>(GetProcAddress(module,"Original"));
    auto run=reinterpret_cast<Run>(GetProcAddress(module,"InvokeKnown"));Check(original&&run);
    auto count=reinterpret_cast<LONG(__cdecl*)()>(GetProcAddress(module,"OriginalCount"));Check(count!=nullptr);
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    auto pe=reinterpret_cast<const IMAGE_NT_HEADERS32*>(reinterpret_cast<const BYTE*>(module)+dos->e_lfanew);
    d2vr_native_image::Guard guard{true,reinterpret_cast<uintptr_t>(module),pe->OptionalHeader.SizeOfImage};
    Check(!d2vr_throw_compare::Bridge::Prepare(original,Resolve,{}));
    Check(d2vr_throw_compare::Bridge::Prepare(original,Resolve,guard));
    Check(d2vr_throw_compare::Bridge::Prepare(original,Resolve,guard));
    for(unsigned mode=0;mode<2;++mode)for(unsigned n=0;n<1000;++n) {
        State beforeNative{},afterNative{},beforeBridge{},afterBridge{};uintptr_t caller=0;
        fire.velocity[0]=3;fire.velocity[1]=4;fire.velocity[2]=0;fire.velocity[3]=.75f;
        SetLastError(0x1234);
        Check(run(&fire,reinterpret_cast<void*>(original),&beforeNative,&afterNative,&caller)==0x5a5a1234);
        Check(GetLastError()==0x1234);
        modify=mode!=0;SetLastError(0x1234);
        Check(run(&fire,d2vr_throw_compare::Bridge::Entry(),&beforeBridge,&afterBridge,&caller)==0x5a5a1234);
        Check(GetLastError()==0x1234);
        Check(!memcmp(afterNative.regs,afterBridge.regs,sizeof(afterNative.regs)));
        // Native comparison loads the (possibly steered) velocity into XMM1.
        // Every other floating-point register/control/status word must match.
        Check(!memcmp(afterNative.floating,afterBridge.floating,176));
        Check(!memcmp(afterNative.floating+192,afterBridge.floating+192,96));
        Check(!memcmp(afterBridge.floating+176,fire.velocity,16));
        Check(fire.velocity[0]==(modify?4.f:3.f)&&fire.velocity[1]==(modify?0.f:4.f));
        Check(fire.velocity[2]==(modify?3.f:0.f)&&fire.velocity[3]==.75f);
    }
    Check(calls==2000);
    Check(count()==4000);
    puts("PASS private throw comparison:2000 native-vs-adapter calls,fire EDI,stack arguments/return,GPR/EFLAGS,x87/SSE/MXCSR,LastError,native velocity consumption");
}

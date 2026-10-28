#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include <cstdint>
#include <cstring>
struct __declspec(align(16)) State {DWORD regs[9];DWORD padding[3];BYTE floating[512];};
static __declspec(align(16)) DWORD pattern[4]={0x12345678,0x87654321,0xabcdef01,0x0f1e2d3c};
static __declspec(align(16)) BYTE incoming[512];
static __declspec(align(16)) float recorded[16];
static uintptr_t args[6]{};static unsigned fireCalls=0,compareCalls=0;
extern "C" __declspec(dllexport) const void* __cdecl Result(unsigned index) {
    if(index==0)return incoming;if(index==1)return recorded;if(index==2)return args;
    if(index==3)return &fireCalls;return &compareCalls;
}
static void __cdecl RecordFire(void* self,void* weapon,const float* matrix,uintptr_t a,uintptr_t b,uintptr_t c) {
    _fxsave(incoming);args[5]=GetLastError();args[0]=reinterpret_cast<uintptr_t>(self);args[1]=reinterpret_cast<uintptr_t>(weapon);
    args[2]=a;args[3]=b;args[4]=c;std::memcpy(recorded,matrix,64);++fireCalls;
    SetLastError(0x87654321);
}
extern "C" __declspec(dllexport) __declspec(naked) void __fastcall OriginalFire(void*,void*,void*,const float*,uintptr_t,uintptr_t,uintptr_t) {
    __asm {
        push ebp
        mov ebp,esp
        push dword ptr [ebp+24]
        push dword ptr [ebp+20]
        push dword ptr [ebp+16]
        push dword ptr [ebp+12]
        push dword ptr [ebp+8]
        push ecx
        call RecordFire
        add esp,24
        mov esp,ebp
        pop ebp
        mov eax,5a5a1234h
        push 247h
        popfd
        ret 20
    }
}
extern "C" __declspec(dllexport) __declspec(naked) int __cdecl OriginalCompare(const float*,const float*,float) {
    __asm {
        push ebp
        mov ebp,esp
        and esp,-16
        sub esp,50h // exact production comparison prologue, no EXE loaded
        inc compareCalls
        mov edx,[ebp+12]
        movaps xmm1,[edx]
        mov eax,5a5a1234h
        mov esp,ebp
        pop ebp
        push 247h
        popfd
        ret
    }
}
extern "C" __declspec(dllexport) __declspec(naked) uintptr_t __cdecl InvokeModel(void*,const float*,float*,State*) {
    __asm {
        push ebp
        mov ebp,esp
        push ebx
        push esi
        push edi
        sub esp,544
        and esp,-16
        mov [ebp-16],esp
        fxsave [esp]
        fldpi
        movaps xmm0,pattern
        movaps xmm1,pattern
        movaps xmm2,pattern
        movaps xmm3,pattern
        movaps xmm4,pattern
        movaps xmm5,pattern
        movaps xmm6,pattern
        movaps xmm7,pattern
        push 3f000000h
        push dword ptr [ebp+16]
        push dword ptr [ebp+12]
        mov eax,12345678h
        mov ebx,23456789h
        mov ecx,34567890h
        mov edx,45678901h
        mov esi,56789012h
        mov edi,67890123h
        call dword ptr [ebp+8]
        lea esp,[esp+12]
        pushfd
        pushad
        mov esi,esp
        mov edi,[ebp+20]
        mov ecx,9
        rep movsd
        add edi,12
        fxsave [edi]
        popad
        popfd
        mov ecx,[ebp-16]
        fxrstor [ecx]
        lea esp,[ebp-12]
        pop edi
        pop esi
        pop ebx
        mov esp,ebp
        pop ebp
        ret
    }
}
extern "C" __declspec(dllexport) __declspec(naked) uintptr_t __cdecl InvokeFire(void*,void*,void*,const float*,State*) {
    __asm {
        push ebp
        mov ebp,esp
        push ebx
        push esi
        push edi
        sub esp,544
        and esp,-16
        mov [ebp-16],esp
        fxsave [esp]
        fldpi
        movaps xmm0,pattern
        movaps xmm1,pattern
        movaps xmm2,pattern
        movaps xmm3,pattern
        movaps xmm4,pattern
        movaps xmm5,pattern
        movaps xmm6,pattern
        movaps xmm7,pattern
        push 33333333h
        push 22222222h
        push 11111111h
        push dword ptr [ebp+20]
        push dword ptr [ebp+16]
        mov ecx,[ebp+12]
        mov ebx,23456789h
        mov esi,45678901h
        mov edi,56789012h
        call dword ptr [ebp+8]
        pushfd
        pushad
        mov esi,esp
        mov edi,[ebp+24]
        mov ecx,9
        rep movsd
        add edi,12
        fxsave [edi]
        popad
        popfd
        mov ecx,[ebp-16]
        fxrstor [ecx]
        lea esp,[ebp-12]
        pop edi
        pop esi
        pop ebx
        mov esp,ebp
        pop ebp
        ret
    }
}

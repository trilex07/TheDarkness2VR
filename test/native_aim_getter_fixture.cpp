#include <windows.h>
#include <cstdint>
struct __declspec(align(16)) State {DWORD regs[9];DWORD padding[3];BYTE floating[512];};
extern "C" __declspec(dllexport) __declspec(naked) float* __fastcall Original(void*,void*) {
    __asm lea eax,[ecx+80h]
    __asm ret
}
static __declspec(align(16)) DWORD pattern[4]={0x12345678,0x87654321,0xabcdef01,0x0f1e2d3c};
extern "C" __declspec(dllexport) __declspec(naked) float* __cdecl InvokeKnown(void*,void*,State*,State*,uintptr_t*) {
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
        mov eax,[ebp+24]
        lea edx,returned
        mov [eax],edx
        fldpi
        movaps xmm0,pattern
        movaps xmm1,pattern
        movaps xmm2,pattern
        movaps xmm3,pattern
        movaps xmm4,pattern
        movaps xmm5,pattern
        movaps xmm6,pattern
        movaps xmm7,pattern
        mov eax,12345678h
        mov ebx,23456789h
        mov ecx,[ebp+8]
        mov edx,34567890h
        mov esi,45678901h
        mov edi,56789012h
        push 247h
        popfd
        pushfd
        pushad
        mov esi,esp
        mov edi,[ebp+16]
        mov ecx,9
        rep movsd
        add edi,12
        fxsave [edi]
        popad
        popfd
        call dword ptr [ebp+12]
    returned:
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
extern "C" __declspec(dllexport) __declspec(naked) float* __cdecl InvokeOther(void*,void*) {
    __asm mov ecx,[esp+4]
    __asm call dword ptr [esp+8]
    __asm ret
}

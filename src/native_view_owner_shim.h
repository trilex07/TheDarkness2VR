#pragma once
#if defined(_MSC_VER) && defined(_M_IX86)
// Replaces only private-copy FF D0 / 8B 08 / 83 39 00 at985F75.
// Forward the original thiscall(out-reference) once, observe while its native
// strong reference is live, then replay MOV ECX,[EAX]; CMP [ECX],0 exactly.
#define D2VR_VIEW_OWNER_SHIM(name,helper) \
static void __declspec(naked) name() { \
    __asm push dword ptr [esp+4] \
    __asm call eax \
    __asm pushfd \
    __asm pushad \
    __asm mov eax,esp \
    __asm sub esp,528 \
    __asm and esp,0xfffffff0 \
    __asm mov [esp+512],eax \
    __asm fxsave [esp] \
    __asm fninit \
    __asm push dword ptr [eax+28] \
    __asm push dword ptr [eax+16] \
    __asm call helper \
    __asm add esp,8 \
    __asm fxrstor [esp] \
    __asm mov esp,[esp+512] \
    __asm popad \
    __asm popfd \
    __asm mov ecx,[eax] \
    __asm cmp dword ptr [ecx],0 \
    __asm ret 4 \
}
#endif

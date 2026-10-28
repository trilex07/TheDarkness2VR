#pragma once
#if defined(_MSC_VER) && defined(_M_IX86)
// Replaces only private-copy FF D0 / 8B 08 / 83 39 00 at985F75.
// +174 returns the scene, NOT the local player. Preserve its original receiver
// (RegionMgr) as well as the live scene reference for the selected-player query.
// Forward thiscall(out-reference) once; replay MOV ECX,[EAX]; CMP [ECX],0.
#define D2VR_VIEW_OWNER_SHIM(name,helper) \
static void __declspec(naked) name() { \
    __asm push ecx \
    __asm push dword ptr [esp+8] \
    __asm call eax \
    __asm pushfd \
    __asm pushad \
    __asm mov eax,esp \
    __asm sub esp,528 \
    __asm and esp,0xfffffff0 \
    __asm mov [esp+512],eax \
    __asm fxsave [esp] \
    __asm fninit \
    __asm push dword ptr [eax+36] \
    __asm push dword ptr [eax+28] \
    __asm push dword ptr [eax+16] \
    __asm call helper \
    __asm add esp,12 \
    __asm fxrstor [esp] \
    __asm mov esp,[esp+512] \
    __asm popad \
    __asm popfd \
    __asm add esp,4 \
    __asm mov ecx,[eax] \
    __asm cmp dword ptr [ecx],0 \
    __asm ret 4 \
}
#endif

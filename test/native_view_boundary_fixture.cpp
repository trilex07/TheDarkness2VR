#include <windows.h>
#include <intrin.h>
static volatile LONG calls=0;
static uintptr_t caller=0;
extern "C" __declspec(dllexport) __declspec(noinline) unsigned __cdecl FixturePadding(unsigned value) {
    for(volatile unsigned i=0;i<64;++i)value=value*33+i;
    return value;
}
extern "C" __declspec(dllexport) __declspec(noinline) BOOL WINAPI FixtureQuery(LARGE_INTEGER* out) {
    caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    InterlockedIncrement(&calls);SetLastError(0x13572468);
    if(!out)return FALSE;
    out->QuadPart=0x123456789abcdefLL;return TRUE;
}
extern "C" __declspec(dllexport) __declspec(naked) BOOL __cdecl InvokeKnown(LARGE_INTEGER*,void*) {
    __asm push esi
    __asm mov esi,[esp+12]
    __asm push dword ptr [esp+8]
    __asm call FixtureQuery
    __asm pop esi
    __asm ret
}
extern "C" __declspec(dllexport) __declspec(naked) BOOL __cdecl InvokeOther(LARGE_INTEGER*,void*) {
    __asm push esi
    __asm mov esi,[esp+12]
    __asm push dword ptr [esp+8]
    __asm call FixtureQuery
    __asm nop
    __asm pop esi
    __asm ret
}
extern "C" __declspec(dllexport) uintptr_t __cdecl FixtureCaller(){return caller;}
extern "C" __declspec(dllexport) LONG __cdecl FixtureCalls(){return calls;}

#pragma once
// Capture the unmodified x86 caller context before a C++ prologue can reuse
// EBX/EBP. Impl is stdcall(device, first, data, count, slot, frame, owner).
// Its ret 28 removes only our arguments. ret 16 retains the public D3D9 ABI.
// No TLS native pointer, executable patch or persistent context is involved.
#define D2VR_CONSTANT_GATE(Name, Impl) \
    __declspec(naked) static HRESULT STDMETHODCALLTYPE Name( \
        IDirect3DDevice9*,UINT,const float*,UINT) { \
        __asm mov eax,esp \
        __asm push ebx \
        __asm push ebp \
        __asm push eax \
        __asm push [eax+16] \
        __asm push [eax+12] \
        __asm push [eax+8] \
        __asm push [eax+4] \
        __asm call Impl \
        __asm ret 16 \
    }

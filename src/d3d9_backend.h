#pragma once
#include <windows.h>
#include <d3d9on12.h>

namespace d2vr_backend {
enum class Mode { Native = 0, System9On12 = 1 };
inline bool ProfileAllowed(Mode mode, bool imageGuard, bool protectedPresent, int submitMode, bool sharedGpuCapture) {
    return mode==Mode::Native || (mode==Mode::System9On12 && imageGuard && protectedPresent &&
        submitMode==4 && sharedGpuCapture);
}
// A successful factory creation alone is not proof that the requested adapter
// uses 9On12. Reject a native device before installing any device hooks.
inline bool DeviceMatches(Mode mode, IDirect3DDevice9* device) {
    if (mode == Mode::Native) return true;
    if (mode != Mode::System9On12 || !device) return false;
    IDirect3DDevice9On12* interop = nullptr;
    const HRESULT hr = device->QueryInterface(__uuidof(IDirect3DDevice9On12), reinterpret_cast<void**>(&interop));
    const bool matches = SUCCEEDED(hr) && interop;
    if (interop) interop->Release();
    return matches;
}
inline HRESULT CreateEx(HMODULE systemD3d9, UINT sdk, Mode mode, IDirect3D9Ex** output) {
    if (!output) return E_POINTER;
    *output = nullptr;
    if (!systemD3d9) return E_INVALIDARG;
    if (mode == Mode::Native) {
        using Create = HRESULT (WINAPI*)(UINT, IDirect3D9Ex**);
        const auto create = reinterpret_cast<Create>(GetProcAddress(systemD3d9, "Direct3DCreate9Ex"));
        return create ? create(sdk, output) : HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
    }
    if (mode != Mode::System9On12) return E_INVALIDARG;
    const auto create = reinterpret_cast<PFN_Direct3DCreate9On12Ex>(
        GetProcAddress(systemD3d9, "Direct3DCreate9On12Ex"));
    if (!create) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
    D3D9ON12_ARGS args{};
    args.Enable9On12 = TRUE;
    // The SYSTEM runtime owns the adapter/device/queues. No wrapper, foreign
    // d3d9.dll, private vtable, deferred caller pointers or EXE-image mutation.
    // An explicit request never silently falls back to the native backend.
    return create(sdk, &args, 1, output);
}
} // namespace d2vr_backend

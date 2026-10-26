// Counterexample for a proposed recorder architecture, not a rendering benchmark.
// A successful Set followed by Get is not an atomic capture of that Set's input.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include "../../src/small_record_copy.h"

static void Require(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s lastError=%lu\n", what, GetLastError());
        ExitProcess(1);
    }
}
static void Ok(HRESULT hr, const char* what) {
    if (hr != D3D_OK) {
        std::fprintf(stderr, "FAIL %s hr=%08lX\n", what, static_cast<unsigned long>(hr));
        ExitProcess(1);
    }
}
static HRESULT Set(IDirect3DDevice9* d, bool vs, const float* p) {
    return vs ? d->SetVertexShaderConstantF(7, p, 1) : d->SetPixelShaderConstantF(7, p, 1);
}
static HRESULT Get(IDirect3DDevice9* d, bool vs, float* p) {
    return vs ? d->GetVertexShaderConstantF(7, p, 1) : d->GetPixelShaderConstantF(7, p, 1);
}
struct Writer {
    IDirect3DDevice9* device;
    IDirect3DStateBlock9* block;
    HANDLE start, done;
    bool vs, useBlock;
    float values[4];
    HRESULT hr = E_PENDING;
};
static DWORD WINAPI WriteOtherState(void* arg) {
    auto& w = *static_cast<Writer*>(arg);
    Require(WaitForSingleObject(w.start, 5000) == WAIT_OBJECT_0, "writer start");
    w.hr = w.useBlock ? w.block->Apply() : Set(w.device, w.vs, w.values);
    Require(SetEvent(w.done) != FALSE, "writer done");
    return 0;
}
static void CheckInterleave(IDirect3DDevice9* device, bool vs, bool useBlock) {
    const float a[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    const float b[4] = { 11.0f, 12.0f, 13.0f, 14.0f };
    // Exactly as in the current recorder, copy the specific call's raw source
    // synchronously while the caller still owns it. Never read it on a worker.
    float owned[4]{};
    d2vr_record_copy::LegacyCopy(owned, a, sizeof(a));
    IDirect3DStateBlock9* block = nullptr;
    Ok(Set(device, vs, b), "prepare other state");
    Ok(device->CreateStateBlock(D3DSBT_ALL, &block), "capture other state");
    Ok(Set(device, vs, a), "original setter");
    Writer writer{ device, block, CreateEventW(nullptr, FALSE, FALSE, nullptr),
        CreateEventW(nullptr, FALSE, FALSE, nullptr), vs, useBlock };
    Require(writer.start && writer.done, "writer events");
    std::memcpy(writer.values, b, sizeof(b));
    HANDLE thread = CreateThread(nullptr, 0, WriteOtherState, &writer, 0, nullptr);
    Require(thread != nullptr, "writer thread");
    Require(SetEvent(writer.start) != FALSE, "allow legal state change");
    Require(WaitForSingleObject(writer.done, 5000) == WAIT_OBJECT_0, "state change completed");
    Require(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0, "writer joined");
    Ok(writer.hr, "other writer");
    float observed[4]{};
    Ok(Get(device, vs, observed), "getter after original setter");
    Require(std::memcmp(observed, b, sizeof(b)) == 0, "Get must return last writer");
    Require(std::memcmp(observed, a, sizeof(a)) != 0, "Get must NOT capture original payload");
    Require(std::memcmp(owned, a, sizeof(a)) == 0, "owned original payload remains exact");
    std::printf("PASS %s %s: setter/getter D3D_OK, getter returns other writer; owned raw snapshot preserved\n",
        vs ? "VS" : "PS", useBlock ? "StateBlock::Apply" : "SetConstantF");
    CloseHandle(thread);
    CloseHandle(writer.done);
    CloseHandle(writer.start);
    block->Release();
}
int main() {
    static_assert(sizeof(void*) == 4, "Match the x86 game ABI");
    wchar_t systemPath[MAX_PATH]{};
    const UINT n = GetSystemDirectoryW(systemPath, MAX_PATH);
    Require(n > 0 && n + 10 < MAX_PATH, "system directory");
    wcscat_s(systemPath, L"\\d3d9.dll");
    // Absolute system module only: no repo/Steam proxy and no runtime detours.
    HMODULE module = LoadLibraryW(systemPath);
    Require(module != nullptr, "load system D3D9");
    wchar_t loaded[MAX_PATH]{};
    Require(GetModuleFileNameW(module, loaded, MAX_PATH) > 0 &&
        _wcsicmp(loaded, systemPath) == 0, "system D3D9 identity");
    using Create = HRESULT (WINAPI*)(UINT, IDirect3D9Ex**);
    auto create = reinterpret_cast<Create>(GetProcAddress(module, "Direct3DCreate9Ex"));
    Require(create != nullptr, "D3D9Ex export");
    IDirect3D9Ex* api = nullptr;
    Ok(create(D3D_SDK_VERSION, &api), "D3D9Ex");
    WNDCLASSW klass{};
    klass.lpfnWndProc = DefWindowProcW;
    klass.hInstance = GetModuleHandleW(nullptr);
    klass.lpszClassName = L"D2vrConstantOwnershipTest";
    Require(RegisterClassW(&klass) != 0, "hidden window class");
    HWND window = CreateWindowW(klass.lpszClassName, L"D2vr offline ownership test", WS_OVERLAPPED,
        0, 0, 8, 8, nullptr, nullptr, klass.hInstance, nullptr);
    Require(window != nullptr, "hidden window");
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth = pp.BackBufferHeight = 8;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    pp.hDeviceWindow = window;
    IDirect3DDevice9Ex* device = nullptr;
    // Exact observed creation flags: HW vertex processing + MULTITHREADED.
    Ok(api->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
        &pp, nullptr, &device), "owned multithreaded device");
    const bool stages[] = { false, true };
    for (bool vs : stages) {
        const float initial[4] = { 20, 21, 22, 23 };
        float actual[4]{};
        Ok(Set(device, vs, initial), "uncontended setter");
        Ok(Get(device, vs, actual), "uncontended getter");
        Require(std::memcmp(initial, actual, sizeof(initial)) == 0, "uncontended identity");
        CheckInterleave(device, vs, false);
        CheckInterleave(device, vs, true);
    }
    device->Release();
    api->Release();
    DestroyWindow(window);
    UnregisterClassW(klass.lpszClassName, klass.hInstance);
    FreeLibrary(module);
    std::puts("PASS 4 native counterexamples; driver Get cannot replace raw recorder capture. No Draw/Present/Steam/OpenXR.");
    return 0;
}

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "../src/composite_shader_patch.h"

#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); std::exit(1); } } while (0)
#ifdef D2VR_BASELINE
static constexpr unsigned kOrdinaryProfileReads = 1;
#else
static constexpr unsigned kOrdinaryProfileReads = 0;
#endif

namespace d2vr_replay {
enum : uint32_t { SI_COMMENT = 0xfffe, SI_END = 0xffff };
#include "actual_shader_helpers.inl" // Extracted from the current src/stereo_replay.inl.

static int g_submitMode = 4;
static bool g_disableDepthOfField = true;
static bool g_disableCompositeHeightFog = false;
static bool g_disableFxaa = true;
static int g_replayEye = -1;
static std::unordered_map<const void*, uint32_t> g_psHashByPtr;
static std::unordered_map<IDirect3DPixelShader9*, int> g_psInverse, g_psViewExtents;
struct RecLockGuard {};
static void Log(const char*, ...) {}

struct FakeShader final : IDirect3DPixelShader9 {
    std::vector<DWORD> code;
    ULONG refs = 1;
    unsigned getFunctionCalls = 0;
    explicit FakeShader(std::vector<DWORD> bytes) : code(std::move(bytes)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG n = --refs; if (!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetFunction(void* data, UINT* bytes) override {
        ++getFunctionCalls;
        if (!bytes) return D3DERR_INVALIDCALL;
        const UINT required = static_cast<UINT>(code.size() * sizeof(DWORD));
        if (data && *bytes < required) return D3DERR_INVALIDCALL;
        if (data) std::memcpy(data, code.data(), required);
        *bytes = required;
        return S_OK;
    }
};

static unsigned profileReads = 0;
static UINT profileValue = 0;
static UINT WINAPI MockProfile(const char*, const char*, INT, const char*) {
    ++profileReads;
    return profileValue;
}
static std::vector<std::vector<DWORD>> forwarded;
static unsigned createCalls = 0;
static bool failNext = false;
static HRESULT createResult = S_OK;
static const DWORD* rejectedInput = nullptr;
static IDirect3DDevice9* lastDevice = nullptr;
static const DWORD* lastInput = nullptr;
static IDirect3DPixelShader9** lastOutput = nullptr;
static HRESULT STDMETHODCALLTYPE FakeCreate(IDirect3DDevice9* device, const DWORD* code,
                                             IDirect3DPixelShader9** out) {
    ++createCalls;
    lastDevice = device; lastInput = code; lastOutput = out;
    // A rejecting native API need not dereference malformed input. The hook
    // must still forward its exact arguments/HRESULT without touching the gap.
    if (code == rejectedInput) {
        if (SUCCEEDED(createResult) && out) {
            *out = new FakeShader({0xffff0300u, 0x0000ffffu});
            g_psHashByPtr[*out] = 0xdecafbadu; // simulate an old identity at a recycled native pointer
        }
        return createResult;
    }
    const size_t n = SafeShaderTokenCount(code);
    CHECK(n > 0);
    forwarded.emplace_back(code, code + n + 1);
    if (failNext) { failNext = false; return D3DERR_INVALIDCALL; }
    if (FAILED(createResult)) return createResult;
    if (out) *out = new FakeShader(forwarded.back());
    return createResult;
}
struct OrigFns {
    HRESULT (STDMETHODCALLTYPE* CreatePixelShader)(IDirect3DDevice9*, const DWORD*, IDirect3DPixelShader9**) = FakeCreate;
};
static OrigFns g_orig;

#define GetPrivateProfileIntA MockProfile
#include "actual_ps_create_hook.inl" // Exact current Hook_CreatePixelShader body.
#undef GetPrivateProfileIntA

// Shader-model-3 bytecode with one comment. Its complete FNV hash is the
// production ShadowScreenDepth selector, so the diagnostic branch is testable
// without publishing a game shader or using a GPU.
static const DWORD target[] = { 0xffff0300u, 0x0002fffeu, 0x2042005eu, 0x00000005u, 0x0000ffffu };
static const DWORD ordinary[] = { 0xffff0300u, 0x0001fffeu, 0x11223344u, 0x0000ffffu };
static const DWORD replacement[] = { 0xffff0300u, 0x02000001u, 0x800f0800u, 0x80e40000u, 0x0000ffffu };

static void ResetFixture() {
    profileReads = createCalls = 0;
    profileValue = 0;
    failNext = false;
    createResult = S_OK;
    rejectedInput = lastInput = nullptr;
    lastDevice = nullptr; lastOutput = nullptr;
    forwarded.clear();
    g_psHashByPtr.clear();
    g_psInverse.clear();
    g_psViewExtents.clear();
}
static void WriteReplacement() {
    CHECK(CreateDirectoryA("test-output", nullptr) || GetLastError() == ERROR_ALREADY_EXISTS);
    FILE* file = nullptr;
    CHECK(fopen_s(&file, "test-output\\shadow_screen_depth.psh", "wb") == 0 && file);
    CHECK(std::fwrite(replacement, sizeof(DWORD), 5, file) == 5);
    CHECK(std::fclose(file) == 0);
}
static void CheckShader(IDirect3DPixelShader9* out, const DWORD* source, size_t count,
                        int inverseBase = -1) {
    CHECK(out != nullptr);
    auto* fake = static_cast<FakeShader*>(out);
    CHECK(fake->refs == 1 && fake->getFunctionCalls == 2);
    CHECK(g_psHashByPtr.at(out) == HashBytes(source, count * sizeof(DWORD)));
    CHECK(g_psInverse.at(out) == inverseBase && g_psViewExtents.at(out) == -1);
    CHECK(out->Release() == 0);
}
static void OrdinaryAndFailure() {
    ResetFixture();
    profileValue = 1;
    IDirect3DPixelShader9* out = nullptr;
    auto* device = reinterpret_cast<IDirect3DDevice9*>(uintptr_t(0x1111));
    CHECK(Hook_CreatePixelShader(device, ordinary, &out) == S_OK);
    CHECK(profileReads == kOrdinaryProfileReads && createCalls == 1 && forwarded[0] == std::vector<DWORD>(ordinary, ordinary + 4));
    CheckShader(out, ordinary, 4);

    ResetFixture();
    out = nullptr;
    createResult = D3DERR_INVALIDCALL;
    CHECK(Hook_CreatePixelShader(device, ordinary, &out) == D3DERR_INVALIDCALL);
    CHECK(out == nullptr && profileReads == kOrdinaryProfileReads && createCalls == 1);
}
static void TargetOverrideAndFallback() {
    CHECK(HashBytes(target, sizeof(target)) == 0xC4D1FB10u);
    ResetFixture();
    IDirect3DPixelShader9* out = nullptr;
    auto* device = reinterpret_cast<IDirect3DDevice9*>(uintptr_t(0x1111));
    CHECK(Hook_CreatePixelShader(device, target, &out) == S_OK);
    CHECK(profileReads == 1 && createCalls == 1 && forwarded[0] == std::vector<DWORD>(target, target + 5));
    CheckShader(out, target, 5);

    WriteReplacement();
    ResetFixture();
    profileValue = 1;
    out = nullptr;
    CHECK(Hook_CreatePixelShader(device, target, &out) == S_OK);
    CHECK(profileReads == 1 && createCalls == 1 && forwarded[0] == std::vector<DWORD>(replacement, replacement + 5));
    CheckShader(out, target, 5, 27);

    ResetFixture();
    profileValue = 1;
    failNext = true;
    out = nullptr;
    CHECK(Hook_CreatePixelShader(device, target, &out) == S_OK);
    CHECK(profileReads == 1 && createCalls == 2);
    CHECK(forwarded[0] == std::vector<DWORD>(replacement, replacement + 5));
    CHECK(forwarded[1] == std::vector<DWORD>(target, target + 5));
    CheckShader(out, target, 5);
}
static void DeviceRecreationAndEyes() {
    ResetFixture();
    IDirect3DPixelShader9* first = nullptr;
    IDirect3DPixelShader9* second = nullptr;
    auto* deviceA = reinterpret_cast<IDirect3DDevice9*>(uintptr_t(0x1111));
    auto* deviceB = reinterpret_cast<IDirect3DDevice9*>(uintptr_t(0x2222));
    g_replayEye = 0;
    CHECK(Hook_CreatePixelShader(deviceA, ordinary, &first) == S_OK);
    g_replayEye = 1;
    createResult = S_FALSE;
    CHECK(Hook_CreatePixelShader(deviceB, ordinary, &second) == S_FALSE);
    CHECK(createCalls == 2 && first != second && profileReads == 2 * kOrdinaryProfileReads);
    CHECK(forwarded[0] == forwarded[1] && forwarded[0] == std::vector<DWORD>(ordinary, ordinary + 4));
    CheckShader(first, ordinary, 4);
    CheckShader(second, ordinary, 4);
}
static void MalformedCommentGap() {
    SYSTEM_INFO system{}; GetSystemInfo(&system);
    const size_t page = system.dwPageSize, pageWords = page / sizeof(DWORD);
    CHECK(pageWords >= 4 && 2 * pageWords + 1 < 8192);
    auto* memory = static_cast<DWORD*>(VirtualAlloc(nullptr, 3 * page,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    CHECK(memory != nullptr);
    memory[0] = 0xffff0300u;
    memory[1] = static_cast<DWORD>((2 * pageWords - 2) << 16) | SI_COMMENT;
    memory[2 * pageWords] = SI_END;
#ifndef D2VR_BASELINE
    CHECK(ValidatedPixelShaderTokenCount(memory) == 2 * pageWords);
#endif
    auto* device = reinterpret_cast<IDirect3DDevice9*>(uintptr_t(0x5555));
    for (DWORD protection : {PAGE_NOACCESS, PAGE_READWRITE | PAGE_GUARD}) {
        DWORD old = 0;
        CHECK(VirtualProtect(memory + pageWords, page, protection, &old));
        // The old boundary-only walker accepts this readable END while skipping
        // the inaccessible comment payload. No test read touches that payload.
        CHECK(SafeShaderTokenCount(memory) == 2 * pageWords);
#ifndef D2VR_BASELINE
        CHECK(ValidatedPixelShaderTokenCount(memory) == 0);
#endif
        ResetFixture();
        rejectedInput = memory; createResult = D3DERR_INVALIDCALL;
        auto* sentinel = reinterpret_cast<IDirect3DPixelShader9*>(uintptr_t(0x7777));
        IDirect3DPixelShader9* out = sentinel;
        CHECK(Hook_CreatePixelShader(device, memory, &out) == D3DERR_INVALIDCALL);
        CHECK(createCalls == 1 && lastDevice == device && lastInput == memory && lastOutput == &out);
        CHECK(out == sentinel && profileReads == kOrdinaryProfileReads && forwarded.empty());
        CHECK(g_psHashByPtr.empty() && g_psInverse.empty() && g_psViewExtents.empty());
        MEMORY_BASIC_INFORMATION region{};
        CHECK(VirtualQuery(memory + pageWords, &region, sizeof(region)) == sizeof(region));
        CHECK(region.Protect == protection); // especially PAGE_GUARD must remain set
#ifndef D2VR_BASELINE
        // Native validation owns its result, even if our source identity is
        // unknown. Metadata may inspect only its validated GetFunction output.
        ResetFixture(); rejectedInput = memory; createResult = S_FALSE;
        out = nullptr;
        CHECK(Hook_CreatePixelShader(device, memory, &out) == S_FALSE);
        CHECK(createCalls == 1 && lastInput == memory && profileReads == 0 && forwarded.empty());
        CHECK(out && !g_psHashByPtr.count(out));
        auto* native = static_cast<FakeShader*>(out);
        CHECK(native->refs == 1 && native->getFunctionCalls == 2);
        CHECK(out->Release() == 0);
        CHECK(VirtualQuery(memory + pageWords, &region, sizeof(region)) == sizeof(region));
        CHECK(region.Protect == protection);
#endif
        CHECK(VirtualProtect(memory + pageWords, page, PAGE_READWRITE, &old));
    }
#ifndef D2VR_BASELINE
    // Also reject an unreadable initial token and END without consuming guard.
    for (size_t blockedPage : {size_t(0), size_t(2)}) {
        DWORD old = 0;
        CHECK(VirtualProtect(memory + blockedPage * pageWords, page, PAGE_READWRITE | PAGE_GUARD, &old));
        ResetFixture(); rejectedInput = memory; createResult = D3DERR_DEVICELOST;
        IDirect3DPixelShader9* out = nullptr;
        CHECK(Hook_CreatePixelShader(device, memory, &out) == D3DERR_DEVICELOST);
        CHECK(createCalls == 1 && lastInput == memory && out == nullptr && profileReads == 0);
        MEMORY_BASIC_INFORMATION region{};
        CHECK(VirtualQuery(memory + blockedPage * pageWords, &region, sizeof(region)) == sizeof(region));
        CHECK(region.Protect == (PAGE_READWRITE | PAGE_GUARD));
        CHECK(VirtualProtect(memory + blockedPage * pageWords, page, PAGE_READWRITE, &old));
    }
#endif
    CHECK(VirtualFree(memory, 0, MEM_RELEASE));
    std::puts("PASS malformed comment gaps: original forwarding/HRESULT and unchanged NOACCESS/GUARD");
}
static void PrivateShader(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    CHECK(bool(file));
    const auto bytes = file.tellg();
    CHECK(bytes > 0 && bytes <= 32768 && bytes % sizeof(DWORD) == 0);
    std::vector<DWORD> original(static_cast<size_t>(bytes) / sizeof(DWORD));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(original.data()), bytes);
    CHECK(bool(file));
    CHECK(SafeShaderTokenCount(original.data()) + 1 == original.size());
    const auto saved = original;
    std::vector<DWORD> patched;
    const unsigned patch = d2vr_composite::Build(original.data(), original.size(), true, false, patched);
    ResetFixture();
    IDirect3DPixelShader9* out = nullptr;
    auto* device = reinterpret_cast<IDirect3DDevice9*>(uintptr_t(0x3333));
    CHECK(Hook_CreatePixelShader(device, original.data(), &out) == S_OK);
    CHECK(createCalls == 1 && profileReads == kOrdinaryProfileReads);
    CHECK(forwarded[0] == (patch ? patched : original));
    CHECK(original == saved);
    CHECK(g_psHashByPtr.at(out) == HashBytes(original.data(), static_cast<size_t>(bytes)));
    auto* fake = static_cast<FakeShader*>(out);
    CHECK(fake->refs == 1 && fake->getFunctionCalls == 2);
    CHECK(g_psInverse.count(out) == 1 && g_psViewExtents.count(out) == 1);
    CHECK(out->Release() == 0);
    std::printf("PASS private PS %08X patch=%u\n", HashBytes(original.data(), static_cast<size_t>(bytes)), patch);
}
} // namespace d2vr_replay

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    using namespace d2vr_replay;
    OrdinaryAndFailure();
    TargetOverrideAndFallback();
    DeviceRecreationAndEyes();
    MalformedCommentGap();
    for (int i = 1; i < argc; ++i) PrivateShader(argv[i]);
    std::puts("PASS actual PS Create hook: bytecode, HRESULT, fallback, COM refs, device recreation, eyes");
}

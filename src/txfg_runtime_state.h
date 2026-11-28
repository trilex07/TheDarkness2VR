#pragma once
// Shared atomic live state for Darkness2VR TXRFG Win32 D3D11 runtime integration and Optimization UI.
// Default state is Recommended (OFF): zero extra VRAM, zero GPU dispatches, bit-exact passthrough.
// Persists to %LOCALAPPDATA%\Darkness2VR\txrfg-optimization.ini so Darkness2VR.ini stays untouched.
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <windows.h>

namespace d2vr_txfg {

enum OptProfile : int {
    ProfileRecommended = 0, // Safe default: TXRFG OFF
    ProfileQuality     = 1, // TXRFG 2x optical-flow + M12 Cross-GPU Auto
    ProfilePerformance = 2, // TXRFG 2x optical-flow + M12 Cross-GPU Auto + 2.0 ms GPU budget
    ProfileCustom      = 3,
};

struct LiveConfig {
    int profile{ProfileRecommended};
    bool frameGenEnabled{false};
    bool crossGpuAuto{false};
    int gpuBudgetTenthsMs{0}; // 0..100 -> 0.0..10.0 ms
};

inline std::atomic<int> profile{ProfileRecommended};
inline std::atomic<bool> frameGenEnabled{false};
inline std::atomic<bool> crossGpuAuto{false};
inline std::atomic<int> gpuBudgetTenthsMs{0};
inline std::atomic<unsigned> configSerial{0};

inline std::atomic<bool> compiled{false};
inline std::atomic<bool> active{false};
inline std::atomic<bool> initialized{false};
inline std::atomic<bool> hasVisMask{false};
inline std::atomic<std::uint32_t> maxShaderModel{0};
inline std::atomic<std::uint32_t> activeCrossGpuFlags{0};
inline std::atomic<std::uint64_t> pushedFrames{0};
inline std::atomic<std::uint64_t> nativeSubmitted{0};
inline std::atomic<std::uint64_t> synthSubmitted{0};
inline std::atomic<std::uint64_t> fallbacks{0};
inline std::atomic<std::uint64_t> missedSlots{0};
inline std::atomic<std::uint64_t> resets{0};
inline std::atomic<unsigned> nativeFpsTenths{0};
inline std::atomic<unsigned> synthFpsTenths{0};
inline std::atomic<unsigned> totalFpsTenths{0};
inline std::atomic<unsigned> gpuMsHundredths{0};

inline void ApplyProfilePreset(LiveConfig& cfg, int p) noexcept {
    if (p < ProfileRecommended || p > ProfileCustom) p = ProfileRecommended;
    cfg.profile = p;
    if (p == ProfileRecommended) {
        cfg.frameGenEnabled = false;
        cfg.crossGpuAuto = false;
        cfg.gpuBudgetTenthsMs = 0;
    } else if (p == ProfileQuality) {
        cfg.frameGenEnabled = true;
        cfg.crossGpuAuto = true;
        cfg.gpuBudgetTenthsMs = 0;
    } else if (p == ProfilePerformance) {
        cfg.frameGenEnabled = true;
        cfg.crossGpuAuto = true;
        cfg.gpuBudgetTenthsMs = 20; // 2.0 ms
    }
}

inline LiveConfig LoadLive() noexcept {
    LiveConfig c{};
    c.profile = profile.load(std::memory_order_relaxed);
    c.frameGenEnabled = frameGenEnabled.load(std::memory_order_relaxed);
    c.crossGpuAuto = crossGpuAuto.load(std::memory_order_relaxed);
    c.gpuBudgetTenthsMs = gpuBudgetTenthsMs.load(std::memory_order_relaxed);
    return c;
}

inline void StoreLive(const LiveConfig& c) noexcept {
    const int p = (c.profile >= ProfileRecommended && c.profile <= ProfileCustom) ? c.profile : ProfileRecommended;
    const int b = c.gpuBudgetTenthsMs < 0 ? 0 : (c.gpuBudgetTenthsMs > 100 ? 100 : c.gpuBudgetTenthsMs);
    profile.store(p, std::memory_order_relaxed);
    frameGenEnabled.store(c.frameGenEnabled, std::memory_order_relaxed);
    crossGpuAuto.store(c.crossGpuAuto, std::memory_order_relaxed);
    gpuBudgetTenthsMs.store(b, std::memory_order_relaxed);
    configSerial.fetch_add(1, std::memory_order_release);
}

inline bool GetOptimizationIniPathW(wchar_t* out, size_t cap) noexcept {
    if (!out || cap < 32) return false;
    out[0] = L'\0';
    wchar_t base[MAX_PATH]{};
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
    if (!n || n >= MAX_PATH - 48) return false;
    wchar_t dir[MAX_PATH]{};
    swprintf_s(dir, L"%s\\Darkness2VR", base);
    CreateDirectoryW(dir, nullptr);
    swprintf_s(out, cap, L"%s\\txrfg-optimization.ini", dir);
    return true;
}

inline void LoadFromIniW(const wchar_t* iniPath) noexcept {
    if (!iniPath || !iniPath[0] || GetFileAttributesW(iniPath) == INVALID_FILE_ATTRIBUTES) return;
    LiveConfig c = LoadLive();
    const int p = static_cast<int>(GetPrivateProfileIntW(L"Optimization", L"TxrfgOptProfile", -1, iniPath));
    if (p >= ProfileRecommended && p <= ProfileCustom) {
        c.profile = p;
        if (p != ProfileCustom) ApplyProfilePreset(c, p);
    }
    const int fg = static_cast<int>(GetPrivateProfileIntW(L"Optimization", L"TxrfgFrameGen", -1, iniPath));
    if (fg == 0 || fg == 1) c.frameGenEnabled = (fg == 1);
    const int cg = static_cast<int>(GetPrivateProfileIntW(L"Optimization", L"TxrfgCrossGpu", -1, iniPath));
    if (cg == 0 || cg == 1) c.crossGpuAuto = (cg == 1);
    wchar_t bud[32]{};
    if (GetPrivateProfileStringW(L"Optimization", L"TxrfgGpuBudgetMs", L"", bud, 32, iniPath) > 0 && bud[0]) {
        wchar_t* end = nullptr;
        const float v = std::wcstof(bud, &end);
        if (end != bud && std::isfinite(v) && v >= 0.0f && v <= 10.0f) {
            c.gpuBudgetTenthsMs = static_cast<int>(std::lround(v * 10.0f));
        }
    }
    StoreLive(c);
}

inline bool SaveToIniW(const wchar_t* iniPath, const LiveConfig& c) noexcept {
    if (!iniPath || !iniPath[0]) return false;
    wchar_t buf[32]{};
    bool ok = true;
    swprintf_s(buf, L"%d", c.profile);
    ok = WritePrivateProfileStringW(L"Optimization", L"TxrfgOptProfile", buf, iniPath) != 0 && ok;
    ok = WritePrivateProfileStringW(L"Optimization", L"TxrfgFrameGen", c.frameGenEnabled ? L"1" : L"0", iniPath) != 0 && ok;
    ok = WritePrivateProfileStringW(L"Optimization", L"TxrfgCrossGpu", c.crossGpuAuto ? L"1" : L"0", iniPath) != 0 && ok;
    const int b = c.gpuBudgetTenthsMs < 0 ? 0 : (c.gpuBudgetTenthsMs > 100 ? 100 : c.gpuBudgetTenthsMs);
    swprintf_s(buf, L"%.1f", static_cast<double>(b) / 10.0);
    ok = WritePrivateProfileStringW(L"Optimization", L"TxrfgGpuBudgetMs", buf, iniPath) != 0 && ok;
    return ok;
}

inline void LoadDefaultIni() noexcept {
    wchar_t path[MAX_PATH]{};
    if (GetOptimizationIniPathW(path, _countof(path))) LoadFromIniW(path);
}

inline bool SaveDefaultIni(const LiveConfig& c) noexcept {
    wchar_t path[MAX_PATH]{};
    return GetOptimizationIniPathW(path, _countof(path)) && SaveToIniW(path, c);
}

} // namespace d2vr_txfg

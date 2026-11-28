#pragma once
// M11 Universal TXRFG Win32 D3D9Ex -> D3D11 interop adapter.
// Default OFF (Recommended profile): zero GPU dispatches, zero extra VRAM, bit-exact passthrough.
// Unsupported features (depth/MV HSFG, M10 Neural Repair, M8 HAM, DFR, 3x/4x) remain strictly disabled.
#include <d3d11.h>
#include <windows.h>
#include <txfg/txfg.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace txrfg_interop {

enum class OptProfile : int {
    Recommended = 0, // Safe default until owner headset acceptance (FG OFF)
    Quality     = 1, // TXRFG 2x optical-flow + M12 Cross-GPU Auto
    Performance = 2, // TXRFG 2x optical-flow + M12 Cross-GPU Auto + 2.0 ms GPU budget
    Custom      = 3,
};

struct Capabilities {
    bool hasColor{true};
    bool hasDepth{false};
    bool hasMotionVectors{false};
    bool hasHsfg{false};
    bool hasNeuralRepair{false};
    bool hasAdaptiveMultiFrame{false};
    bool hasVisibilityMask{false};
    bool hasDfr{false};
    bool hasCrossGpu{true};
    bool hasGpuBudget{true};
    uint32_t maxShaderModel{0u};
    uint32_t activeCrossGpuFlags{0u};
};

struct Config {
    OptProfile profile{OptProfile::Recommended};
    bool frameGenEnabled{false};
    bool crossGpuAuto{false};
    float gpuBudgetMs{0.f}; // 0.0 = uncapped, >0 clamped to [0, 10]
};

inline void ApplyProfile(Config& cfg, OptProfile profile) noexcept {
    if (profile == OptProfile::Custom) {
        cfg.profile = OptProfile::Custom;
        return;
    }
    cfg.profile = profile;
    if (profile == OptProfile::Recommended) {
        cfg.frameGenEnabled = false;
        cfg.crossGpuAuto = false;
        cfg.gpuBudgetMs = 0.f;
    } else if (profile == OptProfile::Quality) {
        cfg.frameGenEnabled = true;
        cfg.crossGpuAuto = true;
        cfg.gpuBudgetMs = 0.f;
    } else { // Performance
        cfg.frameGenEnabled = true;
        cfg.crossGpuAuto = true;
        cfg.gpuBudgetMs = 2.0f;
    }
}

inline void RestoreRecommended(Config& cfg) noexcept {
    ApplyProfile(cfg, OptProfile::Recommended);
}

inline Config LoadConfigIniW(const wchar_t* iniPath) noexcept {
    Config cfg{};
    if (!iniPath || !iniPath[0]) return cfg;
    const int p = static_cast<int>(GetPrivateProfileIntW(L"Optimization", L"TxrfgOptProfile", 0, iniPath));
    cfg.profile = (p >= 0 && p <= 3) ? static_cast<OptProfile>(p) : OptProfile::Recommended;
    if (cfg.profile != OptProfile::Custom) {
        ApplyProfile(cfg, cfg.profile);
    }
    if (GetPrivateProfileIntW(L"Optimization", L"TxrfgFrameGen", -1, iniPath) != -1) {
        cfg.frameGenEnabled = GetPrivateProfileIntW(L"Optimization", L"TxrfgFrameGen", 0, iniPath) == 1;
    }
    if (GetPrivateProfileIntW(L"Optimization", L"TxrfgCrossGpu", -1, iniPath) != -1) {
        cfg.crossGpuAuto = GetPrivateProfileIntW(L"Optimization", L"TxrfgCrossGpu", 0, iniPath) == 1;
    }
    wchar_t bud[32]{};
    if (GetPrivateProfileStringW(L"Optimization", L"TxrfgGpuBudgetMs", L"", bud, 32, iniPath) > 0 && bud[0]) {
        wchar_t* end = nullptr;
        const float v = std::wcstof(bud, &end);
        if (end != bud && std::isfinite(v) && v >= 0.f && v <= 10.f) cfg.gpuBudgetMs = v;
    }
    return cfg;
}

inline bool SaveConfigIniW(const wchar_t* iniPath, const Config& cfg) noexcept {
    if (!iniPath || !iniPath[0]) return false;
    wchar_t buf[32]{};
    bool ok = true;
    swprintf_s(buf, L"%d", static_cast<int>(cfg.profile));
    if (!WritePrivateProfileStringW(L"Optimization", L"TxrfgOptProfile", buf, iniPath)) ok = false;
    if (!WritePrivateProfileStringW(L"Optimization", L"TxrfgFrameGen", cfg.frameGenEnabled ? L"1" : L"0", iniPath)) ok = false;
    if (!WritePrivateProfileStringW(L"Optimization", L"TxrfgCrossGpu", cfg.crossGpuAuto ? L"1" : L"0", iniPath)) ok = false;
    const float cleanBud = (std::isfinite(cfg.gpuBudgetMs) && cfg.gpuBudgetMs > 0.f) ? (std::min)(cfg.gpuBudgetMs, 10.f) : 0.f;
    swprintf_s(buf, L"%.1f", cleanBud);
    if (!WritePrivateProfileStringW(L"Optimization", L"TxrfgGpuBudgetMs", buf, iniPath)) ok = false;
    return ok;
}

class D3D11Bridge {
public:
    D3D11Bridge() = default;
    ~D3D11Bridge() { Shutdown(); }
    D3D11Bridge(const D3D11Bridge&) = delete;
    D3D11Bridge& operator=(const D3D11Bridge&) = delete;

    void Configure(const Config& cfg) noexcept {
        cfg_ = cfg;
        cfg_.gpuBudgetMs = (std::isfinite(cfg_.gpuBudgetMs) && cfg_.gpuBudgetMs > 0.f)
            ? (std::min)(cfg_.gpuBudgetMs, 10.f) : 0.f;
        SyncRuntimeConfig();
    }

    const Config& CurrentConfig() const noexcept { return cfg_; }
    Capabilities QueryCapabilities() const noexcept { return caps_; }
    bool Initialized() const noexcept { return ctx_ != nullptr; }
    uint64_t PushedFrames() const noexcept { return pushed_; }
    uint64_t SynthesizedFrames() const noexcept { return synthesized_; }
    uint64_t FallbackFrames() const noexcept { return fallbacks_; }
    float LastGpuMs() const noexcept { return lastGpuMs_; }
    float AvgGpuMs() const noexcept { return gpuSamples_ ? static_cast<float>(gpuMsSum_ / static_cast<double>(gpuSamples_)) : 0.f; }

    void ResetHistory() noexcept {
        if (ctx_) txfg_reset(ctx_);
    }

    bool SetVisibilityMask(const txfg_visibility_mask_desc* desc) noexcept {
        if (!ctx_) return false;
        if (txfg_set_visibility_mask(ctx_, desc) != TXFG_OK) return false;
        caps_.hasVisibilityMask = (desc != nullptr);
        return true;
    }

    bool Init(ID3D11Device* device, uint32_t eyeWidth, uint32_t eyeHeight, DXGI_FORMAT format) noexcept {
        if (!device || eyeWidth < 64u || eyeHeight < 64u) return false;
        if (ctx_ && device_ == device && width_ == eyeWidth && height_ == eyeHeight && format_ == format) {
            SyncRuntimeConfig();
            return true;
        }
        Shutdown();
        txfg_create_info ci{};
        ci.struct_size = sizeof ci;
        ci.api = TXFG_API_D3D11;
        ci.device = device;
        ci.width = eyeWidth;
        ci.height = eyeHeight;
        ci.color_format = static_cast<uint32_t>(format);
        ci.engine = TXFG_ENGINE_NATIVE;
        ci.max_multiplier = 2;
        if (txfg_create(&ci, &ctx_) != TXFG_OK || !ctx_) {
            ctx_ = nullptr;
            return false;
        }
        device_ = device;
        width_ = eyeWidth;
        height_ = eyeHeight;
        format_ = format;
        appliedCrossGpu_ = -1;
        appliedBudgetMs_ = -1.f;
        SyncRuntimeConfig();
        return true;
    }

    bool PushStereo(ID3D11DeviceContext* dc, ID3D11Texture2D* leftEye, ID3D11Texture2D* rightEye,
                    uint64_t frameId, const txfg_pose pose[2], const txfg_fov fov[2], bool reset) noexcept {
        if (!ctx_ || !dc || !leftEye || !rightEye || !cfg_.frameGenEnabled) return false;
        SyncRuntimeConfig();
        txfg_frame_desc fd{};
        fd.struct_size = sizeof fd;
        fd.frame_id = frameId;
        fd.flags = reset ? TXFG_FRAME_RESET : 0u;
        fd.color[0] = {leftEye, 0, 0};
        fd.color[1] = {rightEye, 0, 0};
        for (int e = 0; e < 2; ++e) {
            fd.pose[e] = pose[e];
            fd.fov[e] = fov[e];
        }
        if (txfg_push_frame(ctx_, dc, &fd) != TXFG_OK) return false;
        ++pushed_;
        return true;
    }

    bool Synthesize(ID3D11DeviceContext* dc, ID3D11Texture2D* outLeft, ID3D11Texture2D* outRight,
                    float t, txfg_synth_result* outRes = nullptr) noexcept {
        if (!ctx_ || !dc || !outLeft || !outRight || !cfg_.frameGenEnabled) return false;
        txfg_synth_desc sd{};
        sd.struct_size = sizeof sd;
        sd.t = std::clamp(t, 0.05f, 0.95f);
        sd.flags = 0u;
        sd.out_color[0] = {outLeft, 0, 0};
        sd.out_color[1] = {outRight, 0, 0};
        txfg_synth_result res{};
        res.struct_size = sizeof res;
        const txfg_result r = txfg_synthesize(ctx_, dc, &sd, &res);
        if (outRes) *outRes = res;
        if (r == TXFG_OK && res.quality_class == TXFG_QUALITY_SYNTHESIZED) {
            if (res.gpu_ms_last > 0.f) {
                lastGpuMs_ = res.gpu_ms_last;
                gpuMsSum_ += static_cast<double>(res.gpu_ms_last);
                ++gpuSamples_;
            }
            ++synthesized_;
            return true;
        }
        ++fallbacks_;
        return false;
    }

    bool SynthesizeExtrap(ID3D11DeviceContext* dc, ID3D11Texture2D* outLeft, ID3D11Texture2D* outRight,
                          float t, txfg_synth_result* outRes = nullptr) noexcept {
        if (!ctx_ || !dc || !outLeft || !outRight || !cfg_.frameGenEnabled) return false;
        txfg_synth_desc sd{};
        sd.struct_size = sizeof sd;
        sd.t = std::clamp(t, 1.05f, 1.50f);
        sd.flags = TXFG_SYNTH_EXTRAPOLATE_WORLD_ONLY;
        sd.out_color[0] = {outLeft, 0, 0};
        sd.out_color[1] = {outRight, 0, 0};
        txfg_synth_result res{};
        res.struct_size = sizeof res;
        const txfg_result r = txfg_synthesize(ctx_, dc, &sd, &res);
        if (outRes) *outRes = res;
        if (r == TXFG_OK && res.quality_class == TXFG_QUALITY_SYNTHESIZED) {
            if (res.gpu_ms_last > 0.f) {
                lastGpuMs_ = res.gpu_ms_last;
                gpuMsSum_ += static_cast<double>(res.gpu_ms_last);
                ++gpuSamples_;
            }
            ++synthesized_;
            return true;
        }
        ++fallbacks_;
        return false;
    }

    void Shutdown() noexcept {
        if (ctx_) {
            txfg_destroy(ctx_);
            ctx_ = nullptr;
        }
        device_ = nullptr;
        width_ = height_ = 0u;
        format_ = DXGI_FORMAT_UNKNOWN;
        appliedCrossGpu_ = -1;
        appliedBudgetMs_ = -1.f;
        caps_.hasVisibilityMask = false;
        lastGpuMs_ = 0.f;
        gpuMsSum_ = 0.0;
        gpuSamples_ = 0;
    }

private:
    void SyncRuntimeConfig() noexcept {
        if (!ctx_) return;
        const int cgWant = cfg_.crossGpuAuto ? 1 : 0;
        if (cgWant != appliedCrossGpu_) {
            txfg_result r = TXFG_OK;
            if (cgWant) {
                txfg_cross_gpu_config cg{};
                cg.struct_size = sizeof cg;
                cg.mode = TXFG_CROSS_GPU_AUTO;
                r = txfg_set_cross_gpu_config(ctx_, &cg);
            } else {
                r = txfg_set_cross_gpu_config(ctx_, nullptr);
            }
            if (r == TXFG_OK) appliedCrossGpu_ = cgWant;
        }
        if (std::fabs(cfg_.gpuBudgetMs - appliedBudgetMs_) > 1e-5f) {
            if (txfg_set_gpu_budget_ms(ctx_, cfg_.gpuBudgetMs) == TXFG_OK) {
                appliedBudgetMs_ = cfg_.gpuBudgetMs;
            }
        }
        (void)txfg_set_neural_repair_config(ctx_, nullptr);
        txfg_gpu_capabilities gc{};
        gc.struct_size = sizeof gc;
        if (txfg_get_gpu_capabilities(ctx_, &gc) == TXFG_OK) {
            caps_.maxShaderModel = gc.max_shader_model;
            caps_.activeCrossGpuFlags = gc.active_m12_flags;
        }
    }

    txfg_context ctx_{};
    ID3D11Device* device_{};
    uint32_t width_{}, height_{};
    DXGI_FORMAT format_{DXGI_FORMAT_UNKNOWN};
    Config cfg_{};
    Capabilities caps_{};
    int appliedCrossGpu_{-1};
    float appliedBudgetMs_{-1.f};
    uint64_t pushed_{}, synthesized_{}, fallbacks_{};
    float lastGpuMs_{0.f};
    double gpuMsSum_{0.0};
    uint64_t gpuSamples_{0};
};

} // namespace txrfg_interop

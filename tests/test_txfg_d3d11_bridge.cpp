#include "txfg_d3d11_bridge.h"
#include <wrl/client.h>
#include <cstdio>
#include <vector>

using Microsoft::WRL::ComPtr;

int main() {
    using namespace txrfg_interop;
    // 1. Default OFF / Recommended + capability gating contract.
    Config cfg{};
    if (cfg.profile != OptProfile::Recommended || cfg.frameGenEnabled || cfg.crossGpuAuto || cfg.gpuBudgetMs != 0.f) {
        std::puts("FAIL: default config must be Recommended / OFF");
        return 1;
    }
    ApplyProfile(cfg, OptProfile::Quality);
    if (!cfg.frameGenEnabled || !cfg.crossGpuAuto || cfg.gpuBudgetMs != 0.f) {
        std::puts("FAIL: Quality profile");
        return 1;
    }
    ApplyProfile(cfg, OptProfile::Performance);
    if (!cfg.frameGenEnabled || !cfg.crossGpuAuto || cfg.gpuBudgetMs < 1.9f) {
        std::puts("FAIL: Performance profile");
        return 1;
    }
    RestoreRecommended(cfg);
    if (cfg.profile != OptProfile::Recommended || cfg.frameGenEnabled || cfg.crossGpuAuto || cfg.gpuBudgetMs != 0.f) {
        std::puts("FAIL: RestoreRecommended");
        return 1;
    }

    // 2. INI persistence round-trip.
    wchar_t tmpDir[MAX_PATH]{}, tmpIni[MAX_PATH]{};
    if (!GetTempPathW(MAX_PATH, tmpDir) || !GetTempFileNameW(tmpDir, L"txf", 0, tmpIni)) return 1;
    ApplyProfile(cfg, OptProfile::Performance);
    if (!SaveConfigIniW(tmpIni, cfg)) { std::puts("FAIL: SaveConfigIniW"); return 1; }
    const Config loaded = LoadConfigIniW(tmpIni);
    DeleteFileW(tmpIni);
    if (loaded.profile != OptProfile::Performance || !loaded.frameGenEnabled || !loaded.crossGpuAuto || loaded.gpuBudgetMs < 1.9f) {
        std::puts("FAIL: LoadConfigIniW round-trip");
        return 1;
    }

    // 3. Real Win32 D3D11 hardware/WARP TXRFG synthesis + M12 Auto + GPU budget verification.
    ComPtr<ID3D11Device> dev;
    ComPtr<ID3D11DeviceContext> dc;
    D3D_FEATURE_LEVEL fl{};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, &dc);
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, &dc);
    }
    if (FAILED(hr) || !dev || !dc) {
        std::puts("SKIP: no D3D11 device");
        return 0;
    }

    const uint32_t w = 256, h = 256;
    auto makeTex = [&](uint32_t shift, ComPtr<ID3D11Texture2D>& out) -> bool {
        D3D11_TEXTURE2D_DESC td{};
        td.Width = w; td.Height = h; td.MipLevels = td.ArraySize = td.SampleDesc.Count = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        std::vector<uint32_t> px(w * h);
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                const bool bar = (x >= 60 + shift && x < 100 + shift);
                px[y * w + x] = bar ? 0xff40d0ffu : 0xff202830u;
            }
        }
        D3D11_SUBRESOURCE_DATA sd{px.data(), w * 4, 0};
        return SUCCEEDED(dev->CreateTexture2D(&td, &sd, &out));
    };

    ComPtr<ID3D11Texture2D> aL, aR, bL, bR, outL, outR;
    if (!makeTex(0, aL) || !makeTex(0, aR) || !makeTex(16, bL) || !makeTex(16, bR) || !makeTex(0, outL) || !makeTex(0, outR)) {
        std::puts("FAIL: CreateTexture2D");
        return 1;
    }

    D3D11Bridge bridge;
    if (!bridge.Init(dev.Get(), w, h, DXGI_FORMAT_R8G8B8A8_UNORM)) {
        std::puts("FAIL: bridge.Init");
        return 1;
    }
    const auto caps = bridge.QueryCapabilities();
    if (!caps.hasColor || caps.hasHsfg || caps.hasNeuralRepair || caps.hasAdaptiveMultiFrame || caps.hasVisibilityMask || caps.hasDfr) {
        std::puts("FAIL: unsupported features must stay disabled in capabilities");
        return 1;
    }

    txfg_pose pose[2]{};
    txfg_fov fov[2]{};
    for (int e = 0; e < 2; ++e) {
        pose[e].orientation[3] = 1.f;
        pose[e].position[0] = e ? 0.032f : -0.032f;
        fov[e] = {-0.785f, 0.785f, 0.785f, -0.785f};
    }

    // Feature-OFF must be no-op.
    if (bridge.PushStereo(dc.Get(), aL.Get(), aR.Get(), 1, pose, fov, true)) {
        std::puts("FAIL: Feature-OFF must not push");
        return 1;
    }

    // Enable Quality (TXRFG 2x + M12 Auto) and synthesize.
    ApplyProfile(cfg, OptProfile::Quality);
    bridge.Configure(cfg);
    if (!bridge.PushStereo(dc.Get(), aL.Get(), aR.Get(), 10, pose, fov, true) ||
        !bridge.PushStereo(dc.Get(), bL.Get(), bR.Get(), 11, pose, fov, false)) {
        std::puts("FAIL: PushStereo");
        return 1;
    }
    txfg_synth_result res{};
    if (!bridge.Synthesize(dc.Get(), outL.Get(), outR.Get(), 0.5f, &res) || bridge.SynthesizedFrames() != 1) {
        std::puts("FAIL: Synthesize");
        return 1;
    }
    const auto capsOn = bridge.QueryCapabilities();
    std::printf("Win32 TXRFG D3D11 interop PASS: pushed=%llu synth=%llu sm=0x%X m12Flags=0x%X\n",
                (unsigned long long)bridge.PushedFrames(), (unsigned long long)bridge.SynthesizedFrames(),
                capsOn.maxShaderModel, capsOn.activeCrossGpuFlags);
    bridge.Shutdown();
    return 0;
}

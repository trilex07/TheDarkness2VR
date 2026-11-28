#include "txfg_d3d11_bridge.h"
#include "txfg_runtime_state.h"
#include "txfg_xr_runtime.h"
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
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

    // 2. INI persistence round-trip (bridge + d2vr_txfg live state).
    wchar_t tmpDir[MAX_PATH]{}, tmpIni[MAX_PATH]{};
    if (!GetTempPathW(MAX_PATH, tmpDir) || !GetTempFileNameW(tmpDir, L"txf", 0, tmpIni)) return 1;
    ApplyProfile(cfg, OptProfile::Performance);
    if (!SaveConfigIniW(tmpIni, cfg)) { std::puts("FAIL: SaveConfigIniW"); return 1; }
    const Config loaded = LoadConfigIniW(tmpIni);
    if (loaded.profile != OptProfile::Performance || !loaded.frameGenEnabled || !loaded.crossGpuAuto || loaded.gpuBudgetMs < 1.9f) {
        DeleteFileW(tmpIni);
        std::puts("FAIL: LoadConfigIniW round-trip");
        return 1;
    }
    {
        d2vr_txfg::LiveConfig lc{};
        d2vr_txfg::ApplyProfilePreset(lc, d2vr_txfg::ProfilePerformance);
        if (!d2vr_txfg::SaveToIniW(tmpIni, lc)) { DeleteFileW(tmpIni); std::puts("FAIL: SaveToIniW"); return 1; }
        d2vr_txfg::LiveConfig off{};
        d2vr_txfg::StoreLive(off);
        d2vr_txfg::LoadFromIniW(tmpIni);
        const auto rtLoaded = d2vr_txfg::LoadLive();
        DeleteFileW(tmpIni);
        if (rtLoaded.profile != d2vr_txfg::ProfilePerformance || !rtLoaded.frameGenEnabled || rtLoaded.gpuBudgetTenthsMs != 20) {
            std::puts("FAIL: d2vr_txfg LoadFromIniW round-trip");
            return 1;
        }
        d2vr_txfg::StoreLive(off);
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

    if (bridge.PushStereo(dc.Get(), aL.Get(), aR.Get(), 1, pose, fov, true)) {
        std::puts("FAIL: Feature-OFF must not push");
        return 1;
    }

    ApplyProfile(cfg, OptProfile::Quality);
    bridge.Configure(cfg);
    if (!bridge.PushStereo(dc.Get(), aL.Get(), aR.Get(), 10, pose, fov, true) ||
        !bridge.PushStereo(dc.Get(), bL.Get(), bR.Get(), 11, pose, fov, false)) {
        std::puts("FAIL: PushStereo");
        return 1;
    }
    txfg_synth_result res{};
    if (!bridge.SynthesizeExtrap(dc.Get(), outL.Get(), outR.Get(), 1.5f, &res) || bridge.SynthesizedFrames() != 1) {
        std::puts("FAIL: SynthesizeExtrap");
        return 1;
    }
    const auto capsOn = bridge.QueryCapabilities();
    std::printf("Darkness2VR Win32 TXRFG D3D11 interop PASS: pushed=%llu synth=%llu sm=0x%X m12Flags=0x%X\n",
                (unsigned long long)bridge.PushedFrames(), (unsigned long long)bridge.SynthesizedFrames(),
                capsOn.maxShaderModel, capsOn.activeCrossGpuFlags);
    bridge.Shutdown();

    // 4. End-to-end SubmitMode=4 dual-swapchain OpenXR 2x cadence + XR_TIMEOUT_EXPIRED compliance proof (45 Hz -> 90 Hz).
    {
        struct MockDualXrApi {
            ID3D11Device* dev{};
            std::vector<ComPtr<ID3D11Texture2D>> chainTex[2];
            XrSwapchain chains[2]{static_cast<XrSwapchain>(0x71001ull), static_cast<XrSwapchain>(0x71002ull)};
            int createdCount{0};
            unsigned acquiredIdx[2]{}, acquireCalls[2]{}, releasedCount[2]{};
            bool currentlyAcquired[2]{};
            bool failNextWaitTimeoutEye0{false};
            bool illegalRelease{false};

            int EyeFor(XrSwapchain sc) const noexcept { return sc == chains[0] ? 0 : (sc == chains[1] ? 1 : -1); }

            XrResult xrCreateSwapchain(XrSession, const XrSwapchainCreateInfo* ci, XrSwapchain* out) noexcept {
                const int e = createdCount++;
                if (e < 0 || e >= 2) return XR_ERROR_LIMIT_REACHED;
                chainTex[e].clear();
                D3D11_TEXTURE2D_DESC td{};
                td.Width = ci->width; td.Height = ci->height; td.MipLevels = td.ArraySize = td.SampleDesc.Count = 1;
                td.Format = static_cast<DXGI_FORMAT>(ci->format);
                td.Usage = D3D11_USAGE_DEFAULT;
                td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
                for (int i = 0; i < 2; ++i) {
                    ComPtr<ID3D11Texture2D> t;
                    if (FAILED(dev->CreateTexture2D(&td, nullptr, &t))) return XR_ERROR_RUNTIME_FAILURE;
                    chainTex[e].push_back(t);
                }
                *out = chains[e];
                return XR_SUCCESS;
            }
            XrResult xrDestroySwapchain(XrSwapchain sc) noexcept {
                const int e = EyeFor(sc);
                if (e >= 0) { chainTex[e].clear(); currentlyAcquired[e] = false; }
                return XR_SUCCESS;
            }
            XrResult xrEnumerateSwapchainImages(XrSwapchain sc, uint32_t cap, uint32_t* outCount, XrSwapchainImageBaseHeader* imgs) noexcept {
                const int e = EyeFor(sc);
                if (e < 0) return XR_ERROR_HANDLE_INVALID;
                *outCount = static_cast<uint32_t>(chainTex[e].size());
                if (cap == 0) return XR_SUCCESS;
                auto* d = reinterpret_cast<XrSwapchainImageD3D11KHR*>(imgs);
                for (uint32_t i = 0; i < *outCount && i < cap; ++i) d[i].texture = chainTex[e][i].Get();
                return XR_SUCCESS;
            }
            XrResult xrAcquireSwapchainImage(XrSwapchain sc, const XrSwapchainImageAcquireInfo*, uint32_t* idx) noexcept {
                const int e = EyeFor(sc);
                if (e < 0 || currentlyAcquired[e]) return XR_ERROR_CALL_ORDER_INVALID;
                currentlyAcquired[e] = true;
                ++acquireCalls[e];
                *idx = (acquiredIdx[e]++) % static_cast<uint32_t>(chainTex[e].size());
                return XR_SUCCESS;
            }
            XrResult xrWaitSwapchainImage(XrSwapchain sc, const XrSwapchainImageWaitInfo*) noexcept {
                const int e = EyeFor(sc);
                if (e < 0 || !currentlyAcquired[e]) return XR_ERROR_CALL_ORDER_INVALID;
                if (e == 0 && failNextWaitTimeoutEye0) { failNextWaitTimeoutEye0 = false; return XR_TIMEOUT_EXPIRED; }
                return XR_SUCCESS;
            }
            XrResult xrReleaseSwapchainImage(XrSwapchain sc, const XrSwapchainImageReleaseInfo*) noexcept {
                const int e = EyeFor(sc);
                if (e < 0 || !currentlyAcquired[e]) { illegalRelease = true; return XR_ERROR_CALL_ORDER_INVALID; }
                currentlyAcquired[e] = false;
                ++releasedCount[e];
                return XR_SUCCESS;
            }
        };

        const uint32_t eyeW = 256, eyeH = 256;
        MockDualXrApi mockApi{dev.Get()};
        const XrSession fakeSession = static_cast<XrSession>(0x5678ull);
        const XrSwapchain nativeSwapchains[2]{static_cast<XrSwapchain>(0x20001ull), static_cast<XrSwapchain>(0x20002ull)};

        d2vr_txfg::LiveConfig qc{};
        d2vr_txfg::ApplyProfilePreset(qc, d2vr_txfg::ProfileQuality);
        d2vr_txfg::StoreLive(qc);

        d2vr_txfg::TxrfgXrRuntime<MockDualXrApi> rt;
        rt.Configure(mockApi, fakeSession, dev.Get(), dc.Get(), eyeW, eyeH, DXGI_FORMAT_R8G8B8A8_UNORM);

        ComPtr<ID3D11Texture2D> eyeTex[2];
        if (!makeTex(0, eyeTex[0]) || !makeTex(0, eyeTex[1])) { std::puts("FAIL: eyeTex"); return 1; }
        auto uploadEye = [&](uint32_t shiftPx) {
            std::vector<uint32_t> px(eyeW * eyeH);
            for (uint32_t y = 0; y < eyeH; ++y) {
                for (uint32_t x = 0; x < eyeW; ++x) {
                    const bool bar = (x >= 50 + shiftPx && x < 90 + shiftPx && y >= 40 && y < 210);
                    px[y * eyeW + x] = bar ? 0xff40d0ffu : 0xff202830u;
                }
            }
            dc->UpdateSubresource(eyeTex[0].Get(), 0, nullptr, px.data(), eyeW * 4, 0);
            dc->UpdateSubresource(eyeTex[1].Get(), 0, nullptr, px.data(), eyeW * 4, 0);
        };

        constexpr int64_t kPeriod90 = 11'111'111LL;
        unsigned nativeEndFrames = 0, synthEndFrames = 0;
        for (unsigned slot = 0; slot < 200; ++slot) {
            const int64_t tNs = static_cast<int64_t>(slot + 1) * kPeriod90;
            const bool isNative = (slot % 2 == 0);
            XrCompositionLayerProjectionView views[2]{};
            for (int e = 0; e < 2; ++e) {
                views[e].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
                views[e].pose.orientation = {0, 0, 0, 1};
                views[e].pose.position = {e ? 0.032f : -0.032f, 0, 0};
                views[e].fov = {-0.785f, 0.785f, 0.785f, -0.785f};
                views[e].subImage.swapchain = nativeSwapchains[e];
                views[e].subImage.imageRect = {{0, 0}, {static_cast<int32_t>(eyeW), static_cast<int32_t>(eyeH)}};
            }
            XrCompositionLayerProjection proj{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
            proj.viewCount = 2; proj.views = views;
            const XrCompositionLayerBaseHeader* layerPtr = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&proj);
            XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
            end.displayTime = tNs;
            end.layerCount = isNative ? 1u : 0u;
            end.layers = isNative ? &layerPtr : nullptr;

            bool substituted = false;
            float gpuMs = 0.f;
            if (isNative) {
                uploadEye((slot / 2) % 16 * 6u);
                rt.OnNativeStereoEyes(eyeTex[0].Get(), eyeTex[1].Get(), proj, tNs);
                ++nativeEndFrames;
            } else {
                if (slot == 11) mockApi.failNextWaitTimeoutEye0 = true;
                substituted = rt.TrySubstituteSynthetic(tNs, end, gpuMs);
                if (slot == 11 && substituted) { std::puts("FAIL: slot 11 timeout must fall back"); return 1; }
                if (substituted) {
                    const auto* sp = reinterpret_cast<const XrCompositionLayerProjection*>(end.layers[0]);
                    if (end.layerCount != 1u || sp->views[0].subImage.swapchain != mockApi.chains[0] ||
                        sp->views[1].subImage.swapchain != mockApi.chains[1]) {
                        std::puts("FAIL: synthetic dual swapchain substitution mismatch");
                        return 1;
                    }
                    ++synthEndFrames;
                }
            }
            rt.AfterEndFrame(true, isNative, substituted, gpuMs, tNs, slot + 1);
        }
        if (nativeEndFrames != 100u || synthEndFrames != 98u || rt.SynthSubmitted() != 98u ||
            mockApi.releasedCount[0] != 98u || mockApi.releasedCount[1] != 98u ||
            mockApi.acquireCalls[0] != 98u || mockApi.acquireCalls[1] != 98u ||
            mockApi.illegalRelease || mockApi.currentlyAcquired[0] || mockApi.currentlyAcquired[1]) {
            std::printf("FAIL: Darkness2VR OpenXR counts native=%u synth=%u acq=(%u,%u) rel=(%u,%u)\n",
                        nativeEndFrames, synthEndFrames, mockApi.acquireCalls[0], mockApi.acquireCalls[1],
                        mockApi.releasedCount[0], mockApi.releasedCount[1]);
            return 1;
        }
        std::printf("Darkness2VR OpenXR 2x TXRFG PROOF PASS: native=%u (%.1f fps) synth=%u (%.1f fps) total=%.1f fps gpu=%.2f ms missed=%llu\n",
                    nativeEndFrames, double(d2vr_txfg::nativeFpsTenths.load()) / 10.0,
                    synthEndFrames, double(d2vr_txfg::synthFpsTenths.load()) / 10.0,
                    double(d2vr_txfg::totalFpsTenths.load()) / 10.0,
                    double(d2vr_txfg::gpuMsHundredths.load()) / 100.0,
                    (unsigned long long)rt.MissedSlots());
        rt.Close();
        d2vr_txfg::LiveConfig off{};
        d2vr_txfg::StoreLive(off);
    }

    // 5. Full Darkness2VR SubmitMode=4 stereo eye resolution (2048x2048/eye) end-to-end GPU + copy benchmark.
    if (fl >= D3D_FEATURE_LEVEL_11_0) {
        const uint32_t fw = 2048u, fh = 2048u;
        D3D11_TEXTURE2D_DESC td{};
        td.Width = fw; td.Height = fh; td.MipLevels = td.ArraySize = td.SampleDesc.Count = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_TEXTURE2D_DESC otd = td;
        otd.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        ComPtr<ID3D11Texture2D> srcL, srcR, inL, inR, outL2, outR2, dstL, dstR;
        if (SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &srcL)) &&
            SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &srcR)) &&
            SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &inL)) &&
            SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &inR)) &&
            SUCCEEDED(dev->CreateTexture2D(&otd, nullptr, &outL2)) &&
            SUCCEEDED(dev->CreateTexture2D(&otd, nullptr, &outR2)) &&
            SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &dstL)) &&
            SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &dstR))) {
            D3D11Bridge bFull;
            if (bFull.Init(dev.Get(), fw, fh, DXGI_FORMAT_B8G8R8A8_UNORM)) {
                Config qFull{};
                ApplyProfile(qFull, OptProfile::Quality);
                bFull.Configure(qFull);
                LARGE_INTEGER freq{}, t0{}, t1{};
                QueryPerformanceFrequency(&freq);
                double cpuSubmitMsSum = 0.0;
                for (uint64_t f = 1; f <= 16; ++f) {
                    QueryPerformanceCounter(&t0);
                    dc->CopyResource(inL.Get(), srcL.Get());
                    dc->CopyResource(inR.Get(), srcR.Get());
                    (void)bFull.PushStereo(dc.Get(), inL.Get(), inR.Get(), f, pose, fov, f == 1);
                    if (f >= 2) {
                        txfg_synth_result sr{};
                        if (bFull.SynthesizeExtrap(dc.Get(), outL2.Get(), outR2.Get(), 1.5f, &sr)) {
                            dc->CopyResource(dstL.Get(), outL2.Get());
                            dc->CopyResource(dstR.Get(), outR2.Get());
                        }
                    }
                    dc->Flush();
                    QueryPerformanceCounter(&t1);
                    if (f >= 2) cpuSubmitMsSum += double(t1.QuadPart - t0.QuadPart) * 1000.0 / double(freq.QuadPart);
                    Sleep(5);
                }
                std::printf("Full-res Darkness2VR 2x TXRFG (2048x2048/eye, incl. 4x GPU copies): synth=%llu last_gpu=%.2f ms avg_gpu=%.2f ms avg_cpu_submit=%.2f ms\n",
                            (unsigned long long)bFull.SynthesizedFrames(),
                            static_cast<double>(bFull.LastGpuMs()),
                            static_cast<double>(bFull.AvgGpuMs()),
                            cpuSubmitMsSum / 15.0);
                bFull.Shutdown();
            }
        }
    }
    return 0;
}

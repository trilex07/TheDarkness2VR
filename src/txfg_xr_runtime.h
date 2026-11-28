#pragma once
// Darkness2VR OpenXR <-> TXRFG D3D11 Runtime Integration (Win32 x86).
// Adapted from PreSequelVR 0f992ed to Darkness2VR's SubmitMode=4 dual-eye OpenXR architecture:
//   - Lazy initialization ONLY when Optimization frameGenEnabled == true (Recommended/OFF = zero VRAM/GPU).
//   - Native stereo ingestion from Darkness2VR's finalized per-eye D3D11 textures (post-DLSS/NIS/SMAA).
//   - Causal 2x extrapolation (TXFG_SYNTH_EXTRAPOLATE_WORLD_ONLY, t in [1.05, 1.50]) submitted on dedicated
//     per-eye synthetic OpenXR swapchains during held/gap periods, preserving VR Settings, Weapon Wheel, and Cinema.
//   - Zero blocking GPU synchronization (xrWaitSwapchainImage timeout=0, no CPU readbacks, no pipeline stalls).
//   - Strict OpenXR 1.0 swapchain ownership: XR_TIMEOUT_EXPIRED retains acquisition and retries wait without double-acquire.
#include <d3d11.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include "txfg_d3d11_bridge.h"
#include "txfg_runtime_state.h"

namespace d2vr_txfg {

struct DirectOpenXrApi {
    XrResult xrCreateSwapchain(XrSession s, const XrSwapchainCreateInfo* ci, XrSwapchain* sc) noexcept {
        return ::xrCreateSwapchain(s, ci, sc);
    }
    XrResult xrDestroySwapchain(XrSwapchain sc) noexcept {
        return ::xrDestroySwapchain(sc);
    }
    XrResult xrEnumerateSwapchainImages(XrSwapchain sc, uint32_t cap, uint32_t* cnt, XrSwapchainImageBaseHeader* imgs) noexcept {
        return ::xrEnumerateSwapchainImages(sc, cap, cnt, imgs);
    }
    XrResult xrAcquireSwapchainImage(XrSwapchain sc, const XrSwapchainImageAcquireInfo* ai, uint32_t* idx) noexcept {
        return ::xrAcquireSwapchainImage(sc, ai, idx);
    }
    XrResult xrWaitSwapchainImage(XrSwapchain sc, const XrSwapchainImageWaitInfo* wi) noexcept {
        return ::xrWaitSwapchainImage(sc, wi);
    }
    XrResult xrReleaseSwapchainImage(XrSwapchain sc, const XrSwapchainImageReleaseInfo* ri) noexcept {
        return ::xrReleaseSwapchainImage(sc, ri);
    }
};

template<class ApiT = DirectOpenXrApi>
class TxrfgXrRuntime {
public:
    using LogFn = void(*)(const char* msg);

    void Configure(ApiT& api, XrSession session, ID3D11Device* device, ID3D11DeviceContext* context,
                   unsigned eyeWidth, unsigned eyeHeight, DXGI_FORMAT format, LogFn logFn = nullptr) noexcept {
        const bool geometryChanged = (session_ != session || device_ != device ||
                                      eyeWidth_ != eyeWidth || eyeHeight_ != eyeHeight || format_ != format);
        if (geometryChanged && initialized_) {
            ReleaseGpuResources();
            initFailed_ = false;
        }
        api_ = &api;
        session_ = session;
        device_ = device;
        context_ = context;
        eyeWidth_ = eyeWidth;
        eyeHeight_ = eyeHeight;
        format_ = format;
        logFn_ = logFn;
        compiled.store(true, std::memory_order_release);
        SyncConfigFromLive();
    }

    // Ingest a fresh SubmitMode=4 native stereo pair directly from Darkness2VR's per-eye D3D11 textures.
    void OnNativeStereoEyes(ID3D11Texture2D* leftEye, ID3D11Texture2D* rightEye,
                            const XrCompositionLayerProjection& nativeLayer, std::int64_t displayTimeNs) noexcept {
        SyncConfigFromLive();
        if (!liveCfg_.frameGenEnabled) {
            if (initialized_) ReleaseGpuResources();
            return;
        }
        if (!leftEye || !rightEye || nativeLayer.viewCount != 2) return;
        D3D11_TEXTURE2D_DESC ld{}, rd{};
        leftEye->GetDesc(&ld);
        rightEye->GetDesc(&rd);
        if (ld.Width != rd.Width || ld.Height != rd.Height || ld.Width < 64u || ld.Height < 64u) return;
        if (!EnsureInitialized(ld.Width, ld.Height)) return;

        context_->CopyResource(inEye_[0], leftEye);
        context_->CopyResource(inEye_[1], rightEye);

        txfg_pose poses[2]{};
        txfg_fov fovs[2]{};
        for (int e = 0; e < 2; ++e) {
            const auto& v = nativeLayer.views[e];
            poses[e].orientation[0] = v.pose.orientation.x;
            poses[e].orientation[1] = v.pose.orientation.y;
            poses[e].orientation[2] = v.pose.orientation.z;
            poses[e].orientation[3] = v.pose.orientation.w;
            poses[e].position[0] = v.pose.position.x;
            poses[e].position[1] = v.pose.position.y;
            poses[e].position[2] = v.pose.position.z;
            fovs[e] = {v.fov.angleLeft, v.fov.angleRight, v.fov.angleUp, v.fov.angleDown};
        }

        bool reset = !haveNative_;
        if (!reset) {
            const std::int64_t dt = displayTimeNs - lastNativeTime_;
            if (dt <= 0 || dt > 200'000'000LL) {
                reset = true;
            } else {
                const float dx = poses[0].position[0] - lastNativePos_[0];
                const float dy = poses[0].position[1] - lastNativePos_[1];
                const float dz = poses[0].position[2] - lastNativePos_[2];
                const float dot = std::fabs(
                    poses[0].orientation[0] * lastNativeOri_[0] +
                    poses[0].orientation[1] * lastNativeOri_[1] +
                    poses[0].orientation[2] * lastNativeOri_[2] +
                    poses[0].orientation[3] * lastNativeOri_[3]);
                if (dx * dx + dy * dy + dz * dz > 0.25f || dot < 0.9659f) reset = true;
            }
        }
        if (reset) {
            ++resets_;
            resets.store(resets_, std::memory_order_relaxed);
        }

        if (bridge_.PushStereo(context_, inEye_[0], inEye_[1], ++pushSerial_, poses, fovs, reset)) {
            if (!reset && haveNative_ && displayTimeNs > lastNativeTime_) {
                const std::int64_t gap = displayTimeNs - lastNativeTime_;
                nativeIntervalNs_ = nativeIntervalNs_ > 0 ? (nativeIntervalNs_ * 3 + gap) / 4 : gap;
            } else {
                nativeIntervalNs_ = 0;
            }
            haveNative_ = true;
            lastWasReset_ = reset;
            lastNativeTime_ = displayTimeNs;
            for (int k = 0; k < 3; ++k) lastNativePos_[k] = poses[0].position[k];
            for (int k = 0; k < 4; ++k) lastNativeOri_[k] = poses[0].orientation[k];
            lastNativeLayer_ = nativeLayer;
            lastNativeViews_[0] = nativeLayer.views[0];
            lastNativeViews_[1] = nativeLayer.views[1];
            lastNativeLayer_.views = lastNativeViews_;
            haveLastNativeLayer_ = true;
            synthSinceNative_ = 0;
            pushedFrames.store(bridge_.PushedFrames(), std::memory_order_relaxed);
            PublishCapabilities();
        } else {
            haveNative_ = false;
            haveLastNativeLayer_ = false;
            lastWasReset_ = true;
            bridge_.ResetHistory();
        }
    }

    // Attempt to synthesize a causal 2x intermediate frame on a held/gap OpenXR slot and populate or
    // substitute end.layers[0] with our dedicated per-eye synthetic swapchains.
    bool TrySubstituteSynthetic(std::int64_t displayTimeNs, XrFrameEndInfo& end, float& outGpuMs) noexcept {
        outGpuMs = 0.0f;
        SyncConfigFromLive();
        if (!liveCfg_.frameGenEnabled) {
            if (initialized_) ReleaseGpuResources();
            return false;
        }
        if (!initialized_ || synthSwapchain_[0] == XR_NULL_HANDLE || synthSwapchain_[1] == XR_NULL_HANDLE) return false;

        const XrCompositionLayerProjection* baseProj = nullptr;
        if (end.layers && end.layerCount >= 1 && end.layers[0] &&
            end.layers[0]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
            baseProj = reinterpret_cast<const XrCompositionLayerProjection*>(end.layers[0]);
        } else if (haveLastNativeLayer_ && end.layerCount == 0) {
            baseProj = &lastNativeLayer_;
        }
        if (!baseProj || baseProj->viewCount != 2) return false;

        if (!haveNative_ || lastWasReset_ || bridge_.PushedFrames() < 2 || synthSinceNative_ >= 1u) {
            ++missedSlots_;
            missedSlots.store(missedSlots_, std::memory_order_relaxed);
            return false;
        }
        const std::int64_t elapsed = displayTimeNs - lastNativeTime_;
        if (elapsed <= 0 || nativeIntervalNs_ <= 0 || elapsed > (nativeIntervalNs_ * 5) / 4) {
            ++missedSlots_;
            missedSlots.store(missedSlots_, std::memory_order_relaxed);
            return false;
        }

        const float frac = std::clamp(static_cast<float>(elapsed) / static_cast<float>(nativeIntervalNs_), 0.20f, 0.50f);
        const float t = 1.0f + frac;

        for (int e = 0; e < 2; ++e) {
            if (!synthAcquired_[e]) {
                std::uint32_t idx{};
                XrSwapchainImageAcquireInfo acq{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                if (XR_FAILED(api_->xrAcquireSwapchainImage(synthSwapchain_[e], &acq, &idx)) ||
                    idx >= synthImages_[e].size()) {
                    ++missedSlots_;
                    missedSlots.store(missedSlots_, std::memory_order_relaxed);
                    return false;
                }
                synthAcquired_[e] = true;
                synthWaited_[e] = false;
                synthAcquiredIdx_[e] = idx;
            }
            if (!synthWaited_[e]) {
                XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                wait.timeout = 0;
                const XrResult wr = api_->xrWaitSwapchainImage(synthSwapchain_[e], &wait);
                if (wr != XR_SUCCESS) {
                    ++missedSlots_;
                    missedSlots.store(missedSlots_, std::memory_order_relaxed);
                    return false;
                }
                synthWaited_[e] = true;
            }
        }

        txfg_synth_result res{};
        const bool ok = bridge_.SynthesizeExtrap(context_, outEye_[0], outEye_[1], t, &res);
        if (!ok) {
            ReleaseWaitedSynthImages();
            fallbacks.store(bridge_.FallbackFrames(), std::memory_order_relaxed);
            return false;
        }

        for (int e = 0; e < 2; ++e) {
            context_->CopyResource(synthImages_[e][synthAcquiredIdx_[e]], outEye_[e]);
        }
        context_->Flush();

        if (!ReleaseWaitedSynthImages()) {
            ++missedSlots_;
            missedSlots.store(missedSlots_, std::memory_order_relaxed);
            return false;
        }

        synthLayer_ = *baseProj;
        synthLayer_.views = synthViews_;
        for (int e = 0; e < 2; ++e) {
            synthViews_[e] = baseProj->views[e];
            synthViews_[e].subImage.swapchain = synthSwapchain_[e];
            synthViews_[e].subImage.imageRect.offset = {0, 0};
            synthViews_[e].subImage.imageRect.extent = {static_cast<int32_t>(eyeWidth_), static_cast<int32_t>(eyeHeight_)};
            synthViews_[e].pose.orientation = {res.pose[e].orientation[0], res.pose[e].orientation[1],
                                               res.pose[e].orientation[2], res.pose[e].orientation[3]};
            synthViews_[e].pose.position = {res.pose[e].position[0], res.pose[e].position[1], res.pose[e].position[2]};
            synthViews_[e].fov = {res.fov[e].angle_left, res.fov[e].angle_right, res.fov[e].angle_up, res.fov[e].angle_down};
        }
        if (end.layerCount == 0) {
            substitutedLayers_[0] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&synthLayer_);
            end.layers = substitutedLayers_;
            end.layerCount = 1;
        } else {
            const std::uint32_t count = (std::min)(end.layerCount, static_cast<std::uint32_t>(12));
            for (std::uint32_t i = 0; i < count; ++i) substitutedLayers_[i] = end.layers[i];
            substitutedLayers_[0] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&synthLayer_);
            end.layers = substitutedLayers_;
        }
        ++synthSinceNative_;
        outGpuMs = res.gpu_ms_last > 0.0f ? res.gpu_ms_last : bridge_.LastGpuMs();
        return true;
    }

    void NoteDiscontinuity() noexcept {
        if (haveNative_) {
            haveNative_ = false;
            haveLastNativeLayer_ = false;
            lastWasReset_ = true;
            nativeIntervalNs_ = 0;
            synthSinceNative_ = 0;
            bridge_.ResetHistory();
        }
    }

    void AfterEndFrame(bool xrAccepted, bool nativeFresh, bool synthSubstituted,
                       float lastGpuMs, std::int64_t displayTimeNs, unsigned frameNumber) noexcept {
        if (xrAccepted) {
            if (nativeFresh) {
                ++nativeSubmitted_;
                ++winNative_;
                nativeSubmitted.store(nativeSubmitted_, std::memory_order_relaxed);
            } else if (synthSubstituted) {
                ++synthSubmitted_;
                ++winSynth_;
                synthSubmitted.store(synthSubmitted_, std::memory_order_relaxed);
                if (lastGpuMs > 0.0f) {
                    gpuMsHundredths.store(static_cast<unsigned>(std::lround(lastGpuMs * 100.0f)), std::memory_order_relaxed);
                }
            }
        }
        if (winStartTimeNs_ == 0) winStartTimeNs_ = displayTimeNs;
        const std::int64_t winDt = displayTimeNs - winStartTimeNs_;
        if (winDt >= 2'000'000'000LL) {
            const double sec = static_cast<double>(winDt) / 1e9;
            const double nFps = static_cast<double>(winNative_) / sec;
            const double sFps = static_cast<double>(winSynth_) / sec;
            const double tFps = nFps + sFps;
            nativeFpsTenths.store(static_cast<unsigned>(std::lround(nFps * 10.0)), std::memory_order_relaxed);
            synthFpsTenths.store(static_cast<unsigned>(std::lround(sFps * 10.0)), std::memory_order_relaxed);
            totalFpsTenths.store(static_cast<unsigned>(std::lround(tFps * 10.0)), std::memory_order_relaxed);
            if ((liveCfg_.frameGenEnabled || synthSubmitted_ > 0) && logFn_) {
                const auto caps = bridge_.QueryCapabilities();
                char buf[320]{};
                _snprintf_s(buf, sizeof(buf), _TRUNCATE,
                    "event=txrfg_status frame=%u enabled=%u profile=%d native_fps=%.1f synth_fps=%.1f total_fps=%.1f "
                    "gpu_ms=%.2f pushed=%llu native_sub=%llu synth_sub=%llu fallbacks=%llu missed=%llu resets=%llu "
                    "vis_mask=%u sm=0x%x m12=0x%x",
                    frameNumber, liveCfg_.frameGenEnabled ? 1u : 0u, liveCfg_.profile, nFps, sFps, tFps,
                    static_cast<double>(bridge_.LastGpuMs()),
                    static_cast<unsigned long long>(bridge_.PushedFrames()),
                    static_cast<unsigned long long>(nativeSubmitted_),
                    static_cast<unsigned long long>(synthSubmitted_),
                    static_cast<unsigned long long>(bridge_.FallbackFrames()),
                    static_cast<unsigned long long>(missedSlots_),
                    static_cast<unsigned long long>(resets_),
                    caps.hasVisibilityMask ? 1u : 0u, caps.maxShaderModel, caps.activeCrossGpuFlags);
                logFn_(buf);
            }
            winStartTimeNs_ = displayTimeNs;
            winNative_ = 0;
            winSynth_ = 0;
        }
    }

    XrSwapchain SynthSwapchainHandle(int eye) const noexcept { return (eye == 0 || eye == 1) ? synthSwapchain_[eye] : XR_NULL_HANDLE; }
    std::uint64_t NativeSubmitted() const noexcept { return nativeSubmitted_; }
    std::uint64_t SynthSubmitted() const noexcept { return synthSubmitted_; }
    std::uint64_t MissedSlots() const noexcept { return missedSlots_; }
    const txrfg_interop::D3D11Bridge& Bridge() const noexcept { return bridge_; }

    void Close() noexcept {
        ReleaseGpuResources();
        api_ = nullptr;
        session_ = XR_NULL_HANDLE;
        device_ = nullptr;
        context_ = nullptr;
    }

private:
    bool ReleaseWaitedSynthImages() noexcept {
        bool allOk = true;
        XrSwapchainImageReleaseInfo rel{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        for (int e = 0; e < 2; ++e) {
            if (synthAcquired_[e] && synthWaited_[e]) {
                const XrResult rr = api_->xrReleaseSwapchainImage(synthSwapchain_[e], &rel);
                if (XR_SUCCEEDED(rr)) {
                    synthAcquired_[e] = false;
                    synthWaited_[e] = false;
                } else {
                    allOk = false;
                }
            }
        }
        return allOk;
    }

    void SyncConfigFromLive() noexcept {
        const unsigned serial = configSerial.load(std::memory_order_acquire);
        if (serial == seenSerial_ && configSynced_) return;
        seenSerial_ = serial;
        configSynced_ = true;
        liveCfg_ = LoadLive();
        txrfg_interop::Config bc{};
        bc.profile = static_cast<txrfg_interop::OptProfile>(liveCfg_.profile);
        bc.frameGenEnabled = liveCfg_.frameGenEnabled;
        bc.crossGpuAuto = liveCfg_.crossGpuAuto;
        bc.gpuBudgetMs = static_cast<float>(liveCfg_.gpuBudgetTenthsMs) / 10.0f;
        bridge_.Configure(bc);
        active.store(liveCfg_.frameGenEnabled && initialized_, std::memory_order_relaxed);
        PublishCapabilities();
    }

    void PublishCapabilities() noexcept {
        const auto caps = bridge_.QueryCapabilities();
        initialized.store(initialized_, std::memory_order_relaxed);
        active.store(liveCfg_.frameGenEnabled && initialized_, std::memory_order_relaxed);
        hasVisMask.store(caps.hasVisibilityMask, std::memory_order_relaxed);
        maxShaderModel.store(caps.maxShaderModel, std::memory_order_relaxed);
        activeCrossGpuFlags.store(caps.activeCrossGpuFlags, std::memory_order_relaxed);
    }

    bool EnsureInitialized(unsigned eyeW, unsigned eyeH) noexcept {
        if (initialized_ && eyeW == eyeWidth_ && eyeH == eyeHeight_) return true;
        if (initialized_) {
            ReleaseGpuResources();
            initFailed_ = false;
        }
        if (initFailed_ || !api_ || session_ == XR_NULL_HANDLE || !device_ || !context_ || eyeW < 64u || eyeH < 64u) {
            return false;
        }
        eyeWidth_ = eyeW;
        eyeHeight_ = eyeH;
        const DXGI_FORMAT fmt = (format_ != DXGI_FORMAT_UNKNOWN) ? format_ : DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        if (!bridge_.Init(device_, eyeW, eyeH, fmt)) {
            initFailed_ = true;
            if (logFn_) logFn_("event=txrfg_init_failed stage=bridge_init");
            return false;
        }

        D3D11_TEXTURE2D_DESC td{};
        td.Width = eyeW;
        td.Height = eyeH;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = fmt;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_TEXTURE2D_DESC outTd = td;
        outTd.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        if (outTd.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) outTd.Format = DXGI_FORMAT_B8G8R8A8_TYPELESS;
        else if (outTd.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) outTd.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;

        for (int e = 0; e < 2; ++e) {
            if (FAILED(device_->CreateTexture2D(&td, nullptr, &inEye_[e])) ||
                FAILED(device_->CreateTexture2D(&outTd, nullptr, &outEye_[e]))) {
                ReleaseGpuResources();
                initFailed_ = true;
                if (logFn_) logFn_("event=txrfg_init_failed stage=eye_textures");
                return false;
            }
        }

        XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        ci.format = fmt;
        ci.sampleCount = 1;
        ci.width = eyeW;
        ci.height = eyeH;
        ci.faceCount = 1;
        ci.arraySize = 1;
        ci.mipCount = 1;
        for (int e = 0; e < 2; ++e) {
            if (XR_FAILED(api_->xrCreateSwapchain(session_, &ci, &synthSwapchain_[e])) ||
                synthSwapchain_[e] == XR_NULL_HANDLE) {
                synthSwapchain_[e] = XR_NULL_HANDLE;
                ReleaseGpuResources();
                initFailed_ = true;
                if (logFn_) logFn_("event=txrfg_init_failed stage=synth_swapchain");
                return false;
            }
            std::uint32_t count{};
            if (XR_FAILED(api_->xrEnumerateSwapchainImages(synthSwapchain_[e], 0, &count, nullptr)) || !count || count > 16) {
                ReleaseGpuResources();
                initFailed_ = true;
                return false;
            }
            std::vector<XrSwapchainImageD3D11KHR> imgs(count);
            for (auto& im : imgs) im.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
            if (XR_FAILED(api_->xrEnumerateSwapchainImages(synthSwapchain_[e], count, &count,
                                                           reinterpret_cast<XrSwapchainImageBaseHeader*>(imgs.data())))) {
                ReleaseGpuResources();
                initFailed_ = true;
                return false;
            }
            synthImages_[e].resize(count);
            for (std::uint32_t i = 0; i < count; ++i) synthImages_[e][i] = imgs[i].texture;
        }

        initialized_ = true;
        PublishCapabilities();
        if (logFn_) {
            const auto caps = bridge_.QueryCapabilities();
            char buf[256]{};
            _snprintf_s(buf, sizeof(buf), _TRUNCATE,
                "event=txrfg_initialized eye=%ux%u fmt=%u synth_images=%u sm=0x%x m12=0x%x mode=2x_extrap_world_only",
                eyeW, eyeH, static_cast<unsigned>(fmt), static_cast<unsigned>(synthImages_[0].size()),
                caps.maxShaderModel, caps.activeCrossGpuFlags);
            logFn_(buf);
        }
        return true;
    }

    void ReleaseGpuResources() noexcept {
        bridge_.Shutdown();
        for (int e = 0; e < 2; ++e) {
            if (inEye_[e]) { inEye_[e]->Release(); inEye_[e] = nullptr; }
            if (outEye_[e]) { outEye_[e]->Release(); outEye_[e] = nullptr; }
            synthImages_[e].clear();
            synthAcquired_[e] = false;
            synthWaited_[e] = false;
            synthAcquiredIdx_[e] = 0;
            if (synthSwapchain_[e] != XR_NULL_HANDLE && api_) {
                (void)api_->xrDestroySwapchain(synthSwapchain_[e]);
                synthSwapchain_[e] = XR_NULL_HANDLE;
            }
        }
        initialized_ = false;
        haveNative_ = false;
        haveLastNativeLayer_ = false;
        lastWasReset_ = true;
        nativeIntervalNs_ = 0;
        synthSinceNative_ = 0;
        initialized.store(false, std::memory_order_relaxed);
        active.store(false, std::memory_order_relaxed);
    }

    ApiT* api_{};
    XrSession session_{XR_NULL_HANDLE};
    ID3D11Device* device_{};
    ID3D11DeviceContext* context_{};
    unsigned eyeWidth_{}, eyeHeight_{};
    DXGI_FORMAT format_{DXGI_FORMAT_UNKNOWN};
    LogFn logFn_{};

    txrfg_interop::D3D11Bridge bridge_;
    LiveConfig liveCfg_{};
    unsigned seenSerial_{};
    bool configSynced_{};
    bool initialized_{};
    bool initFailed_{};

    ID3D11Texture2D* inEye_[2]{};
    ID3D11Texture2D* outEye_[2]{};
    XrSwapchain synthSwapchain_[2]{XR_NULL_HANDLE, XR_NULL_HANDLE};
    std::vector<ID3D11Texture2D*> synthImages_[2];
    bool synthAcquired_[2]{false, false};
    bool synthWaited_[2]{false, false};
    std::uint32_t synthAcquiredIdx_[2]{0, 0};

    XrCompositionLayerProjection lastNativeLayer_{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    XrCompositionLayerProjectionView lastNativeViews_[2]{};
    bool haveLastNativeLayer_{false};

    XrCompositionLayerProjection synthLayer_{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    XrCompositionLayerProjectionView synthViews_[2]{};
    const XrCompositionLayerBaseHeader* substitutedLayers_[12]{};

    std::uint64_t pushSerial_{};
    bool haveNative_{};
    bool lastWasReset_{true};
    std::int64_t lastNativeTime_{};
    std::int64_t nativeIntervalNs_{};
    float lastNativePos_[3]{};
    float lastNativeOri_[4]{};
    unsigned synthSinceNative_{};

    std::uint64_t nativeSubmitted_{};
    std::uint64_t synthSubmitted_{};
    std::uint64_t missedSlots_{};
    std::uint64_t resets_{};
    std::int64_t winStartTimeNs_{};
    std::uint64_t winNative_{};
    std::uint64_t winSynth_{};
};

} // namespace d2vr_txfg

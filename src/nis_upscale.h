// NIS upscaling bridge for SingularityVR - research/nvidia-vr-upscaling
//
// Uses official NVIDIA Image Scaling SDK v1.0.3 (MIT, (c) NVIDIA CORPORATION).
// See nis/licence.txt, nis/NIS_Config.h, nis/NIS_Scaler.h, nis/NIS_Main.hlsl.
//
// Design:
//   SBS D3D11 source (g_frameSrc11, full side-by-side)
//     -> per-eye NIS dispatch sampling eye viewport directly (NIS_VIEWPORT_SUPPORT=1)
//     -> per-eye UAV output texture (persistent)
//     -> CopyResource to OpenXR swapchain image (swapchains stay non-UAV)
//
// Why an intermediate: OpenXR swapchains are created without
// XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT (SteamVR/VirtualDesktop runtimes do
// not guarantee UAV support). Retaining normal COLOR_ATTACHMENT|TRANSFER_DST
// usage and using exactly one persistent UAV texture per eye + one final GPU
// copy is the minimum-copy correct path.

#pragma once

#include <cstring>
#include <cmath>
#include <cstdint>

namespace nis_upscale {

// ---------------------------------------------------------------- modes
enum Mode { kOff = 0, kNIS = 1, kNISSharpen = 2 };

inline Mode ParseMode(const char* s) {
    if (!s || !*s) return kOff;
    char tmp[32] = {};
    size_t n = 0;
    for (; s[n] && n < sizeof(tmp) - 1; ++n) {
        char c = s[n];
        if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
        tmp[n] = c;
    }
    tmp[n] = 0;
    if (!strcmp(tmp, "nis")) return kNIS;
    if (!strcmp(tmp, "nissharpen") || !strcmp(tmp, "nis_sharpen") ||
        !strcmp(tmp, "nis-sharpen") || !strcmp(tmp, "sharpen"))
        return kNISSharpen;
    return kOff;  // unknown -> safe fallback
}

inline const char* ModeName(Mode m) {
    switch (m) {
        case kNIS: return "NIS";
        case kNISSharpen: return "NISSharpen";
        default: return "Off";
    }
}

// ---------------------------------------------------------------- clamping
inline float ClampSharpness(float v) {
    if (!(v == v)) return 0.35f;
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

inline float ClampRenderScale(float v) {
    if (!(v == v)) return 1.0f;
    if (v < 0.5f) return 0.5f;
    if (v > 1.0f) return 1.0f;
    return v;
}

inline float ClampOutputScale(float v) {
    if (!(v == v)) return 1.0f;
    if (v < 1.0f) return 1.0f;
    if (v > 1.5f) return 1.5f;
    return v;
}

// ---------------------------------------------------------------- alignment
inline uint32_t AlignUp(uint32_t v, uint32_t a) { return (v + a - 1) / a * a; }

// Scaled SBS dimensions for a given render scale.
// recSbsW = recommended per-eye W * 2 (full SBS width), recH = recommended H.
// Same normalized scale on both axes (no geometry distortion).
// SBS width aligned to 4 (keeps each eye even for clean seam split), height to 2.
inline void ComputeRenderSBS(uint32_t recSbsW, uint32_t recH, float renderScale,
                             uint32_t* outW, uint32_t* outH) {
    float s = ClampRenderScale(renderScale);
    if (!recSbsW || !recH) { *outW = 0; *outH = 0; return; }
    uint32_t w = (uint32_t)(recSbsW * s + 0.5f);
    uint32_t h = (uint32_t)(recH * s + 0.5f);
    w = AlignUp(w, 4);
    h = AlignUp(h, 2);
    if (w < 640) w = 640;
    if (h < 480) h = 480;
    if (w > 16384) w = 16384;
    if (h > 16384) h = 16384;
    *outW = w; *outH = h;
}

// Per-eye input size derived from an SBS source (exact halves, no overlap).
inline void EyeInputFromSBS(uint32_t sbsW, uint32_t sbsH,
                            uint32_t* eyeW, uint32_t* eyeH) {
    *eyeW = sbsW / 2;
    *eyeH = sbsH;
}

// Output (swapchain) per-eye size: recommended * outputScale, aligned.
// Never derived from the (possibly reduced) render size.
inline void ComputeOutputEye(uint32_t recEyeW, uint32_t recEyeH, float outputScale,
                             uint32_t fallbackEyeW, uint32_t fallbackEyeH,
                             uint32_t* outW, uint32_t* outH) {
    float s = ClampOutputScale(outputScale);
    if (recEyeW && recEyeH) {
        *outW = AlignUp((uint32_t)(recEyeW * s + 0.5f), 2);
        *outH = AlignUp((uint32_t)(recEyeH * s + 0.5f), 2);
    } else {
        *outW = fallbackEyeW; *outH = fallbackEyeH;
    }
}

// Source viewport (eye region inside the full SBS texture).
// eye 0 = left half [0, eyeW), eye 1 = right half [eyeW, 2*eyeW).
// The two rects are disjoint by construction; the seam pixel belongs to
// exactly one eye and the scaler kernel never crosses it (each dispatch reads
// only its own viewport plus its own clamp-to-edge border).
struct Rect { uint32_t x, y, w, h; };

inline Rect EyeSrcRect(int eye, uint32_t sbsW, uint32_t sbsH) {
    Rect r = {};
    uint32_t eyeW = sbsW / 2;
    r.x = (eye == 1) ? eyeW : 0;
    r.y = 0;
    r.w = eyeW;
    r.h = sbsH;
    return r;
}

// Engagement predicate: the ONLY conditions under which NIS may run.
// Pure logic (no D3D) so the Off-mode regression is unit-testable:
// mode Off never engages, regardless of every other flag.
inline bool ShouldEngageUpscale(int mode, bool ready, bool failedOnce,
                                bool cropEyes, bool haveSrc) {
    if (mode != kNIS && mode != kNISSharpen) return false;  // Off AND bogus
    if (!ready || failedOnce) return false;
    if (!cropEyes || !haveSrc) return false;
    return true;
}

// Resource recreation predicate: recreate persistent NIS textures only when
// something that changes their shape changed.
inline bool NeedsRecreate(uint32_t curInW, uint32_t curInH,
                          uint32_t curOutW, uint32_t curOutH,
                          uint32_t wantInW, uint32_t wantInH,
                          uint32_t wantOutW, uint32_t wantOutH) {
    return curInW != wantInW || curInH != wantInH ||
           curOutW != wantOutW || curOutH != wantOutH;
}

// Expected dimensions table (documentation + tests):
//   recEye 2632x2684 -> recSBS 5264x2684
//   1.00 -> SBS 5264x2684, eye 2632x2684
//   0.85 -> SBS 4476x2282, eye 2238x2282
//   0.77 -> SBS 4056x2068, eye 2028x2068 (4053.3->4056; 2066.7->2068)
//   0.70 -> SBS 3688x1880, eye 1844x1880 (3684.8->3688; 1878.8->1880)
// Output at OutputScale=1.0 is always 2632x2684 per eye (from recommended).

}  // namespace nis_upscale

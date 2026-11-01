// src/dlss/dlss_jitter.h - sub-pixel Halton (2,3) projection jitter math & verified sign constants
// for DLSS 4.5 and FSR 3.1/4.
#pragma once
#include <stdint.h>

namespace d2vr::dlss::jitter {

// Halton radical inverse, index >= 1.
inline float halton(uint32_t i, uint32_t b) {
    float f = 1.0f, r = 0.0f;
    while (i) { f /= (float)b; r += f * (float)(i % b); i /= b; }
    return r;
}

// Phase i of an n-phase sequence: sample offset in render pixels, [-0.5, 0.5), x right, y down.
inline void phase_offset(uint32_t i, uint32_t n, float* sx, float* sy) {
    const uint32_t k = (n ? i % n : 0) + 1;
    *sx = halton(k, 2) - 0.5f;
    *sy = halton(k, 3) - 0.5f;
}

// Clip-space shift for a w x h viewport: clip.x += ax * clip.w, clip.y += ay * clip.w.
inline void ndc_shift(float sx, float sy, uint32_t w, uint32_t h, float* ax, float* ay) {
    *ax = w ? -2.0f * sx / (float)w : 0.0f;
    *ay = h ? 2.0f * sy / (float)h : 0.0f;
}

// Applies that shift to rows [first, last) of an uploaded c0..c3 row-vector WVP block.
inline void shift_rows(float* block, uint32_t start, uint32_t first, uint32_t last, float ax, float ay) {
    for (uint32_t r = first; r < last; ++r) {
        float* row = block + (r - start) * 4;
        row[0] += ax * row[3];
        row[1] += ay * row[3];
    }
}

// Verified by tools/dlss-host-tests.cpp ("projection jitter sign"): DLSS reads InJitterOffsetX/Y
// as where the content moved (-sx, -sy).
const float kReportX = -1.0f, kReportY = -1.0f;
const float kFsrReportX = -1.0f, kFsrReportY = -1.0f;
const float kFsrMvSign = 1.0f;

} // namespace d2vr::dlss::jitter

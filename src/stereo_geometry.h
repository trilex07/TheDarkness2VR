// stereo_geometry.h - shared stereo transform math (production + unit tests).
// Row-vector convention: clip = v * M (Evolution Engine / D3D9).
#pragma once
#include "stereo_math.h"
#include <cmath>
#include <cstdint>

namespace d2vr_stereo {

// Captured MotionBlur_16 VS, full-byte FNV identity and CTAB register proof.
// current is the resolved WVP compensation base, not merely its CTAB label.
// Unknown shaders/registers and head-history-dependent paths remain unchanged.
inline int PreviousProjectionBase(uint32_t hash, int current, int previous,
                                  bool enabled, bool headGeometry) {
    return enabled && !headGeometry && hash==0x73EBA724u && current==0 && previous==4 ? 4 : -1;
}

// A centered blit samples source NDC = output NDC * crop. The submitted
// frustum must describe those rays, not the headset's recommended frustum.
inline bool RenderedHalfFov(float projectionScale, float matrixScale, float crop,
                            float* angle) {
    if (!angle || !std::isfinite(projectionScale) || !std::isfinite(matrixScale) ||
        !std::isfinite(crop) || projectionScale <= 0 || matrixScale <= 0 ||
        crop <= 0 || crop > 1) return false;
    *angle = std::atan(crop / (projectionScale * matrixScale));
    return std::isfinite(*angle) && *angle > 0;
}

struct EyeShiftParams {
    float separation = 0.0f;
    float convergence = 2.0f;
    int mode = 0;  // 0 = translation (clip.x += Px*dx), 1 = shear
    float transX = 0.0f;
    float transY = 0.0f;
    float fovScaleX = 1.0f;
    float fovScaleY = 1.0f;
    bool headGeometry = false;
    float headClip[16]{};
};

inline float RowLength(const float* m, int col) {
    return std::sqrt(m[col] * m[col] + m[4 + col] * m[4 + col] + m[8 + col] * m[8 + col]);
}

inline void EstimatePxPy(const float* p, bool cameraKnown, float cameraPx, float cameraPy,
                         float* px, float* py) {
    const float l0 = RowLength(p, 0);
    const float l1 = RowLength(p, 1);
    const float lw = RowLength(p, 3);
    *px = cameraKnown ? cameraPx : ((lw > 1e-5f) ? (l0 / lw) : 1.0f);
    *py = cameraKnown ? cameraPy : ((lw > 1e-5f) ? (l1 / lw) : 1.0f);
}

// Same transform as stereo_replay.inl ApplyEyeShiftToMatrix (without projection estimate publish).
inline void ApplyEyeShiftToMatrix(float* p, int n, const EyeShiftParams& sp, bool cameraKnown,
                                  float cameraPx, float cameraPy, const float headClip[16] = nullptr) {
    if (n < 4 || !p) return;
    float px = 0, py = 0;
    EstimatePxPy(p, cameraKnown, cameraPx, cameraPy, &px, &py);

    if (sp.fovScaleX != 1.0f) {
        for (int i = 0; i < 4; ++i) p[i * 4 + 0] *= sp.fovScaleX;
    }
    if (sp.fovScaleY != 1.0f) {
        for (int i = 0; i < 4; ++i) p[i * 4 + 1] *= sp.fovScaleY;
    }

    if (sp.mode == 0) {
        p[12] += px * sp.transX * sp.fovScaleX;
        p[13] += py * sp.transY * sp.fovScaleY;
    } else {
        for (int i = 0; i < 4; ++i) p[i * 4 + 0] += sp.separation * p[i * 4 + 3];
        p[12] -= sp.separation * sp.convergence;
    }
    if (sp.headGeometry && headClip) d2vr_math::Multiply(p, headClip, p);
}

inline void BuildPerEyeTranslation(int eye, int polarity, float fullIpd, float* transXOut) {
    const float sign = (polarity == 0) ? ((eye == 0) ? +1.0f : -1.0f) : ((eye == 0) ? -1.0f : +1.0f);
    *transXOut = sign * 0.5f * fullIpd;
}

// Clip-space x for a view-space point (x,y,z,1) after row-vector multiply by 4x4 WVP.
inline bool ProjectClipX(const float m[16], float x, float y, float z, float* clipX, float* clipW) {
    const float cx = x * m[0] + y * m[4] + z * m[8] + m[12];
    const float cw = x * m[3] + y * m[7] + z * m[11] + m[15];
    if (!std::isfinite(cx) || !std::isfinite(cw) || std::fabs(cw) < 1e-8f) return false;
    *clipX = cx;
    *clipW = cw;
    return true;
}

inline float NdcXFromClip(float clipX, float clipW) { return clipX / clipW; }

// Inter-ocular distance in metres from OpenXR-style per-eye positions (same orientation).
inline float InterEyeDistanceMeters(float lx, float ly, float lz, float rx, float ry, float rz) {
    const float dx = lx - rx, dy = ly - ry, dz = lz - rz;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline float GameUnitsFromMeters(float metres, float gameUnitsPerMeter) {
    return metres * gameUnitsPerMeter;
}

// Minimal OpenXR FOV -> D3D-style row-major projection (for reference tests).
inline void ProjectionFromOpenXrFov(float tanLeft, float tanRight, float tanUp, float tanDown,
                                    float nearZ, float farZ, float out[16]) {
    const float invW = 1.0f / (tanRight - tanLeft);
    const float invH = 1.0f / (tanUp - tanDown);
    for (int i = 0; i < 16; ++i) out[i] = 0.0f;
    out[0] = 2.0f * invW;
    out[5] = 2.0f * invH;
    out[8] = (tanRight + tanLeft) * invW;
    out[9] = (tanUp + tanDown) * invH;
    out[10] = farZ / (nearZ - farZ);
    out[11] = -1.0f;
    out[14] = (nearZ * farZ) / (nearZ - farZ);
    out[15] = 0.0f;
}

struct Vec3 {
    float x, y, z;
};
inline Vec3 Add(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vec3 Scale(Vec3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
inline float Len(Vec3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
inline float Dist(Vec3 a, Vec3 b) { return Len({ a.x - b.x, a.y - b.y, a.z - b.z }); }

// Head midpoint from left/right eye positions (OpenXR view poses).
inline Vec3 EyeMidpoint(Vec3 left, Vec3 right) { return Scale(Add(left, right), 0.5f); }

// Deterministic PRNG for fuzz tests (xorshift32).
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1u) {}
    uint32_t NextU32() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float NextFloat01() { return (NextU32() & 0xFFFFFFu) / float(0x1000000u); }
    float Range(float lo, float hi) { return lo + (hi - lo) * NextFloat01(); }
};

}  // namespace d2vr_stereo

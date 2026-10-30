// stereo_math.h - pure row-vector matrix helpers for the Darkness2VR stereo pipeline.
// No D3D dependencies so the unit tests can exercise them directly.
//
// Engine convention (verified against captured Evolution Engine shader bytecode and the
// extracted DeferredLight/DeferredShadow shader libraries): row vectors, clip = v * M.
//
// Proven facts these helpers rely on (evidence: run_40332 frame traces + shader CTABs):
//   * WorldViewProjection lives at c0..c3, ScreenTextureProjection at c4..c7/c8..c11 and
//     is exactly WVP composed with the viewport bias S: u=(x+w)/2, v=(w-y)/2.
//   * Registers like c8..c11 of un-analyzed shaders are OFTEN light parameters
//     (LightViewPosRadius, ShadowParams, light direction) and must never be shifted.
//   * "InverseProjection" reconstructs VIEW space (diagonal inverse of the perspective
//     projection); the height-fog variant carries an inverse VIEW projection (world
//     space). Both are handled by the same rotation-aware translation rule.
#pragma once
#include <cmath>
#if defined(_M_IX86) || defined(_M_X64) || defined(__SSE__)
#include <xmmintrin.h>
#endif

namespace d2vr_math {

// Alias-safe row-vector composition. Used only on identified projection blocks.
inline void Multiply(const float a[16], const float b[16], float out[16]) {
#if defined(_M_IX86) || defined(_M_X64) || defined(__SSE__)
    const __m128 b0=_mm_loadu_ps(b),b1=_mm_loadu_ps(b+4),b2=_mm_loadu_ps(b+8),b3=_mm_loadu_ps(b+12);
    float result[16];
    // Keep the initial +0 operation: folding it away changes signed zeros
    // for underflowed products under the game's current MXCSR rounding mode.
    volatile float initial=0.0f;
    for(unsigned r=0;r<4;++r) {
        // Match scalar +0, then k=0..3 accumulation exactly. No reassociation,
        // approximate reciprocals or FMA. Read every row before writing output
        // so full and partial overlaps with either source remain valid.
        __m128 sum=_mm_set1_ps(initial);
        sum=_mm_add_ps(sum,_mm_mul_ps(_mm_set1_ps(a[r*4]),b0));
        sum=_mm_add_ps(sum,_mm_mul_ps(_mm_set1_ps(a[r*4+1]),b1));
        sum=_mm_add_ps(sum,_mm_mul_ps(_mm_set1_ps(a[r*4+2]),b2));
        sum=_mm_add_ps(sum,_mm_mul_ps(_mm_set1_ps(a[r*4+3]),b3));
        _mm_storeu_ps(result+r*4,sum);
    }
    for(unsigned r=0;r<4;++r)_mm_storeu_ps(out+r*4,_mm_loadu_ps(result+r*4));
#else
    float result[16]{};
    for (int r=0;r<4;++r) for(int c=0;c<4;++c)
        for(int k=0;k<4;++k) result[r*4+c]+=a[r*4+k]*b[k*4+c];
    for(int i=0;i<16;++i) out[i]=result[i];
#endif
}

inline bool Invert(const float in[16], float out[16]) {
    double a[4][8]{};
    for(int r=0;r<4;++r) for(int c=0;c<4;++c) {
        if(!std::isfinite(in[r*4+c])) return false;
        a[r][c]=in[r*4+c]; a[r][c+4]=(r==c);
    }
    for(int c=0;c<4;++c) {
        int pivot=c;
        for(int r=c+1;r<4;++r) if(std::fabs(a[r][c])>std::fabs(a[pivot][c])) pivot=r;
        if(std::fabs(a[pivot][c])<1e-10) return false;
        for(int k=0;k<8;++k) { double t=a[c][k]; a[c][k]=a[pivot][k]; a[pivot][k]=t; }
        const double scale=a[c][c];
        for(double& v:a[c]) v/=scale;
        for(int r=0;r<4;++r) if(r!=c) {
            const double f=a[r][c];
            for(int k=0;k<8;++k) a[r][k]-=f*a[c][k];
        }
    }
    for(int r=0;r<4;++r) for(int c=0;c<4;++c) {
        if(!std::isfinite(a[r][c+4])) return false;
    }
    for(int r=0;r<4;++r) for(int c=0;c<4;++c) out[r*4+c]=float(a[r][c+4]);
    return true;
}

// Camera-local to tracking-origin transform, row-vector convention. Reflect Z
// to convert OpenXR RH/-Z forward to the engine's D3D LH/+Z forward.
inline bool TrackingPose(const float q[4], const float p[3], float out[16]) {
    double n=0;
    for(int i=0;i<4;++i) { if(!std::isfinite(q[i])) return false; n+=double(q[i])*q[i]; }
    if(n<1e-8) return false;
    for(int i=0;i<3;++i) if(!std::isfinite(p[i])) return false;
    const float s=float(1/std::sqrt(n));
    const float x=-q[0]*s,y=-q[1]*s,z=q[2]*s,w=q[3]*s;
    const float m[16]={1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w),0,
        2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w),0,
        2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y),0,
        p[0],p[1],-p[2],1};
    for(int i=0;i<16;++i) out[i]=m[i];
    return true;
}

// Both tracking translation and stereo baseline must use the same world units.
// Subtract the origin before scaling to avoid cancellation at large XR origins.
inline bool RelativeTrackingView(const float q[4], const float p[3],
                                 const float rq[4], const float rp[3],
                                 float unitsPerMeter, float out[16]) {
    if(!std::isfinite(unitsPerMeter) || unitsPerMeter<=0) return false;
    float relative[3],zero[3]{},current[16],reference[16],inverseCurrent[16];
    for(int i=0;i<3;++i) {
        if(!std::isfinite(p[i]) || !std::isfinite(rp[i])) return false;
        relative[i]=(p[i]-rp[i])*unitsPerMeter;
        if(!std::isfinite(relative[i])) return false;
    }
    if(!TrackingPose(q,relative,current) || !TrackingPose(rq,zero,reference) ||
       !Invert(current,inverseCurrent)) return false;
    Multiply(reference,inverseCurrent,out);
    return true;
}

// A real view transform conjugated into clip space, not a UV displacement.
// P^-1 * deltaView * P changes perspective/depth, including roll and forward lean.
inline bool HeadClipTransform(const float inverseProjection[16], const float deltaView[16],
                              float clip[16], float inverseClip[16]) {
    float projection[16], tmp[16];
    if(!Invert(inverseProjection,projection)) return false;
    Multiply(inverseProjection,deltaView,tmp); Multiply(tmp,projection,clip);
    return Invert(clip,inverseClip);
}

// True for the engine's view-space InverseProjection: inverse of the D3D perspective
// projection (diagonal x/y, z/w block). A world-space inverse carries the view rotation
// in rows 0..1 and the translation in row 3 and fails the off-diagonal test.
inline bool IsViewInverseProjection(const float m[16]) {
    if (!m) return false;
    const float off = std::fabs(m[1]) + std::fabs(m[2]) + std::fabs(m[3]) +
                      std::fabs(m[4]) + std::fabs(m[6]) + std::fabs(m[7]);
    return off <= 1e-4f && std::fabs(m[11]) > 1e-4f && m[0] > 0.125f && m[0] < 5.0f &&
           m[5] > 0.0625f && m[5] < 5.0f;
}

// Structural identity of the ScreenTextureProjection: STP = WVP * S.
inline bool IsScreenTextureProjection(const float stp[16], const float wvp[16]) {
    if (!stp || !wvp) return false;
    float scale = 1.0f;
    for (int i = 0; i < 16; ++i) {
        const float a = std::fabs(wvp[i]);
        if (a > scale) scale = a;
    }
    const float tol = 5e-3f * scale;
    for (int r = 0; r < 4; ++r) {
        const float* s = stp + r * 4;
        const float* w = wvp + r * 4;
        if (std::fabs(s[0] - 0.5f * (w[0] + w[3])) > tol) return false;
        if (std::fabs(s[1] - 0.5f * (w[3] - w[1])) > tol) return false;
        if (std::fabs(s[2] - w[2]) > tol) return false;
        if (std::fabs(s[3] - w[3]) > tol) return false;
    }
    return true;
}

// STP -> clip-space matrix (undo the viewport bias): x=2u-w, y=w-2v.
inline void UnbiasScreenTextureProjection(float m[16]) {
    for (int r = 0; r < 4; ++r) {
        m[r * 4 + 0] = 2.0f * m[r * 4 + 0] - m[r * 4 + 3];
        m[r * 4 + 1] = m[r * 4 + 3] - 2.0f * m[r * 4 + 1];
    }
}

// clip-space matrix -> STP (restore the viewport bias).
inline void RebiasScreenTextureProjection(float m[16]) {
    for (int r = 0; r < 4; ++r) {
        m[r * 4 + 0] = 0.5f * (m[r * 4 + 0] + m[r * 4 + 3]);
        m[r * 4 + 1] = 0.5f * (m[r * 4 + 3] - m[r * 4 + 1]);
    }
}

// Eye translation to subtract from a reconstruction matrix' OUTPUT space.
// For a view-space inverse the output space is the view space; for an inverse view
// projection (world space) the same rule rotates automatically because rows 0..1 of the
// matrix are (1/Px)*invViewRow0 / (1/Py)*invViewRow1 - the norms cancel the projection.
// Returns false when the matrix has no usable rows.
inline bool InverseMatrixTranslation(const float m[16], float ex, float ey, float t[3]) {
    if (!m || !t) return false;
    const float l0 = std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
    const float l1 = std::sqrt(m[4] * m[4] + m[5] * m[5] + m[6] * m[6]);
    if (l0 < 1e-6f || l1 < 1e-6f) {
        t[0] = t[1] = t[2] = 0.0f;
        return false;
    }
    const float px = 1.0f / l0, py = 1.0f / l1;
    t[0] = ex * px * m[0] + ey * py * m[4];
    t[1] = ex * px * m[1] + ey * py * m[5];
    t[2] = ex * px * m[2] + ey * py * m[6];
    return true;
}

// Post-multiply by T(-t): the reconstructed position loses t after the perspective
// divide (v*T(-t) scales the offset by w, the shader divides by w afterwards).
inline void SubtractOutputTranslation(float m[16], const float t[3]) {
    if (!m || !t) return;
    for (int r = 0; r < 4; ++r) {
        m[r * 4 + 0] -= t[0] * m[r * 4 + 3];
        m[r * 4 + 1] -= t[1] * m[r * 4 + 3];
        m[r * 4 + 2] -= t[2] * m[r * 4 + 3];
    }
}

// Projection coefficients from a WVP block: Px = |col0|/|col3|, Py = |col1|/|col3|.
// Valid for uniformly scaled objects only - callers publish through a median filter.
inline bool EstimateProjection(const float m[16], float* px, float* py) {
    if (!m || !px || !py) return false;
    const float l0 = std::sqrt(m[0] * m[0] + m[4] * m[4] + m[8] * m[8]);
    const float l1 = std::sqrt(m[1] * m[1] + m[5] * m[5] + m[9] * m[9]);
    const float lw = std::sqrt(m[3] * m[3] + m[7] * m[7] + m[11] * m[11]);
    if (lw <= 1e-5f) return false;
    *px = l0 / lw;
    *py = l1 / lw;
    return true;
}

// ViewExtents (DeferredLight family) converts NDC to a view-space direction:
//   x_view = ndc.x * z * ViewExtents.x
// A widened render scales NDC, so ViewExtents.x/.y must follow the same scale or the
// projected light/shadow compresses toward the screen centre and slides with the camera.
inline void ApplyViewExtentsScale(float v[4], float fovScaleX, float fovScaleY) {
    if (!v) return;
    if (fovScaleX > 0.0f && fovScaleX != 1.0f) v[0] /= fovScaleX;
    if (fovScaleY > 0.0f && fovScaleY != 1.0f) v[1] /= fovScaleY;
}

}  // namespace d2vr_math

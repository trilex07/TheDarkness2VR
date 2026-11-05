#pragma once
#include "stereo_math.h"

namespace d2vr_reflection_eye {
// Admitted VS12CCC8DB / VS293F456B / skinned VS1CB7CEE2:
// c8-position followed by tangent DP3s (after bone deformation when skinned).
// Camera origin in a row-vector perspective projects to (0,0,k,0).
inline bool Center(const float matrix[16],float out[3]) {
    if(!matrix || !out)return false;
    const double perspective=double(matrix[3])*matrix[3]+
        double(matrix[7])*matrix[7]+double(matrix[11])*matrix[11];
    if(!std::isfinite(perspective) || perspective<1e-8)return false;
    float inverse[16];
    if(!d2vr_math::Invert(matrix,inverse) || !std::isfinite(inverse[11]) ||
       std::fabs(inverse[11])<1e-8f)return false;
    float result[3];
    for(int i=0;i<3;++i) {
        result[i]=inverse[8+i]/inverse[11];
        if(!std::isfinite(result[i]))return false;
    }
    for(int i=0;i<3;++i)out[i]=result[i];
    return true;
}
inline bool Correct(const float rawWvp[16],const float effectiveWvp[16],
                    const float nativeEye[4],float out[4]) {
    if(!nativeEye || !out || !std::isfinite(nativeEye[3]))return false;
    float raw[3],eye[3];
    if(!Center(rawWvp,raw) || !Center(effectiveWvp,eye))return false;
    double error=0;
    for(int i=0;i<3;++i) {
        if(!std::isfinite(nativeEye[i]))return false;
        const double d=double(raw[i])-nativeEye[i];error+=d*d;
    }
    // Reject unknown/stale register ownership rather than changing a light or
    // another camera's parameter. Captured float-bank residuals are <0.00042.
    if(error>1e-6)return false;
    for(int i=0;i<3;++i)out[i]=eye[i];
    out[3]=nativeEye[3]; // animation time, not a homogeneous coordinate
    return true;
}
}

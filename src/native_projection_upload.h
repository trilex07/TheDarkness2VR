#pragma once
#include "stereo_math.h"
#include <cstring>
namespace d2vr_native_projection {
// Pinned native InverseProjection producer AD4021..AD4067 copies scene+100,
// scales element14 by [F185C8]=0.95, then calls BBE8F0 to invert it. This is
// shader reconstruction data, not the camera's unmodified culling projection.
// The runtime installer separately validates that producer and its constant.
inline bool ExpectedInverseUpload(const float* cameraProjection,float* output) {
    if(!cameraProjection || !output)return false;
    float reconstruction[16];std::memcpy(reconstruction,cameraProjection,sizeof(reconstruction));
    reconstruction[14]*=.95f;
    return d2vr_math::Invert(reconstruction,output);
}
}

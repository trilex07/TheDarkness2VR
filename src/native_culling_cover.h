#pragma once
#include "native_camera_cover.h"
#include "stereo_math.h"

namespace d2vr_culling_cover {
// Prepare separate culling input; never edit the render camera's projection,
// FOV or view-projection. The native frustum contains owning vector pointers:
// copying the camera/frustum object is not a valid way to construct a scratch one.
struct alignas(16) Input {
    float viewProjection[16]{},inverseViewProjection[16]{};
    float fov=0;
};
inline bool Prepare(const float* view,const float* projection,const float* nativeViewProjection,
                    float fov,float aspect,float nearZ,float farZ,const float eyes[2][4],
                    float coverage,float crop,Input& output) {
    if(!view || !nativeViewProjection ||
       !d2vr_camera_cover::Projection(projection,fov,aspect,nearZ,farZ))return false;
    float current[16];d2vr_math::Multiply(view,projection,current);
    for(int i=0;i<16;++i)if(!std::isfinite(current[i]) || !std::isfinite(nativeViewProjection[i]) ||
        std::fabs(current[i]-nativeViewProjection[i])>2e-4f*(1+std::fabs(current[i])))return false;
    d2vr_camera_cover::Plan plan;
    if(!d2vr_camera_cover::Cover(fov,aspect,eyes,coverage,crop,plan))return false;
    float wider[16];std::memcpy(wider,projection,sizeof(wider));
    wider[0]=plan.px;wider[5]=plan.py;
    Input candidate;candidate.fov=plan.fov;
    d2vr_math::Multiply(view,wider,candidate.viewProjection);
    if(!d2vr_math::Invert(candidate.viewProjection,candidate.inverseViewProjection))return false;
    output=candidate;return true;
}
}

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
    bool stereoUnion=false;
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
// Fit a single off-axis frustum around both actual eye projections. The eyes
// must share depth/horizon planes, as the production scale/head/eye transform
// does. Work in camera space so large world translations do not corrupt the
// fit. Positive homogeneous w makes each fractional coordinate's extrema occur
// at the eight vertices; fitting those vertices therefore contains each volume.
inline bool PrepareStereo(const float* view,const float* projection,const float* nativeVP,
                          const float eyeProjection[2][16],float fov,float aspect,
                          float nearZ,float farZ,Input& output) {
    if(!view || !nativeVP || !eyeProjection ||
       !d2vr_camera_cover::Projection(projection,fov,aspect,nearZ,farZ))return false;
    for(unsigned row=0;row<4;++row)if(!std::isfinite(view[row*4+3]) ||
        std::fabs(view[row*4+3]-(row==3?1.f:0.f))>2e-4f)return false;
    float check[16];d2vr_math::Multiply(view,projection,check);
    for(int i=0;i<16;++i)if(!std::isfinite(check[i]) || !std::isfinite(nativeVP[i]) ||
        std::fabs(check[i]-nativeVP[i])>2e-4f*(1+std::fabs(check[i])))return false;
    // Eye0 maps to the identity in its own clip coordinates. Include its
    // exact bounds directly, avoiding an inverse/multiply and its roundoff.
    float lower[2]={-1,-1},upper[2]={1,1};
    for(unsigned eye=1;eye<2;++eye) {
        float inverse[16],relative[16];
        if(!d2vr_math::Invert(eyeProjection[eye],inverse))return false;
        d2vr_math::Multiply(inverse,eyeProjection[0],relative);
        // Preserve the native depth interval. Do not extend the far plane past
        // the projective horizon, which would make the native AABB unbounded.
        for(unsigned row=0;row<4;++row)for(unsigned col=2;col<4;++col)
            if(!std::isfinite(relative[row*4+col]) ||
               std::fabs(relative[row*4+col]-(row==col?1.f:0.f))>2e-4f)return false;
        for(unsigned corner=0;corner<8;++corner) {
            const float p[4]={corner&1?1.f:-1.f,corner&2?1.f:-1.f,corner&4?1.f:0.f,1};
            float clip[4]{};
            for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)
                clip[col]+=p[row]*relative[row*4+col];
            if(!std::isfinite(clip[3]) || clip[3]<=0)return false;
            for(unsigned axis=0;axis<2;++axis) {
                const float value=clip[axis]/clip[3];
                if(!std::isfinite(value) || std::fabs(value)>1e6f)return false;
                lower[axis]=(std::min)(lower[axis],value);
                upper[axis]=(std::max)(upper[axis],value);
            }
        }
    }
    float fit[16]{};fit[10]=fit[15]=1;
    for(unsigned axis=0;axis<2;++axis) {
        const float padding=2e-4f*(1+(std::max)(std::fabs(lower[axis]),std::fabs(upper[axis])));
        lower[axis]-=padding;upper[axis]+=padding;
        const float span=upper[axis]-lower[axis];
        if(!std::isfinite(span) || span<=1e-5f)return false;
        fit[axis*5]=2/span;
        fit[12+axis]=-(upper[axis]+lower[axis])/span;
    }
    float coveredProjection[16];
    d2vr_math::Multiply(eyeProjection[0],fit,coveredProjection);
    Input candidate;candidate.stereoUnion=true;
    d2vr_math::Multiply(view,coveredProjection,candidate.viewProjection);
    if(!d2vr_math::Invert(candidate.viewProjection,candidate.inverseViewProjection))return false;
    for(float value:candidate.inverseViewProjection)if(!std::isfinite(value))return false;
    for(unsigned corner=0;corner<8;++corner) {
        const float p[4]={corner&1?1.f:-1.f,corner&2?1.f:-1.f,corner&4?1.f:0.f,1};
        double w=0;
        for(unsigned row=0;row<4;++row)w+=double(p[row])*candidate.inverseViewProjection[row*4+3];
        if(!std::isfinite(w) || w<1e-8)return false;
    }
    output=candidate;return true;
}
}

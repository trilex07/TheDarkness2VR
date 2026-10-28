#pragma once
#include "native_camera_cover.h"
#include "stereo_math.h"
#include <limits>

namespace d2vr_culling_cover {
// Prepare separate culling input; never edit the render camera's projection,
// FOV or view-projection. The native frustum contains owning vector pointers:
// copying the camera/frustum object is not a valid way to construct a scratch one.
struct alignas(16) Input {
    float viewProjection[16]{},inverseViewProjection[16]{};
    float fov=0;
    bool stereoUnion=false;
    float lower[3]{},upper[3]{};
    bool haveBounds=false;
    double corners[16][3]{};
    bool haveCorners=false;
};
// Keep camera-local inversions in double precision. Inverting a rounded world
// VP loses far-corner accuracy when the native camera has a large translation.
inline bool InvertBoundsMatrix(const float* matrix,double inverse[4][4]) {
    double a[4][8]{};
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c) {
        if(!std::isfinite(matrix[r*4+c]))return false;
        a[r][c]=matrix[r*4+c];a[r][c+4]=r==c?1.:0.;
    }
    for(unsigned c=0;c<4;++c) {
        unsigned pivot=c;
        for(unsigned r=c+1;r<4;++r)if(std::fabs(a[r][c])>std::fabs(a[pivot][c]))pivot=r;
        if(std::fabs(a[pivot][c])<1e-10)return false;
        for(unsigned k=0;k<8;++k)std::swap(a[c][k],a[pivot][k]);
        const double scale=a[c][c];for(double& value:a[c])value/=scale;
        for(unsigned r=0;r<4;++r)if(r!=c) {
            const double factor=a[r][c];
            for(unsigned k=0;k<8;++k)a[r][k]-=factor*a[c][k];
        }
    }
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c) {
        inverse[r][c]=a[r][c+4];if(!std::isfinite(inverse[r][c]))return false;
    }
    return true;
}
inline bool PrepareStereoBounds(const float* view,const float eyeProjection[2][16],Input& candidate) {
    double inverseView[4][4],lower[3],upper[3];
    if(!InvertBoundsMatrix(view,inverseView))return false;
    for(unsigned axis=0;axis<3;++axis){lower[axis]=std::numeric_limits<double>::infinity();upper[axis]=-lower[axis];}
    for(unsigned eye=0;eye<2;++eye) {
        double inverseEye[4][4];if(!InvertBoundsMatrix(eyeProjection[eye],inverseEye))return false;
        for(unsigned corner=0;corner<8;++corner) {
            const double p[4]={corner&1?1.:-1.,corner&2?1.:-1.,corner&4?1.:0.,1.};
            double local[4]{},world[4]{};
            for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)local[col]+=p[row]*inverseEye[row][col];
            for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)world[col]+=local[row]*inverseView[row][col];
            if(!std::isfinite(world[3]) || world[3]<1e-8)return false;
            for(unsigned axis=0;axis<3;++axis) {
                const double value=world[axis]/world[3];if(!std::isfinite(value))return false;
                candidate.corners[eye*8+corner][axis]=value;
                lower[axis]=(std::min)(lower[axis],value);upper[axis]=(std::max)(upper[axis],value);
            }
        }
    }
    for(unsigned axis=0;axis<3;++axis) {
        // Outward float rounding; no guessed world-space expansion constant.
        candidate.lower[axis]=std::nextafter(float(lower[axis]),-std::numeric_limits<float>::infinity());
        candidate.upper[axis]=std::nextafter(float(upper[axis]),std::numeric_limits<float>::infinity());
        if(!std::isfinite(candidate.lower[axis]) || !std::isfinite(candidate.upper[axis]))return false;
    }
    candidate.haveBounds=candidate.haveCorners=true;return true;
}
inline bool ValidStereoBounds(const Input& input) {
    if(!input.haveBounds)return false;
    for(unsigned axis=0;axis<3;++axis)
        if(!std::isfinite(input.lower[axis]) || !std::isfinite(input.upper[axis]) ||
           input.lower[axis]>input.upper[axis])return false;
    return true;
}
inline bool ExpandOwnedBounds(const Input& input,float bounds[8]) {
    if(!ValidStereoBounds(input))return false;
    for(unsigned axis=0;axis<3;++axis)
        if(!std::isfinite(bounds[axis]) || !std::isfinite(bounds[4+axis]) ||
           bounds[axis]>bounds[4+axis])return false;
    // Keep the native bounds too: the correction may only widen, never shrink.
    for(unsigned axis=0;axis<3;++axis) {
        bounds[axis]=(std::min)(bounds[axis],input.lower[axis]);
        bounds[4+axis]=(std::max)(bounds[4+axis],input.upper[axis]);
    }
    return true;
}
inline bool ValidStereoFrustum(const Input& input) {
    if(!ValidStereoBounds(input) || !input.haveCorners)return false;
    for(const auto& corner:input.corners)for(unsigned axis=0;axis<3;++axis)
        if(!std::isfinite(corner[axis]) || corner[axis]<input.lower[axis] || corner[axis]>input.upper[axis])return false;
    return true;
}
inline bool ExpandOwnedFrustum(const Input& input,float planes[24],float bounds[8]) {
    if(!ValidStereoFrustum(input))return false;
    float expandedBounds[8];std::memcpy(expandedBounds,bounds,sizeof(expandedBounds));
    if(!ExpandOwnedBounds(input,expandedBounds))return false;
    float offsets[6];
    for(unsigned plane=0;plane<6;++plane) {
        const float* p=planes+plane*4;
        double length=0;
        for(unsigned axis=0;axis<4;++axis)if(!std::isfinite(p[axis]))return false;
        for(unsigned axis=0;axis<3;++axis)length+=double(p[axis])*p[axis];
        if(length<1e-20 || !std::isfinite(length))return false;
        double support=std::numeric_limits<double>::infinity();
        for(const auto& corner:input.corners) {
            double dot=0;for(unsigned axis=0;axis<3;++axis)dot+=corner[axis]*p[axis];
            support=(std::min)(support,dot);
        }
        const float outward=std::nextafter(float(support),-std::numeric_limits<float>::infinity());
        if(!std::isfinite(outward))return false;
        // Native plane convention is dot(normal, world)-offset >= 0 inside.
        // Keep its exact normal and only move offset outward to contain both
        // actual eye volumes. Never tighten a plane that already contains them.
        offsets[plane]=(std::min)(p[3],outward);
    }
    // Commit only after every field is validated. Normals, type vectors, flags
    // and the bounds' W fields keep their original values.
    for(unsigned plane=0;plane<6;++plane)planes[plane*4+3]=offsets[plane];
    std::memcpy(bounds,expandedBounds,sizeof(expandedBounds));
    return true;
}
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
    if(!PrepareStereoBounds(view,eyeProjection,candidate))return false;
    output=candidate;return true;
}
}

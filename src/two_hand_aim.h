#pragma once
#include <openxr/openxr.h>
#include <cmath>

namespace d2vr_twohand {
// Rotate the dominant aim onto the rear-to-front hand line. Keep the right
// origin and controller roll; a crossed/overlapping/untracked hand is rejected.
inline bool Pose(const XrPosef& right,const XrVector3f& left,XrPosef& out) {
    const float* p=&right.position.x;const float* l=&left.x;
    for(int i=0;i<3;++i)if(!std::isfinite(p[i]) || !std::isfinite(l[i]))return false;
    auto q=right.orientation;
    const float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    if(!std::isfinite(n) || n<.5f || n>1.5f)return false;
    const float s=1/std::sqrt(n);q.x*=s;q.y*=s;q.z*=s;q.w*=s;
    const XrVector3f f{-2*(q.x*q.z+q.w*q.y),-2*(q.y*q.z-q.w*q.x),-1+2*(q.x*q.x+q.y*q.y)};
    XrVector3f d{left.x-p[0],left.y-p[1],left.z-p[2]};
    const float distance=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
    if(!std::isfinite(distance) || distance<.12f || distance>.85f)return false;
    d.x/=distance;d.y/=distance;d.z/=distance;
    const float dot=f.x*d.x+f.y*d.y+f.z*d.z;
    if(dot<.15f)return false; // support hand must be in front, never behind grip
    XrQuaternionf a{f.y*d.z-f.z*d.y,f.z*d.x-f.x*d.z,f.x*d.y-f.y*d.x,1+dot};
    const float an=1/std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z+a.w*a.w);
    a.x*=an;a.y*=an;a.z*=an;a.w*=an;
    out=right;out.orientation={a.w*q.x+a.x*q.w+a.y*q.z-a.z*q.y,
        a.w*q.y-a.x*q.z+a.y*q.w+a.z*q.x,
        a.w*q.z+a.x*q.y-a.y*q.x+a.z*q.w,
        a.w*q.w-a.x*q.x-a.y*q.y-a.z*q.z};
    return true;
}
inline float SupportedCone(float cone,bool supported) {
    return supported && std::isfinite(cone) && cone>0 ? cone*.5f:cone;
}
}

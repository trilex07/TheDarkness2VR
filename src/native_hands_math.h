#pragma once
#include "stereo_math.h"
#include <algorithm>
namespace d2vr_hands {
struct V {
    float x,y,z;
    V operator+(V b) const { return {x+b.x,y+b.y,z+b.z}; }
    V operator-(V b) const { return {x-b.x,y-b.y,z-b.z}; }
    V operator*(float s) const { return {x*s,y*s,z*s}; }
};
inline float Dot(V a,V b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline V Cross(V a,V b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline float Length(V a) { return std::sqrt(Dot(a,a)); }
inline V Position(const float* m) { return {m[12],m[13],m[14]}; }
inline bool Valid(const float* m) {
    const float zero[3]{},forward[3]={m[8],m[9],m[10]}; float out[16];
    return d2vr_aim::BuildRayMatrix(m,zero,forward,out) &&
        std::fabs(m[3])+std::fabs(m[7])+std::fabs(m[11])+std::fabs(m[15]-1)<.001f;
}
// Row-vector rigid delta mapping a segment onto a new segment. No scale.
inline bool SegmentDelta(V a,V b,V na,V nb,float out[16]) {
    V u=b-a,v=nb-na; const float ul=Length(u),vl=Length(v);
    if(ul<.001f || vl<.001f) return false;
    u=u*(1/ul); v=v*(1/vl);
    V q=Cross(u,v); float w=1+Dot(u,v);
    if(w<.0001f) { q=Cross(u,std::fabs(u.y)<.9f ? V{0,1,0}:V{1,0,0}); w=0; }
    const float n=std::sqrt(Dot(q,q)+w*w); q=q*(1/n); w/=n;
    const float x=q.x,y=q.y,z=q.z;
    float rot[16]={1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w),0,
        2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w),0,
        2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y),0,0,0,0,1};
    for(int c=0;c<3;++c) out[12+c]=0;
    std::memcpy(out,rot,sizeof(rot));
    out[12]=na.x-a.x*rot[0]-a.y*rot[4]-a.z*rot[8];
    out[13]=na.y-a.x*rot[1]-a.y*rot[5]-a.z*rot[9];
    out[14]=na.z-a.x*rot[2]-a.y*rot[6]-a.z*rot[10];
    return true;
}
inline bool PoseArm(float bones[][16],int upper,int elbow,int wrist,int lowerTwist,int upperTwist,
                    const float target[16]) {
    if(!Valid(target)) return false;
    for(int i=upper;i<=upperTwist;++i) if(!Valid(bones[i])) return false;
    V s=Position(bones[upper]),e=Position(bones[elbow]),h=Position(bones[wrist]),t=Position(target);
    const float a=Length(e-s),b=Length(h-e); V axis=t-s; float d=Length(axis);
    if(a<.02f || b<.02f || a>1 || b>1 || d<.005f || d>2) return false;
    axis=axis*(1/d);
    // Limited shoulder translation beyond reach; full clavicle/body IK is separate.
    if(d>a+b-.001f) { s=s+axis*(d-(a+b-.001f)); d=a+b-.001f; }
    V bend=e-s; bend=bend-axis*Dot(bend,axis);
    if(Length(bend)<.001f) bend=Cross(axis,std::fabs(axis.y)<.9f ? V{0,1,0}:V{1,0,0});
    bend=bend*(1/Length(bend));
    float along=(a*a-b*b+d*d)/(2*d); along=std::max(-a,std::min(a,along));
    const V ne=s+axis*along+bend*std::sqrt(std::max(0.f,a*a-along*along));
    float ud[16],ld[16],inverse[16],hd[16];
    if(!SegmentDelta(Position(bones[upper]),e,s,ne,ud) || !SegmentDelta(e,h,ne,t,ld) ||
       !d2vr_math::Invert(bones[wrist],inverse)) return false;
    d2vr_math::Multiply(inverse,target,hd);
    // Wrist, five fingers and the weapon socket share one rigid delta.
    for(int i=wrist;i<lowerTwist;++i) d2vr_math::Multiply(bones[i],hd,bones[i]);
    for(int i:{upper,upperTwist}) d2vr_math::Multiply(bones[i],ud,bones[i]);
    for(int i:{elbow,lowerTwist}) d2vr_math::Multiply(bones[i],ld,bones[i]);
    return true;
}
}

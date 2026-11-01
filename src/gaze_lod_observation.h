#pragma once
#include <cmath>
#include <cstdint>

// Read-only observation policy. Caller must provide owned objects in one
// coordinate space. This never reads game memory or changes a native LOD.
namespace d2vr_gaze_lod {
struct Vec { float x=0,y=0,z=0; };
struct Sample {
    Vec eye, direction, center;
    float radius=0; // conservative world-space bounding sphere
    std::uint64_t now=0,gazeTime=0;
    int selected=0,levels=0;
    bool gazeValid=false,ownerVerified=false,decoration=false;
};
enum class Region { Unknown, Protected, Peripheral };
struct Result { Region region=Region::Unknown; float nearestAngle=0; bool candidate=false; };
inline bool Finite(Vec p) { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z); }
inline Result Observe(const Sample& s) {
    Result r;
    if(!s.ownerVerified || !s.gazeValid || s.now<s.gazeTime || s.now-s.gazeTime>100 ||
       s.levels<1 || s.selected<0 || s.selected>=s.levels ||
       !Finite(s.eye)||!Finite(s.direction)||!Finite(s.center)||
       !std::isfinite(s.radius)||s.radius<0)return r;
    const double x=double(s.center.x)-s.eye.x,y=double(s.center.y)-s.eye.y,z=double(s.center.z)-s.eye.z;
    const double distance=std::sqrt(x*x+y*y+z*z);
    const double norm=std::sqrt(double(s.direction.x)*s.direction.x+
        double(s.direction.y)*s.direction.y+double(s.direction.z)*s.direction.z);
    if(norm<.5 || norm>1.5)return r;
    r.region=Region::Protected;
    if(distance<=s.radius || distance<1e-6)return r;
    double cosine=(x*s.direction.x+y*s.direction.y+z*s.direction.z)/(distance*norm);
    cosine=cosine<-1?-1:cosine>1?1:cosine;
    const double angle=std::acos(cosine)-std::asin(s.radius/distance);
    r.nearestAngle=float(angle>0?angle:0);
    // Wide 35-degree half-angle protects the whole object's angular footprint.
    if(r.nearestAngle>.6108652382f)r.region=Region::Peripheral;
    r.candidate=s.decoration && s.selected+1<s.levels && r.region==Region::Peripheral;
    return r;
}
}

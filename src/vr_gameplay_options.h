#pragma once
#include "vr_settings.h"

namespace d2vr_gameplay {
// Native locomotion follows the level selected-camera basis. Rotate only XR
// movement by current head yaw relative to that same recenter reference.
inline bool HeadMovement(const XrPosef& reference,const XrPosef& head,float& x,float& y) {
    using namespace d2vr_menu::detail;
    double r[4],h[4];
    if(!Pose(reference,r)||!Pose(head,h)||!std::isfinite(x)||!std::isfinite(y))return false;
    const auto rf=Rotate(r,{0,0,-1}),hf=Rotate(h,{0,0,-1});
    const double rn=std::hypot(rf.x,rf.z),hn=std::hypot(hf.x,hf.z);
    if(rn<.1||hn<.1)return false; // Looking vertically has no reliable heading.
    const double c=(rf.x*hf.x+rf.z*hf.z)/(rn*hn);
    const double s=(rf.z*hf.x-rf.x*hf.z)/(rn*hn);
    const float ox=x,oy=y;
    x=float(c*ox-s*oy);y=float(s*ox+c*oy);return true;
}
// Direction preview, not a collision query or impact marker. The synthetic
// endpoint plane is perpendicular to the actual firearm aim, eight metres out.
inline bool WeaponBeam(const XrPosef& aim,const XrPosef& head,XrSpace space,
                       XrPosef& pose,XrExtent2Df& size) {
    double q[4];if(!space||!d2vr_menu::detail::Pose(aim,q))return false;
    const auto f=d2vr_menu::detail::Rotate(q,{0,0,-8});
    d2vr_menu::Surface surface;surface.identity=surface.frame=1;surface.space=space;
    surface.pose=aim;surface.pose.position.x+=float(f.x);
    surface.pose.position.y+=float(f.y);surface.pose.position.z+=float(f.z);
    surface.width=surface.height=1;surface.cursor={0,0,1,1};
    d2vr_menu::Aim ray;ray.frame=1;ray.space=space;ray.tracked=ray.menu=true;ray.pose=aim;
    return d2vr_settings::Laser(surface,ray,head,pose,size);
}
}

#pragma once
#include "vr_menu_pointer.h"
namespace d2vr_menu {
// The input ray and compositor beam consume the same submitted menu surface.
inline bool ControllerHit(Surface surface,const XrPosef& pose,bool tracked,
                          XrSpace space,Hit& hit) {
    Aim aim;aim.frame=surface.frame;aim.space=space;aim.pose=pose;
    aim.tracked=tracked;aim.menu=true;
    return Project(surface,aim,hit) && hit.inside;
}
inline bool CursorReady(const Hit& hit,int x,int y,double age) {
    return hit.inside && std::isfinite(age) && age>=0 && age<=.15 &&
        std::abs(int64_t(hit.x)-x)<=8 && std::abs(int64_t(hit.y)-y)<=8;
}
}

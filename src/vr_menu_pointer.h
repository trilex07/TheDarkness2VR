#pragma once
#include <openxr/openxr.h>
#include <cmath>
#include <cstdint>
#include <limits>

namespace d2vr_menu {
struct CursorRect {int32_t x=0,y=0;uint32_t width=0,height=0;};
// Supplied by the renderer that submits the actual menu surface. No default
// plane, inferred menu state, head-pose fallback or guessed pixel dimensions.
struct Surface {
    uint64_t identity=0,frame=0;
    XrSpace space=XR_NULL_HANDLE;
    XrPosef pose{};
    float width=0,height=0;
    CursorRect cursor;
};
struct Aim {
    uint64_t frame=0;
    XrSpace space=XR_NULL_HANDLE;
    XrPosef pose{};
    bool tracked=false,menu=false;
    float trigger=0;
    bool back=false;
    float scrollY=0;
    double time=0; // monotonic application time in seconds, never display FPS
};
struct Hit {
    bool inside=false;
    double u=0,v=0,distance=0;
    int32_t x=0,y=0;
    XrVector3f point{};
};
namespace detail {
struct V {double x,y,z;};
inline bool Finite(V v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
inline V Add(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V Scale(V a,double b){return {a.x*b,a.y*b,a.z*b};}
inline V Cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool Pose(const XrPosef& pose,double q[4]) {
    const auto& r=pose.orientation;
    q[0]=r.x;q[1]=r.y;q[2]=r.z;q[3]=r.w;
    double n=0;for(unsigned i=0;i<4;++i){if(!std::isfinite(q[i]))return false;n+=q[i]*q[i];}
    if(!Finite({pose.position.x,pose.position.y,pose.position.z}) || std::fabs(n-1)>.001)return false;
    n=std::sqrt(n);for(unsigned i=0;i<4;++i)q[i]/=n;return true;
}
inline V Rotate(const double q[4],V v) {
    const V axis{q[0],q[1],q[2]},t=Scale(Cross(axis,v),2);
    return Add(v,Add(Scale(t,q[3]),Cross(axis,t)));
}
inline bool Rect(const CursorRect& r) {
    if(!r.width || !r.height)return false;
    return int64_t(r.x)+r.width-1<=INT32_MAX && int64_t(r.y)+r.height-1<=INT32_MAX;
}
}
// OpenXR RH coordinates: aim forward -Z, surface normal +Z, top-left UV origin.
// Outside hits are retained for a captured slider drag; hover cannot acquire
// outside the menu. Output stays untouched on rejection.
inline bool Project(const Surface& surface,const Aim& aim,Hit& output) {
    if(!surface.identity || !surface.frame || surface.frame!=aim.frame ||
       !surface.space || surface.space!=aim.space || !aim.menu || !aim.tracked ||
       !std::isfinite(surface.width) || !std::isfinite(surface.height) ||
       surface.width<=0 || surface.height<=0 || !detail::Rect(surface.cursor))return false;
    double sq[4],aq[4];if(!detail::Pose(surface.pose,sq)||!detail::Pose(aim.pose,aq))return false;
    const auto direction=detail::Rotate(aq,{0,0,-1});
    const detail::V origin{aim.pose.position.x,aim.pose.position.y,aim.pose.position.z};
    const detail::V relative{origin.x-surface.pose.position.x,origin.y-surface.pose.position.y,origin.z-surface.pose.position.z};
    sq[0]=-sq[0];sq[1]=-sq[1];sq[2]=-sq[2];
    const auto local=detail::Rotate(sq,relative),ray=detail::Rotate(sq,direction);
    if(ray.z>=-1e-8 || local.z<=0)return false; // backface, parallel, or behind the ray
    Hit hit;hit.distance=-local.z/ray.z;
    hit.u=(local.x+ray.x*hit.distance)/surface.width+.5;
    hit.v=.5-(local.y+ray.y*hit.distance)/surface.height;
    const double x=std::floor(surface.cursor.x+hit.u*surface.cursor.width);
    const double y=std::floor(surface.cursor.y+hit.v*surface.cursor.height);
    if(!std::isfinite(hit.distance)||!std::isfinite(x)||!std::isfinite(y)||
       x<INT32_MIN||x>INT32_MAX||y<INT32_MIN||y>INT32_MAX)return false;
    hit.inside=hit.u>=0 && hit.u<=1 && hit.v>=0 && hit.v<=1;
    // The inclusive geometric right/bottom edge belongs to the final pixel.
    hit.x=int32_t(x);hit.y=int32_t(y);
    if(hit.u==1)hit.x=int32_t(int64_t(surface.cursor.x)+surface.cursor.width-1);
    if(hit.v==1)hit.y=int32_t(int64_t(surface.cursor.y)+surface.cursor.height-1);
    const auto point=detail::Add(origin,detail::Scale(direction,hit.distance));
    constexpr double limit=std::numeric_limits<float>::max();
    if(!detail::Finite(point)||std::fabs(point.x)>limit||std::fabs(point.y)>limit||std::fabs(point.z)>limit)return false;
    hit.point={float(point.x),float(point.y),float(point.z)};
    output=hit;return true;
}
struct Actions {
    bool visible=false,move=false,press=false,release=false,captured=false;
    uint64_t surface=0;
    Hit hit;
};
// Produces logical pointer events only. The native bridge owns device merging:
// a synthetic release must never release a physical mouse button. Closing a
// menu/tracking loss cancels capture once; reacquisition needs trigger release.
class Capture {
    uint64_t frame_=0,surface_=0;
    bool down_=false,armed_=false;
    Actions Cancel() {
        Actions a;a.release=down_;a.surface=surface_;
        down_=armed_=false;surface_=0;return a;
    }
public:
    Actions Reset(){frame_=0;return Cancel();}
    Actions Update(const Surface& surface,const Aim& aim) {
        Hit hit;
        if(!std::isfinite(aim.trigger)||aim.trigger<0||aim.trigger>1||!Project(surface,aim,hit))
            return Cancel();
        if(aim.frame<frame_){frame_=aim.frame;return Cancel();}
        if(aim.frame==frame_)return {}; // same application frame cannot click twice
        frame_=aim.frame;
        if(surface_ && surface_!=surface.identity) return Cancel();
        Actions a;a.surface=surface.identity;a.hit=hit;
        if(aim.trigger<=.35f)armed_=true;
        if(down_ && aim.trigger<=.35f){a.release=true;down_=false;}
        if(!down_ && armed_ && aim.trigger>=.65f) {
            armed_=false; // a press outside must not click on later re-entry
            if(hit.inside){a.press=true;down_=true;surface_=surface.identity;}
        }
        a.captured=down_;a.visible=hit.inside||down_;a.move=a.visible||a.release;
        if(!down_)surface_=0;
        return a;
    }
};
struct NavigationActions {bool back=false;int scroll=0;};
// Navigation shares the submitted menu's identity but Back does not require a
// ray hit. Scroll applies only under the pointer (or during its slider capture).
// The native adapter converts scroll notches to its own documented units and
// merges Back with the existing controller route rather than issuing it twice.
class Navigation {
    uint64_t frame_=0,surface_=0;
    XrSpace space_=XR_NULL_HANDLE;
    double time_=0,scroll_=0;
    bool backArmed_=false;
public:
    void Reset(){frame_=surface_=0;space_=XR_NULL_HANDLE;time_=scroll_=0;backArmed_=false;}
    NavigationActions Update(const Surface& surface,const Aim& aim,bool captured=false) {
        NavigationActions a;
        double sq[4];
        if(!surface.identity || !surface.frame || surface.frame!=aim.frame ||
           !surface.space || surface.space!=aim.space || !aim.menu || !aim.tracked ||
           !std::isfinite(surface.width) || !std::isfinite(surface.height) ||
           surface.width<=0 || surface.height<=0 || !detail::Rect(surface.cursor) ||
           !detail::Pose(surface.pose,sq) || !std::isfinite(aim.time) || aim.time<=0 ||
           !std::isfinite(aim.scrollY) || aim.scrollY < -1 || aim.scrollY > 1) {
            Reset();return a;
        }
        if(frame_==aim.frame)return a;
        const bool continuous=surface_==surface.identity && space_==surface.space &&
            aim.frame>frame_ && aim.time>time_ && aim.time-time_<=.25;
        const double dt=continuous ? aim.time-time_:0;
        if(!continuous){scroll_=0;backArmed_=false;}
        frame_=aim.frame;surface_=surface.identity;space_=surface.space;time_=aim.time;
        if(!aim.back)backArmed_=true;
        if(aim.back && backArmed_){a.back=true;backArmed_=false;scroll_=0;return a;}
        Hit hit;
        if(!Project(surface,aim,hit) || (!hit.inside && !captured) || std::fabs(aim.scrollY)<=.25f) {
            scroll_=0;return a;
        }
        // Eight notches/second at full deflection; deadzone removes stick drift.
        // Reset on reversal: no hidden debt before scrolling the opposite way.
        if(scroll_*aim.scrollY<0)scroll_=0;
        const double speed=(std::fabs(double(aim.scrollY))-.25)/.75*8;
        scroll_+=(aim.scrollY>0 ? speed:-speed)*dt;
        a.scroll=int(scroll_);scroll_-=a.scroll;
        return a;
    }
};
}

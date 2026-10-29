#pragma once
#include "vr_menu_pointer.h"
#include "adaptive_fps.h"

namespace d2vr_settings {
constexpr int Width=640,Height=360;
// One layout drives drawing and hit testing. Coordinates belong to the actual
// compositor quad, never the native game's unknown menu surface.
struct Rect {
    int x,y,w,h;
    bool Contains(int px,int py)const{return px>=x && py>=y && px<x+w && py<y+h;}
};
inline Rect ModeRect(unsigned i){return {32+int(i)*192,144,176,56};}
constexpr Rect CloseRect{480,276,128,48};
// Reacquisition must see release before accepting a new toggle edge.
class ToggleAction {
    bool armed_=false;
public:
    bool Update(bool active,bool down) {
        if(!active){armed_=false;return false;}
        if(!down){armed_=true;return false;}
        if(!armed_)return false;
        armed_=false;return true;
    }
};
inline bool Neutral(bool tracked,uint16_t buttons,float lt,float rt,float lx,float ly,
                    float rx,float ry,float leftGrip,float rightGrip) {
    const float values[]={lt,rt,lx,ly,rx,ry,leftGrip,rightGrip};
    for(float v:values)if(!std::isfinite(v))return false;
    return tracked && !buttons && lt>=0 && rt>=0 && lt<=.05f && rt<=.05f &&
        leftGrip>=0 && rightGrip>=0 && leftGrip<=.15f && rightGrip<=.15f &&
        std::hypot(lx,ly)<=.15f && std::hypot(rx,ry)<=.15f;
}
class Panel {
    d2vr_menu::Capture capture_;
    d2vr_menu::Navigation navigation_;
    d2vr_menu::Surface surface_;
    d2vr_menu::Actions pointer_;
    d2vr_menu::Aim aim_;
    bool open_=false,presented_=false,lease_=false;
    uint64_t identity_=0;
    uint64_t inputFrame_=0;
public:
    bool Open()const{return open_;}
    bool Presented()const{return presented_;}
    const d2vr_menu::Surface& Surface()const{return surface_;}
    const d2vr_menu::Actions& Pointer()const{return pointer_;}
    const d2vr_menu::Aim& Aim()const{return aim_;}
    void Close(){open_=presented_=false;inputFrame_=0;capture_.Reset();navigation_.Reset();pointer_={};}
    void SuspendSurface(){presented_=false;inputFrame_=0;capture_.Reset();pointer_={};}
    bool Toggle(const XrPosef& head,XrSpace space) {
        if(open_){Close();return false;}
        double q[4];if(!space || !d2vr_menu::detail::Pose(head,q))return false;
        // Latch a level, world-anchored panel in front of the current head.
        // This is also the exact pose later supplied to XrCompositionLayerQuad.
        const auto forward=d2vr_menu::detail::Rotate(q,{0,0,-1});
        const double length=std::hypot(forward.x,forward.z);
        if(length<.1)return false;
        const double yaw=std::atan2(-forward.x,-forward.z);
        surface_={};surface_.identity=++identity_;if(!identity_)surface_.identity=++identity_;
        surface_.space=space;surface_.width=1.28f;surface_.height=.72f;
        surface_.cursor={0,0,Width,Height};
        surface_.pose.orientation={0,float(std::sin(yaw*.5)),0,float(std::cos(yaw*.5))};
        surface_.pose.position={head.position.x+float(forward.x/length*1.5),
            head.position.y,head.position.z+float(forward.z/length*1.5)};
        capture_.Reset();navigation_.Reset();pointer_={};inputFrame_=0;
        open_=lease_=true;presented_=false;return true;
    }
    // A panel that failed submission must not retain a clickable surface.
    void Submitted(bool success){if(open_){presented_=success;if(!success)Close();}}
    // Return ownership of XR controls only after release/neutral, including on
    // close or tracking loss. This never changes physical keyboard/gamepad data.
    bool OwnInput(bool neutral) {
        if(open_)lease_=true;
        else if(neutral)lease_=false;
        return lease_;
    }
    void Input(const d2vr_menu::Aim& aim,d2vr_adaptive::Mode& mode) {
        if(!open_)return;
        if(!aim.frame || !aim.menu || !aim.tracked || aim.space!=surface_.space){Close();return;}
        // Several controller polls can share one XR frame. Preserve hover and
        // capture, but never dispatch a click twice or erase the visible dot.
        if(aim.frame==inputFrame_){pointer_.press=pointer_.release=false;return;}
        if(aim.frame<inputFrame_){SuspendSurface();return;}
        inputFrame_=aim.frame;
        const bool wasCaptured=pointer_.captured;pointer_={};
        surface_.frame=aim.frame;
        if(navigation_.Update(surface_,aim,wasCaptured).back){Close();return;}
        if(!presented_)return; // Back can cancel an opening/pending panel
        pointer_=capture_.Update(surface_,aim);
        aim_=aim;
        if(pointer_.press) {
            const auto& hit=pointer_.hit;
            if(CloseRect.Contains(hit.x,hit.y)){Close();return;}
            for(unsigned i=0;i<3;++i)if(ModeRect(i).Contains(hit.x,hit.y))
                mode=static_cast<d2vr_adaptive::Mode>(i);
        }
    }
};
// A thin compositor quad between the actual aim origin and its finite menu hit.
// Its long local +Y axis follows the ray; +Z faces the current head. A collinear
// head/ray has no visible segment, so retain the hit dot instead of a bad basis.
inline bool Laser(const d2vr_menu::Surface& surface,const d2vr_menu::Aim& aim,
                  const XrPosef& head,XrPosef& pose,XrExtent2Df& size){
    using namespace d2vr_menu;
    Hit hit;double q[4];
    if(!Project(surface,aim,hit)||!hit.inside||!detail::Pose(head,q)||
       hit.distance<=.04||hit.distance>10)return false;
    const detail::V origin{aim.pose.position.x,aim.pose.position.y,aim.pose.position.z};
    detail::V y{hit.point.x-origin.x,hit.point.y-origin.y,hit.point.z-origin.z};
    const double length=std::sqrt(y.x*y.x+y.y*y.y+y.z*y.z);
    if(!std::isfinite(length)||length<=.04)return false;
    y=detail::Scale(y,1/length);
    const double beamLength=length-.022;
    const auto center=detail::Add(origin,detail::Scale(y,.02+beamLength*.5));
    const detail::V view{head.position.x-center.x,head.position.y-center.y,head.position.z-center.z};
    auto x=detail::Cross(y,view);
    const double widthBasis=std::sqrt(x.x*x.x+x.y*x.y+x.z*x.z);
    if(!std::isfinite(widthBasis)||widthBasis<1e-5)return false;
    x=detail::Scale(x,1/widthBasis);const auto z=detail::Cross(x,y);
    // Unit quaternion from the orthonormal column basis (x,y,z).
    double a,b,c,w;const double trace=x.x+y.y+z.z;
    if(trace>0){const double s=std::sqrt(1+trace)*2;
        w=s*.25;a=(y.z-z.y)/s;b=(z.x-x.z)/s;c=(x.y-y.x)/s;}
    else if(x.x>y.y&&x.x>z.z){const double s=std::sqrt(1+x.x-y.y-z.z)*2;
        a=s*.25;b=(y.x+x.y)/s;c=(z.x+x.z)/s;w=(y.z-z.y)/s;}
    else if(y.y>z.z){const double s=std::sqrt(1+y.y-x.x-z.z)*2;
        a=(y.x+x.y)/s;b=s*.25;c=(z.y+y.z)/s;w=(z.x-x.z)/s;}
    else {const double s=std::sqrt(1+z.z-x.x-y.y)*2;
        a=(z.x+x.z)/s;b=(z.y+y.z)/s;c=s*.25;w=(x.y-y.x)/s;}
    XrPosef result{{float(a),float(b),float(c),float(w)},
        {float(center.x),float(center.y),float(center.z)}};
    if(!detail::Pose(result,q))return false;
    pose=result;size={.002f,float(beamLength)};return true;
}
}

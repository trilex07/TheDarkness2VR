#pragma once
// Gaze ray -> point on the cinema/menu quad -> game window client pixel.
// Pure math (OpenXR quad: centred on pose, +X right, +Y up, image top at +Y).
#include <cmath>

namespace d2vr_gaze_menu {

struct V3 { float x,y,z; };
struct Quat { float x,y,z,w; };

inline V3 Rotate(const Quat& q,const V3& v) {
    const V3 t{2*(q.y*v.z-q.z*v.y),2*(q.z*v.x-q.x*v.z),2*(q.x*v.y-q.y*v.x)};
    return {v.x+q.w*t.x+(q.y*t.z-q.z*t.y),v.y+q.w*t.y+(q.z*t.x-q.x*t.z),v.z+q.w*t.z+(q.x*t.y-q.y*t.x)};
}

// u,v in [0,1], v down. False when the ray misses or hits the back side.
inline bool HitQuad(const V3& origin,const V3& dir,const V3& centre,const Quat& q,float width,float height,
                    float& u,float& v) {
    if(!(width>0) || !(height>0)) return false;
    const V3 n=Rotate(q,{0,0,1});
    const float denom=dir.x*n.x+dir.y*n.y+dir.z*n.z;
    if(!(denom<-1e-4f)) return false; // must face the viewer
    const V3 oc{centre.x-origin.x,centre.y-origin.y,centre.z-origin.z};
    const float t=(oc.x*n.x+oc.y*n.y+oc.z*n.z)/denom;
    if(!(t>0) || !std::isfinite(t)) return false;
    const V3 p{origin.x+dir.x*t-centre.x,origin.y+dir.y*t-centre.y,origin.z+dir.z*t-centre.z};
    const V3 local=Rotate({-q.x,-q.y,-q.z,q.w},p);
    u=local.x/width+.5f; v=.5f-local.y/height;
    return u>=0 && u<=1 && v>=0 && v<=1;
}

// Cursor moves only when the target changed by at least `step` pixels, so a
// steady gaze never fights the native cursor every frame.
struct CursorFilter {
    int x=-100000,y=-100000;
    bool Changed(int nx,int ny,int step) {
        if(std::abs(nx-x)<step && std::abs(ny-y)<step) return false;
        x=nx; y=ny; return true;
    }
    void Reset() { x=y=-100000; }
};

} // namespace d2vr_gaze_menu

#pragma once
// Gaze-centred (foveated) sharpening geometry. Pure math, host-testable.
// The eye image was rendered with an asymmetric FOV (OpenXR angles, radians).
// Direction is in that eye's view space (+X right, +Y up, -Z forward).
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace d2vr_fovea {

struct Rect { uint32_t x=0,y=0,w=0,h=0; bool Empty() const { return !w || !h; } };

// Pixel (x right, y down) of a view-space direction on an eye image.
inline bool GazePixel(float dx,float dy,float dz,float angleLeft,float angleRight,float angleUp,float angleDown,
                      uint32_t width,uint32_t height,float& px,float& py) {
    if(!(dz<-1e-3f) || !width || !height) return false;
    const float tl=std::tan(angleLeft), tr=std::tan(angleRight), tu=std::tan(angleUp), td=std::tan(angleDown);
    if(!(tr>tl) || !(tu>td)) return false;
    const float tx=dx/-dz, ty=dy/-dz;
    px=(tx-tl)/(tr-tl)*float(width);
    py=(tu-ty)/(tu-td)*float(height);
    return std::isfinite(px) && std::isfinite(py) &&
           px>=0 && py>=0 && px<float(width) && py<float(height);
}

// Square centred on (px,py), edge = fraction*min(width,height), aligned to
// 8 px and clamped inside the image (shifted, not shrunk, at the borders).
inline Rect Around(float px,float py,uint32_t width,uint32_t height,float fraction) {
    Rect r;
    if(!width || !height || !(fraction>0) || !std::isfinite(px) || !std::isfinite(py)) return r;
    uint32_t edge=uint32_t(std::min(width,height)*std::min(fraction,1.f));
    edge=std::max<uint32_t>(32,edge&~7u);
    r.w=std::min(edge,width); r.h=std::min(edge,height);
    const float x0=std::clamp(px-r.w*.5f,0.f,float(width-r.w)), y0=std::clamp(py-r.h*.5f,0.f,float(height-r.h));
    r.x=uint32_t(x0)&~7u; r.y=uint32_t(y0)&~7u;
    return r;
}

// Sharpness for each pass: periphery (whole image), transition ring, fovea.
struct Levels { float periphery, transition, fovea; };
inline Levels Grade(float periphery,float fovea) {
    periphery=std::clamp(periphery,0.f,1.f); fovea=std::clamp(fovea,0.f,1.f);
    return {periphery,(periphery+fovea)*.5f,fovea};
}

} // namespace d2vr_fovea

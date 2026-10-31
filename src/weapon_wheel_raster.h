#pragma once
// CPU rasteriser for d2vr_weapon_wheel_graphics triangles into a premultiplied
// BGRA8 image (one OpenXR quad texture). Runs only when the wheel image
// changes (open / hover / loadout), never per frame. 2x2 supersampled.
// Pure C++: host-testable, no allocation (caller owns the pixel buffer).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "weapon_wheel_graphics.h"

namespace d2vr_wheel_raster {

struct Target {
    uint32_t* pixels=nullptr; // BGRA, premultiplied alpha, rows top-down
    int width=0, height=0, stride=0; // stride in pixels
    int x0=0, y0=0, size=0;   // square viewport mapping plane [-1,1]^2
};

inline void Clear(Target& t) {
    if(!t.pixels) return;
    for(int y=t.y0;y<t.y0+t.size && y<t.height;++y)
        std::memset(t.pixels+size_t(y)*t.stride+t.x0,0,sizeof(uint32_t)*size_t(std::min(t.size,t.width-t.x0)));
}

namespace detail {
inline uint8_t Byte(float v) { v=v<0?0:v>1?1:v; return uint8_t(v*255.f+.5f); }
inline float Edge(float ax,float ay,float bx,float by,float px,float py) {
    return (bx-ax)*(py-ay)-(by-ay)*(px-ax);
}
}

// Straight-alpha triangle, source-over into the premultiplied target. One
// sample per pixel centre with a top-left fill rule, so triangles sharing an
// edge never blend twice (no seams); anti-aliasing comes from DrawWheel's 2x2
// supersampled scratch image.
inline void Triangle(Target& t,const d2vr_weapon_wheel_graphics::Vertex& a,
                     const d2vr_weapon_wheel_graphics::Vertex& b,
                     const d2vr_weapon_wheel_graphics::Vertex& c) {
    if(!t.pixels || t.size<=0) return;
    const float s=t.size*.5f;
    float ax=t.x0+(a.x+1)*s, ay=t.y0+(a.y+1)*s;
    float bx=t.x0+(b.x+1)*s, by=t.y0+(b.y+1)*s;
    float cx=t.x0+(c.x+1)*s, cy=t.y0+(c.y+1)*s;
    float area=detail::Edge(ax,ay,bx,by,cx,cy);
    if(!std::isfinite(area) || std::fabs(area)<1e-6f) return;
    if(area<0) { std::swap(bx,cx); std::swap(by,cy); }
    // Positive area in y-down pixels: an edge is top/left when it runs
    // leftwards along a horizontal top, or upwards (dy<0) on the left side.
    const auto topLeft=[](float x0,float y0,float x1,float y1) {
        return (y0==y1 && x1<x0) || y1<y0;
    };
    const bool tl0=topLeft(ax,ay,bx,by), tl1=topLeft(bx,by,cx,cy), tl2=topLeft(cx,cy,ax,ay);
    const int minX=std::max({t.x0,0,int(std::floor(std::min({ax,bx,cx})))});
    const int maxX=std::min({t.x0+t.size-1,t.width-1,int(std::ceil(std::max({ax,bx,cx})))});
    const int minY=std::max({t.y0,0,int(std::floor(std::min({ay,by,cy})))});
    const int maxY=std::min({t.y0+t.size-1,t.height-1,int(std::ceil(std::max({ay,by,cy})))});
    const float alpha=std::min(1.f,std::max(0.f,a.a)), inv=1-alpha;
    const float r=a.r*alpha,g=a.g*alpha,bl=a.b*alpha; // flat colour per triangle
    for(int y=minY;y<=maxY;++y) for(int x=minX;x<=maxX;++x) {
        const float px=x+.5f, py=y+.5f;
        const float e0=detail::Edge(ax,ay,bx,by,px,py), e1=detail::Edge(bx,by,cx,cy,px,py),
                    e2=detail::Edge(cx,cy,ax,ay,px,py);
        if(e0<0 || e1<0 || e2<0 || (e0==0 && !tl0) || (e1==0 && !tl1) || (e2==0 && !tl2)) continue;
        uint32_t& d=t.pixels[size_t(y)*t.stride+x];
        const float db=(d&255)/255.f, dg=((d>>8)&255)/255.f, dr=((d>>16)&255)/255.f, da=(d>>24)/255.f;
        d=uint32_t(detail::Byte(bl+db*inv)) | uint32_t(detail::Byte(g+dg*inv))<<8 |
          uint32_t(detail::Byte(r+dr*inv))<<16 | uint32_t(detail::Byte(alpha+da*inv))<<24;
    }
}

constexpr int kSupersample=2;
// Scratch pixels DrawWheel needs for a viewport of `size` pixels.
inline size_t ScratchPixels(int size) { return size_t(size)*size*kSupersample*kSupersample; }

// Draws one wheel into the viewport (cleared first) through a 2x2 supersampled
// scratch image (premultiplied box filter). Returns emitted vertices.
inline unsigned DrawWheel(Target& t,const d2vr_weapon_wheel_graphics::Wheel& wheel,bool leftHand,
                          uint32_t* scratch) {
    if(!t.pixels || !scratch || t.size<=0 || t.x0<0 || t.y0<0 ||
       t.x0+t.size>t.width || t.y0+t.size>t.height) return 0;
    const int big=t.size*kSupersample;
    Target hi{scratch,big,big,big,0,0,big};
    std::memset(scratch,0,ScratchPixels(t.size)*sizeof(uint32_t));
    unsigned vertices=0;
    d2vr_weapon_wheel_graphics::BuildWheel(wheel,leftHand,
        [&](const d2vr_weapon_wheel_graphics::Vertex& a,const d2vr_weapon_wheel_graphics::Vertex& b,
            const d2vr_weapon_wheel_graphics::Vertex& c) { Triangle(hi,a,b,c); vertices+=3; });
    for(int y=0;y<t.size;++y) for(int x=0;x<t.size;++x) {
        unsigned sum[4]{};
        for(int j=0;j<kSupersample;++j) for(int i=0;i<kSupersample;++i) {
            const uint32_t p=scratch[size_t(y*kSupersample+j)*big+x*kSupersample+i];
            for(int k=0;k<4;++k) sum[k]+=(p>>(8*k))&255;
        }
        const unsigned n=kSupersample*kSupersample;
        t.pixels[size_t(t.y0+y)*t.stride+t.x0+x]=((sum[0]+n/2)/n) | ((sum[1]+n/2)/n)<<8 |
            ((sum[2]+n/2)/n)<<16 | ((sum[3]+n/2)/n)<<24;
    }
    return vertices;
}

} // namespace d2vr_wheel_raster

#pragma once
// Ammo counter geometry for a small controller-attached quad. Pure vector
// triangles (same Vertex/emit contract as weapon_wheel_graphics.h), no fonts.
// Plane coordinates: x right, y down, square [-1,1]. Seven-segment digits.
#include <cstdint>
#include "weapon_wheel_graphics.h"

namespace d2vr_ammo_hud {
using d2vr_weapon_wheel_graphics::Vertex;

// Segment bits a..g (a top, b top-right, c bottom-right, d bottom, e bottom-left, f top-left, g middle).
inline uint8_t Segments(int digit) {
    static const uint8_t table[10]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};
    return digit>=0 && digit<=9 ? table[digit] : 0x40; // '-' for unknown
}

namespace detail {
template<class Emit> inline void Rect(Emit& emit,float x0,float y0,float x1,float y1,float r,float g,float b,float a) {
    const Vertex p0{x0,y0,r,g,b,a}, p1{x1,y0,r,g,b,a}, p2{x1,y1,r,g,b,a}, p3{x0,y1,r,g,b,a};
    emit(p0,p1,p2); emit(p0,p2,p3);
}
// One digit cell with its top-left at (x,y), width w, height h.
template<class Emit> inline void Digit(Emit& emit,uint8_t seg,float x,float y,float w,float h,float t,
                                       float r,float g,float b) {
    const float m=y+h*.5f;
    if(seg&0x01) Rect(emit,x+t,y,x+w-t,y+t,r,g,b,1);              // a
    if(seg&0x02) Rect(emit,x+w-t,y+t,x+w,m-t*.5f,r,g,b,1);         // b
    if(seg&0x04) Rect(emit,x+w-t,m+t*.5f,x+w,y+h-t,r,g,b,1);       // c
    if(seg&0x08) Rect(emit,x+t,y+h-t,x+w-t,y+h,r,g,b,1);           // d
    if(seg&0x10) Rect(emit,x,m+t*.5f,x+t,y+h-t,r,g,b,1);           // e
    if(seg&0x20) Rect(emit,x,y+t,x+t,m-t*.5f,r,g,b,1);             // f
    if(seg&0x40) Rect(emit,x+t,m-t*.5f,x+w-t,m+t*.5f,r,g,b,1);     // g
}
}

// Each unknown value shows "--"; each value clamps at 9999. low = warning colour.
template<class Emit> inline void Build(int ammo,int reserve,bool low,Emit&& emit) {
    detail::Rect(emit,-1,-.62f,1,.62f,.018f,.018f,.023f,.82f);            // plate
    detail::Rect(emit,-1,.56f,1,.62f,.74f,.055f,.075f,1);                  // crimson underline
    int digits[9]{}, n=0;
    for(int value:{ammo,reserve}) {
        if(n)digits[n++]=-2; // separator
        if(value<0) { digits[n++]=-1; digits[n++]=-1; }
        else {
            if(value>9999)value=9999;
            int tmp[4],k=0;
            do {tmp[k++]=value%10;value/=10;}while(value && k<4);
            while(k)digits[n++]=tmp[--k];
        }
    }
    const float gap=.045f,w=std::fmin(.34f,(1.8f-(n-1)*gap)/n),h=.92f,t=w*.17f;
    const float total=n*w+(n-1)*gap, x0=-total*.5f, y0=-.46f;
    const float r=low ? .95f : .92f, g=low ? .25f : .895f, b=low ? .2f : .845f;
    for(int i=0;i<n;++i) {
        const float x=x0+i*(w+gap);
        if(digits[i]==-2) {
            const Vertex a{x+w-t,y0,.7f,.7f,.7f,1}, b0{x+w,y0,.7f,.7f,.7f,1};
            const Vertex c{x+t,y0+h,.7f,.7f,.7f,1}, d{x,y0+h,.7f,.7f,.7f,1};
            emit(a,b0,c);emit(a,c,d);
        } else detail::Digit(emit,Segments(digits[i]),x,y0,w,h,t,r,g,b);
    }
}

template<class Emit> inline void Build(int ammo,bool low,Emit&& emit) { Build(ammo,-1,low,emit); }

} // namespace d2vr_ammo_hud

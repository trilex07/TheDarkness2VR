#pragma once

// Original vector UI geometry. No D3D/OpenXR calls, allocation, fonts or textures.
// Emit accepts three Vertex arguments for each straight-alpha triangle.
// Coordinates are planar x-right/y-down; root maps this plane into each hand's
// opening anchor. Slot 0 is at the top, increasing clockwise. Build only when
// wheel state changes and retain the resulting GPU geometry while it is shown.
#include <cmath>
#include <cstddef>
#include "../assets/weapon-wheel/icons/weapon_icons.h"

namespace d2vr_weapon_wheel_graphics {

struct Vertex { float x,y,r,g,b,a; };
struct Color { float r,g,b,a; };
struct Point { float x,y; };
struct Placement { float x=0,y=0,radius=.9f; };
struct Slot {
    d2vr_weapon_icons::Icon icon=d2vr_weapon_icons::Icon::EmptyHand;
    bool available=false;
    bool dual=false; // draw the icon twice (dual wield)
};
struct Wheel {
    int count=4;
    int selected=-1;
    int equipped=-1;
    Slot slots[8]{};
};

constexpr float Pi=3.14159265358979323846f;
constexpr unsigned MaxVerticesPerWheel=4096;
constexpr unsigned MaxVerticesDual=MaxVerticesPerWheel*2;
constexpr float IconRadius=.69f;

inline int Count(const Wheel& wheel) { return wheel.count<0?0:wheel.count>8?8:wheel.count; }
inline float SlotAngle(int index,int count) {
    return count>0?-Pi*.5f+(2*Pi*index)/count:-Pi*.5f;
}
inline Point SlotCenter(int index,int count,const Placement& p=Placement{}) {
    if(count<1 || count>8 || index<0 || index>=count) return {p.x,p.y};
    const float a=SlotAngle(index,count);
    return {p.x+std::cos(a)*p.radius*IconRadius,p.y+std::sin(a)*p.radius*IconRadius};
}

namespace detail {
inline Vertex V(float x,float y,Color c) { return {x,y,c.r,c.g,c.b,c.a}; }
template<class Emit> inline void Triangle(Emit& emit,Point a,Point b,Point c,Color color) {
    emit(V(a.x,a.y,color),V(b.x,b.y,color),V(c.x,c.y,color));
}
template<class Emit> inline void Quad(Emit& emit,float x0,float y0,float x1,float y1,Color color) {
    Triangle(emit,{x0,y0},{x1,y0},{x1,y1},color);
    Triangle(emit,{x0,y0},{x1,y1},{x0,y1},color);
}
template<class Emit> inline void Arc(Emit& emit,const Placement& p,float inner,float outer,
                                    float start,float end,int steps,Color color) {
    for(int i=0;i<steps;++i) {
        const float a=start+(end-start)*float(i)/steps;
        const float b=start+(end-start)*float(i+1)/steps;
        const Point q0{p.x+inner*p.radius*std::cos(a),p.y+inner*p.radius*std::sin(a)};
        const Point q1{p.x+outer*p.radius*std::cos(a),p.y+outer*p.radius*std::sin(a)};
        const Point q2{p.x+outer*p.radius*std::cos(b),p.y+outer*p.radius*std::sin(b)};
        const Point q3{p.x+inner*p.radius*std::cos(b),p.y+inner*p.radius*std::sin(b)};
        Triangle(emit,q0,q1,q2,color);
        if(inner>0) Triangle(emit,q0,q2,q3,color);
    }
}
template<class Emit> inline void Icon(Emit& emit,d2vr_weapon_icons::Icon icon,
                                     float x,float y,float size,bool mirror,Color color) {
    const auto shape=d2vr_weapon_icons::Get(icon);
    for(std::size_t j=0;j<shape.count;++j) {
        const auto& polygon=shape.polygons[j];
        if(polygon.count<3) continue;
        const auto at=[&](std::size_t k) {
            const auto& v=polygon.points[k];
            return Point{x+(mirror?-1.f:1.f)*(v.x-50.f)*size/100.f,y+(v.y-50.f)*size/100.f};
        };
        // Icon polygons may have either original winding. Normalize it after
        // mirroring so all triangles follow the same y-down convention.
        for(std::size_t k=1;k+1<polygon.count;++k) {
            const Point a=at(0),b=at(k),c=at(k+1);
            if((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)<0) Triangle(emit,a,c,b,color);
            else Triangle(emit,a,b,c,color);
        }
    }
}
} // namespace detail

template<class Emit> inline void BuildWheel(const Wheel& wheel,bool leftHand,Emit&& emit,
                                           const Placement& p=Placement{}) {
    const int count=Count(wheel);
    if(!count || !std::isfinite(p.x) || !std::isfinite(p.y) ||
       !std::isfinite(p.radius) || p.radius<=0) return;
    const Color dark{.018f,.018f,.023f,.94f};
    const Color idle{.20f,.185f,.18f,.90f};
    const Color selectedFill{.26f,.022f,.035f,.95f};
    const Color white{.92f,.895f,.845f,1.f};
    const Color muted{.29f,.275f,.27f,1.f};
    const Color accent{.74f,.055f,.075f,1.f};
    const int steps=(64+count-1)/count;
    for(int i=0;i<count;++i) {
        const bool available=wheel.slots[i].available;
        const bool selected=available && i==wheel.selected;
        const float center=SlotAngle(i,count),half=Pi/count;
        const float gap=count>1?.012f:0.f;
        const float a=center-half+gap,b=center+half-gap;
        detail::Arc(emit,p,.305f,.986f,a,b,steps,selected?selectedFill:dark);
        detail::Arc(emit,p,.986f,1.f,a,b,steps,selected?white:available?idle:Color{.075f,.068f,.068f,.8f});
        detail::Arc(emit,p,.295f,.305f,a,b,steps,selected?accent:idle);
        const Point icon=SlotCenter(i,count,p);
        // Icon unions use opaque ink so overlapping convex components do not
        // darken each other through repeated alpha blending.
        const Color iconColor=available?white:Color{muted.r,muted.g,muted.b,1.f};
        if(wheel.slots[i].dual) {
            // Two overlapping copies; the front copy is drawn last in opaque ink.
            detail::Icon(emit,wheel.slots[i].icon,icon.x-.06f*p.radius,icon.y-.05f*p.radius,.38f*p.radius,false,iconColor);
            const Color gapInk=selected?selectedFill:dark; // separates the two silhouettes
            detail::Icon(emit,wheel.slots[i].icon,icon.x+.06f*p.radius,icon.y+.06f*p.radius,.44f*p.radius,false,
                         Color{gapInk.r,gapInk.g,gapInk.b,1.f});
            detail::Icon(emit,wheel.slots[i].icon,icon.x+.06f*p.radius,icon.y+.06f*p.radius,.38f*p.radius,false,iconColor);
        } else detail::Icon(emit,wheel.slots[i].icon,icon.x,icon.y,.46f*p.radius,false,iconColor);
        if(i==wheel.equipped && available) {
            Placement marker{p.x+.925f*p.radius*std::cos(center),p.y+.925f*p.radius*std::sin(center),p.radius};
            detail::Arc(emit,marker,0,.028f,0,2*Pi,8,accent);
        }
    }
    detail::Arc(emit,p,0,.25f,0,2*Pi,64,dark);
    detail::Arc(emit,p,.25f,.257f,0,2*Pi,64,accent);
    detail::Icon(emit,d2vr_weapon_icons::Icon::EmptyHand,p.x,p.y-.035f*p.radius,
                 .27f*p.radius,leftHand,white);
    // Explicit L/R letter paths; do not depend on a font atlas.
    const float s=p.radius*.8f,x=p.x-.05f*s,y=p.y+.14f*s,w=.024f*s;
    detail::Quad(emit,x,y,x+w,y+.10f*s,white);
    if(leftHand) detail::Quad(emit,x,y+.076f*s,x+.09f*s,y+.10f*s,white);
    else {
        detail::Quad(emit,x,y,x+.09f*s,y+w,white);
        detail::Quad(emit,x,y+.045f*s,x+.09f*s,y+.069f*s,white);
        detail::Quad(emit,x+.066f*s,y,x+.09f*s,y+.069f*s,white);
        detail::Triangle(emit,{x+.041f*s,y+.064f*s},{x+.07f*s,y+.064f*s},{x+.11f*s,y+.10f*s},white);
        detail::Triangle(emit,{x+.041f*s,y+.064f*s},{x+.11f*s,y+.10f*s},{x+.08f*s,y+.10f*s},white);
    }
}

// Preview/flat-layout convenience. In VR prefer BuildWheel twice with separate
// hand anchors; both wheels intentionally have identical slot geometry.
template<class Emit> inline void BuildDual(const Wheel& left,const Wheel& right,Emit&& emit) {
    BuildWheel(left,true,emit,Placement{-.5f,0,.43f});
    BuildWheel(right,false,emit,Placement{.5f,0,.43f});
}

} // namespace d2vr_weapon_wheel_graphics

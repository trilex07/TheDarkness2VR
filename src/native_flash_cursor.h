#pragma once
#include <cmath>
#include <cstdint>

namespace d2vr_flash_cursor {
// Full rendered texture, not the XR output allocation or physical HWND. Crop
// is the imageRect actually submitted on the quad, normalized to that texture.
struct Image {
    int width=0,height=0;
    double x=0,y=0,w=0,h=0;
};
struct Movie {
    int width=0,height=0,viewW=0,viewH=0,offsetX=0,offsetY=0;
    float scaleX=0,scaleY=0;
};
struct Point {int x=0,y=0;};
inline bool Valid(const Image& image,const Movie& movie) {
    const double values[]={image.x,image.y,image.w,image.h,movie.scaleX,movie.scaleY};
    for(double value:values)if(!std::isfinite(value))return false;
    if(image.width<=0 || image.height<=0 || image.width>16384 || image.height>16384 ||
       movie.viewW!=image.width || movie.viewH!=image.height ||
       movie.width<=0 || movie.height<=0 || movie.width>16384 || movie.height>16384 ||
       image.x<0 || image.y<0 || image.w<=0 || image.h<=0 ||
       image.x+image.w>1.00000001 || image.y+image.h>1.00000001 ||
       movie.offsetX || movie.offsetY)return false;
    // Only the measured CEDC30 zero-offset full-viewport contract is admitted.
    // A different letterbox/scaling mode needs separate producer evidence.
    return std::fabs(double(movie.scaleX)-double(movie.width)/movie.viewW)<1e-6 &&
           std::fabs(double(movie.scaleY)-double(movie.height)/movie.viewH)<1e-6;
}
// Native 6D56B0 rounds float32 (raw -/+ halfOffset) * scale with floor(v+.5).
// It is not multiplication by the eye-cache size, nor client-to-Flash scaling.
inline Point Convert(Point raw,const Movie& movie) {
    const float x=(float(raw.x)-float(movie.offsetX)*.5f)*movie.scaleX;
    const float y=(float(raw.y)+float(movie.offsetY)*.5f)*movie.scaleY;
    return {int(std::floor(x+.5f)),int(std::floor(y+.5f))};
}
inline bool MapRaw(const Image& image,const Movie& movie,double u,double v,Point& output) {
    if(!Valid(image,movie) || !std::isfinite(u) || !std::isfinite(v) ||
       u<0 || v<0 || u>1 || v>1)return false;
    // Map the displayed crop back to the complete native render viewport.
    // Preserve the native integer-pixel input convention at inclusive edges.
    const double x=(image.x+u*image.w)*image.width;
    const double y=(image.y+v*image.h)*image.height;
    Point raw{int(std::floor(x+.5)),int(std::floor(y+.5))};
    if(raw.x>=image.width)raw.x=image.width-1;
    if(raw.y>=image.height)raw.y=image.height-1;
    output=raw;return true;
}
inline bool Map(const Image& image,const Movie& movie,double u,double v,Point& output) {
    Point raw;if(!MapRaw(image,movie,u,v,raw))return false;
    output=Convert(raw,movie);return true;
}
// Production dispatch contract, also used by the CPU actual-body fixture.
// Original is invoked exactly once, even when ownership/math refuses.
template<class Original,class Project> bool Transform(bool enabled,uintptr_t caller,
        uintptr_t movie,int* x,int* y,Original original,Project project) {
    original(movie,x,y);
    if(!enabled || caller!=0xCB727C || !movie || !x || !y)return false;
    Point point;
    if(!project(movie,point))return false;
    *x=point.x;*y=point.y;return true;
}
}

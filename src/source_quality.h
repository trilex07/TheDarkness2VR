#pragma once
// Bounded source sizing. Exact is an explicit native render target request;
// existing Native/Quality/High modes retain their aspect-preserving policy.
#include <cstdint>
#include <cmath>

namespace d2vr_source_quality {
// OFF keeps the validated baseline. Native installs the same scoped observer
// but returns the real client size; Quality/High add approximately 26.6%/56.3% pixels.
enum class Mode : unsigned { Off=0, Native=1, Quality=2, High=3, Exact=4 };
struct Size { unsigned width=0,height=0; };
inline Mode Parse(unsigned value) {return value<=4?static_cast<Mode>(value):Mode::Off;}
inline Size Choose(Mode mode,unsigned width,unsigned height,Size exact={}) {
    if(mode==Mode::Off || unsigned(mode)>4 || width<640 || height<480 ||
       width>3840 || height>3840 || uint64_t(width)*height>8294400) return {};
    if(mode==Mode::Exact) {
        if(exact.width<640 || exact.height<480 || exact.width>3840 || exact.height>3840 ||
           uint64_t(exact.width)*exact.height>8294400)return {};
        return exact;
    }
    if(mode==Mode::Native) return {width,height};
    const double factor=mode==Mode::Quality?1.125:1.25;
    // Refuse rather than silently crop, distort or downgrade an oversized plan.
    const auto w=unsigned(std::floor(width*factor*.5+.5))*2;
    const auto h=unsigned(std::floor(height*factor*.5+.5))*2;
    if(w>3840 || h>3840 || uint64_t(w)*h>8294400 || w<width || h<height) return {};
    return {w,h};
}
inline bool Eligible(bool ok,bool hasRect,uintptr_t hwnd,uintptr_t ownedHwnd,
                     uintptr_t caller,uintptr_t nativeCaller) {
    return ok && hasRect && hwnd && hwnd==ownedHwnd && caller && caller==nativeCaller;
}
}

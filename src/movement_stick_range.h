#pragma once
// Shared Trilex movement-stick calibration. Keep identical across all three mods.
#include <algorithm>
#include <cmath>
namespace trilex::input {
inline int ClampOuterPercent(int value) { return std::clamp(value,0,40); }
// Zero is an exact pass-through. Otherwise remap the radial range [0,1-d] to
// [0,1], preserving direction and analog speed. No inner deadzone or forced run.
inline void CalibrateMovementStick(float& x,float& y,int outerPercent) {
    const int outer=ClampOuterPercent(outerPercent);
    if (!outer) return;
    if (!std::isfinite(x) || !std::isfinite(y)) { x=0; y=0; return; }
    const float radius=std::hypot(x,y);
    if (!radius) return;
    const float limit=1.f-.01f*static_cast<float>(outer);
    const float scale=std::min(radius/limit,1.f)/radius;
    x*=scale; y*=scale;
}
// Optional bounded input evidence: raw directional peaks and current mapped
// values, at most 60 reports spaced five seconds apart. No controller identity assumptions.
struct MovementStickProbe {
    float peakForward=0,peakBack=0,peakLeft=0,peakRight=0;
    unsigned reports=0;
    unsigned long long next=0;
    bool Observe(float x,float y,unsigned long long now) {
        if (reports>=60) return false;
        if (std::isfinite(x) && std::isfinite(y)) {
            peakForward=std::max(peakForward,y); peakBack=std::max(peakBack,-y);
            peakLeft=std::max(peakLeft,-x); peakRight=std::max(peakRight,x);
        }
        if (now<next) return false;
        next=now+5000; ++reports; return true;
    }
};
}

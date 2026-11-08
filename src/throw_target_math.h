#pragma once
#include <cmath>
#include <limits>
namespace d2vr_throw_target {
// Angular distance first; world distance must never beat a better aligned enemy.
inline float Score(const float origin[3],const float direction[3],const float point[3]) {
    double dot=0,length=0,axis=0;
    for(unsigned i=0;i<3;++i) {
        const double v=double(point[i])-origin[i];
        if(!std::isfinite(v) || !std::isfinite(direction[i]))return (std::numeric_limits<float>::max)();
        dot+=v*direction[i];length+=v*v;axis+=double(direction[i])*direction[i];
    }
    if(length<1e-8 || axis<.25 || axis>4 || dot<=0)return (std::numeric_limits<float>::max)();
    return float(1.-dot/std::sqrt(length*axis));
}
}

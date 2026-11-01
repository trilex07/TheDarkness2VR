#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace d2vr_camera_event {
// 00631560..00631572 / 0043BCB0: owner+828 -> controller ->
// controller+74 smart reference -> native avatar. Unlike avatar+5E0, this
// route identified the live producer in the 2026-11-01 event capture.
template<class Read> bool Matches(uint32_t owner,uint32_t avatar,Read read,uint32_t ownerTable=0xfdf170) {
    const auto word=[&](uint32_t base,uint32_t offset,uint32_t& result) {
        return base>=0x10000 && !(base&3) && base<=0x7fffffff-offset && read(base+offset,result);
    };
    uint32_t table=0,reference=0,controller=0,avatarReference=0,found=0;
    return word(avatar,0,table) && table==0x1001630 &&
        word(owner,0,table) && table==ownerTable &&
        word(owner,0x828,reference) && word(reference,0,controller) &&
        word(controller,0,table) && table==0xfe3ee8 &&
        word(controller,0x74,avatarReference) && word(avatarReference,0,found) && found==avatar;
}
struct Sample {
    float impactPosition[3]{},impactRotation[3]{};
    float environmentalPosition[3]{},environmentalRotation[3]{};
};
inline float Amount(const Sample& sample) {
    float magnitude=0;
    for(const auto* vector:{sample.impactPosition,sample.impactRotation,
                           sample.environmentalPosition,sample.environmentalRotation})
        for(unsigned i=0;i<3;++i) {
            if(!std::isfinite(vector[i]))return 0;
            magnitude=(std::max)(magnitude,std::fabs(vector[i]));
        }
    // Recorded paused residue is ~0.00012. Walking noise lives in +8C0
    // and is deliberately absent from Sample. Haptic scaling is a candidate.
    return std::clamp((magnitude-.01f)/.75f,0.f,1.f);
}
}

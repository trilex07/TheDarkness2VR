#pragma once
#include <cstdint>
#include <cmath>

namespace d2vr_cinema {
enum class Mode { Gameplay, Scene3D, Video2D };
enum class Override { Auto, Gameplay, Scene3D };
// One atomic word publishes the requested mode and its monotonic expiry together.
inline Override TemporaryOverride(uint64_t request,uint64_t nowMs) {
    return nowMs >= (request >> 1) ? Override::Auto :
        (request & 1) ? Override::Scene3D : Override::Gameplay;
}
inline uint64_t RequestOpposite(Mode current,uint64_t nowMs) {
    return ((nowMs+60000) << 1) | (current==Mode::Gameplay ? 1 : 0);
}
struct Playback {
    uintptr_t handles[16]{};
    void Observe(uintptr_t handle,bool playing) {
        if(!handle)return;
        uintptr_t* empty=nullptr;
        for(auto& slot:handles) {
            if(slot==handle) {slot=playing?handle:0;return;}
            if(!slot && !empty)empty=&slot;
        }
        if(playing && empty)*empty=handle;
    }
    bool Active()const {for(auto h:handles)if(h)return true;return false;}
};
// Missing observations are UNKNOWN, never a sticky cinematic from a previous frame.
inline Mode Select(bool enabled,bool sameFrame,bool cinematic,bool video,Override manual=Override::Auto) {
    if(!enabled)return Mode::Gameplay;
    if(manual==Override::Gameplay)return Mode::Gameplay;
    if(manual==Override::Scene3D)return sameFrame ? Mode::Scene3D : Mode::Gameplay;
    return video ? Mode::Video2D :
        sameFrame && cinematic ? Mode::Scene3D : Mode::Gameplay;
}
// Crop both rendered projections to the same angular rectangle before placing
// them on equal-sized eye-specific screens. Never stretch unequal eye frusta.
inline float CommonCrop(float halfAngle,float commonAngle) {
    if(!std::isfinite(halfAngle) || !std::isfinite(commonAngle) ||
       commonAngle<=0 || halfAngle<commonAngle || halfAngle>=1.56f)return 0;
    return std::tan(commonAngle)/std::tan(halfAngle);
}
}

#pragma once
#include <cstdint>
#include <cmath>

namespace d2vr_cinema {
enum class Mode { Gameplay, Scene3D, Video2D };
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
inline Mode Select(bool enabled,bool sameFrame,bool cinematic,bool video) {
    return !enabled ? Mode::Gameplay : video ? Mode::Video2D :
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

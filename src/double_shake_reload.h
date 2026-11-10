#pragma once
#include "vr_input.h"

namespace d2vr_input {
// Four alternating powered strokes = two shakes. Each hand owns its history.
// No angular/native camera input is produced. Call once per current grip sample.
struct DoubleShakeReload {
    double last=-1,started=-1,strokeAt=-1,lastStroke=-1,cooldown=-1;
    int axis=-1,direction=0,candidate=0;
    unsigned strokes=0;
    bool armed=false;
    bool Update(bool allowed,Vec3 velocity,double time) {
        const double dt=last<0?0:time-last;
        if(!allowed || !std::isfinite(time) || !std::isfinite(velocity.x) ||
           !std::isfinite(velocity.y) || !std::isfinite(velocity.z) ||
           (last>=0 && (dt<0 || dt>.15))) {*this={};return false;}
        if(time==last)return false;
        last=time;
        if(time<cooldown)return false;
        const float speed=Length(velocity);
        if(!std::isfinite(speed)){*this={};return false;}
        if(!armed){if(speed<.35f)armed=true;return false;}
        if((started>=0 && time-started>.9) || (lastStroke>=0 && time-lastStroke>.35)) {
            *this={};last=time;armed=speed<.35f;return false;
        }
        const float values[]={velocity.x,velocity.y,velocity.z};
        if(axis<0) {
            if(speed<1.2f)return false;
            axis=0;
            for(int i=1;i<3;++i)if(std::fabs(values[i])>std::fabs(values[axis]))axis=i;
            started=time;
        }
        const float v=values[axis];
        // A brief acceleration spike or a diagonal weak component is not a stroke.
        if(std::fabs(v)<1.2f){candidate=0;strokeAt=-1;return false;}
        const int sign=v>0?1:-1;
        if(sign==direction){candidate=0;strokeAt=-1;return false;}
        if(candidate!=sign){candidate=sign;strokeAt=time;return false;}
        if(time-strokeAt<.035)return false;
        direction=sign;lastStroke=time;candidate=0;strokeAt=-1;
        if(++strokes<4)return false;
        *this={};last=time;cooldown=time+1.;return true;
    }
};
}

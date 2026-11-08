#pragma once
#include "vr_input.h"

namespace d2vr_input {
struct HeartPullOutput { bool eat=false, ownsGrip=false, fired=false; };
// A deliberate inward pull requests the stock contextual devour action. Native
// targeting remains authoritative; this recognizer never chooses a corpse.
struct HeartPull {
    bool armed=false, down=false, consumed=false;
    double last=-1, started=0, eatUntil=0;
    Vec3 origin{}, direction{}, previous{};
    float distance=0;
    HeartPullOutput Update(bool context,bool tracked,float squeeze,Vec3 hand,Vec3 head,double time) {
        HeartPullOutput out;
        const double dt=last<0 ? 0:time-last;
        const float radius=Length(Difference(hand,head));
        if(!context || !tracked || !std::isfinite(time) || !std::isfinite(squeeze) ||
           squeeze<0 || squeeze>1 || !std::isfinite(radius) ||
           (last>=0 && (dt<0 || dt>.15))) {
            *this={}; return out; // release required after tracking/context loss
        }
        last=time;
        if(squeeze<.35f) {
            out.ownsGrip=consumed; // swallow the release of an eaten-heart stroke
            armed=true;down=consumed=false;eatUntil=0;return out;
        }
        if(!armed)return out;
        if(!down && squeeze>.65f) {
            down=true;started=time;origin=previous=hand;distance=radius;
            if(radius<.30f || radius>1.10f) {armed=false;down=false;return out;}
            direction=Difference(head,hand);
            direction={direction.x/radius,direction.y/radius,direction.z/radius};
        }
        if(!down)return out;
        if(!consumed && dt>0) {
            const float step=Length(Difference(hand,previous));
            if(!std::isfinite(step) || step>.20f || step/float(dt)>5.f) {
                armed=down=false;return out;
            }
            const auto travel=Difference(hand,origin);
            const float inward=travel.x*direction.x+travel.y*direction.y+travel.z*direction.z;
            // Both controller travel and head-relative contraction are needed:
            // looking/leaning toward a stationary controller is not a pull.
            if(time-started>=.08 && time-started<=1.0 && inward>=.18f &&
               distance-radius>=.14f && radius<=.65f) {
                consumed=true;eatUntil=time+1.20;out.fired=true;
            }
        }
        previous=hand;
        out.ownsGrip=consumed;
        out.eat=consumed && time<eatUntil;
        return out;
    }
};
}

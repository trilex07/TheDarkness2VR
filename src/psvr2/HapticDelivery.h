#pragma once
#include "TriggerPolicy.h"
namespace d2vr::psvr2 {
// A finite pulse belongs to an event, not an input poll. Continuous effects
// may renew only after their requested duration, never on every frame.
struct HapticDelivery {
    GripHaptic last{};
    std::uint64_t event=0, until=0;
    bool active=false;
    bool needsApply(const GripHaptic& value,std::uint64_t identity,
                    std::uint64_t now,bool continuous) const {
        if(!gripHapticActive(value))return false;
        return identity!=event || value.amplitude!=last.amplitude ||
            value.frequency!=last.frequency || value.durationNanoseconds!=last.durationNanoseconds ||
            (continuous && now>=until);
    }
    void applied(const GripHaptic& value,std::uint64_t identity,std::uint64_t now) {
        last=value;event=identity;until=now+(value.durationNanoseconds+999999)/1000000;active=true;
    }
    void stopped(){*this={};}
};
}

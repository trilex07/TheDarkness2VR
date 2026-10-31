#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <initializer_list>
#include "../src/present_overlap.h"
#define CHECK(x) do {if(!(x)){std::printf("FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using d2vr_present_overlap::Policy;
static unsigned checks=0;
#define C(x) do{CHECK(x);++checks;}while(0)
int main() {
    { // A callback inside the native forward must never forward this slot twice.
        d2vr_present_overlap::Slot slot;slot.armed=true;unsigned forwards=0;
        CHECK(d2vr_present_overlap::Early(slot,[&]()->long {
            ++forwards;
            CHECK(!d2vr_present_overlap::Early(slot,[&]()->long{++forwards;return 7;}));
            return -9;
        }));
        CHECK(d2vr_present_overlap::Finish(slot,[&]()->long{++forwards;return 0;})==-9);
        CHECK(forwards==1);
    }

    // Warm-up: no overlap before both EMAs have kWarmup plain samples.
    {
        Policy p;
        for(unsigned i=0;i<Policy::kWarmup;++i) {C(!p.Allow());p.ObservePresent(0.36);p.ObserveWait(1.40);}
        C(p.Allow() && std::fabs(p.WaitEma()-1.40)<1e-9 && std::fabs(p.PresentEma()-0.36)<1e-9);
    }
    // Margin: wait must exceed Present by kMarginMs.
    for(double wait:{0.30,0.36,0.60,0.6099,0.61,0.62,1.4}) {
        Policy p;for(unsigned i=0;i<Policy::kWarmup;++i){p.ObservePresent(0.36);p.ObserveWait(wait);}
        C(p.Allow()==(wait>=0.36+Policy::kMarginMs));
    }
    // Missing Present samples keep it closed (plain waits alone never admit).
    {Policy p;for(unsigned i=0;i<1000;++i)p.ObserveWait(5);C(!p.Allow());}
    // Overrun: GPU done before the early Present returned -> immediate backoff,
    // kBackoff plain re-measurements before the next overlap.
    {
        Policy p;for(unsigned i=0;i<Policy::kWarmup;++i){p.ObservePresent(0.36);p.ObserveWait(1.4);}
        C(p.Allow());
        p.ObserveOverlapped(0.40,0.0,true);
        C(!p.Allow() && p.overruns==1 && p.Cooldown()==Policy::kBackoff);
        for(unsigned i=0;i<Policy::kBackoff-1;++i){p.ObserveWait(1.4);C(!p.Allow());}
        p.ObserveWait(1.4);C(p.Allow() && p.Cooldown()==0);
        // Recovered window shrinking below Present+margin closes overlap again.
        for(unsigned i=0;i<64;++i)p.ObserveWait(0.4);
        C(!p.Allow());
    }
    // Overlapped frames keep the window estimate honest (present + residual).
    {
        Policy p;for(unsigned i=0;i<Policy::kWarmup;++i){p.ObservePresent(0.36);p.ObserveWait(1.4);}
        for(unsigned i=0;i<10000;++i){C(p.Allow());p.ObserveOverlapped(0.36,1.04,false);}
        C(std::fabs(p.WaitEma()-1.40)<1e-6 && p.overruns==0 && p.overlapped==10000);
        // A slow Present stream (1.5 ms) closes overlap before it can delay XR.
        for(unsigned i=0;i<64;++i)p.ObservePresent(1.5);
        C(!p.Allow());
    }
    // Garbage samples are ignored.
    {
        Policy p;for(unsigned i=0;i<Policy::kWarmup;++i){p.ObservePresent(0.36);p.ObserveWait(1.4);}
        const double w=p.WaitEma(),pr=p.PresentEma();
        p.ObserveWait(std::numeric_limits<double>::quiet_NaN());p.ObserveWait(-1);p.ObserveWait(1e9);
        p.ObservePresent(std::numeric_limits<double>::quiet_NaN());p.ObservePresent(-3);
        C(p.WaitEma()==w && p.PresentEma()==pr && p.Allow());
    }
    // Forward-once slot: exactly one forward per outer call in every order.
    {
        using namespace d2vr_present_overlap;
        for(unsigned trial=0;trial<100000;++trial) {
            unsigned calls=0;const long hr=long(trial%3==0?0:trial%3==1?0x08760878:0x88760868);
            auto fwd=[&]{++calls;return hr;};
            Slot s;s.armed=(trial&1)!=0;
            const bool early=(trial&2)!=0;
            const bool armedAtEntry=s.armed;
            const bool did=early && Early(s,fwd);
            C(did==(early && armedAtEntry));
            if(did) C(!Early(s,fwd));                 // never a second early forward
            const unsigned beforeFinish=calls;
            C(Finish(s,fwd)==hr);
            C(calls==1 && (did ? beforeFinish==1 : beforeFinish==0));
            C(!s.armed && !s.done);                   // slot cleared for the next call
        }
        Slot unarmed;unsigned n=0;auto f=[&]{++n;return 0L;};
        C(!Early(unarmed,f) && n==0);
    }
    std::printf("PASS present overlap policy/slot: %u checks (warm-up, margin, overrun backoff/recovery, slow Present, NaN, exactly-once forward)\n",checks);
    return 0;
}

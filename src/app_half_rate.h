#pragma once
#include <cmath>
namespace d2vr_adaptive {
// Deadline is based on the actual previous frame start: missed deadlines never
// accumulate debt, and a runtime already pacing at half rate adds no extra wait.
class AppHalfRate {
    double previous_=0;
public:
    void Reset(){previous_=0;}
    double Delay(double now,double hz,bool half)const {
        if(!half || previous_<=0 || !std::isfinite(now) || !std::isfinite(hz) || hz<1 || hz>1000)return 0;
        const double elapsed=now-previous_;
        return elapsed>=0 && elapsed<2/hz ? 2/hz-elapsed:0;
    }
    void Started(double now){previous_=now;}
};
}


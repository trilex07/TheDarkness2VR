#pragma once
#include <cstdint>

// Overlap the native mirror Present with the right-eye GPU wait.
//
// Evidence (mode-4 combat, PID 26264): the render thread spins ~1.4 ms per
// frame in DrainOwnerProducer until the GPU finishes the right eye, and only
// AFTER that runs the native SwapChain Present (0.36 ms mean). The Present
// depends on no right-eye result: by then RT0 holds the restored live image
// and every pair command is already queued ahead of it. Forwarding it inside
// the wait hides its CPU wall time without moving any OpenXR call.
//
// Zero added latency is enforced by measurement, not assumed: overlap is only
// admitted while the measured wait exceeds the measured Present by a margin,
// and any frame where the GPU finished before the early Present returned
// (possible delay of xrEndFrame) backs off and re-measures plain frames.
// Pure policy: no Windows/D3D calls, deterministic for offline tests.
namespace d2vr_present_overlap {
class Policy {
    double wait_=0,present_=0;
    unsigned waitSamples_=0,presentSamples_=0,cooldown_=0;
    static void Blend(double& ema,unsigned& n,double sample) {
        if(!(sample>=0) || sample>1000) return;           // NaN/negative/absurd sample ignored
        ema = n ? ema + kAlpha*(sample-ema) : sample;
        if(n<0xffffffffu) ++n;
    }
public:
    static constexpr double kAlpha=0.125;     // ~8-frame memory
    static constexpr double kMarginMs=0.25;   // wait must exceed Present by this much
    static constexpr unsigned kWarmup=32;     // plain measurements before first overlap
    static constexpr unsigned kBackoff=90;    // plain frames after a possible overrun
    uint64_t overlapped=0,overruns=0,plain=0;

    bool Allow() const {
        return waitSamples_>=kWarmup && presentSamples_>=kWarmup && !cooldown_ &&
            wait_>=present_+kMarginMs;
    }
    // Native Present wall time measured around the original forward.
    void ObservePresent(double ms) { Blend(present_,presentSamples_,ms); }
    // Plain frame: the full right-eye wait window, Present not overlapped.
    void ObserveWait(double ms) { Blend(wait_,waitSamples_,ms); ++plain; if(cooldown_) --cooldown_; }
    // Overlapped frame. If the GPU was already done when Present returned,
    // xrEndFrame may have been delayed: back off and re-measure plain frames.
    void ObserveOverlapped(double presentMs,double residualWaitMs,bool completedDuringPresent) {
        ++overlapped;
        ObservePresent(presentMs);
        if(completedDuringPresent) { ++overruns; cooldown_=kBackoff; return; }
        Blend(wait_,waitSamples_,presentMs+residualWaitMs); // the window was at least this long
    }
    double WaitEma() const {return wait_;}
    double PresentEma() const {return present_;}
    unsigned Cooldown() const {return cooldown_;}
};

// Forward-once slot for the outer native Present call on one thread. Early()
// may run the forward from inside before(); Finish() then returns that result
// instead of forwarding a second time.
struct Slot { bool armed=false,done=false; long result=0; };
template<class Forward> bool Early(Slot& slot,Forward&& forward) {
    if(!slot.armed || slot.done) return false;
    slot.result=forward(); slot.done=true; return true;
}
template<class Forward> long Finish(Slot& slot,Forward&& forward) {
    const long result=slot.done ? slot.result : forward();
    slot=Slot{};
    return result;
}
}

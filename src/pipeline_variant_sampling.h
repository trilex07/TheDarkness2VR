#pragma once
#include <cstdint>
#include <limits>

namespace d2vr_pipeline_warmup {
// Round-robin sampling of actual device state. No setter shadow, raw COM
// identity cache or negative variant cache. Exactly one capture attempt per
// presented frame; transient failures stay eligible on subsequent visits.
// Coverage is eventual for stable draw counts, not guaranteed for short-lived
// pipelines. This avoids claiming exhaustive NVIDIA pipeline specialization.
class VariantSampler {
    uint32_t draw_=0,probe_=0;
public:
    bool Observe() {
        const bool sample=draw_==probe_;
        if(draw_!=std::numeric_limits<uint32_t>::max())++draw_;
        return sample;
    }
    void BeginFrame() {
        probe_=draw_ ? uint32_t((uint64_t(probe_)+1)%draw_) : 0;
        draw_=0;
    }
};
}

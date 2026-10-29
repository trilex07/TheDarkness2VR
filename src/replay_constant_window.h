#pragma once
#include "replay_constant_batch.h"

namespace d2vr_replay_batch {
// Opt-in extension, independent from the adjacent-packet A/B. No device-state
// cache: dirty/value banks live ONLY until the caller's next barrier. Transform
// each original packet first, then retain the final bits for each register.
// VS/PS banks are independent D3D9 state. There is no draw, query, state-block,
// or resource mutation between these writes and their barrier. The caller may
// admit only independent bindings, including validated public render states.
class ConstantWindow {
    float values_[2][256 * 4]; // no initialization/read of unowned register holes
    bool dirty_[2][256];
    unsigned limits_[2]{}, first_[2]{}, end_[2]{};
    bool contiguous_[2]{true,true};
    bool enabled_ = false;
public:
    void Reset(unsigned vsLimit, unsigned psLimit) {
        limits_[0] = vsLimit > 256 ? 256 : vsLimit;
        limits_[1] = psLimit > 224 ? 224 : psLimit;
        memset(dirty_, 0, sizeof(dirty_));
        first_[0] = limits_[0]; first_[1] = limits_[1]; end_[0] = end_[1] = 0;
        contiguous_[0]=contiguous_[1]=true;
        enabled_ = true;
    }
    void Abort() { enabled_ = false; } // next Reset starts a new independent stream
    template<class Send> void Flush(Send send) {
        if (!enabled_) return;
        bool failed = false;
        for (unsigned bank = 0; bank < 2; ++bank) {
            unsigned first = first_[bank]; const unsigned last = end_[bank];
            first_[bank] = limits_[bank]; end_[bank] = 0;
            const bool contiguous=contiguous_[bank];contiguous_[bank]=true;
            if(contiguous && first<last) {
                // A union of overlapping/adjacent uploads has no holes. Consume
                // before driver entry, exactly as the sparse path does.
                memset(dirty_[bank]+first,0,last-first);
                failed |= send(bank==0?Stage::Vertex:Stage::Pixel,first,
                    values_[bank]+first*4,last-first)<0;
                continue;
            }
            // Empty barriers are O(1); scan only the written bounding span.
            for (; first < last;) {
                if (!dirty_[bank][first]) { ++first; continue; }
                unsigned end = first;
                while (end < last && dirty_[bank][end]) {
                    dirty_[bank][end] = false; ++end; // consume before driver entry
                }
                const Stage stage = bank == 0 ? Stage::Vertex : Stage::Pixel;
                failed |= send(stage, first, values_[bank] + first * 4, end - first) < 0;
                first = end;
            }
        }
        if (failed) enabled_ = false;
    }
    template<class Send> void Upload(Stage stage, unsigned first, const float* data,
                                     unsigned count, Send send) {
        const unsigned bank = stage == Stage::Vertex ? 0 : 1;
        const unsigned limit = limits_[bank];
        if (!enabled_ || stage == Stage::None || !data || !count ||
            first >= limit || count > limit - first) {
            Flush(send);
            send(stage, first, data, count); // preserve invalid/unknown API arguments
            return;
        }
        memcpy(values_[bank] + first * 4, data, size_t(count) * 16);
        memset(dirty_[bank] + first, 1, count);
        if(end_[bank] && (first>end_[bank] || first+count<first_[bank]))
            contiguous_[bank]=false; // conservative: later hole fills use sparse path
        if (first < first_[bank]) first_[bank] = first;
        if (first + count > end_[bank]) end_[bank] = first + count;
    }
};
} // namespace d2vr_replay_batch

#pragma once
#include <cstdint>
#include <cstring>

namespace d2vr_replay_batch {
enum class Stage : uint8_t { None, Vertex, Pixel };

// Render-thread local receipt at the actual driver forwarding boundary. Never
// substitutes record counts for API calls; ordinary production omits the audit.
struct UploadCounts {
    uint64_t records[2]{}, calls[2]{}, vectors[2]{}, failures[2]{};
    uint64_t exceptions = 0;
    void Record(Stage stage) { ++records[stage == Stage::Vertex ? 0 : 1]; }
    template<class Send> long Forward(Stage stage, unsigned first, const float* data,
                                     unsigned count, Send send) {
        const unsigned index = stage == Stage::Vertex ? 0 : 1;
        ++calls[index]; // count entry, even if the driver throws or returns failure
        vectors[index] += count;
        const long result = send(stage, first, data, count);
        failures[index] += result < 0;
        return result;
    }
    void Add(const UploadCounts& other) {
        for (unsigned i = 0; i < 2; ++i) {
            records[i] += other.records[i]; calls[i] += other.calls[i];
            vectors[i] += other.vectors[i]; failures[i] += other.failures[i];
        }
        exceptions += other.exceptions;
    }
};

// Replay-local packet assembly, NOT a persistent device-state cache. Only
// ascending, exactly adjacent float4 ranges of the SAME stage can share a call.
// Every other command is a barrier, including queries/state-block restores.
// Transform each original upload BEFORE appending: widening a packet before
// ApplyShiftToUpload would change partial-matrix and upload-count semantics.
class Constants {
    float values_[256 * 4]; // deliberately uninitialized; only written spans submit
    Stage stage_ = Stage::None;
    unsigned first_ = 0, end_ = 0, vsLimit_ = 0, psLimit_ = 0;
    bool enabled_ = false;
    const float* borrowed_ = nullptr;
public:
    Constants(bool enabled, unsigned vsLimit, unsigned psLimit)
        : vsLimit_(vsLimit > 256 ? 256 : vsLimit),
          psLimit_(psLimit > 224 ? 224 : psLimit), enabled_(enabled) {}
    bool CanAppend(Stage stage, unsigned first, unsigned count) const {
        const unsigned limit = stage == Stage::Vertex ? vsLimit_ : psLimit_;
        return enabled_ && stage_ != Stage::None && stage == stage_ &&
               first == end_ && count && first < limit && count <= limit - first;
    }
    void Abort() { stage_ = Stage::None; enabled_ = false; borrowed_=nullptr; }
    template<class Send> void Flush(Send send) {
        if (stage_ == Stage::None) return;
        const auto stage = stage_; const auto first = first_, count = end_ - first_;
        const float* data=borrowed_?borrowed_:values_+first*4;
        borrowed_=nullptr;
        stage_ = Stage::None; // consume BEFORE entering the driver; never retry a call
        if (send(stage, first, data, count) < 0) enabled_ = false;
    }
    template<class Send> void Barrier(Stage next, Send send) {
        if (next == Stage::None || next != stage_) Flush(send);
    }
    template<class Send> void Upload(Stage stage, unsigned first, const float* src,
                                     unsigned count, Send send, bool durable=false) {
        const unsigned limit = stage == Stage::Vertex ? vsLimit_ : psLimit_;
        if (!enabled_ || stage == Stage::None || !src || !count ||
            first >= limit || count > limit - first) {
            Flush(send);
            send(stage, first, src, count); // invalid/unknown calls preserve original args
            return;
        }
        if (stage != stage_ || first != end_) Flush(send);
        if (!enabled_) { send(stage, first, src, count); return; }
        if (stage_ == Stage::None) {
            stage_ = stage; first_ = end_ = first;
            // Only explicitly durable recorder-arena spans may be borrowed.
            // Transformed stack packets keep the original owned-copy path.
            if(durable){borrowed_=src;end_=first+count;return;}
        }
        if(borrowed_){memcpy(values_+first_*4,borrowed_,size_t(end_-first_)*16);borrowed_=nullptr;}
        memcpy(values_ + first * 4, src, size_t(count) * 4 * sizeof(float));
        end_ += count;
    }
};
} // namespace d2vr_replay_batch

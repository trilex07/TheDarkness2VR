#pragma once
// Gaze sample shared from the x64 PSVR2 bridge to the x86 game DLL through a
// named section "Local\Darkness2VR.Gaze.<gamePid>". Single writer (bridge),
// seqlock: seq is odd while writing. Identical layout on x86 and x64.
#include <atomic>
#include <cstdint>
#include <cstring>

namespace d2vr::psvr2 {

constexpr uint32_t kGazeMagic=0x5A473244; // "D2GZ"
constexpr uint32_t kGazeVersion=1;
enum GazeFlags : uint32_t {
    GazeCombinedValid=1u<<0, GazeLeftValid=1u<<1, GazeRightValid=1u<<2,
    GazeBlinkLeft=1u<<3, GazeBlinkRight=1u<<4, GazeFoveatedValid=1u<<5,
};
struct GazeSample {
    uint64_t publishMs=0;      // bridge GetTickCount64 at publish (same clock as the game)
    int64_t deviceTimestamp=0; // Toolkit wearable.timestamp
    uint32_t frameCounter=0;
    uint32_t flags=0;
    float dirCombined[3]{};    // raw Toolkit axes, normalised
    float dirLeft[3]{}, dirRight[3]{};
    float foveatedCombined[3]{};
    uint32_t trackingState=0;
    float convergenceMm=0;
};
struct GazeShared {
    uint32_t magic, version;
    std::atomic<uint32_t> seq;
    uint32_t reserved;
    GazeSample sample;
};
static_assert(sizeof(std::atomic<uint32_t>)==4, "atomic layout");
static_assert(sizeof(GazeSample)==80, "GazeSample layout must match on x86/x64");

inline void GazeWrite(GazeShared& s,const GazeSample& v) {
    const uint32_t q=s.seq.load(std::memory_order_relaxed);
    s.seq.store(q+1,std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    std::memcpy(&s.sample,&v,sizeof v);
    std::atomic_thread_fence(std::memory_order_release);
    s.seq.store(q+2,std::memory_order_release);
}
// False when no stable copy was obtained (writer active or never written).
inline bool GazeRead(const GazeShared& s,GazeSample& out) {
    for(int attempt=0;attempt<4;++attempt) {
        const uint32_t a=s.seq.load(std::memory_order_acquire);
        if(a&1u || !a) continue;
        std::memcpy(&out,&s.sample,sizeof out);
        std::atomic_thread_fence(std::memory_order_acquire);
        if(s.seq.load(std::memory_order_relaxed)==a) return s.magic==kGazeMagic && s.version==kGazeVersion;
    }
    return false;
}

} // namespace d2vr::psvr2

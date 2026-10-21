#pragma once
// Shared D3D9 frame-boundary state machine for live hooks and offline replay.
// Policies are explicit; no hidden heuristics in TraceOnly / PresentPrimary.

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace d2vr_frame {

enum class EventType : uint8_t {
    BeginScene = 0,
    EndScene,
    DevicePresent,
    PresentEx,
    SwapChainPresent,
    Reset,
    ResetEx,
    ResetSucceeded,
};

enum class Policy : int {
    // One logical XR boundary per real presentation call (Device / Ex / SwapChain).
    PresentPrimary = 0,
    // rc20/rc20b: nested EndScene closes + 9 ms debounce when Device Present never seen.
    LegacyRc20b = 1,
    // Record pipeline events only; hooks must not call VrFrame from this machine.
    TraceOnly = 2,
};

inline const char* EventTypeName(EventType t) {
    switch (t) {
        case EventType::BeginScene: return "BeginScene";
        case EventType::EndScene: return "EndScene";
        case EventType::DevicePresent: return "DevicePresent";
        case EventType::PresentEx: return "PresentEx";
        case EventType::SwapChainPresent: return "SwapChainPresent";
        case EventType::Reset: return "Reset";
        case EventType::ResetEx: return "ResetEx";
        case EventType::ResetSucceeded: return "ResetSucceeded";
    }
    return "?";
}

struct Event {
    EventType type = EventType::BeginScene;
    uint64_t timestampMs = 0;
    uint32_t threadId = 0;
    int sceneDepthAfter = 0; // BeginScene: depth after increment; EndScene: depth after decrement
    uint32_t drawCountSinceLast = 0;
    uint32_t recordCount = 0;
};

struct Decision {
    bool requestVrFrame = false;
    bool incrementGameFrameId = false;
    const char* reason = nullptr;
};

struct Counters {
    uint64_t events = 0;
    uint64_t beginScene = 0;
    uint64_t endScene = 0;
    uint64_t devicePresent = 0;
    uint64_t presentEx = 0;
    uint64_t swapChainPresent = 0;
    uint64_t reset = 0;
    uint64_t vrFrameRequests = 0;
    uint64_t vrFrameSuppressedDebounce = 0;
    uint64_t vrFrameSuppressedOpenScenes = 0;
    uint64_t vrFrameSuppressedTraceOnly = 0;
    uint64_t duplicateBoundarySameMs = 0;
    uint64_t logicalFrames = 0;
};

class BoundaryMachine {
public:
    explicit BoundaryMachine(Policy policy = Policy::PresentPrimary) { Reset(policy); }

    void Reset(Policy policy) {
        policy_ = policy;
        openBeginScenes_ = 0;
        devicePresentSeen_ = false;
        lastEndSceneVrMs_ = 0;
        lastVrFrameMs_ = 0;
        inReset_ = false;
        std::memset(&counters_, 0, sizeof(counters_));
    }

    Policy GetPolicy() const { return policy_; }
    const Counters& Stats() const { return counters_; }
    int OpenBeginScenes() const { return openBeginScenes_; }
    bool DevicePresentSeen() const { return devicePresentSeen_; }

    Decision OnEvent(const Event& e) {
        Decision d{};
        ++counters_.events;
        switch (e.type) {
            case EventType::BeginScene:
                ++counters_.beginScene;
                ++openBeginScenes_;
                return d;
            case EventType::EndScene:
                ++counters_.endScene;
                if (openBeginScenes_ > 0) --openBeginScenes_;
                else openBeginScenes_ = 0;
                return DecideEndScene(e);
            case EventType::DevicePresent:
                ++counters_.devicePresent;
                devicePresentSeen_ = true;
                return DecidePresent(e, "DevicePresent");
            case EventType::PresentEx:
                ++counters_.presentEx;
                devicePresentSeen_ = true;
                return DecidePresent(e, "PresentEx");
            case EventType::SwapChainPresent:
                ++counters_.swapChainPresent;
                devicePresentSeen_ = true;
                return DecidePresent(e, "SwapChainPresent");
            case EventType::Reset:
            case EventType::ResetEx:
                ++counters_.reset;
                inReset_ = true;
                openBeginScenes_ = 0;
                lastEndSceneVrMs_ = 0;
                return d;
            case EventType::ResetSucceeded:
                inReset_ = false;
                openBeginScenes_ = 0;
                return d;
            default:
                return d;
        }
    }

    // Legacy rc20b debounce interval (milliseconds), measured not guessed.
    static constexpr uint64_t kLegacyEndSceneDebounceMs = 9;

private:
    Decision DecidePresent(const Event& e, const char* tag) {
        Decision d{};
        if (policy_ == Policy::TraceOnly) {
            ++counters_.vrFrameSuppressedTraceOnly;
            d.reason = "trace-only";
            return d;
        }
        if (inReset_) {
            d.reason = "reset-active";
            return d;
        }
        if (policy_ == Policy::LegacyRc20b && !devicePresentSeen_) {
            // Legacy still prefers Present when it exists; this path is Present*.
        }
        d.requestVrFrame = true;
        d.incrementGameFrameId = true;
        d.reason = tag;
        NoteVrFrame(e.timestampMs);
        ++counters_.logicalFrames;
        return d;
    }

    Decision DecideEndScene(const Event& e) {
        Decision d{};
        if (policy_ != Policy::LegacyRc20b) {
            if (policy_ == Policy::TraceOnly)
                ++counters_.vrFrameSuppressedTraceOnly;
            d.reason = policy_ == Policy::PresentPrimary ? "present-primary-no-endscene" : "trace-only";
            return d;
        }
        if (inReset_) {
            d.reason = "reset-active";
            return d;
        }
        if (devicePresentSeen_) {
            d.reason = "present-already-seen";
            return d;
        }
        if (openBeginScenes_ != 0) {
            ++counters_.vrFrameSuppressedOpenScenes;
            d.reason = "nested-beginscene";
            return d;
        }
        if (e.timestampMs - lastEndSceneVrMs_ < kLegacyEndSceneDebounceMs) {
            ++counters_.vrFrameSuppressedDebounce;
            d.reason = "debounce";
            return d;
        }
        lastEndSceneVrMs_ = e.timestampMs;
        d.requestVrFrame = true;
        d.incrementGameFrameId = true;
        d.reason = "EndScene@42(fallback)";
        NoteVrFrame(e.timestampMs);
        ++counters_.logicalFrames;
        return d;
    }

    void NoteVrFrame(uint64_t ms) {
        if (lastVrFrameMs_ == ms) ++counters_.duplicateBoundarySameMs;
        lastVrFrameMs_ = ms;
        ++counters_.vrFrameRequests;
    }

    Policy policy_ = Policy::PresentPrimary;
    int openBeginScenes_ = 0;
    bool devicePresentSeen_ = false;
    bool inReset_ = false;
    uint64_t lastEndSceneVrMs_ = 0;
    uint64_t lastVrFrameMs_ = 0;
    Counters counters_{};
};

// --- Offline timing / rate helpers (used by simulator) ---

inline double MaxVrFrameHzFromLegacyDebounce() {
    return 1000.0 / static_cast<double>(BoundaryMachine::kLegacyEndSceneDebounceMs);
}

struct PaceScenario {
    double gameHz = 60.0;
    double endScenesPerGameFrame = 1.0;
    double xrHz = 90.0;
    uint64_t durationMs = 1000;
    Policy policy = Policy::LegacyRc20b;
};

struct PaceResult {
    uint64_t vrFrameRequests = 0;
    uint64_t endSceneEvents = 0;
    double vrFrameHz = 0.0;
};

inline PaceResult SimulatePace(const PaceScenario& s) {
    BoundaryMachine m(s.policy);
    PaceResult r{};
    const uint64_t stepMs = std::max<uint64_t>(1, static_cast<uint64_t>(1000.0 / std::max(s.gameHz, 1.0)));
    const uint64_t endStep =
        std::max<uint64_t>(1, static_cast<uint64_t>(stepMs / std::max(s.endScenesPerGameFrame, 1.0)));
    for (uint64_t t = 0; t < s.durationMs; t += endStep) {
        Event bs{EventType::BeginScene, t, 0, 1, 0, 0};
        m.OnEvent(bs);
        Event es{EventType::EndScene, t, 0, 0, 0, 0};
        const Decision d = m.OnEvent(es);
        ++r.endSceneEvents;
        if (d.requestVrFrame) ++r.vrFrameRequests;
    }
    r.vrFrameHz = s.durationMs ? (1000.0 * r.vrFrameRequests / s.durationMs) : 0.0;
    return r;
}

} // namespace d2vr_frame

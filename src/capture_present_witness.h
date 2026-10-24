#pragma once
// Diagnostic-only call witness. This does NOT prove player-camera ownership or
// that every engine swapchain is hooked. No Windows/COM dependency for tests.
#include <cstdint>
#include <cstring>
namespace d2vr_capture {
inline bool IsPresent(const char* reason) {
    return reason && (!std::strcmp(reason,"DevicePresent") ||
        !std::strcmp(reason,"PresentEx") || !std::strcmp(reason,"SwapChainPresent"));
}
struct PresentWitness {
    uint64_t epoch=0, serial=0, frame=0;
    uintptr_t device=0, caller=0;
    uint32_t thread=0;
    bool active=false, resetting=false;
    const char* kind=nullptr;
    bool Enter(const char* reason, uintptr_t dev, uintptr_t ret, uint32_t tid, uint64_t id) {
        if (active || resetting || !IsPresent(reason) || !dev || !tid) return false;
        ++serial; frame=id; device=dev; caller=ret; thread=tid; kind=reason; active=true;
        return true;
    }
    bool Matches(uint64_t seq, uint64_t gen, uint64_t id, uint32_t tid) const {
        return active && !resetting && seq==serial && gen==epoch && id==frame && tid==thread;
    }
    // VrFrame latches the NEXT XR frame at the tail of the previous Present.
    // Its live eye draws BETWEEN calls; replay/submission close at the next one.
    bool InCaptureWindow(uint64_t seq, uint64_t gen, uint64_t id,
                         uint32_t tid, uint64_t currentFrame) const {
        if (resetting || gen!=epoch || tid!=thread) return false;
        if (currentFrame==id) return Matches(seq,gen,id,tid);
        return currentFrame+1==id && serial+1==seq && frame==currentFrame;
    }
    void Leave() { active=false; }
    void Reset() { ++epoch; active=false; resetting=true; }
    void ResetResult(bool success) { resetting=!success; }
};
}

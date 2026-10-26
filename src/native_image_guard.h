#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>

namespace d2vr_native_image {
// Opt-in isolation, NOT a way to defeat native address derivation. Latch before
// any installer runs. Native hash inputs include both code and class vtables.
struct Guard {
    bool enabled = false;
    uintptr_t base = 0;
    size_t bytes = 0;
    bool Allows(uintptr_t target, size_t length) const {
        if (!enabled) return true; // unchanged production/diagnostic default
        constexpr auto maximum = (std::numeric_limits<uintptr_t>::max)();
        if (!base || !bytes || bytes > maximum-base || !target || !length || length > maximum-target)
            return false; // unknown/overflowing intervals are not permission
        return target+length <= base || target >= base+bytes;
    }
    bool ConfigAllowed(bool xrDisabled, int stereoMode, bool captureRequested,
                       bool imageObserverRequested=false) const {
        // Configuration admission is PENDING until the external implementation
        // observer has actually acquired the native swapchain and forwarded a
        // real successful Present. The caller enforces that second runtime gate.
        if(imageObserverRequested) return enabled && !xrDisabled && stereoMode==1;
        return !enabled || (xrDisabled && stereoMode == 0 && !captureRequested);
    }
};
}

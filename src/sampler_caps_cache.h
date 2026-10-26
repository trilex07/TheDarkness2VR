#pragma once
#include <d3d9.h>
#include <algorithm>
#include <atomic>
#include <cstdint>

namespace d2vr_sampling {
// Device caps are immutable within a device lifetime. Never retain a COM
// reference; HookDevice/Reset/ResetEx invalidate even a reused pointer.
inline std::atomic<uint64_t> g_samplerCapsGeneration{1};
inline void InvalidateSamplerCaps() {
    g_samplerCapsGeneration.fetch_add(1, std::memory_order_release);
}

class SamplerCapsCache {
    IDirect3DDevice9* device_ = nullptr;
    uint64_t generation_ = 0;
    DWORD maxAnisotropy_ = 0;
public:
    template<class Query>
    DWORD Resolve(IDirect3DDevice9* dev, DWORD value, DWORD requested, Query query) {
        const uint64_t generation = g_samplerCapsGeneration.load(std::memory_order_acquire);
        DWORD maximum = maxAnisotropy_;
        if (dev != device_ || generation != generation_ || !maximum) {
            device_ = nullptr; generation_ = 0; maxAnisotropy_ = 0;
            D3DCAPS9 caps{};
            if (!dev || FAILED(query(dev, &caps)) || caps.MaxAnisotropy < 1) return value;
            maximum = caps.MaxAnisotropy;
            // An invalidation during the driver call must not publish stale caps.
            if (generation == g_samplerCapsGeneration.load(std::memory_order_acquire)) {
                device_ = dev; generation_ = generation; maxAnisotropy_ = maximum;
            }
        }
        return std::min(maximum, std::max(value, requested));
    }
    DWORD Resolve(IDirect3DDevice9* dev, DWORD value, DWORD requested) {
        return Resolve(dev, value, requested,
            [](IDirect3DDevice9* d, D3DCAPS9* caps) { return d->GetDeviceCaps(caps); });
    }
};
// No shared mutable device entry or driver calls from a different thread.
inline thread_local SamplerCapsCache g_samplerCapsCache;
} // namespace d2vr_sampling

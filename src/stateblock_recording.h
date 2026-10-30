#pragma once
#include <atomic>
namespace d2vr_stateblock {
// Begin/EndStateBlock builds an object; its setters do not change live state.
// The proxy owns one hooked device. Keep its exact identity so a different
// device cannot suppress this device's recording or metadata publication.
inline std::atomic<const void*> recordingDevice{nullptr};
inline bool Active(const void* device) {
    return device && recordingDevice.load(std::memory_order_acquire)==device;
}
inline void Began(const void* device,long result) {
    if(result>=0)recordingDevice.store(device,std::memory_order_release);
}
inline void Ended(const void* device,long result) {
    if(result>=0) {
        const void* expected=device;
        recordingDevice.compare_exchange_strong(expected,nullptr,std::memory_order_acq_rel);
    }
}
}

#pragma once
#include <atomic>
#include <cstdint>
namespace d2vr::psvr2 {
// Native callbacks publish numbers only. OpenXR/IPC work stays on input sync.
inline std::atomic<std::uint32_t> shotSerial[2]{};
inline void noteShot(unsigned hand){if(hand<2)shotSerial[hand].fetch_add(1,std::memory_order_release);}
}

#pragma once
#include <cstdint>

namespace d2vr_dialogue {
// SHA-pinned 9F2770 pushes ESI, then the angle argument before calling
// camera+8C at 9F278C. At that setter's entry, the avatar method's return
// address is therefore exactly stack[3]. No stack walking or heuristic scan.
inline bool ConversationCaller(uintptr_t direct,uintptr_t outer) {
    return direct==0x9f278e && (outer==0xaf7a2b || outer==0xaf7afb);
}
}

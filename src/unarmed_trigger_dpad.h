#pragma once
#include <cstdint>

namespace d2vr_input {
// VR controllers have no D-pad. The stock pad binds:
//   D-pad down  -> draw / take a weapon into the main (right) hand
//   D-pad right -> dual wield (weapon into the off (left) hand)
// When the native inventory confirms a hand slot is EMPTY, that hand's
// trigger is translated into the matching D-pad pulse:
//   left trigger  + empty off-hand slot  -> DPAD_RIGHT
//   right trigger + empty main-hand slot -> DPAD_DOWN
// Unknown native state never translates; the trigger passes through unchanged.
enum class HandSlot : uint8_t { Unknown=0, Empty=1, Armed=2 };

struct UnarmedTriggerDpad {
    static constexpr uint8_t kPress=128, kRelease=40;
    static constexpr uint64_t kPulseMs=120;
    static constexpr uint16_t kDpadDown=0x0002, kDpadRight=0x0008; // XINPUT_GAMEPAD_DPAD_*
    bool down[2]{}, consumed[2]{};
    uint64_t pulseUntil[2]{};

    void Reset() { *this=UnarmedTriggerDpad{}; }

    // [0] = left trigger / off-hand slot, [1] = right trigger / main-hand slot.
    // Triggers are rewritten in place (zeroed while translated). Returns D-pad
    // bits to OR into wButtons; firedMask bit h is set on a new translation.
    uint16_t Update(uint8_t& leftTrigger,uint8_t& rightTrigger,HandSlot offHand,HandSlot mainHand,
                    bool context,uint64_t nowMs,int* firedMask=nullptr) {
        uint16_t buttons=0; int fired=0;
        for(int h=0;h<2;++h) {
            uint8_t& trigger=h ? rightTrigger:leftTrigger;
            const HandSlot slot=h ? mainHand:offHand;
            const bool wasDown=down[h];
            down[h]=wasDown ? trigger>kRelease : trigger>=kPress;
            const bool empty=context && slot==HandSlot::Empty;
            if(!context) pulseUntil[h]=0;
            if(empty && down[h] && !wasDown) {
                consumed[h]=true; pulseUntil[h]=nowMs+kPulseMs; fired|=1<<h;
            }
            if(!down[h] && trigger<=kRelease && !empty) consumed[h]=false;
            // An empty hand has no trigger use; a translated press stays hidden
            // until release even after the weapon appears, so it never fires.
            if(empty || consumed[h]) trigger=0;
            if(nowMs<pulseUntil[h]) buttons|=h ? kDpadDown:kDpadRight;
        }
        if(firedMask) *firedMask=fired;
        return buttons;
    }
};
}

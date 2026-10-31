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
// Double tap of the X button (PSVR2 left Square) -> stock D-pad weapon action.
// Hold X stays untouched (Darkness II: devour hearts); the first tap reaches the
// game (reload). The SECOND press within kWindowMs is hidden from the game until
// release and replaced by a D-pad pulse: empty main hand -> DPAD_DOWN (draw),
// otherwise (armed or unknown) -> DPAD_RIGHT (dual wield).
struct DoubleTapDpad {
    static constexpr uint64_t kWindowMs=350, kPulseMs=150;
    static constexpr uint16_t kX=0x4000, kDpadDown=0x0002, kDpadRight=0x0008;
    bool down=false, swallow=false;
    uint64_t lastPress=0, pulseUntil=0;
    uint16_t pulse=0;
    void Reset() { *this=DoubleTapDpad{}; }
    // Rewrites buttons in place. firedBits receives the D-pad bit on a new action.
    void Update(uint16_t& buttons,HandSlot mainHand,bool context,uint64_t nowMs,uint16_t* firedBits=nullptr) {
        if(firedBits) *firedBits=0;
        if(!context) { Reset(); return; }
        const bool x=(buttons&kX)!=0;
        if(x && !down) {
            if(lastPress && nowMs-lastPress<=kWindowMs) {
                swallow=true; lastPress=0;
                pulse=mainHand==HandSlot::Empty ? kDpadDown:kDpadRight;
                pulseUntil=nowMs+kPulseMs;
                if(firedBits) *firedBits=pulse;
            } else lastPress=nowMs ? nowMs:1;
        }
        down=x;
        if(!x) swallow=false;
        if(swallow) buttons&=uint16_t(~kX);
        if(nowMs<pulseUntil) buttons|=pulse;
    }
};
}

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
// Double tap of the X button (PSVR2 left Square) -> a configurable pad button.
// Hold X stays untouched (Darkness II: devour hearts); the first tap reaches the
// game (reload). The SECOND press within kWindowMs is hidden from the game until
// release and replaced by a pulse of `target` (resolved by the caller at the press
// edge). target==0 disables the action (X passes through unchanged).
struct DoubleTapDpad {
    static constexpr uint64_t kWindowMs=350, kPulseMs=150;
    static constexpr uint16_t kX=0x4000, kDpadDown=0x0002, kDpadRight=0x0008;
    bool down=false, swallow=false;
    uint64_t lastPress=0, pulseUntil=0;
    uint16_t pulse=0;
    void Reset() { *this=DoubleTapDpad{}; }
    void Update(uint16_t& buttons,uint16_t target,bool context,uint64_t nowMs,uint16_t* firedBits=nullptr) {
        if(firedBits) *firedBits=0;
        if(!context) { Reset(); return; }
        const bool x=(buttons&kX)!=0;
        if(x && !down) {
            if(target && lastPress && nowMs-lastPress<=kWindowMs) {
                swallow=true; lastPress=0; pulse=target; pulseUntil=nowMs+kPulseMs;
                if(firedBits) *firedBits=pulse;
            } else lastPress=nowMs ? nowMs:1;
        }
        down=x;
        if(!x) swallow=false;
        if(swallow) buttons&=uint16_t(~kX);
        if(nowMs<pulseUntil) buttons|=pulse;
    }
};

// Name -> XInput button bits for the [Input] DoubleTapX setting.
inline bool ParsePadButton(const char* v,uint16_t& bits,bool& autoByHand) {
    struct E { const char* n; uint16_t b; };
    static const E table[]={{"DPAD_UP",0x0001},{"DPAD_DOWN",0x0002},{"DPAD_LEFT",0x0004},{"DPAD_RIGHT",0x0008},
        {"START",0x0010},{"BACK",0x0020},{"L3",0x0040},{"R3",0x0080},{"LB",0x0100},{"RB",0x0200},
        {"A",0x1000},{"B",0x2000},{"X",0x4000},{"Y",0x8000},{"OFF",0}};
    autoByHand=false;
    auto eq=[](const char* a,const char* b){ for(;*a&&*b;++a,++b){ char x=*a,y=*b; if(x>='a'&&x<='z')x-=32; if(y>='a'&&y<='z')y-=32; if(x!=y) return false;} return *a==*b; };
    if(eq(v,"AUTO")) { autoByHand=true; bits=0; return true; }
    for(const auto& e:table) if(eq(v,e.n)) { bits=e.b; return true; }
    return false;
}
// TOGGLE target for the X double tap. Firearm codes follow d2vr::psvr2::Firearm:
// 0 None, 1 Pistol, 2 Automatic, 3 Burst, 4 Shotgun, 5 Charged, 6 Smg, 7 Rifle.
// Stock pad: D-pad right selects the two-handed weapon, D-pad left the one-handed
// (pistols/uzi), D-pad down draws a weapon. Two-handed in the right hand -> LEFT,
// one-handed -> RIGHT, empty main hand -> DOWN, unknown kind -> alternate.
enum class WeaponClass : uint8_t { Unknown=0, OneHanded, TwoHanded };
inline WeaponClass ClassifyFirearm(uint8_t right,uint8_t left) {
    auto small=[](uint8_t k){ return k==1 || k==2 || k==6; };
    switch(right) {
    case 1: case 6: return WeaponClass::OneHanded;
    case 3: case 4: case 5: case 7: return WeaponClass::TwoHanded;
    case 2: return small(left) ? WeaponClass::OneHanded:WeaponClass::TwoHanded; // lone automatic = rifle
    default: return WeaponClass::Unknown;
    }
}
inline uint16_t ToggleTarget(WeaponClass held,HandSlot mainHand,uint16_t lastSent) {
    if(mainHand==HandSlot::Empty) return 0x0002;
    if(held==WeaponClass::TwoHanded) return 0x0004;
    if(held==WeaponClass::OneHanded) return 0x0008;
    return lastSent==0x0008 ? 0x0004:0x0008;
}
}

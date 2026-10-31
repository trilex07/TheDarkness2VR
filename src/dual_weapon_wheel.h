#pragma once
// Physical weapon wheel (hold R3), right stick flicks (up jump / down crouch).
// Pure logic: no Windows, D3D, OpenXR or engine access, host-testable.
//
// Native evidence (DarknessII.exe d46bb6cb...4ec1, see
// docs/research/weapon-wheel-native-inventory.md):
//  - inventory weapon slots: 0 = two-handed, 1 = first one-handed,
//    2 = second one-handed (globals 0x1110258 (bss, 0), 0x10CC248 = 1,
//    0x10CC24C = 2); 8 = empty hand.
//  - native hand index 0 = right/main, 1 = left/off. inventory+0x7B9 swaps
//    which one-handed slot belongs to which hand (native 0x8BE7E0, 0x6FDC2C).
//  - stock pad: DPAD_RIGHT -> two-handed, DPAD_LEFT -> alternates the main
//    hand between the one-handed weapons (headset log 2026-10-31 23:08).
// The wheel only presses stock pad buttons; the game keeps every equip rule.
#include <cmath>
#include <cstdint>

namespace d2vr_wheel {

// Canonical slots. The runtime maps the game's slot ids onto these.
constexpr int kSlotTwoHanded=0, kSlotOneA=1, kSlotOneB=2, kSlotEmpty=8, kSlotOther=9;
constexpr uint16_t kDpadUp=0x0001, kDpadDown=0x0002, kDpadLeft=0x0004, kDpadRight=0x0008;
constexpr uint16_t kL3=0x0040, kR3=0x0080;

// ---------------------------------------------------------------- R3 hold
// Press: R3 is hidden from the game. Held >= kHoldMs: wheels open (R3 never
// reaches the game). Released earlier: a kTapMs R3 pulse is forwarded, so the
// stock R3 action still works on a tap. L3 during the press (recenter chord)
// cancels and leaves both sticks to the chord logic.
struct HoldGate {
    static constexpr uint64_t kHoldMs=220, kTapMs=100;
    enum class Event { None, Open, Commit, Cancel };
    bool down=false, open=false, raw=false;
    uint64_t pressedAt=0, tapUntil=0;
    void Reset() { *this=HoldGate{}; }
    // allowed: gameplay context in which a wheel may be shown.
    // forwardR3 receives whether the game should see R3 this poll.
    Event Update(bool r3,bool l3,bool allowed,uint64_t now,bool& forwardR3) {
        Event event=Event::None;
        const bool pressed=r3 && !down, released=!r3 && down;
        down=r3;
        if(pressed) { raw=l3 || !allowed; pressedAt=now; }
        if(open && (l3 || !allowed)) { open=false; raw=r3; event=Event::Cancel; }
        else if(down && !open && !raw) {
            if(l3 || !allowed) raw=true;
            else if(now-pressedAt>=kHoldMs) { open=true; event=Event::Open; }
        }
        if(released) {
            if(open) { open=false; event=Event::Commit; }
            else if(!raw) tapUntil=now+kTapMs;
            raw=false;
        }
        forwardR3=raw ? r3 : now<tapUntil;
        return event;
    }
};

// ------------------------------------------------------ right stick Y
// Head owns pitch, so the right stick Y never reaches the game in gameplay.
// A flick up fires `up` (jump), a flick down fires `down` (crouch toggle);
// one pulse per flick, rearmed only after the stick returns near centre.
// Within 45 degrees of vertical; more sideways deflection stays a turn.
struct StickFlick {
    static constexpr float kEngage=.75f, kRearm=.30f;
    static constexpr uint64_t kPulseMs=120;
    enum class Dir : uint8_t { None, Up, Down };
    bool armed=true;
    Dir pulse=Dir::None;
    uint64_t pulseUntil=0;
    void Reset() { *this=StickFlick{}; }
    // Returns the direction fired this poll (None otherwise). active receives
    // the direction whose pulse is held this poll.
    Dir Update(float x,float y,bool allowed,uint64_t now,Dir& active) {
        Dir fired=Dir::None;
        if(!(std::isfinite(x) && std::isfinite(y))) x=y=0;
        const float ay=std::fabs(y);
        if(!allowed) { armed=ay<kRearm; pulseUntil=0; }
        else if(armed && ay>=kEngage && std::fabs(x)<=ay) {
            armed=false; fired=pulse=y>0 ? Dir::Up : Dir::Down; pulseUntil=now+kPulseMs;
        }
        if(ay<kRearm) armed=true;
        active=allowed && now<pulseUntil ? pulse : Dir::None;
        return fired;
    }
};

// ------------------------------------------------------------ hand sector
// Planar offset of the hand from the wheel centre: u right, v DOWN, metres.
// Slot 0 is at the top, slots proceed clockwise (matches
// d2vr_weapon_wheel_graphics::SlotCenter). Returns -1 inside the dead zone.
inline int SectorFromOffset(float u,float v,int count,float deadM) {
    if(count<1 || count>8 || !(std::isfinite(u) && std::isfinite(v))) return -1;
    if(u*u+v*v<deadM*deadM) return -1;
    if(count==1) return 0;
    const float pi=3.14159265358979323846f;
    float a=std::atan2(u,-v);
    if(a<0) a+=2*pi;
    int s=int(std::floor((a+pi/count)*count/(2*pi)));
    return s>=count ? 0 : s;
}

// Keeps the highlighted sector until the hand clearly enters another one or
// returns inside the dead zone. touched records any highlight during an open.
struct HandPick {
    int hover=-1; bool touched=false;
    void Reset() { hover=-1; touched=false; }
    void Update(float u,float v,int count,float deadM) {
        const int s=SectorFromOffset(u,v,count,deadM);
        hover=s;
        if(s>=0) touched=true;
    }
};

// --------------------------------------------------------------- loadout
struct Loadout {
    bool valid=false;
    bool owned[3]{};       // saved weapon present in slot 0/1/2
    int hand[2]{8,8};      // canonical slot in native hand 0 (right) / 1 (left); 8 empty, 9 other
    bool swapped=false;    // inventory+0x7B9
    int RightOneHanded() const { return swapped ? kSlotOneB : kSlotOneA; }
    int LeftOneHanded() const { return swapped ? kSlotOneA : kSlotOneB; }
};
struct Menu {
    int count=0;
    int slot[4]{8,8,8,8};  // native slot shown in each sector
    int equipped=-1;       // sector currently in that hand
};
// One wheel: every owned weapon, one-handed first, then two-handed.
// equipped = the weapon in the main (right) hand.
inline Menu WeaponMenu(const Loadout& l) {
    Menu m;
    if(!l.valid) return m;
    for(int slot:{l.RightOneHanded(),l.LeftOneHanded(),kSlotTwoHanded})
        if(l.owned[slot]) m.slot[m.count++]=slot;
    for(int i=0;i<m.count;++i) if(m.slot[i]==l.hand[0]) m.equipped=i;
    return m;
}

// Closed-loop equip of one weapon into the main hand with stock D-pad presses
// (headset log 2026-10-31 23:08: DPAD_LEFT alternates the main hand between the
// two one-handed weapons; DPAD_RIGHT selects the two-handed weapon; DPAD_DOWN
// draws from an empty hand). Press, wait for the native hand to change (or
// kSettleMs), re-evaluate; give up after kMaxPresses.
struct Equip {
    static constexpr uint64_t kPulseMs=150, kSettleMs=700;
    static constexpr int kMaxPresses=4;
    enum class Result { Idle, Pending, Done, Failed };
    int target=-1, presses=0, lastHand=-1;
    uint16_t bits=0;
    uint64_t pulseUntil=0, settleUntil=0;
    void Start(int slot) { *this=Equip{}; target=slot; }
    bool Active() const { return target>=0; }
    static uint16_t Press(int hand0,int target) {
        if(target==kSlotTwoHanded) return kDpadRight;
        if(hand0==kSlotEmpty) return kDpadDown;
        return kDpadLeft; // from the two-handed or the other one-handed weapon
    }
    // Call every poll with a fresh loadout (valid=false when unknown).
    Result Step(const Loadout& l,uint64_t now) {
        if(target<0) return Result::Idle;
        if(now<pulseUntil) return Result::Pending;
        if(!l.valid) {
            if(now>=settleUntil+kSettleMs) { target=-1; return Result::Failed; }
            return Result::Pending;
        }
        if(l.hand[0]==target) { target=-1; return Result::Done; }
        const bool changed=presses && l.hand[0]!=lastHand;
        if(presses && !changed && now<settleUntil) return Result::Pending;
        if(presses>=kMaxPresses || !l.owned[target]) { target=-1; return Result::Failed; }
        bits=Press(l.hand[0],target); lastHand=l.hand[0]; ++presses;
        pulseUntil=now+kPulseMs; settleUntil=pulseUntil+kSettleMs;
        return Result::Pending;
    }
    uint16_t Bits(uint64_t now) const { return target>=0 && now<pulseUntil ? bits : 0; }
};

// Pad button names for [Input] CrouchButton / JumpButton / AButton. 0 = OFF.
inline bool ParseCrouchButton(const char* v,uint16_t& bits) {
    struct E { const char* n; uint16_t b; };
    static const E table[]={{"R3",kR3},{"L3",kL3},{"B",0x2000},{"A",0x1000},{"X",0x4000},{"Y",0x8000},
        {"DPAD_UP",kDpadUp},{"DPAD_DOWN",kDpadDown},{"LB",0x0100},{"RB",0x0200},{"OFF",0}};
    if(!v) return false;
    for(const auto& e:table) {
        const char* a=v; const char* b=e.n; bool same=true;
        for(;*a||*b;++a,++b) {
            char x=*a,y=*b; if(x>='a'&&x<='z') x=char(x-32);
            if(x!=y) { same=false; break; }
        }
        if(same) { bits=e.b; return true; }
    }
    return false;
}

} // namespace d2vr_wheel

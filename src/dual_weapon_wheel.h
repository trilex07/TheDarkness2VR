#pragma once
// Dual physical weapon wheels (hold R3) and right-stick-down crouch toggle.
// Pure logic: no Windows, D3D, OpenXR or engine access, host-testable.
//
// Native evidence (DarknessII.exe d46bb6cb...4ec1, see
// docs/research/weapon-wheel-native-inventory.md):
//  - inventory weapon slots: 0 = two-handed, 1 = first one-handed,
//    2 = second one-handed (globals 0x1110258 (bss, 0), 0x10CC248 = 1,
//    0x10CC24C = 2); 8 = empty hand.
//  - native hand index 0 = right/main, 1 = left/off. inventory+0x7B9 swaps
//    which one-handed slot belongs to which hand (native 0x8BE7E0, 0x6FDC2C).
//  - stock pad: DPAD_RIGHT -> two-handed, DPAD_LEFT -> one-handed (dual when
//    the second one-handed weapon is owned, native 0x4458A0).
// The wheels only REQUEST a stock pad action; the game keeps every equip rule.
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

// ------------------------------------------------------- stick-down toggle
// One stock button pulse per downward right-stick flick. Rearms only after
// the stick returns above kRearm. While engaged the stick Y is hidden.
struct StickDownToggle {
    static constexpr float kEngage=-.75f, kRearm=-.30f;
    static constexpr uint64_t kPulseMs=120;
    bool engaged=false, armed=true;
    uint64_t pulseUntil=0;
    void Reset() { *this=StickDownToggle{}; }
    // Returns true when a new toggle fired this poll. pulse/suppressY outputs.
    bool Update(float x,float y,bool allowed,uint64_t now,bool& pulse,bool& suppressY) {
        bool fired=false;
        if(!(std::isfinite(x) && std::isfinite(y))) x=y=0;
        if(!allowed) { engaged=false; armed=y>kRearm; pulseUntil=0; }
        else if(!engaged && armed && y<=kEngage && std::fabs(x)<=.6f*std::fabs(y)) {
            engaged=true; armed=false; pulseUntil=now+kPulseMs; fired=true;
        }
        if(y>kRearm) { engaged=false; armed=true; }
        pulse=allowed && now<pulseUntil;
        suppressY=engaged;
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
// Right hand: its one-handed weapon and the two-handed weapon.
inline Menu RightMenu(const Loadout& l) {
    Menu m;
    if(!l.valid) return m;
    const int one=l.RightOneHanded();
    if(l.owned[one]) m.slot[m.count++]=one;
    if(l.owned[kSlotTwoHanded]) m.slot[m.count++]=kSlotTwoHanded;
    for(int i=0;i<m.count;++i) if(m.slot[i]==l.hand[0]) m.equipped=i;
    return m;
}
// Left hand: its one-handed weapon (dual wield). No stock "empty left hand"
// action is proven yet, so the left wheel offers only owned weapons.
inline Menu LeftMenu(const Loadout& l) {
    Menu m;
    if(!l.valid) return m;
    const int one=l.LeftOneHanded();
    if(l.owned[one]) m.slot[m.count++]=one;
    for(int i=0;i<m.count;++i) if(m.slot[i]==l.hand[1]) m.equipped=i;
    return m;
}

enum class Action : uint8_t { None, TwoHanded, OneHanded };
inline uint16_t PadBits(Action a) {
    return a==Action::TwoHanded ? kDpadRight : a==Action::OneHanded ? kDpadLeft : 0;
}
inline bool IsOneHanded(int slot) { return slot==kSlotOneA || slot==kSlotOneB; }
// Picks are native slots (or -1 = wheel not touched). The right (main) hand
// wins a conflict: a two-handed weapon occupies both hands.
inline Action Resolve(const Loadout& l,int rightPick,int leftPick) {
    if(!l.valid) return Action::None;
    if(rightPick==kSlotTwoHanded)
        return l.owned[kSlotTwoHanded] && l.hand[0]!=kSlotTwoHanded ? Action::TwoHanded : Action::None;
    const bool wantOne=(IsOneHanded(rightPick) && l.owned[rightPick]) ||
                       (IsOneHanded(leftPick) && l.owned[leftPick]);
    if(!wantOne) return Action::None;
    const bool rightOk=!IsOneHanded(rightPick) || l.hand[0]==rightPick ||
                       (rightPick<0 && IsOneHanded(l.hand[0]));
    const bool leftOk=!IsOneHanded(leftPick) || l.hand[1]==leftPick;
    return rightOk && leftOk ? Action::None : Action::OneHanded;
}
// Closed-loop check of a requested action against a later native sample.
inline bool Satisfied(Action a,const Loadout& l) {
    if(!l.valid) return false;
    if(a==Action::TwoHanded) return l.hand[0]==kSlotTwoHanded;
    if(a==Action::OneHanded) return IsOneHanded(l.hand[0]);
    return true;
}

// One requested action: pulse, then verify within kVerifyMs.
struct Request {
    static constexpr uint64_t kPulseMs=150, kVerifyMs=1500;
    Action action=Action::None;
    uint64_t pulseUntil=0, deadline=0;
    enum class Result { Pending, None, Done, TimedOut };
    void Start(Action a,uint64_t now) {
        action=a; pulseUntil=a==Action::None ? 0 : now+kPulseMs;
        deadline=a==Action::None ? 0 : now+kVerifyMs;
    }
    uint16_t Bits(uint64_t now) const { return now<pulseUntil ? PadBits(action) : 0; }
    Result Check(const Loadout& l,uint64_t now) {
        if(action==Action::None) return Result::None;
        if(now>=pulseUntil && Satisfied(action,l)) { action=Action::None; return Result::Done; }
        if(now>=deadline) { action=Action::None; return Result::TimedOut; }
        return Result::Pending;
    }
};

// Pad button for the crouch toggle ([Input] CrouchButton). 0 = OFF.
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

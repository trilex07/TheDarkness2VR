#include "../../src/psvr2/TriggerPolicy.h"

#include <iostream>

namespace {
int failures = 0;
void require(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "FAILED: " << name << '\n';
        ++failures;
    }
}

d2vr::psvr2::FrameGates live() {
    d2vr::psvr2::FrameGates gates{};
    gates.toolkitEnabled = true;
    gates.steamVrCompatibleRuntime = true;
    gates.openXrFocused = true;
    gates.gameplayActive = true;
    gates.inactiveReason = d2vr::psvr2::TriggerOffReason::Loading;
    return gates;
}
}

int main() {
    using namespace d2vr::psvr2;
    const auto gates = live();
    HandInput idle{};
    idle.firearm = Firearm::Pistol;
    const auto pistol = selectHand(gates, idle, TriggerHand::Right);
    require(pistol.effect == TriggerEffect::Weapon && pistol.startPosition == 4
        && pistol.endPosition == 6 && pistol.strength == 3
        && pistol.hand == TriggerHand::Right && validTriggerCommand(pistol),
        "pistol resistance");

    idle.firearm = Firearm::Smg;
    const auto smg = selectHand(gates, idle, TriggerHand::Left);
    require(smg.effect == TriggerEffect::Weapon && smg.startPosition == 3
        && smg.endPosition == 5 && smg.strength == 4
        && smg.hand == TriggerHand::Left, "smg resistance");

    idle.firearm = Firearm::Rifle;
    const auto rifle = selectHand(gates, idle, TriggerHand::Right);
    require(rifle.effect == TriggerEffect::Weapon && rifle.strength == 5, "rifle resistance");

    idle.firearm = Firearm::Shotgun;
    const auto shotgun = selectHand(gates, idle, TriggerHand::Right);
    require(shotgun.effect == TriggerEffect::Weapon && shotgun.startPosition == 2
        && shotgun.endPosition == 4 && shotgun.strength == 7, "shotgun resistance");

    idle.firearm = Firearm::Charged;
    const auto charged = selectHand(gates, idle, TriggerHand::Left);
    require(charged.effect == TriggerEffect::SlopeFeedback
        && charged.startStrength == 3 && charged.endStrength == 8, "charged slope");

    HandInput firing = idle;
    firing.firearm = Firearm::Smg;
    firing.fireDown = true;
    firing.fireHeldMilliseconds = 200;
    const auto smgFire = selectHand(gates, firing, TriggerHand::Left);
    require(smgFire.effect == TriggerEffect::Vibration && smgFire.position == 3
        && smgFire.amplitude == 7 && smgFire.frequency == 55
        && validTriggerCommand(smgFire), "smg fire vibration");

    firing.firearm = Firearm::Pistol;
    firing.fireHeldMilliseconds = 10;
    const auto pistolFire = selectHand(gates, firing, TriggerHand::Right);
    require(pistolFire.effect == TriggerEffect::Vibration, "pistol fire pulse");
    firing.fireHeldMilliseconds = 200;
    const auto pistolHeld = selectHand(gates, firing, TriggerHand::Right);
    require(pistolHeld.effect == TriggerEffect::Weapon && pistolHeld.strength == 3,
        "pistol returns to wall");

    firing.firearm = Firearm::Shotgun;
    firing.fireHeldMilliseconds = 20;
    const auto shotgunFire = selectHand(gates, firing, TriggerHand::Right);
    require(shotgunFire.effect == TriggerEffect::Feedback && shotgunFire.strength == 8,
        "shotgun fire feedback");

    firing.ammo = AmmoGate::Empty;
    firing.firearm = Firearm::Rifle;
    const auto empty = selectHand(gates, firing, TriggerHand::Right);
    require(empty.effect == TriggerEffect::Weapon, "empty blocks fire vibration");

    HandInput left{};
    left.firearm = Firearm::Pistol;
    HandInput right{};
    right.firearm = Firearm::Shotgun;
    right.fireDown = true;
    right.fireHeldMilliseconds = 10;
    const auto leftCmd = selectHand(gates, left, TriggerHand::Left);
    const auto rightCmd = selectHand(gates, right, TriggerHand::Right);
    require(leftCmd.effect == TriggerEffect::Weapon && leftCmd.hand == TriggerHand::Left
        && rightCmd.effect == TriggerEffect::Feedback && rightCmd.hand == TriggerHand::Right
        && leftCmd != rightCmd, "independent hands");

    auto closed = gates;
    closed.openXrFocused = false;
    require(selectHand(closed, left, TriggerHand::Left).offReason == TriggerOffReason::Unfocused
        && selectHand(closed, right, TriggerHand::Right).offReason == TriggerOffReason::Unfocused,
        "focus loss clears both");
    closed.openXrFocused = true;
    closed.gameplayActive = false;
    closed.inactiveReason = TriggerOffReason::Menu;
    require(selectHand(closed, left, TriggerHand::Left).offReason == TriggerOffReason::Menu,
        "menu clears");
    left.reloading = true;
    require(selectHand(gates, left, TriggerHand::Left).offReason == TriggerOffReason::Reload,
        "reload clears");

    require(resolveFirearm(Firearm::Automatic, Firearm::None) == Firearm::Rifle, "lone automatic is rifle");
    require(resolveFirearm(Firearm::Automatic, Firearm::Pistol) == Firearm::Smg, "paired automatic is smg");
    require(resolveFirearm(Firearm::Automatic, Firearm::Automatic) == Firearm::Smg, "dual automatic is smg");
    require(firearmFromBehaviorVtable(0x00F88140u) == Firearm::Pistol, "pistol vtable");
    require(firearmFromBehaviorVtable(0x00F3A8B8u) == Firearm::Shotgun, "shotgun vtable");
    require(firearmFromBehaviorVtable(0x00F7188Cu) == Firearm::Shotgun, "d2 shotgun vtable");
    require(firearmFromBehaviorVtable(0x00FB8C98u) == Firearm::Automatic, "automatic vtable");
    require(firearmFromBehaviorVtable(0x1010AE0u) == Firearm::None, "fire component is not a profile");

    WeaponLatch latch{};
    require(noteFirearmChange(latch, Firearm::Pistol), "equip releases once");
    consumeFirearmReset(latch);
    require(!noteFirearmChange(latch, Firearm::Pistol), "same weapon holds");
    require(noteFirearmChange(latch, Firearm::Shotgun), "weapon change releases");
    require(weaponChangedReset(TriggerHand::Left).offReason == TriggerOffReason::WeaponChanged,
        "weapon change reason");

    auto disabled = gates;
    disabled.toolkitEnabled = false;
    require(selectHand(disabled, left, TriggerHand::Right).offReason == TriggerOffReason::Disabled,
        "disabled");
    require(selectHand(gates, HandInput{}, TriggerHand::Left).offReason
        == TriggerOffReason::UnsupportedWeapon, "empty hand");

    HandInput strike{};
    strike.firearm = Firearm::Pistol;
    strike.fireDown = true;
    strike.tentacle = TentacleCue::Strike;
    const auto strikeCmd = selectHand(gates, strike, TriggerHand::Right);
    require(strikeCmd.effect == TriggerEffect::Feedback && strikeCmd.position == 3
        && strikeCmd.strength == 8 && strikeCmd.hand == TriggerHand::Right
        && validTriggerCommand(strikeCmd), "tentacle strike");

    HandInput grip{};
    grip.tentacle = TentacleCue::Grip;
    const auto gripCmd = selectHand(gates, grip, TriggerHand::Left);
    require(gripCmd.effect == TriggerEffect::Weapon && gripCmd.startPosition == 2
        && gripCmd.endPosition == 5 && gripCmd.strength == 6
        && validTriggerCommand(gripCmd), "tentacle grip");

    grip.tentacle = TentacleCue::Throw;
    const auto throwCmd = selectHand(gates, grip, TriggerHand::Left);
    require(throwCmd.effect == TriggerEffect::Vibration && throwCmd.position == 5
        && throwCmd.amplitude == 8 && throwCmd.frequency == 20
        && validTriggerCommand(throwCmd), "tentacle throw");

    grip.tentacle = TentacleCue::Eat;
    grip.reloading = true;
    grip.firearm = Firearm::Pistol;
    const auto eatCmd = selectHand(gates, grip, TriggerHand::Left);
    require(eatCmd.effect == TriggerEffect::Feedback && eatCmd.position == 1
        && eatCmd.strength == 8 && validTriggerCommand(eatCmd), "eat beats reload");

    grip.tentacle = TentacleCue::Impale;
    const auto impaleCmd = selectHand(gates, grip, TriggerHand::Left);
    require(impaleCmd.effect == TriggerEffect::SlopeFeedback
        && impaleCmd.startPosition == 2 && impaleCmd.endPosition == 7
        && impaleCmd.startStrength == 3 && impaleCmd.endStrength == 8
        && validTriggerCommand(impaleCmd), "impale slope");

    HandInput rightGun{};
    rightGun.firearm = Firearm::Pistol;
    require(selectHand(gates, grip, TriggerHand::Left).effect == TriggerEffect::SlopeFeedback
        && selectHand(gates, rightGun, TriggerHand::Right).effect == TriggerEffect::Weapon,
        "impale leaves the other gun");
    strike.tentacle = TentacleCue::Strike;
    auto hidden = gates;
    hidden.openXrFocused = false;
    require(selectHand(hidden, strike, TriggerHand::Right).offReason
        == TriggerOffReason::Unfocused, "focus loss clears a strike");
    return failures == 0 ? 0 : 1;
}

#pragma once

// Firearm trigger policy adapted from KHARVOX (MIT, CactusVRStudios) and the
// SingularityVR client shape (MIT, letsgosportsteam). Profiles are Darkness II
// firearms. Commands are Sense adaptive-trigger effects, not gamepad rumble.

#include <array>
#include <cstdint>

namespace d2vr::psvr2 {

enum class TriggerEffect : std::uint8_t {
    Off = 0,
    Weapon = 1,
    Vibration = 2,
    Feedback = 3,
    SlopeFeedback = 4,
    MultiplePositionFeedback = 5,
    MultiplePositionVibration = 6,
};

enum class TriggerHand : std::uint8_t {
    Left = 0,
    Right = 1,
};

enum class TriggerOffReason : std::uint8_t {
    None = 0,
    Disabled,
    UnsupportedRuntime,
    Unfocused,
    Menu,
    Loading,
    Death,
    Cinematic,
    SessionEnd,
    UnsupportedWeapon,
    InvalidMessage,
    Reload,
    WeaponChanged,
};

// Observed from DarknessII.exe behavior vtables (image base 0x400000, same
// image that pins gun VT 0x1010E30 and fire VT 0x1010AE0 / +0x64 == 0xCFBBA0).
enum class Firearm : std::uint8_t {
    None = 0,
    Pistol,       // WeaponSemiAutomaticStateBehavior 0x00F88140
    Automatic,    // WeaponAutomaticStateBehavior 0x00FB8C98 (resolved below)
    Burst,        // WeaponBurstStateBehavior 0x00F72CAC
    Shotgun,      // WeaponShotgunStateBehavior 0x00F3A8B8, D2 0x00F7188C
    Charged,      // WeaponChargedStateBehavior 0x00F84564, D2DGun 0x00F1D95C
    Smg,          // resolved: automatic while another small arm is equipped
    Rifle,        // resolved: lone automatic, or burst
};

enum class AmmoGate : std::uint8_t {
    Unknown = 0,
    Ready,
    Empty,
    Unavailable,
};

struct TriggerCommand {
    TriggerEffect effect{TriggerEffect::Off};
    TriggerHand hand{TriggerHand::Right};
    std::uint8_t startPosition{};
    std::uint8_t endPosition{};
    std::uint8_t strength{};
    std::uint8_t position{};
    std::uint8_t amplitude{};
    std::uint8_t frequency{};
    std::uint8_t startStrength{};
    std::uint8_t endStrength{};
    std::array<std::uint8_t, 10> controlPoints{};
    TriggerOffReason offReason{TriggerOffReason::UnsupportedWeapon};
};

inline bool operator==(const TriggerCommand& left, const TriggerCommand& right) {
    return left.effect == right.effect && left.hand == right.hand
        && left.startPosition == right.startPosition
        && left.endPosition == right.endPosition
        && left.strength == right.strength
        && left.position == right.position
        && left.amplitude == right.amplitude
        && left.frequency == right.frequency
        && left.startStrength == right.startStrength
        && left.endStrength == right.endStrength
        && left.controlPoints == right.controlPoints
        && left.offReason == right.offReason;
}

inline bool operator!=(const TriggerCommand& left, const TriggerCommand& right) {
    return !(left == right);
}

struct FrameGates {
    bool toolkitEnabled{};
    bool steamVrCompatibleRuntime{};
    bool openXrFocused{};
    bool gameplayActive{};
    TriggerOffReason inactiveReason{TriggerOffReason::Loading};
};

// Demon-arm cues. They replace the firearm profile on the hand that is acting.
enum class TentacleCue : std::uint8_t {
    None = 0,
    Strike, // right-arm slash
    Grip,   // left arm closed on an object or enemy
    Throw,  // release / throw
    Eat,    // heart or X execution
    Impale, // A execution, the stab
};

struct HandInput {
    Firearm firearm{Firearm::None};
    bool fireDown{};
    std::uint64_t fireHeldMilliseconds{};
    bool reloading{};
    AmmoGate ammo{AmmoGate::Unknown};
    TentacleCue tentacle{TentacleCue::None};
};

inline const char* firearmName(Firearm firearm) {
    switch (firearm) {
    case Firearm::None: return "none";
    case Firearm::Pistol: return "pistol";
    case Firearm::Automatic: return "automatic";
    case Firearm::Burst: return "burst";
    case Firearm::Shotgun: return "shotgun";
    case Firearm::Charged: return "charged";
    case Firearm::Smg: return "smg";
    case Firearm::Rifle: return "rifle";
    }
    return "unknown";
}

inline Firearm firearmFromBehaviorVtable(std::uint32_t vtable) {
    switch (vtable) {
    case 0x00F88140u: return Firearm::Pistol;
    case 0x00FB8C98u: return Firearm::Automatic;
    case 0x00F72CACu: return Firearm::Burst;
    case 0x00F3A8B8u:
    case 0x00F7188Cu: return Firearm::Shotgun;
    case 0x00F84564u:
    case 0x00F1D95Cu: return Firearm::Charged;
    default: return Firearm::None;
    }
}

// A lone automatic gun is the two-handed rifle. An automatic gun beside
// another equipped firearm is an SMG (dual small arms, including pistol+uzi).
inline Firearm resolveFirearm(Firearm self, Firearm other) {
    if (self == Firearm::Automatic)
        return other == Firearm::None ? Firearm::Rifle : Firearm::Smg;
    if (self == Firearm::Burst) return Firearm::Rifle;
    return self;
}

inline TriggerCommand offCommand(TriggerHand hand, TriggerOffReason reason) {
    TriggerCommand command{};
    command.hand = hand;
    command.offReason = reason;
    return command;
}

inline TriggerCommand weaponCommand(
    TriggerHand hand, std::uint8_t startPosition,
    std::uint8_t endPosition, std::uint8_t strength) {
    TriggerCommand command{};
    command.effect = TriggerEffect::Weapon;
    command.hand = hand;
    command.startPosition = startPosition;
    command.endPosition = endPosition;
    command.strength = strength;
    command.offReason = TriggerOffReason::None;
    return command;
}

inline TriggerCommand vibrationCommand(
    TriggerHand hand, std::uint8_t position,
    std::uint8_t amplitude, std::uint8_t frequency) {
    TriggerCommand command{};
    command.effect = TriggerEffect::Vibration;
    command.hand = hand;
    command.position = position;
    command.amplitude = amplitude;
    command.frequency = frequency;
    command.offReason = TriggerOffReason::None;
    return command;
}

inline TriggerCommand feedbackCommand(
    TriggerHand hand, std::uint8_t position, std::uint8_t strength) {
    TriggerCommand command{};
    command.effect = TriggerEffect::Feedback;
    command.hand = hand;
    command.position = position;
    command.strength = strength;
    command.offReason = TriggerOffReason::None;
    return command;
}

inline TriggerCommand slopeFeedbackCommand(
    TriggerHand hand, std::uint8_t startPosition, std::uint8_t endPosition,
    std::uint8_t startStrength, std::uint8_t endStrength) {
    TriggerCommand command{};
    command.effect = TriggerEffect::SlopeFeedback;
    command.hand = hand;
    command.startPosition = startPosition;
    command.endPosition = endPosition;
    command.startStrength = startStrength;
    command.endStrength = endStrength;
    command.offReason = TriggerOffReason::None;
    return command;
}

inline const char* triggerHandName(TriggerHand hand) {
    return hand == TriggerHand::Left ? "left" : "right";
}

inline const char* triggerOffReasonName(TriggerOffReason reason) {
    switch (reason) {
    case TriggerOffReason::None: return "none";
    case TriggerOffReason::Disabled: return "disabled";
    case TriggerOffReason::UnsupportedRuntime: return "unsupported-runtime";
    case TriggerOffReason::Unfocused: return "focus-lost";
    case TriggerOffReason::Menu: return "menu";
    case TriggerOffReason::Loading: return "loading";
    case TriggerOffReason::Death: return "death";
    case TriggerOffReason::Cinematic: return "cinematic";
    case TriggerOffReason::SessionEnd: return "session-end";
    case TriggerOffReason::UnsupportedWeapon: return "unsupported-weapon";
    case TriggerOffReason::InvalidMessage: return "invalid-message";
    case TriggerOffReason::Reload: return "reload";
    case TriggerOffReason::WeaponChanged: return "weapon-changed";
    }
    return "unknown";
}

inline bool knownTriggerOffReason(TriggerOffReason reason) {
    switch (reason) {
    case TriggerOffReason::None:
    case TriggerOffReason::Disabled:
    case TriggerOffReason::UnsupportedRuntime:
    case TriggerOffReason::Unfocused:
    case TriggerOffReason::Menu:
    case TriggerOffReason::Loading:
    case TriggerOffReason::Death:
    case TriggerOffReason::Cinematic:
    case TriggerOffReason::SessionEnd:
    case TriggerOffReason::UnsupportedWeapon:
    case TriggerOffReason::InvalidMessage:
    case TriggerOffReason::Reload:
    case TriggerOffReason::WeaponChanged:
        return true;
    }
    return false;
}

inline bool validTriggerCommand(const TriggerCommand& command) {
    const std::array<std::uint8_t, 10> emptyPoints{};
    if (command.hand != TriggerHand::Left && command.hand != TriggerHand::Right)
        return false;
    if (command.effect == TriggerEffect::Off)
        return command.startPosition == 0 && command.endPosition == 0
            && command.strength == 0 && command.position == 0
            && command.amplitude == 0 && command.frequency == 0
            && command.startStrength == 0 && command.endStrength == 0
            && command.controlPoints == emptyPoints
            && knownTriggerOffReason(command.offReason)
            && command.offReason != TriggerOffReason::None;
    if (command.offReason != TriggerOffReason::None) return false;
    if (command.effect == TriggerEffect::Weapon)
        return command.position == 0 && command.amplitude == 0
            && command.frequency == 0 && command.startStrength == 0
            && command.endStrength == 0 && command.controlPoints == emptyPoints
            && command.startPosition >= 2 && command.startPosition <= 7
            && command.endPosition > command.startPosition
            && command.endPosition <= 8
            && command.strength >= 1 && command.strength <= 8;
    if (command.effect == TriggerEffect::Vibration)
        return command.startPosition == 0 && command.endPosition == 0
            && command.strength == 0 && command.startStrength == 0
            && command.endStrength == 0 && command.controlPoints == emptyPoints
            && command.position <= 9 && command.amplitude >= 1
            && command.amplitude <= 8 && command.frequency >= 1;
    if (command.effect == TriggerEffect::Feedback)
        return command.startPosition == 0 && command.endPosition == 0
            && command.amplitude == 0 && command.frequency == 0
            && command.startStrength == 0 && command.endStrength == 0
            && command.controlPoints == emptyPoints
            && command.position <= 9 && command.strength >= 1
            && command.strength <= 8;
    if (command.effect == TriggerEffect::SlopeFeedback)
        return command.strength == 0 && command.position == 0
            && command.amplitude == 0 && command.frequency == 0
            && command.controlPoints == emptyPoints
            && command.startPosition < command.endPosition
            && command.endPosition <= 9
            && command.startStrength >= 1 && command.startStrength <= 8
            && command.endStrength >= 1 && command.endStrength <= 8;
    return false;
}

inline TriggerCommand weaponChangedReset(TriggerHand hand) {
    return offCommand(hand, TriggerOffReason::WeaponChanged);
}

// One released frame when the resolved firearm changes, then the new profile.
struct WeaponLatch {
    Firearm applied{Firearm::None};
    bool resetPending{};
};

inline bool noteFirearmChange(WeaponLatch& latch, Firearm now) {
    if (now == latch.applied) return latch.resetPending;
    latch.applied = now;
    latch.resetPending = now != Firearm::None;
    return latch.resetPending;
}

inline void consumeFirearmReset(WeaponLatch& latch) {
    latch.resetPending = false;
}

inline const char* tentacleCueName(TentacleCue cue) {
    switch (cue) {
    case TentacleCue::None: return "none";
    case TentacleCue::Strike: return "strike";
    case TentacleCue::Grip: return "grip";
    case TentacleCue::Throw: return "throw";
    case TentacleCue::Eat: return "eat";
    case TentacleCue::Impale: return "impale";
    }
    return "unknown";
}

inline TriggerCommand tentacleProfile(TriggerHand hand, TentacleCue cue) {
    switch (cue) {
    case TentacleCue::Strike:
        return feedbackCommand(hand, 3, 8);
    case TentacleCue::Grip:
        return weaponCommand(hand, 2, 5, 6);
    case TentacleCue::Throw:
        return vibrationCommand(hand, 5, 8, 20);
    case TentacleCue::Eat:
        return feedbackCommand(hand, 1, 8);
    case TentacleCue::Impale:
        return slopeFeedbackCommand(hand, 2, 7, 3, 8);
    case TentacleCue::None:
        break;
    }
    return offCommand(hand, TriggerOffReason::UnsupportedWeapon);
}

inline TriggerCommand idleProfile(TriggerHand hand, Firearm firearm) {
    switch (firearm) {
    case Firearm::Pistol:
        return weaponCommand(hand, 4, 6, 3);
    case Firearm::Smg:
        return weaponCommand(hand, 3, 5, 4);
    case Firearm::Rifle:
    case Firearm::Burst:
    case Firearm::Automatic:
        return weaponCommand(hand, 3, 5, 5);
    case Firearm::Shotgun:
        return weaponCommand(hand, 2, 4, 7);
    case Firearm::Charged:
        return slopeFeedbackCommand(hand, 2, 8, 3, 8);
    case Firearm::None:
        break;
    }
    return offCommand(hand, TriggerOffReason::UnsupportedWeapon);
}

inline TriggerCommand fireProfile(
    TriggerHand hand, Firearm firearm, std::uint64_t heldMilliseconds) {
    switch (firearm) {
    case Firearm::Pistol:
        if (heldMilliseconds < 80)
            return vibrationCommand(hand, 5, 5, 16);
        break;
    case Firearm::Smg:
        return vibrationCommand(hand, 3, 7, 55);
    case Firearm::Rifle:
    case Firearm::Automatic:
        return vibrationCommand(hand, 3, 6, 32);
    case Firearm::Burst:
        if (heldMilliseconds < 160)
            return vibrationCommand(hand, 3, 6, 28);
        break;
    case Firearm::Shotgun:
        if (heldMilliseconds < 110)
            return feedbackCommand(hand, 2, 8);
        break;
    case Firearm::Charged:
        if (heldMilliseconds < 180)
            return vibrationCommand(hand, 4, 8, 22);
        break;
    case Firearm::None:
        break;
    }
    return idleProfile(hand, firearm);
}

inline TriggerCommand selectHand(
    const FrameGates& gates, const HandInput& hand, TriggerHand which) {
    if (!gates.toolkitEnabled)
        return offCommand(which, TriggerOffReason::Disabled);
    if (!gates.steamVrCompatibleRuntime)
        return offCommand(which, TriggerOffReason::UnsupportedRuntime);
    if (!gates.openXrFocused)
        return offCommand(which, TriggerOffReason::Unfocused);
    if (!gates.gameplayActive)
        return offCommand(which, gates.inactiveReason);
    if (hand.tentacle != TentacleCue::None)
        return tentacleProfile(which, hand.tentacle);
    if (hand.reloading)
        return offCommand(which, TriggerOffReason::Reload);

    const bool ammoBlocksFire =
        hand.ammo == AmmoGate::Empty || hand.ammo == AmmoGate::Unavailable;
    if (hand.fireDown && !ammoBlocksFire)
        return fireProfile(which, hand.firearm, hand.fireHeldMilliseconds);
    return idleProfile(which, hand.firearm);
}

} // namespace d2vr::psvr2

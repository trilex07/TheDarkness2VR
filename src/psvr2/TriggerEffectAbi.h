#pragma once

// POD layout for psvr2_toolkit_set_trigger_effect.
// Matches the PSVR2 Toolkit C API (MIT, Bnuuy Solutions). Sony's confidential
// pad header is not included. See THIRD-PARTY-PSVR2.md.

#include <cstdint>

namespace d2vr::psvr2 {

enum class Controller : std::uint8_t {
    Left = 0,
    Right = 1,
};

enum class EffectMode : std::int32_t {
    Off = 0,
    Feedback = 1,
    Weapon = 2,
    Vibration = 3,
    MultiplePositionFeedback = 4,
    SlopeFeedback = 5,
    MultiplePositionVibration = 6,
};

struct FeedbackParam {
    std::uint8_t position;
    std::uint8_t strength;
    std::uint8_t pad[46];
};

struct WeaponParam {
    std::uint8_t startPosition;
    std::uint8_t endPosition;
    std::uint8_t strength;
    std::uint8_t pad[45];
};

struct VibrationParam {
    std::uint8_t position;
    std::uint8_t amplitude;
    std::uint8_t frequency;
    std::uint8_t pad[45];
};

struct MultipleFeedbackParam {
    std::uint8_t strength[10];
    std::uint8_t pad[38];
};

struct SlopeParam {
    std::uint8_t startPosition;
    std::uint8_t endPosition;
    std::uint8_t startStrength;
    std::uint8_t endStrength;
    std::uint8_t pad[44];
};

struct MultipleVibrationParam {
    std::uint8_t frequency;
    std::uint8_t amplitude[10];
    std::uint8_t pad[37];
};

#pragma pack(push, 4)
struct EffectCommand {
    EffectMode mode;
    std::uint8_t padding[4];
    union {
        FeedbackParam feedback;
        WeaponParam weapon;
        VibrationParam vibration;
        MultipleFeedbackParam multipleFeedback;
        SlopeParam slope;
        MultipleVibrationParam multipleVibration;
        std::uint8_t raw[48];
    };
};
#pragma pack(pop)

static_assert(sizeof(FeedbackParam) == 48, "feedback param");
static_assert(sizeof(WeaponParam) == 48, "weapon param");
static_assert(sizeof(VibrationParam) == 48, "vibration param");
static_assert(sizeof(SlopeParam) == 48, "slope param");
static_assert(sizeof(MultipleFeedbackParam) == 48, "multi feedback param");
static_assert(sizeof(MultipleVibrationParam) == 48, "multi vibration param");
static_assert(sizeof(EffectCommand) == 56, "PSVR2 Toolkit trigger command size");

} // namespace d2vr::psvr2

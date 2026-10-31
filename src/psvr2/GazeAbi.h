#pragma once
// ABI-compatible copy of the PSVR2 Toolkit gaze status (common/hmd2_gaze.h,
// PSVR2Toolkit, Copyright (c) 2026 Bnuuy Solutions, MIT; see THIRD-PARTY-PSVR2.md).
// Only the layout is reproduced; values come from psvr2_toolkit_gaze_status.
#include <cstdint>

namespace d2vr::psvr2::gaze_abi {

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };

struct WearableEye {
    uint32_t originValid; Vec3 originMm;
    uint32_t dirValid; Vec3 dir;
    uint32_t pupilDiaValid; float pupilDiaMm;
    uint32_t pupilPosValid; Vec2 pupilPos;
    uint32_t posGuideValid; Vec2 posGuide;
    uint32_t blinkValid; uint32_t blink;
};
struct Wearable {
    int64_t timestamp;
    uint32_t frameCounter;
    WearableEye left, right;
    uint32_t originCombinedValid; Vec3 originCombinedMm;
    uint32_t dirCombinedValid; Vec3 dirCombined;
    uint32_t convergenceValid;
};
struct Foveated {
    int64_t timestamp;
    uint32_t frameCounter;
    uint32_t trackingState;
    Vec3 dirLeft, dirRight, dirCombined;
    float convergenceMm;
};
struct Status {
    uint8_t magic[2]; uint16_t version; uint32_t size;
    float expL, expR;
    uint32_t ledStatus, expCounterL, expCounterR, ledCounter;
    int32_t dspReturnCode;
    Vec3 lensLeft, lensRight;
    uint32_t userCalibrationId;
    Vec3 frGazeOrigin;
    uint8_t enabledEye, motorSequence, motorStrength;
    Wearable wearable;
    Foveated foveated;
};
static_assert(sizeof(WearableEye)==72, "WearableEye layout");
static_assert(sizeof(Wearable)==192, "Wearable layout");
static_assert(sizeof(Status)==0x148, "hmd2_gaze_status_t is 0x148 bytes");

} // namespace d2vr::psvr2::gaze_abi

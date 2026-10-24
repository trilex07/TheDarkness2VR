#include "../../src/stereo_geometry.h"

#include <cmath>
#include <cstdio>

namespace {

constexpr float kProjectionX = 1.768088f; // Captured inverse-projection coefficient.
constexpr float kProjectionY = 1.732051f;
constexpr float kRecommendedEyeWidth = 2676.0f; // Historical OpenXR runtime log.
constexpr float kConfiguredIpd = 0.065f;         // Game units, not metres.
int g_failures = 0;

void Check(bool ok, const char* label) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", label);
        ++g_failures;
    }
}

void MakePerspective(float px, float py, float out[16]) {
    for (int i = 0; i < 16; ++i) out[i] = 0.0f;
    out[0] = px;
    out[5] = py;
    out[10] = 1.0001f;
    out[11] = 1.0f;  // clip.w = view.z for a centered point.
    out[14] = -0.10001f;
}

float PixelX(const float projection[16], float z, float outputCrop = 1.0f) {
    float clipX = 0.0f, clipW = 0.0f;
    if (!d2vr_stereo::ProjectClipX(projection, 0.0f, 0.0f, z, &clipX, &clipW))
        return NAN;
    // D3D viewport mapping after the final center crop: ndc_out = ndc_source / crop.
    return 0.5f * kRecommendedEyeWidth * (1.0f + clipX / clipW / outputCrop);
}

float ParallelDisparity(float depthGameUnits, float gameIpd, int polarity,
                        float fovScale = 1.0f, float crop = 1.0f) {
    float left[16], right[16];
    MakePerspective(kProjectionX, kProjectionY, left);
    MakePerspective(kProjectionX, kProjectionY, right);
    d2vr_stereo::EyeShiftParams l, r;
    l.fovScaleX = fovScale;
    r.fovScaleX = fovScale;
    d2vr_stereo::BuildPerEyeTranslation(0, polarity, gameIpd, &l.transX);
    d2vr_stereo::BuildPerEyeTranslation(1, polarity, gameIpd, &r.transX);
    d2vr_stereo::ApplyEyeShiftToMatrix(left, 4, l, true, kProjectionX, kProjectionY);
    d2vr_stereo::ApplyEyeShiftToMatrix(right, 4, r, true, kProjectionX, kProjectionY);
    return PixelX(left, depthGameUnits, crop) - PixelX(right, depthGameUnits, crop);
}

float EstimateUnitsPerMeter(float measuredPx, float width, float effectivePx,
                            float depthGameUnits, float xrIpdMeters) {
    const float gameIpd = 2.0f * depthGameUnits * measuredPx / (width * effectivePx);
    return gameIpd / xrIpdMeters;
}

void TestParallelTranslation() {
    const float depths[] = {0.5f, 1.0f, 2.0f, 5.0f, 10.0f};
    float previous = INFINITY;
    for (float z : depths) {
        const float measured = ParallelDisparity(z, kConfiguredIpd, 0);
        const float expected = kRecommendedEyeWidth * kProjectionX * kConfiguredIpd / (2.0f * z);
        char label[96];
        std::snprintf(label, sizeof(label), "parallel mode formula at Z=%.2f game units", z);
        Check(std::fabs(measured - expected) < 0.02f, label);
        std::snprintf(label, sizeof(label), "parallel disparity falls as 1/Z at Z=%.2f", z);
        Check(measured > 0.0f && measured < previous, label);
        previous = measured;
        std::printf("translation: Z=%5.2f GU, left-right=%8.3f px\n", z, measured);
    }
    Check(std::fabs(ParallelDisparity(2.0f, kConfiguredIpd, 1) +
                    ParallelDisparity(2.0f, kConfiguredIpd, 0)) < 0.02f,
          "polarity switch reverses disparity sign");
    Check(std::fabs(ParallelDisparity(1000000.0f, kConfiguredIpd, 0)) < 0.01f,
          "parallel translation tends to zero disparity at infinity");

    const float halfFov = std::atan(1.0f / kProjectionX);
    const float cover = kProjectionX * std::tan(halfFov);
    const float edgeCrop = 0.08f;
    const float widen = cover * (1.0f + edgeCrop);
    const float fovScale = 1.0f / widen;
    const float crop = cover / widen;
    const float effectivePx = kProjectionX * fovScale / crop;
    const float observed = ParallelDisparity(3.0f, kConfiguredIpd, 0, fovScale, crop);
    const float expected = kRecommendedEyeWidth * effectivePx * kConfiguredIpd / 6.0f;
    Check(std::fabs(effectivePx - kProjectionX) < 1e-5f,
          "auto widen and edge crop preserve the submitted focal coefficient");
    Check(std::fabs(observed - expected) < 0.02f,
          "cropped output disparity uses the rendered FOV coefficient");
}

void TestShearConvergence() {
    constexpr float separation = 0.020f; // Per-eye NDC shear from StereoSep.
    constexpr float convergence = 2.0f;  // Game units from StereoConv.
    const float depths[] = {0.5f, 1.0f, 2.0f, 4.0f, 10.0f};
    for (float z : depths) {
        float left[16], right[16];
        MakePerspective(kProjectionX, kProjectionY, left);
        MakePerspective(kProjectionX, kProjectionY, right);
        d2vr_stereo::EyeShiftParams l, r;
        l.mode = r.mode = 1;
        l.separation = +separation;
        r.separation = -separation;
        l.convergence = r.convergence = convergence;
        d2vr_stereo::ApplyEyeShiftToMatrix(left, 4, l, true, kProjectionX, kProjectionY);
        d2vr_stereo::ApplyEyeShiftToMatrix(right, 4, r, true, kProjectionX, kProjectionY);
        const float measured = PixelX(left, z) - PixelX(right, z);
        const float expected = kRecommendedEyeWidth * separation * (1.0f - convergence / z);
        char label[96];
        std::snprintf(label, sizeof(label), "shear formula at Z=%.2f game units", z);
        Check(std::fabs(measured - expected) < 0.03f, label);
        std::printf("shear:       Z=%5.2f GU, left-right=%8.3f px\n", z, measured);
        if (z == convergence)
            Check(std::fabs(measured) < 0.02f, "shear has zero parallax at convergence plane");
    }
}

void TestScaleRecovery() {
    constexpr float xrIpdMeters = 0.064f;
    constexpr float knownUnitsPerMeter = 14.0f;
    constexpr float knownDistanceMeters = 2.0f;
    const float distanceGameUnits = knownDistanceMeters * knownUnitsPerMeter;
    const float gameIpd = xrIpdMeters * knownUnitsPerMeter;
    const float disparity = kRecommendedEyeWidth * kProjectionX * gameIpd /
                            (2.0f * distanceGameUnits);
    const float recovered = EstimateUnitsPerMeter(disparity, kRecommendedEyeWidth,
        kProjectionX, distanceGameUnits, xrIpdMeters);
    Check(std::fabs(recovered - knownUnitsPerMeter) < 1e-4f,
          "known 2 m distance recovers game units per metre from geometric disparity");

    const float assumedOneUnitScale = kConfiguredIpd / xrIpdMeters;
    std::printf("scale: known %.2f m at %.2f GU, XR IPD %.3f m, disparity %.3f px -> %.3f GU/m\n",
        knownDistanceMeters, distanceGameUnits, xrIpdMeters, disparity, recovered);
    std::printf("scale: configured StereoIPD=%.3f GU would only imply %.3f GU/m if it matched measured XR IPD\n",
        kConfiguredIpd, assumedOneUnitScale);
}

} // namespace

int main() {
    TestParallelTranslation();
    TestShearConvergence();
    TestScaleRecovery();
    if (g_failures) {
        std::fprintf(stderr, "FAIL: %d clarity/depth assertion(s)\n", g_failures);
        return 1;
    }
    std::puts("PASS: native stereo translation, polarity, shear convergence, and scale recovery");
    return 0;
}

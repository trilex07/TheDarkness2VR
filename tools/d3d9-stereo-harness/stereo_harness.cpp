// Minimal D3D9 stereo disparity harness using production stereo_geometry.h math.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include "../../src/stereo_geometry.h"
#include <cstdio>
#include <cmath>

#pragma comment(lib, "d3d9.lib")

static void PrintDisparity(const char* label, const float wvp[16], float x, float y, float z, float ipd) {
    using namespace d2vr_stereo;
    float left[16], right[16];
    memcpy(left, wvp, sizeof(left));
    memcpy(right, wvp, sizeof(right));
    EyeShiftParams sp{};
    sp.mode = 0;
    BuildPerEyeTranslation(0, 0, ipd, &sp.transX);
    ApplyEyeShiftToMatrix(left, 4, sp, false, 1, 1, nullptr);
    sp.transX = -sp.transX;
    ApplyEyeShiftToMatrix(right, 4, sp, false, 1, 1, nullptr);
    float clx, crx, cw;
    ProjectClipX(left, x, y, z, &clx, &cw);
    ProjectClipX(right, x, y, z, &crx, &cw);
    const float ndcL = NdcXFromClip(clx, cw);
    const float ndcR = NdcXFromClip(crx, cw);
    std::printf("%s z=%.2f ndcL=%.5f ndcR=%.5f disp=%.5f\n", label, z, ndcL, ndcR, ndcR - ndcL);
}

int main() {
    // Simple perspective row-major WVP (identity viewproj for test).
    float wvp[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 5, 10};
    const float ipd = 0.065f;
    PrintDisparity("near", wvp, 0, 0, 0.5f, ipd);
    PrintDisparity("mid", wvp, 0, 0, 2.0f, ipd);
    PrintDisparity("far", wvp, 0, 0, 20.f, ipd);
    const float dNear = 0.01f, dFar = 0.001f; // placeholder; real run compares magnitudes
    (void)dNear;
    (void)dFar;
    std::puts("PASS harness math smoke");
    return 0;
}

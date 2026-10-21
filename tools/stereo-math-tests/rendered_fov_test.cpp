#include "../../src/stereo_geometry.h"
#include <cstdio>
#include <initializer_list>

int main() {
    // Actual unmodified inverse projection, PS c27..c30, captured in
    // run_43052_frame_003_trace.txt at command 2431 (barber level).
    const float inv[16] = {
        .5655826f,0,0,0, 0,.5773503f,0,0,
        0,0,0,-10.52526f, 0,0,1,10.52632f
    };
    if (!d2vr_math::IsViewInverseProjection(inv)) return 1;
    float p[16];
    if (!d2vr_math::Invert(inv,p)) return 2;
    unsigned checks=0;
    // Check rays across the entire rendered image, including center and edges.
    // Scale/crop variations exercise the production widen and center-blit math.
    for (float scale : {1.f,.5f,.3f}) for (float crop : {1.f,.9f}) {
        float hx=0,hy=0;
        if (!d2vr_stereo::RenderedHalfFov(p[0],scale,crop,&hx) ||
            !d2vr_stereo::RenderedHalfFov(p[5],scale,crop,&hy)) return 3;
        d2vr_stereo::EyeShiftParams sp;
        sp.fovScaleX=sp.fovScaleY=scale;
        float transformed[16];
        for (int i=0;i<16;++i) transformed[i]=p[i];
        d2vr_stereo::ApplyEyeShiftToMatrix(transformed,4,sp,true,p[0],p[5]);
        for (int i=-100;i<=100;++i) {
            const float ndc=i*.01f;
            const float x=ndc*std::tan(hx), y=ndc*std::tan(hy);
            // D3D projection here has clip.w=z. Post-blit NDC divides by crop.
            const float outX=x*transformed[0]/crop;
            const float outY=y*transformed[5]/crop;
            if (std::fabs(outX-ndc)>1e-5f || std::fabs(outY-ndc)>1e-5f) return 4;
            ++checks;
        }
    }
    float h=0;
    // Actual PSVR2 FOVs logged by PID 9392, pair 3000. The old submission
    // mapped the center pixel to opposite nonzero horizontal rays. Neither
    // geometric IPD nor a convergence constant can correct that metadata error.
    const float oldLeftCenter=.5f*(std::tan(-1.073377f)+std::tan(.758284f));
    const float oldRightCenter=.5f*(std::tan(-.758284f)+std::tan(1.073377f));
    if (oldLeftCenter >= -.1f || oldRightCenter <= .1f) return 6;
    if (!d2vr_stereo::RenderedHalfFov(p[0],.3070863f,1,&h)) return 7;
    if (std::fabs(h-1.073377f)>1e-5f || std::fabs(std::tan(-h)+std::tan(h))>1e-6f) return 8;
    if (d2vr_stereo::RenderedHalfFov(0,1,1,&h) ||
        d2vr_stereo::RenderedHalfFov(1,0,1,&h) ||
        d2vr_stereo::RenderedHalfFov(1,1,0,&h)) return 5;
    std::printf("PASS: %u rays from captured production projection match submitted FOV\n",checks);
    return 0;
}

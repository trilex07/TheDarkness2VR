#include "../../src/stereo_geometry.h"
#include <cstdio>
#include <initializer_list>

int main() {
    // Real captured resolution pair. OFF preserves the old allocation exactly;
    // ON removes the 3076x3140 -> 2676x2728 downsample without changing FOV.
    using d2vr_stereo::SelectEyeOutputSize;
    using d2vr_stereo::EyeOutputSize;
    const EyeOutputSize rec{2676,2728},source{3076,3140},limits{8192,8192};
    auto same=[](EyeOutputSize a,EyeOutputSize b){return a.width==b.width && a.height==b.height;};
    if (!same(SelectEyeOutputSize(rec,source,limits,false),rec) ||
        !same(SelectEyeOutputSize(rec,source,limits,true),source)) return 20;
    for (const auto invalid : {EyeOutputSize{0,3140},{3076,0},{1280,855},
                              {3076,2600},{2600,3140},{16385,3140},{8192,8192}})
        if (!same(SelectEyeOutputSize(rec,invalid,{32768,32768},true),rec)) return 21;
    for (const auto invalid : {EyeOutputSize{0,8192},{8192,0},{3000,8192},{8192,3000}})
        if (!same(SelectEyeOutputSize(rec,source,invalid,true),rec)) return 22;
    if (!same(SelectEyeOutputSize(rec,{4096,4096},limits,true),{4096,4096})) return 23;
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
    std::puts("PASS: eye output resolution OFF/native-source/unknown/limited/invalid bounds");
    return 0;
}

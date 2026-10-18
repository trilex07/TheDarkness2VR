#pragma once
// Production stereo-warp pixel shader (ps_3_0). Shared between the runtime proxy
// (d3d9.cpp) and the offline stereo test harness (test/stereo_test.cpp) so the test
// provably executes EXACTLY the code that ships.
//
// Model: parallel-camera reprojection from a single center frame.
//   - colorTex (s0): the game's center-eye color (backbuffer).
//   - depthTex (s1): the game's depth buffer (INTZ), sampled .r = d in [0,1].
//   - params (c0): x = K (separation, UV at near), y = depthViz (0/1),
//                  z = eyeSign (-1 left / +1 right), w = maxDisp clamp (UV).
//
// Standard D3D9 depth: near ~ 0, far ~ 1, d = A - B/Z (A ~ 1). Parallel-camera
// disparity is disp = K * (1 - d): near (d~0) shifts most, far (d~1) ~0, so the
// far plane converges (zero parallax) and there is no floating. A hard clamp
// bounds any bad/garbage depth so the two eyes can never diverge into
// non-fusible doubling.
static const char* kStereoWarpPS =
    "sampler2D colorTex : register(s0);\n"
    "sampler2D depthTex : register(s1);\n"
    "float4 params : register(c0);\n"    // x=K, y=depthViz, z=eyeSign, w=maxDisp
    "float4 vsConst : register(c1);\n"   // xy = (1/width, 1/height)
    "float4 main(float4 vpos : VPOS) : COLOR0 {\n"
    "  float2 uv = vpos.xy * vsConst.xy;\n"
    "  float d = tex2D(depthTex, uv).r;\n"
    "  if (params.y > 0.5) return float4(d, d, d, 1.0);\n"
    "  float disp = params.x * (1.0 - d) * params.z;\n"
    "  disp = clamp(disp, -params.w, params.w);\n"
    "  float2 ruv = float2(uv.x + disp, uv.y);\n"
    "  return tex2D(colorTex, ruv);\n"
    "}\n";

// Pass-through vertex shader: no attribute outputs at all. The pixel shader derives
// uv from VPOS (screen-space pixel position), so NO interpolated attributes are
// required. (FVF/VS attribute interpolation was verified broken on this driver:
// stereo_test's uv probe returned garbage across the quad.)
static const char* kStereoWarpVS =
    "float4 main(float4 pos : POSITION) : POSITION {\n"
    "  return pos;\n"
    "}\n";

// Reference CPU implementation of the same math (for the test harness's
// closed-form expectation and for quick sanity checks).
static inline float StereoWarpDispCPU(float d, float K, float eyeSign, float maxDisp) {
    float disp = K * (1.0f - d) * eyeSign;
    if (disp > maxDisp) disp = maxDisp;
    if (disp < -maxDisp) disp = -maxDisp;
    return disp;
}

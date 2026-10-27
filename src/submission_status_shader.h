#pragma once
// Small procedural bitmap label in the existing final eye blit. No font API,
// texture upload, extra draw, allocation or game D3D9-state modification.
#define D2VR_SUBMISSION_STATUS_HLSL R"hlsl(
float4 SubmissionStatus(float2 uv) {
    if(statusCfg.x<0.5) return 0;
    int2 p=int2(floor((uv-float2(0.37,0.23))/float2(0.006,0.008)));
    if(p.x < -2 || p.x >= 44 || p.y < -2 || p.y >= 7) return 0;
    bool on=statusCfg.x>1.5;
    if(p.x<0 || p.x>=42 || p.y<0 || p.y>=5) return float4(0.015,0.015,0.015,1);
    uint letter=uint(p.x)/6u, x=uint(p.x)%6u;
    uint glyph=0;
    if(letter==0) glyph=33047071; // C
    if(letter==1) glyph=17332798; // P
    if(letter==2) glyph=33080881; // U
    if(letter==4) glyph=33080895; // O
    if(letter==5) glyph=on?18470705:17332767; // N / F
    if(letter==6 && !on) glyph=17332767; // F
    bool ink=x<5u && ((glyph >> (uint(p.y)*5u+4u-min(x,4u))) & 1u)!=0;
    return ink ? (on?float4(0.2,1,0.35,1):float4(1,0.55,0.1,1)) : float4(0.015,0.015,0.015,1);
}
)hlsl"

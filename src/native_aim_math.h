#pragma once
#include <cmath>
#include <cstring>

namespace d2vr_aim {
// Inputs are already in the base camera's LH coordinates (metres, +Z forward).
// C63011..C63037 reads only rows 2 and 3; preserve all other matrix elements.
inline bool BuildRayMatrix(const float base[16], const float origin[3],
                           const float direction[3], float out[16]) {
    for(int i=0;i<16;++i) if(!std::isfinite(base[i])) return false;
    float length=0, radius=0;
    for(int i=0;i<3;++i) {
        if(!std::isfinite(origin[i]) || !std::isfinite(direction[i])) return false;
        length+=direction[i]*direction[i]; radius+=origin[i]*origin[i];
    }
    if(length<.25f || length>4 || radius>9) return false;
    // Refuse corrupt/scaled engine transforms; the world scale is provisional 1:1.
    for(int r=0;r<3;++r) {
        float n=0; for(int c=0;c<3;++c) n+=base[r*4+c]*base[r*4+c];
        if(n<.9f || n>1.1f) return false;
        for(int s=0;s<r;++s) {
            float dot=0; for(int c=0;c<3;++c) dot+=base[r*4+c]*base[s*4+c];
            if(std::fabs(dot)>.05f) return false;
        }
    }
    std::memcpy(out,base,16*sizeof(float));
    for(int c=0;c<3;++c) {
        out[8+c]=0; out[12+c]=base[12+c];
        for(int r=0;r<3;++r) {
            out[8+c]+=direction[r]*base[r*4+c]/std::sqrt(length);
            out[12+c]+=origin[r]*base[r*4+c];
        }
    }
    return true;
}
inline bool BuildWeaponMatrix(const float base[16],const float local[16],float out[16]) {
    const float origin[3]={local[12],local[13],local[14]};
    const float forward[3]={local[8],local[9],local[10]};
    float checked[16];
    if(!BuildRayMatrix(base,origin,forward,checked)) return false;
    // Validate local rotation as well, then compose full roll/pitch/yaw.
    const float zero[3]={0,0,0};
    if(!BuildRayMatrix(local,zero,forward,checked)) return false;
    if(std::fabs(local[3])+std::fabs(local[7])+std::fabs(local[11])+std::fabs(local[15]-1)>.001f) return false;
    if(std::fabs(base[3])+std::fabs(base[7])+std::fabs(base[11])+std::fabs(base[15]-1)>.001f) return false;
    float result[16]{};
    for(int r=0;r<4;++r) for(int c=0;c<4;++c)
        for(int k=0;k<4;++k) result[r*4+c]+=local[r*4+k]*base[k*4+c];
    std::memcpy(out,result,sizeof(result)); return true;
}
}

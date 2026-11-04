#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include "native_view_code_clone.h"

namespace d2vr_shadow_clip {
// Private runtime capture: C4D1FB10, SHA256
// 7fb0cc4d119b3f93336344f4d44bb3907d79465371fa3ef5d67c4062734bb751.
// The shader first forms NDC xy, samples DepthBuffer, then multiplies
// xy by that depth before multiplying InverseProjection. That is not the
// homogeneous clip tuple used by the geometric-head inverse. Keep xy in NDC;
// z remains the sampled depth. All unrelated tokens, including CTAB and shadow
// sampling, stay byte-exact. c223.x selects corrected (1) or native (0) input
// at the actual draw; r14/r15 are unused by this exact native shader.
inline bool Build(const uint32_t* code,size_t words,std::vector<uint32_t>& out) {
    out.clear();
    if(!code || words!=568)return false;
    uint32_t hash=2166136261u;
    const auto* bytes=reinterpret_cast<const unsigned char*>(code);
    for(size_t i=0;i<words*4;++i)hash=(hash^bytes[i])*16777619u;
    if(hash!=0xC4D1FB10u || code[0]!=0xffff0300u || code[567]!=0x0000ffffu ||
       !d2vr_native_clone::MatchesDigest(code,words*4,
        "7fb0cc4d119b3f93336344f4d44bb3907d79465371fa3ef5d67c4062734bb751"))return false;
    constexpr uint32_t instruction[]={0x03000005u,0x80030000u,0x80e40000u,0x80000001u};
    for(size_t i=0;i<4;++i)if(code[234+i]!=instruction[i])return false;
    out.assign(code,code+234);
    // mov r14.x,c223.x; lrp r15.x,r14.x,c2.x,r1.x; mul r0.xy,r0,r15.x.
    // Native c2.x is exactly 1. With flag=0 the original depth multiplication
    // is preserved, including cutscenes without the geometric head inverse.
    out.insert(out.end(),{0x02000001u,0x8001000eu,0xa00000dfu,
                         0x04000012u,0x8001000fu,0x8000000eu,0xa0000002u,0x80000001u,
                         0x03000005u,0x80030000u,0x80e40000u,0x8000000fu});
    out.insert(out.end(),code+238,code+words);
    return true;
}
}

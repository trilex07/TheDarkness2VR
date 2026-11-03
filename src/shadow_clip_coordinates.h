#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace d2vr_shadow_clip {
// Private runtime capture: C4D1FB10, SHA256
// 7fb0cc4d119b3f93336344f4d44bb3907d79465371fa3ef5d67c4062734bb751.
// The shader first forms NDC xy, samples DepthBuffer, then multiplies
// xy by that depth before multiplying InverseProjection. That is not the
// homogeneous clip tuple used by the geometric-head inverse. Keep xy in NDC;
// z remains the sampled depth. All unrelated tokens, including CTAB and shadow
// sampling, stay byte-exact. Input-producer ownership and visual acceptance
// remain open: this candidate assumes the sampled depth is homogeneous clip z.
inline bool Build(const uint32_t* code,size_t words,std::vector<uint32_t>& out) {
    out.clear();
    if(!code || words!=568)return false;
    uint32_t hash=2166136261u;
    const auto* bytes=reinterpret_cast<const unsigned char*>(code);
    for(size_t i=0;i<words*4;++i)hash=(hash^bytes[i])*16777619u;
    if(hash!=0xC4D1FB10u || code[0]!=0xffff0300u || code[567]!=0x0000ffffu)return false;
    constexpr uint32_t instruction[]={0x03000005u,0x80030000u,0x80e40000u,0x80000001u};
    for(size_t i=0;i<4;++i)if(code[234+i]!=instruction[i])return false;
    out.assign(code,code+words);
    // mul r0.xy,r0,r1.x -> mov r0.xy,r0; nop (same token count).
    out[234]=0x02000001u;out[237]=0;
    return true;
}
}

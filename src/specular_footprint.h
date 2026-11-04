#pragma once
#include "native_view_code_clone.h"

namespace d2vr_specular_footprint {
// Selected skin PS BC938BC9, 425 words. Native unit-Z reconstruction loses
// mip normal-length variance, so this does NOT use a Toksvig length estimate.
// Instead replace only the scalar Phong pow at word347 by four normal samples
// at UV +/-ddx(UV)/4 +/-ddy(UV)/4 and average their reflected-light powers.
// Native centre samples still drive diffuse, shadows, ramp colours and sheen.
// No image blur, temporal history or mesh-edge antialiasing. Extra four normal
// texture reads, scoped to this digest-pinned material (including world users).
// Registers r24..31 and c223 are unused throughout the original bytecode.
// Generated instruction tokens: D3DAssemble ps_3_0, checked by GPU regression.
template<class Word>
bool Build(const Word* code,size_t words,std::vector<Word>& out) {
    static_assert(sizeof(Word)==4,"D3D9 token size");out.clear();
    if(!code || words!=425 || code[0]!=0xffff0300u || code[424]!=0x0000ffffu ||
       !d2vr_native_clone::MatchesDigest(code,words*4,
           "da8c8cec1a80e033748b03bf5ac5b7d68673f686af0e89b34318f288be730a27"))return false;
    const Word power[]={0x03000020u,0x80240001u,0x80550002u,0x80000002u};
    if(memcmp(code+347,power,sizeof(power)))return false;
    const Word definition[]={0x05000051u,0xa00f00dfu,0x3e800000u,0x40000000u,0xbf800000u,0x3f800000u};
    const Word prefix[]={
        0x0200005bu,0x80030018u,0x90e40001u,
        0x0200005cu,0x80030019u,0x90e40001u,
        0x03000005u,0x80030018u,0x80e40018u,0xa00000dfu,
        0x03000005u,0x80030019u,0x80e40019u,0xa00000dfu,
        0x02000024u,0x8007001au,0x90e40002u,
        0x02000024u,0x8007001bu,0x90e40004u,
    };
    const Word sample[]={ // UV offsets; decode native XY; reflect view; Phong power
        0x03000002u,0x8003001cu,0x90e40001u,0x81e40018u,
        0x03000002u,0x8003001cu,0x80e4001cu,0x81e40019u,
        0x03000042u,0x800f001du,0x80e4001cu,0xa0e40802u,
        0x03000005u,0x8001001du,0x80ff001du,0x8000001du,
        0x04000004u,0x8003001du,0x80e4001du,0xa05500dfu,0xa0aa00dfu,
        0x0400005au,0x8018001du,0x80e4001du,0x81e4001du,0xa0ff00dfu,
        0x02000007u,0x8008001du,0x80ff001du,
        0x02000006u,0x8004001du,0x80ff001du,
        0x03000008u,0x8004001cu,0x80e4001au,0x80e4001du,
        0x03000002u,0x8004001cu,0x80aa001cu,0x80aa001cu,
        0x04000004u,0x8007001cu,0x80e4001du,0x80aa001cu,0x81e4001au,
        0x03000008u,0x8011001cu,0x80e4001cu,0x80e4001bu,
        0x03000020u,0x8002001cu,0x8000001cu,0x80000002u,
    };
    out.reserve(678);
    out.insert(out.end(),code,code+189);
    out.insert(out.end(),definition,definition+sizeof(definition)/sizeof(*definition));
    out.insert(out.end(),code+189,code+347);
    out.insert(out.end(),prefix,prefix+sizeof(prefix)/sizeof(*prefix));
    for(unsigned tap=0;tap<4;++tap){
        const size_t start=out.size();
        out.insert(out.end(),sample,sample+sizeof(sample)/sizeof(*sample));
        // add r28.xy,v1,+/-r24; add r28.xy,r28,+/-r25.
        out[start+3]=(tap&1)?0x80e40018u:0x81e40018u;
        out[start+7]=(tap&2)?0x80e40019u:0x81e40019u;
        if(!tap)out.insert(out.end(),{0x02000001u,0x8008001fu,0x8055001cu});
        else out.insert(out.end(),{0x03000002u,0x8008001fu,0x80ff001fu,0x8055001cu});
    }
    out.insert(out.end(),{0x03000005u,0x80040001u,0x80ff001fu,0xa00000dfu}); // mean -> r1.z
    out.insert(out.end(),code+351,code+words);return true;
}
}

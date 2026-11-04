#pragma once
#include "native_view_code_clone.h"

namespace d2vr_outline_coverage {
// Exact native outline PS E345B150. Integrate its coverage at four positions
// within one pixel, before compositing. No colour-frame blur or temporal history.
// Original depth, normal thresholds, fog and output colour are retained.
// r0..8/c0..6 are native; r16..28/c223 are private to this replacement.
template<class Word>
bool Build(const Word* code,size_t words,std::vector<Word>& out) {
    static_assert(sizeof(Word)==4,"shader token size");
    out.clear();
    if(!code || words!=379 || code[0]!=0xffff0300u || code[378]!=0x0000ffffu ||
       !d2vr_native_clone::MatchesDigest(code,words*4,
        "6b7fc867061f6bbe0d204e8ff4231bce103106572e569ba6ce653e54e6df9cec"))return false;
    size_t body=1;
    while(body<words-1) {
        const unsigned op=code[body]&0xffffu;
        const size_t n=op==0xfffeu?((code[body]>>16)&0x7fffu):((code[body]>>24)&15u);
        if(op!=0xfffeu && op!=0x51u && op!=0x1fu)break;
        if(body+1+n>=words)return false;
        body+=1+n;
    }
    if(body>=words-1)return false;
    out.insert(out.end(),code,code+body);
    // def c223, .5, .25, 0, 0
    out.insert(out.end(),{0x05000051u,0xa00f00dfu,0x3f000000u,0x3e800000u,0u,0u});
    for(unsigned v=0;v<3;++v) {
        const Word input=0x90e40000u|v;
        const Word mask=v?0x000f0000u:0x00030000u;
        for(unsigned axis=0;axis<2;++axis) {
            const Word reg=16+v*2+axis;
            out.insert(out.end(),{Word(0x0200005bu+axis),Word(0x80000000u|mask|reg),input});
            out.insert(out.end(),{0x03000005u,Word(0x80000000u|mask|reg),Word(0x80e40000u|reg),0xa00000dfu});
        }
    }
    for(unsigned tap=0;tap<4;++tap) {
        for(unsigned v=0;v<3;++v) {
            const Word target=24+v;
            const Word mask=v?0x000f0000u:0x00030000u;
            const Word dx=((tap&1)?0x80e40000u:0x81e40000u)|(16+v*2);
            const Word dy=((tap&2)?0x80e40000u:0x81e40000u)|(17+v*2);
            out.insert(out.end(),{0x03000002u,Word(0x80000000u|mask|target),Word(0x90e40000u|v),dx});
            out.insert(out.end(),{0x03000002u,Word(0x80000000u|mask|target),Word(0x80e40000u|target),dy});
        }
        for(size_t i=body;i<words-1;) {
            const Word token=code[i];const unsigned op=token&0xffffu;
            const size_t n=(token>>24)&15u;
            if(!n || i+1+n>=words || op==0xfffeu)return false;
            out.push_back(token);
            for(size_t a=1;a<=n;++a) {
                Word arg=code[i+a];
                const unsigned type=((arg>>28)&7u)|((arg>>8)&24u),reg=arg&0x7ffu;
                if(type==1 && reg<3) arg=(arg&~Word(0x70001800u|0x7ffu))|0x00000000u|Word(24+reg);
                else if(type==8 && reg==0) arg=(arg&~Word(0x70001800u|0x7ffu))|27u;
                out.push_back(arg);
            }
            i+=1+n;
        }
        if(!tap)out.insert(out.end(),{0x02000001u,0x800f001cu,0x80e4001bu});
        else out.insert(out.end(),{0x03000002u,0x800f001cu,0x80e4001cu,0x80e4001bu});
    }
    out.insert(out.end(),{0x03000005u,0x800f0800u,0x80e4001cu,0xa05500dfu,0x0000ffffu});
    return true;
}
}

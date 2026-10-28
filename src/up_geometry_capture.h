#pragma once
#include <d3d9.h>
#include <cstdint>
#include <cstddef>

namespace d2vr_up_geometry {
constexpr uint64_t maxPayload=8ull*1024*1024;
struct Plan {size_t prefix=0,vertices=0,indices=0;unsigned indexCount=0,indexSize=0;};
inline bool Elements(D3DPRIMITIVETYPE type,unsigned primitives,bool indexed,uint64_t& count) {
    if(type<D3DPT_POINTLIST || type>D3DPT_TRIANGLEFAN || (indexed && type==D3DPT_POINTLIST))return false;
    if(!primitives){count=0;return true;}
    switch(type) {
    case D3DPT_POINTLIST:count=primitives;break;
    case D3DPT_LINELIST:count=uint64_t(primitives)*2;break;
    case D3DPT_LINESTRIP:count=uint64_t(primitives)+1;break;
    case D3DPT_TRIANGLELIST:count=uint64_t(primitives)*3;break;
    default:count=uint64_t(primitives)+2;break;
    }
    return true;
}
inline bool NonIndexed(D3DPRIMITIVETYPE type,unsigned primitives,unsigned stride,Plan& out) {
    uint64_t count=0;
    if(!stride || !Elements(type,primitives,false,count) || count>maxPayload/stride)return false;
    Plan plan;plan.vertices=size_t(count*stride);out=plan;return true;
}
inline bool Indexed(D3DPRIMITIVETYPE type,unsigned minimum,unsigned vertices,unsigned primitives,
                    D3DFORMAT format,unsigned stride,Plan& out) {
    uint64_t count=0;
    if(!stride || !Elements(type,primitives,true,count) ||
       (format!=D3DFMT_INDEX16 && format!=D3DFMT_INDEX32))return false;
    const unsigned indexSize=format==D3DFMT_INDEX16?2:4;
    const uint64_t end=uint64_t(minimum)+vertices;
    if(end>UINT32_MAX || (count && !vertices) || count>UINT32_MAX ||
       end*stride>maxPayload || count*indexSize>maxPayload)return false;
    Plan plan;
    // Preserve original indices/MinVertexIndex. The unused prefix is allocated
    // locally and zeroed, NEVER read from the caller's potentially inaccessible
    // prefix. Only [minimum, minimum+vertices) belongs to this API's input.
    if(count){plan.prefix=size_t(uint64_t(minimum)*stride);plan.vertices=size_t(uint64_t(vertices)*stride);}
    plan.indices=size_t(count*indexSize);plan.indexCount=unsigned(count);plan.indexSize=indexSize;
    out=plan;return true;
}
inline bool SourceRange(const void* source,size_t prefix,size_t bytes) {
    if(!bytes)return true;
    const auto address=reinterpret_cast<uintptr_t>(source);
    return source && prefix<=UINTPTR_MAX-address && bytes<=UINTPTR_MAX-address-prefix;
}
}

#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <intrin.h>
#include <smmintrin.h>
namespace d2vr_mapped_copy {
inline bool Supported() {
    int cpu[4]{}; __cpuid(cpu,1);
    return (cpu[2] & (1 << 19)) != 0;
}
// The caller owns the writable mapping until Unlock. Streaming loads avoid
// repeated uncached reads on WC mappings; ordinary cached mappings are legal.
// Never read outside the supplied interval, including its unaligned edges.
inline void Copy(void* destination,const void* source,size_t bytes,bool streaming) {
    if(!streaming || bytes<256){std::memcpy(destination,source,bytes);return;}
    auto dst=static_cast<unsigned char*>(destination);
    auto src=static_cast<const unsigned char*>(source);
    _mm_mfence(); // Order preceding CPU writes to the mapped WC range.
    while(bytes && (reinterpret_cast<uintptr_t>(src)&15)) {
        *dst++=*src++;--bytes;
    }
    while(bytes>=16) {
        const auto value=_mm_stream_load_si128(reinterpret_cast<__m128i*>(const_cast<unsigned char*>(src)));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst),value);
        src+=16;dst+=16;bytes-=16;
    }
    while(bytes){*dst++=*src++;--bytes;}
    _mm_mfence(); // Complete mapping reads before the native Unlock releases it.
}
}

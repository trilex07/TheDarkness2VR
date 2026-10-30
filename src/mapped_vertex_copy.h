#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#include <smmintrin.h>
#endif

namespace d2vr_ui_vertices {
inline bool StreamingLoadsSupported() {
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    static const bool supported=[] { int cpu[4]{};__cpuid(cpu,1);return (cpu[2]&(1<<19))!=0; }();
    return supported;
#else
    return false;
#endif
}

// Native Lock memory may be write-combined. CRT memcpy (notably rep movsb)
// can turn this readback into byte-sized uncached transactions. Streaming
// loads consume complete lines without changing the native Lock pointer.
// All loads stay inside the mapped range; no mapping/cache-type cache is kept.
inline void CopyMappedVertexBytes(void* destination,const void* source,size_t size,bool streaming=StreamingLoadsSupported()) {
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    if(streaming && size>=64) {
        auto dst=static_cast<uint8_t*>(destination);
        auto src=static_cast<const uint8_t*>(source);
        // Order the application's WC writes before reading, and retire all
        // streaming reads before native Unlock can recycle the mapping.
        _mm_mfence();
        while(size && (reinterpret_cast<uintptr_t>(src)&15)){*dst++=*src++;--size;}
        while(size>=64) {
            auto ptr=reinterpret_cast<__m128i*>(const_cast<uint8_t*>(src));
            const auto a=_mm_stream_load_si128(ptr);
            const auto b=_mm_stream_load_si128(ptr+1);
            const auto c=_mm_stream_load_si128(ptr+2);
            const auto d=_mm_stream_load_si128(ptr+3);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst),a);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+16),b);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+32),c);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+48),d);
            src+=64;dst+=64;size-=64;
        }
        while(size>=16) {
            const auto value=_mm_stream_load_si128(reinterpret_cast<__m128i*>(const_cast<uint8_t*>(src)));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dst),value);
            src+=16;dst+=16;size-=16;
        }
        while(size){*dst++=*src++;--size;}
        _mm_mfence();
        return;
    }
#else
    (void)streaming;
#endif
    std::memcpy(destination,source,size);
}
}

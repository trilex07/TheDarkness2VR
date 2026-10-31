#pragma once
#include <cstdint>

// Same-frame reuse of the mod's own shared-capture copies as the right-eye
// pair's color backups. Each eye capture is already an exact full-surface copy
// of RT0 into a private A8R8G8B8 slot. With split slots, slot 0 is written only
// by the left capture and slot 1 only by the right capture, so those copies can
// replace the separate live-color backup and right-history save copies that
// otherwise sit on the GPU path before the right-eye producer event.
//
// Pure policy, no D3D or Windows calls: identities are compared, never
// dereferenced. Any mismatch keeps the legacy dedicated copies.
namespace d2vr_color_reuse {
// Public D3D9 enum values (d3d9types.h): D3DFMT_A8R8G8B8=21,
// D3DFMT_X8R8G8B8=22, D3DMULTISAMPLE_NONE=0.
constexpr unsigned kA8R8G8B8=21, kX8R8G8B8=22, kMultisampleNone=0;

struct Mirror {
    const void* source=nullptr; // RT0 identity copied by the last successful capture
    uint64_t serial=0;          // capture serial of that copy; 0 = none
};

// The slot is A8R8G8B8. X8R8G8B8 <-> A8R8G8B8 StretchRect keeps all colour
// bits; X8 has no alpha to lose. Every other format may convert lossily.
inline bool LosslessSlotFormat(unsigned format,unsigned multisample) {
    return multisample==kMultisampleNone && (format==kA8R8G8B8 || format==kX8R8G8B8);
}

// True only when the slot holds an exact copy of this target, made by a
// capture issued after pairStart (the capture serial sampled at pair entry).
inline bool Mirrors(const Mirror& slot,const void* target,uint64_t pairStart,
                    unsigned targetWidth,unsigned targetHeight,unsigned targetFormat,
                    unsigned targetMultisample,unsigned slotWidth,unsigned slotHeight) {
    return target && slot.source==target && slot.serial>pairStart &&
        targetWidth && targetHeight && targetWidth==slotWidth && targetHeight==slotHeight &&
        LosslessSlotFormat(targetFormat,targetMultisample);
}

// The copy must be the SAME one validated earlier in this pair: a later
// capture into the slot would have replaced the live pixels.
inline bool Unchanged(const Mirror& slot,const void* target,uint64_t validatedSerial) {
    return validatedSerial && slot.source==target && slot.serial==validatedSerial;
}
}

#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Only the already accepted RC6/RC8d gameplay sites. In particular, none of the
// historical GetFOV probes belongs to this bundle. Validate ALL before writing.
namespace d2vr_tracked_gameplay {
struct Site { uint32_t address; const char* bytes; size_t size; };
inline constexpr Site sites[] = {
    {0xC63009,"\x8B\x82\x9C\x00\x00\x00\xFF\xD0",8},
    {0xFFC85C,"\x10\x48\x50\x00",4},
    {0x504810,"\x8D\x81\x80\x00\x00\x00\xC3",7},
    {0x879C7A,"\x52\x8D\x44\x24\x68\x50\x0F\x29\x84\x24\x8C\x00\x00\x00\xE8\xC3\x03\xE6\xFF\x83\xC4\x0C\x84\xC0",24},
    {0x100505C,"\x20\x32\xD2\x00",4},
    {0x878955,"\x8B\xF9\x8B\x47\x4C",5},
    {0x6DA050,"\x55\x8B\xEC\x83\xE4\xF0\x8B\x45\x08\x0F\x28\x08",12},
    {0xAD949A,"\x52\x8D\xBE\x40\x01\x00\x00\x57\xE8\xA9\x8A\x9A\xFF",13},
    {0xFD6CB0,"\x40\x92\xAD\x00",4},
    {0x481F50,"\x55\x8B\xEC\x83\xE4\xF0\x83\xEC\x50",9},
    {0xCFBD60,"\xE8\x0B\xB8\xF4\xFF",5},
    {0x1010B44,"\xA0\xBB\xCF\x00",4},
    {0xC47570,"\x55\x8B\xEC\x83\xE4\xF0\x83\xEC\x64",9},
};
template<class Read> bool Validate(Read read, uint32_t& failedAddress) {
    unsigned char bytes[32];
    for (const auto& site : sites) {
        if (!read(site.address,bytes,site.size) || std::memcmp(bytes,site.bytes,site.size)) {
            failedAddress=site.address; return false;
        }
    }
    failedAddress=0; return true;
}
inline bool Ready(bool requested, bool identity, bool present, bool xr,
                  int submit, int stereo, bool head, bool nativeHead,
                  bool aim, bool weapon, bool hands, bool gestures) {
    return requested && identity && present && xr && submit==4 && stereo==1 &&
        head && nativeHead && aim && weapon && hands && gestures;
}
}

#pragma once
#include <cstddef>
#include <cstdint>

namespace d2vr_screen_projection {
// Exact archived shaders: c0..c3 produce clip r0 and o0; r0*c4..c7
// produces the depth texture coordinate o4. c4 is clip->texture, so
// applying head/eye compensation to both matrices transforms it twice.
struct Entry { uint32_t hash; size_t bytes; const char* sha256; };
inline constexpr Entry kClipInput[] = {
    {0x1B497D25u,2116,"2f2c13b9c5c5dc2d1264700f3beaa8280d73ce68d37ea4b7ed8139fc777185f9"},
    {0x21F4C448u,2172,"5edf7606958778bfe1b51c6b835c607bf674ab90a26290c2e61434d5618ec5de"},
    {0x40E3244Fu,1992,"313c636549554db748abb8ff045c4c8a9cc89df51514d1909c629337da8f5906"},
    {0x67595089u,2032,"c72bc0312dbd85959aab05bb80b93833ddeb3c60a73a8080256be807244e3134"},
    {0x6A451DFFu,2044,"dddde29bab78b8c794682d930dec20c6eff55aabcf96f089e978ba7e11d331bd"},
    {0x8D19D408u,2056,"e36b8c0695a6bb2987547046fb23f23f2e79682ed4d452ec8c3a924ed33395d3"},
    {0x9E39DAF1u,1948,"bf3a521ace33c7b509785dede723728b10a23b25d65bf85cab78e4f872beaa48"},
    {0xDD1FED65u,1796,"921cdd7b688b8fc51c5ff04d9eff5601ec88d6f770c81f556f35644807a7fdf5"},
    {0xE116840Eu,664,"d6662e01e3732abcf650068db7fb7974334d12939456b03fadaddf4dd554a2d7"},
    {0xF3F1BFFCu,1904,"ffcd20bc1715b3c19836f737c0c0dc6e14b336ed40960a72e9ffe5da6f9388bc"},
};
template<class DigestMatches>
inline int WorldInputRegister(uint32_t hash, size_t bytes, int reflectedBase,
                              DigestMatches matches) {
    if(reflectedBase==4)for(const auto& entry:kClipInput)
        if(hash==entry.hash && bytes==entry.bytes && matches(entry.sha256))return -1;
    return reflectedBase;
}
}

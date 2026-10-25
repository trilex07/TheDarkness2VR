#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace d2vr_composite {
enum Patch : unsigned { None = 0, Dof = 1, HeightFog = 2 };

template<class Word>
inline uint32_t Hash(const Word* words, size_t count) {
    static_assert(sizeof(Word) == 4, "D3D9 token size");
    uint32_t hash = 2166136261u;
    const auto* bytes = reinterpret_cast<const unsigned char*>(words);
    for (size_t i = 0; i < count * sizeof(uint32_t); ++i)
        hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

// Only instruction boundaries count: CTAB/comment data must never be patched.
template<class Word>
inline size_t FindUniqueInstruction(const Word* words, size_t count,
                                   const Word (&instruction)[5]) {
    size_t match = count;
    if (!words || count < 2 || words[0] != 0xffff0300u) return count;
    for (size_t i = 1; i < count;) {
        const uint32_t op = words[i] & 0xffffu;
        if (op == 0xffffu) return i + 1 == count ? match : count;
        const size_t length = op == 0xfffeu ? (words[i] >> 16) & 0x7fffu :
                                            (words[i] >> 24) & 0xfu;
        if (length >= count - i) return count;
        if (op != 0xfffeu && length == 4 &&
            std::memcmp(words + i, instruction, sizeof(instruction)) == 0) {
            if (match != count) return count;
            match = i;
        }
        i += length + 1;
    }
    return count; // missing END
}

// lrp dest, blend, effect, scene -> mov dest, scene; nop; nop.
// Preserve the destination write mask/precision, CTAB and all token offsets.
template<class Word>
inline void BypassBlend(std::vector<Word>& words, size_t at) {
    words[at] = 0x02000001u;
    words[at + 2] = words[at + 4];
    words[at + 3] = words[at + 4] = 0;
}

template<class Word>
inline unsigned Build(const Word* words, size_t count, bool disableDof,
                      bool disableHeightFog, std::vector<Word>& out) {
    out.clear();
    if ((!disableDof && !disableHeightFog) || !words ||
        (count != 489 && count != 421) || words[0] != 0xffff0300u) return None;
    const uint32_t hash = Hash(words, count);
    if (hash != 0x33DE98C1u && hash != 0x75F88E11u) return None;
    // Fog evidence is from frame28216's 33DE98C1 only. Do not generalize to
    // other composites by a CTAB name or a visually similar effect.
    disableHeightFog = disableHeightFog && hash == 0x33DE98C1u;
    if (!disableDof && !disableHeightFog) return None;
    const Word dof[] = {0x04000012u, 0x80270003u, 0x80550000u, 0x80e40001u, 0x80e40002u};
    const Word fog[] = {0x04000012u, 0x80270001u, 0x80000000u, 0xa0e40005u, 0x80e40003u};
    const size_t dofAt = disableDof ? FindUniqueInstruction(words, count, dof) : count;
    const size_t fogAt = disableHeightFog ? FindUniqueInstruction(words, count, fog) : count;
    if ((disableDof && dofAt == count) || (disableHeightFog && fogAt == count)) return None;
    out.assign(words, words + count);
    if (disableDof) BypassBlend(out, dofAt);
    if (disableHeightFog) BypassBlend(out, fogAt);
    return (disableDof ? Dof : None) | (disableHeightFog ? HeightFog : None);
}
} // namespace d2vr_composite

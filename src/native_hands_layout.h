#pragma once
#include <cstddef>
namespace d2vr_hands {
// Both observed local-avatar skeletons share human arm/socket bones2..45.
// Villa:47 bones/3008 palette bytes; combat:130 bones/8320 bytes. Never accept
// an arbitrary larger buffer as proof that its human bone identities match.
inline bool HumanPaletteLayout(std::size_t boneCount,std::size_t matrixFloats) {
    return (boneCount==47 || boneCount==130) && matrixFloats==boneCount*16;
}
}

#pragma once
#include <cstddef>
#include <cstdint>
namespace d2vr_lit_mask {
// Native CTAB nests WVP and ScreenTextureProjection inside LitInstance.
// These exact bytecodes project the SAME (possibly skinned) point through
// c0..3 to POSITION and c8..11 to TEXCOORD2; the PS samples ShadowMask s3.
// Reflect only these proven matrices. c12 eye, c14 light and bones stay native.
struct Entry { uint32_t hash; size_t bytes; const char* sha256; };
inline constexpr Entry kWorldInput[] = {
    {0x15E56CF5u,1988,"752b789d032193ba59cce4fc947bf621f129bc04b5bb27ddf2ee1501c6a9ff4e"},
    {0x24D4B005u,1116,"fbb5f1dcf6d89173dddc4251c24d8b6311bfab8b11b8bc3340109b0e46d183b4"},
    {0x2B09A19Fu,1300,"380060d49167d0a717fb7834fd821486047e2ed2ec68c893bd0cf10e1bb6d123"},
    {0x31A8FC3Au,1828,"69e435a275cd583a6d5e73453f2025494c7435a5480447b83952f6047b805427"},
    {0x63750947u,1736,"e2b08aa3363da72d30e5990297287ffd089c90301ee5a906923661d5617ddff1"},
    {0x653B29FBu,1804,"fa76b0f14c9cfd00d7ddec2748ea2b8c375035439e0b5f8d2d59488b75822010"},
    {0x723B0759u,3144,"eebce67bba95e9ecad635d1e2a76d7bf5f07daf704cd4c9343dc3688bc959c12"},
    {0x76C80E75u,1116,"4202227fee26f769cb0c2a52d5fa6cc858aa5571c3caa636649b46e6463b7bfe"},
    {0x870D3885u,1208,"88c46e278cc1f4f3dd607ac47eef56f50a0e786af849eeb9414f7d1ace66a016"},
    {0x87C8B947u,1804,"0815e8f35fc35a579585a22ae127ec438b7e9e6d1cff98d1f383e8a88f201748"},
    {0x9A004AF9u,3052,"9189823c037c7bb68e6517925477a6ee98f7bd8a0645cb96e83ffb2e5a8a4a73"},
    {0x9D22DBC1u,1116,"e7f0038212a82719177889008d256430f60a69c72ab07046a823bfeaff8d9480"},
    {0x9DDDAA15u,3052,"3dc377983f066f9d2fa90969a9e980fbe17ee678e8f2074a8fe191d950795681"},
    {0xA7540A7Bu,1804,"9f614fbd2ba5185fb47be2ffe0332df47d5c0946ed8d6a734447e2d0b5548323"},
    {0xAB484026u,1896,"c0ca1ca1bd1c45d52f3f4c705ae428384e9eef728922faafaab266e0e4e4bca4"},
    {0xAD140429u,1920,"1cf276e6a97d08018549af46aba2db0610b98ba43e0075e3875e00b21abf858c"},
    {0xBB1CD2B2u,1208,"290472f35e144b3b51b183c38f935e96bc098b103791b035d420f5fbae45d01d"},
    {0xBBCC1217u,1736,"8d2764460cccd236d2f91ac8b7e34fb3ed747f2e47a3d043f3f1cd6aaa37ca59"},
    {0xBD281C41u,1828,"bd438d21f6175d7b2e9c23b7e89feba4598e11037e1f277192f8f4632d6ad0b8"},
    {0xBD5CF943u,1736,"43e569b3664f61d7b955d3dd17074abafcfa138d880e165efb25ad04e49cb850"},
    {0xCEDE6831u,1896,"782b3044bb03ded001c32f8a3870dfcbb1ac03bcb199f307288382caff892ec0"},
    {0xE2FB49D9u,3052,"508602f500bc64365c54df53aa365da17167c94c52d94d81254a03b53d09f061"},
    {0xEA8127EDu,1208,"0a1c75e887f3a9840548493070abc027431c9c9d9990d5361cbfda8d6fc1b1a1"},
    {0xF135DC99u,1828,"7fb4051fb15bb349dbd15b4c8b6e90c75cc78c527920bfbf052819b0dc260496"},
};
template<class DigestMatches>
inline bool Admit(uint32_t hash,size_t bytes,DigestMatches matches) {
    for(const auto& entry:kWorldInput)
        if(hash==entry.hash && bytes==entry.bytes)return matches(entry.sha256);
    return false;
}
}

#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

// Selected render-camera path only, on the two SHA-pinned Steam executables.
// These are not the historical BaseAvatar::GetFOV/FCOS call sites.
namespace d2vr_camera_cover {
constexpr uint32_t site=0x986139, cameraVtable=0xFF2800;
constexpr unsigned char original[5]={0xe8,0xe2,0x41,0x3b,0x00};
struct Pin {uint32_t address; const char* bytes; unsigned size;};
constexpr Pin pins[]={
    {0x986135,"\x8b\x0f\x8b\x09\xe8\xe2\x41\x3b\x00\xd9\x9e\x68\x01\x00\x00",15},
    {0x985E3A,"\x8b\x83\xf4\x03\x00\x00\x83\x38\x00\x8d\xbb\xf4\x03\x00\x00",15},
    {0xD3A320,"\xd9\x81\xfc\x05\x00\x00\xc3",7},
    {0x927020,"\x56\x8b\xf1\xe8\xd8\x0b\xcc\xff\xc7\x06\x00\x28\xff\x00",14},
    {0xFF28DC,"\x10\xa8\x77\x00",4},
    {0xC813B0,"\xf3\x0f\x10\x44\x24\x04\x56\x8b\xf1",9},
    {0xC813CA,"\xe8\x81\x7d\xf2\xff\x8b\xce\xe8\xea\xcc\x83\xff",12},
    {0xBA9193,"\xd9\x86\xf8\x05\x00\x00\xd9\x1c\x24\x6a\x00\xe8\x4d\x8e\xe8\xff",16},
    {0x4BE0C0,"\x55\x8b\xec\x83\xe4\xf0\x81\xec\xe4\x00\x00\x00",12},
    {0x4BE13A,"\x8d\x9e\x20\x06\x00\x00\x52\x8b\xcb\xe8\x88\x9b\x14\x00",14},
    {0x9962D0,"\x8d\x81\x20\x06\x00\x00\xc3",7},
    {0xAD2940,"\x8d\x81\xc0\x04\x00\x00\xc3",7},
    {0xB2CAA0,"\x8d\x81\x00\x05\x00\x00\xc3",7}
};
template<class Read> bool Validate(Read read,uint32_t& failed) {
    unsigned char data[32];
    for(const auto& p:pins) if(!read(p.address,data,p.size) || std::memcmp(data,p.bytes,p.size)) {
        failed=p.address;return false;
    }
    failed=0;return true;
}
#if defined(_MSC_VER) && defined(_M_IX86)
// Shared by the real hook and ABI regression test. Preserve all entry state,
// then execute the ORIGINAL getter exactly once (including its x87 return).
#define D2VR_SELECTED_CAMERA_SHIM(name, helper, originalGetter) \
static void __declspec(naked) name() { \
    __asm pushfd \
    __asm pushad \
    __asm mov eax,esp \
    __asm sub esp,528 \
    __asm and esp,0xfffffff0 \
    __asm mov [esp+512],eax \
    __asm fxsave [esp] \
    __asm fninit \
    __asm mov edx,ebx \
    __asm call helper \
    __asm fxrstor [esp] \
    __asm mov esp,[esp+512] \
    __asm popad \
    __asm popfd \
    __asm jmp dword ptr [originalGetter] \
}
#endif
struct Plan {float fov=0,px=0,py=0;};
inline bool EyeScale(float px,float py,const float angles[4],float coverage,float crop,float& x,float& y) {
    if(!std::isfinite(px) || !std::isfinite(py) || px<=0 || py<=0 ||
       !std::isfinite(coverage) || coverage<1 || coverage>1.5f ||
       !std::isfinite(crop) || crop<1 || crop>1.5f) return false;
    float half[4];
    for(int i=0;i<4;++i) {
        half[i]=std::fabs(angles[i])*coverage;
        if(!std::isfinite(half[i]) || half[i]<.1f || half[i]>1.4f) return false;
    }
    x=1/(px*std::tan(std::max(half[0],half[1]))*crop);
    y=1/(py*std::tan(std::max(half[2],half[3]))*crop);return true;
}
// Full vertical degrees, proven by BA9150 -> A31FF0 (* pi/360 -> tan).
// Retain native aspect and clip distances; conservatively include BOTH lenses.
inline bool Cover(float originalFov,float aspect,const float angles[2][4],
                  float coverage,float crop,Plan& result) {
    if(!std::isfinite(originalFov) || originalFov<10 || originalFov>160 ||
       !std::isfinite(aspect) || aspect<.4f || aspect>3.2f ||
       !std::isfinite(coverage) || coverage<1 || coverage>1.5f ||
       !std::isfinite(crop) || crop<1 || crop>1.5f) return false;
    float x=0,y=0;
    for(int e=0;e<2;++e) for(int i=0;i<4;++i) {
        const float a=std::fabs(angles[e][i])*coverage;
        if(!std::isfinite(a) || a<.1f || a>1.4f) return false;
        const float t=std::tan(a)*crop;
        (i<2?x:y)=std::max(i<2?x:y,t);
    }
    constexpr float radians=.01745329251994329577f;
    const float tangent=std::max(std::tan(originalFov*radians*.5f),std::max(y,x/aspect));
    const float fov=2*std::atan(tangent)/radians;
    if(fov>160 || !std::isfinite(fov)) return false;
    result={fov,1/(aspect*tangent),1/tangent};return true;
}
inline bool Projection(const float* p,float fov,float aspect,float nearZ,float farZ) {
    if(!p || !std::isfinite(fov) || fov<10 || fov>160 || !std::isfinite(aspect) ||
       aspect<.4f || aspect>3.2f || !std::isfinite(nearZ) || !std::isfinite(farZ) ||
       nearZ<=0 || farZ<=nearZ || farZ>1e7f) return false;
    const float y=1/std::tan(fov*.008726646259971648f),q=farZ/(farZ-nearZ);
    float expected[16]={y/aspect,0,0,0, 0,y,0,0, 0,0,q,1, 0,0,-q*nearZ,0};
    for(int i=0;i<16;++i) if(!std::isfinite(p[i]) ||
        std::fabs(p[i]-expected[i])>2e-4f*std::max(1.f,std::fabs(expected[i]))) return false;
    return true;
}
// Quiescent CreateDevice installation only. Never install from the camera hook.
struct Patch {
    unsigned char replacement[5]{};bool installed=false,rollbackUncertain=false;
    template<class Memory> bool Install(Memory& m,uint32_t target) {
        if(installed || rollbackUncertain || std::memcmp(m.bytes(),original,5)) return false;
        replacement[0]=0xe8;const uint32_t rel=target-(site+5);std::memcpy(replacement+1,&rel,4);
        if(!m.writable()) return false;
        std::memcpy(m.bytes(),replacement,5);
        if(m.flush() && m.restore()) {installed=true;return true;}
        std::memcpy(m.bytes(),original,5);const bool f=m.flush(),r=m.restore();
        rollbackUncertain=!(f&&r);return false;
    }
};
}

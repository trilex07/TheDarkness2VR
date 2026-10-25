#include "../../src/tracked_gameplay_gate.h"
#include "../../src/native_camera_cover.h"
#include "../../src/diagnostic_present_patch.h"
#include "../../src/vr_input.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <fstream>

int main(int argc,char** argv) {
    using namespace d2vr_tracked_gameplay;
    unsigned checks=0;
    const auto check=[&](bool value){++checks; if(!value) {std::fprintf(stderr,"FAIL %u\n",checks);std::exit(1);}};
    // No partial opt-in: mono, missing Present, head off and one missing input
    // dependency all leave native code untouched.
    for(unsigned mask=0;mask<1024;++mask) {
        bool f[10]; for(unsigned i=0;i<10;++i) f[i]=(mask&(1u<<i))!=0;
        check(Ready(f[0],f[1],f[2],f[3],4,1,f[4],f[5],f[6],f[7],f[8],f[9])==(mask==1023));
    }
    check(!Ready(true,true,true,true,0,1,true,true,true,true,true,true));
    check(!Ready(true,true,true,true,4,0,true,true,true,true,true,true));
    check(!Ready(true,true,true,true,4,2,true,true,true,true,true,true));
    std::vector<unsigned char> mapped(0xDEC000,0);
    for(const auto& s:sites) std::memcpy(mapped.data()+s.address-0x400000,s.bytes,s.size);
    const auto read=[&](uint32_t address,void* out,size_t size) {
        if(address<0x400000 || uint64_t(address)-0x400000+size>mapped.size()) return false;
        std::memcpy(out,mapped.data()+address-0x400000,size);return true;
    };
    uint32_t failed=0;
    check(Validate(read,failed) && failed==0);
    for(const auto& s:handPackSites) std::memcpy(mapped.data()+s.address-0x400000,s.bytes,s.size);
    check(ValidateHandPack(read,failed) && failed==0);
    for(const auto& s:handPackSites) for(size_t i=0;i<s.size;++i) {
        auto& b=mapped[s.address-0x400000+i];b^=0x80;
        check(!ValidateHandPack(read,failed) && failed==s.address);b^=0x80;
    }
    for(const auto& s:handPackSites)
        check(!ValidateHandPack([&](uint32_t a,void* p,size_t n){return a!=s.address && read(a,p,n);},failed)
              && failed==s.address);
    check(ValidateHandPack(read,failed));
    for(const auto& s:sites) for(size_t i=0;i<s.size;++i) {
        auto& b=mapped[s.address-0x400000+i];b^=0x80;
        check(!Validate(read,failed) && failed==s.address);b^=0x80;
    }
    for(const auto& s:sites) {
        check(!Validate([&](uint32_t a,void* p,size_t n){return a!=s.address && read(a,p,n);},failed) && failed==s.address);
    }
    check(Validate(read,failed));
    for(const auto& s:d2vr_camera_cover::pins) std::memcpy(mapped.data()+s.address-0x400000,s.bytes,s.size);
    check(d2vr_camera_cover::Validate(read,failed));
    for(const auto& s:d2vr_camera_cover::pins) for(unsigned i=0;i<s.size;++i) {
        auto& b=mapped[s.address-0x400000+i];b^=1;
        check(!d2vr_camera_cover::Validate(read,failed) && failed==s.address);b^=1;
    }
    // Optionally validate the actual PE's file-backed site bytes without
    // executing it. This catches transcription errors in the shared pins.
    if(argc>1) {
        std::ifstream input(argv[1],std::ios::binary);
        std::vector<unsigned char> pe((std::istreambuf_iterator<char>(input)),{});
        const auto u32=[&](size_t at){uint32_t v=0;if(at+4>pe.size())std::exit(2);std::memcpy(&v,pe.data()+at,4);return v;};
        const auto u16=[&](size_t at){uint16_t v=0;if(at+2>pe.size())std::exit(2);std::memcpy(&v,pe.data()+at,2);return v;};
        check(pe.size()>0x100 && u16(0)==0x5a4d);
        auto nt=u32(0x3c);check(u32(nt)==0x4550 && u16(nt+4)==0x14c);
        auto section=nt+24+u16(nt+20);
        const auto readPE=[&](uint32_t va,void* out,size_t n) {
            for(unsigned i=0;i<u16(nt+6);++i) {
                auto h=section+40*i;auto rva=u32(h+12),raw=u32(h+20),count=u32(h+16);
                if(va>=0x400000+rva && uint64_t(va)-0x400000-rva+n<=count) {
                    auto offset=uint64_t(raw)+va-0x400000-rva;
                    if(offset+n>pe.size()) return false;
                    std::memcpy(out,pe.data()+offset,n);return true;
                }
            }return false;
        };
        check(Validate(readPE,failed));
        std::printf("Installed EXE gameplay sites: PASS (%zu sites)\n",sizeof(sites)/sizeof(sites[0]));
        check(ValidateHandPack(readPE,failed));
        std::printf("Installed EXE hand pack sites: PASS (%zu sites)\n",
            sizeof(handPackSites)/sizeof(handPackSites[0]));
        if(!d2vr_camera_cover::Validate(readPE,failed)) std::printf("Camera pin mismatch: %08X\n",failed);
        check(d2vr_camera_cover::Validate(readPE,failed));
        std::printf("Installed EXE camera sites: PASS (%zu sites)\n",sizeof(d2vr_camera_cover::pins)/sizeof(d2vr_camera_cover::pins[0]));
    }
    // User's L3+R3 remains a single click even with uneven release/repress.
    d2vr_input::RecenterChord chord;
    check(!chord.Update(true,false,0));check(chord.Update(true,true,.01));
    check(!chord.Update(true,true,.02));check(!chord.Update(false,true,.03));
    check(!chord.Update(true,true,.04));check(!chord.Update(false,false,.05));
    check(chord.Update(true,true,.06));
    d2vr_input::HandSupport support;
    check(!support.Update(0,true,false,.1f));check(support.Update(.13,true,false,.1f));
    check(support.Update(.14,true,false,.22f));check(!support.Update(.15,true,false,.3f));
    check(!support.Update(.16,true,false,.1f));check(!support.Update(.29,false,false,.1f));
    // Left squeeze/active demon grab makes support ineligible immediately,
    // even while the controllers remain close together.
    check(!support.Update(.30,true,false,.1f));check(support.Update(.43,true,false,.1f));
    check(!support.Update(.44,false,false,.1f));
    d2vr_input::Vec3 throwDirection{};
    check(d2vr_input::ThrowDirection({1,0,0},{0,0,-1},.20f,throwDirection));
    check(throwDirection.x>.9f && throwDirection.z<-.2f);
    std::printf("tracked gameplay gate: %u checks PASS\n",checks);
}

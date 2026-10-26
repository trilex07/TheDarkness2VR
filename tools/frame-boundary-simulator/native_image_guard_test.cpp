// Real x86 patch policy regression. No process, native EXE, game or runtime.
#include "../../src/native_image_guard.h"
#include "../../src/diagnostic_present_patch.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <initializer_list>

using d2vr_native_image::Guard;
struct Memory {
    Guard guard{true,0x400000,0xdec000};
    unsigned char storage[5];
    unsigned writes=0,flushes=0,restores=0;
    Memory(){std::memcpy(storage,d2vr_present_patch::original,5);}
    unsigned char* bytes(){return storage;}
    bool writable(){
        if(!guard.Allows(0xa94052,5)) return false;
        ++writes;return true;
    }
    bool flush(){++flushes;return true;}
    bool restore(){++restores;return true;}
};
int main() {
    const Guard guarded{true,0x400000,0xdec000}, legacy{};
    // Both exactly reproduced crash inputs, plus the other current EXE writes.
    const uintptr_t targets[]={0xc63009,0xfc2664,0xa94052,0x986139,0x879c88,
                               0xad94a2,0xcfbd60,0xeeb478,0xb23f7f};
    for(auto target:targets) {
        assert(!guarded.Allows(target,4));
        assert(legacy.Allows(target,4));
    }
    // Data/vtables/imports receive no loophole. External D3D9 COM tables and
    // allocations remain eligible; a range straddling either image edge fails.
    assert(guarded.Allows(0x3ffffc,4));
    assert(!guarded.Allows(0x3fffff,2));
    assert(!guarded.Allows(0x11ebfff,2));
    assert(guarded.Allows(0x11ec000,4));
    assert(guarded.Allows(0x55f00000,4));
    assert(guarded.Allows(0x10000000,4096));
    assert(!guarded.Allows(0,4));assert(!guarded.Allows(0x55f00000,0));
    const auto maximum=(std::numeric_limits<uintptr_t>::max)();
    assert(!guarded.Allows(maximum-1,4));
    const Guard unknown{true}, overflow{true,maximum-1,4};
    assert(!unknown.Allows(0x55f00000,4));
    assert(!overflow.Allows(0x55f00000,4));
    for(bool xrDisabled:{false,true}) for(int stereo:{0,1,2}) for(bool capture:{false,true}) {
        assert(guarded.ConfigAllowed(xrDisabled,stereo,capture)==(xrDisabled && stereo==0 && !capture));
        assert(legacy.ConfigAllowed(xrDisabled,stereo,capture));
    }
    // Use the existing real Present transaction, not a second patch model.
    Memory m;d2vr_present_patch::Patch patch;
    assert(!patch.Install(m,0xa94052,0x55f10000));
    assert(!patch.installed && !patch.rollbackUncertain && !m.writes && !m.flushes && !m.restores);
    assert(!std::memcmp(m.bytes(),d2vr_present_patch::original,5));
    m.guard.enabled=false; // next fresh invocation, not a live config transition
    assert(patch.Install(m,0xa94052,0x55f10000));
    assert(patch.Remove(m));
    assert(m.writes==2 && m.flushes==2 && m.restores==2);
    assert(!std::memcmp(m.bytes(),d2vr_present_patch::original,5));
    std::puts("PASS native image guard: crash inputs, boundaries, mono admission, Present refusal/rollback");
}

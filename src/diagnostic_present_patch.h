#pragma once
// Diagnostic policy shared by the real installer and offline tests.
#include <cstdint>
#include <cstring>
namespace d2vr_present_patch {
inline constexpr unsigned char original[5]={0x8b,0x41,0x0c,0xff,0xd0};
inline bool Identity(const char* sha,unsigned machine,unsigned stamp,unsigned size,unsigned base,unsigned magic) {
    return sha && (!std::strcmp(sha,"d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1") ||
        !std::strcmp(sha,"55c8d634fcbe2996d8397d15d68ea728c999697f1dd40e8779b5afe8bbb4c339")) &&
        machine==0x14c && stamp==0x4f68a875 && size==0xdec000 && base==0x400000 && magic==0x10b;
}
// Platform supplies protection/cache operations. Only use at a quiescent site;
// this is NOT an atomic five-byte patch and does not suspend other threads.
struct Patch {
    unsigned char replacement[5]{};
    bool installed=false, rollbackUncertain=false;
    template<class Memory> bool Install(Memory& m,uint32_t site,uint32_t target) {
        if(installed || rollbackUncertain || std::memcmp(m.bytes(),original,5)) return false;
        replacement[0]=0xe8; uint32_t rel=target-(site+5);
        std::memcpy(replacement+1,&rel,4);
        if(!m.writable()) return false;
        std::memcpy(m.bytes(),replacement,5);
        bool flushed=m.flush();
        if(flushed && m.restore()) {installed=true;return true;}
        // writable() succeeded; if restore failed, Windows leaves old RWX
        // protection in place. Roll back before publishing an installed state.
        std::memcpy(m.bytes(),original,5);
        bool rolled=m.flush(); bool protectedAgain=m.restore();
        rollbackUncertain=!(rolled && protectedAgain);
        return false;
    }
    template<class Memory> bool Remove(Memory& m) {
        if(!installed || std::memcmp(m.bytes(),replacement,5)) return false;
        if(!m.writable()) return false;
        std::memcpy(m.bytes(),original,5);
        bool flushed=m.flush(), protectedAgain=m.restore();
        installed=false;rollbackUncertain=!(flushed && protectedAgain);
        return !rollbackUncertain;
    }
};
// A recursive overlay cycle must NOT forward to the same target again.
// Latch failure for capture acceptance; ordinary nonrecursive calls retain HR.
struct Dispatch {
    bool active=false, fault=false;
    uint64_t entries=0, forwarded=0, returned=0, recursive=0;
    uint64_t ok=0, statuses=0, errors=0;
    int32_t lastHr=0;
    bool haveReturn=false;
    bool Enter() {++entries;if(active){++recursive;fault=true;return false;}active=true;return true;}
    bool Returned(int32_t hr) {
        const bool changed=!haveReturn || hr!=lastHr;
        lastHr=hr;haveReturn=true;
        ++returned;
        if(hr==0) ++ok;
        else if(hr<0) {++errors;fault=true;}
        else ++statuses;
        // Count every return, but avoid synchronous log-file open/flush/close
        // on every repeated success status (e.g. S_PRESENT_OCCLUDED).
        // Transitions and failures remain immediate; periodic totals retain
        // evidence of non-OK returns even after the status returns to D3D_OK.
        return changed || hr<0 || entries<=3 || entries%1000==0 || fault;
    }
    void Leave() {active=false;}
};
}

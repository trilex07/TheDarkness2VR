#pragma once
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <atomic>

namespace d2vr_native_constants {
// Per-call ownership proof, not a stack/page permission cache. The selected
// native builders allocate 0x794 bytes after aligning EBP, then save EBX/ESI/EDI.
// Their simple API forwarders preserve EBP and copy three arguments verbatim.
// Local buffers and separately enabled exact fixed fields of the active owner
// qualify. The naked API gate captures owner EBX before any C++ register reuse.
enum class Stage { Vertex, Pixel };
struct Sites {
    uintptr_t apiReturn[2];
    uintptr_t builderFirst[2],builderEnd[2];
    uintptr_t parentReturn[2][3];
    uintptr_t fieldReturn[2][10] = {};
};
inline constexpr Sites nativeSites{
    {0x00b74740,0x0096e3d0},
    {0x00571670,0x00ad3380},{0x00573750,0x00ad5460},
    {{0x0071f6db,0x0071f71f,0x008f8180},{0x0071f702,0x0071f736,0}},
    {{0x571730,0x57176a,0x571787,0x5717ab,0x5717d0,0x571d6f,0x571e44,0x571e8d,0x572e2e,0x571e68},
     {0xad3440,0xad347a,0xad3497,0xad34bb,0xad34e0,0xad3a7f,0xad3b54,0xad3b9d,0xad4b3e,0xad3b78}}
};
inline std::atomic<bool> enabled{false},admitted{false},faulted{false};
inline std::atomic<bool> materialEnabled{false};
// The common matrix callback may follow several native computations. Only its
// current owner's fixed 0x330 scratch matrix qualifies, never a returned foreign
// pointer. The full producer admission and fresh owner test remain mandatory.
inline constexpr size_t fieldOffset[]={0x10,0xd0,0xd0,0x110,0x220,0x4f0,0x470,0x260,0x580,0x330};
inline constexpr size_t fieldVectors[]={12,4,1,17,4,6,4,4,1,4};
inline constexpr size_t fieldCount=sizeof(fieldOffset)/sizeof(fieldOffset[0]);
struct Counts {uint64_t attempts=0,proved=0,copies=0,bytes=0,faults=0,materialProved=0,materialCopies=0;};
inline thread_local Counts counts;

class Proof {
    const void* source_=nullptr;
    size_t bytes_=0;
    bool material_=false;
    Proof(const void* p,size_t n,bool material=false):source_(p),bytes_(n),material_(material){}
public:
    Proof()=default;
    bool Matches(const void* p,size_t n)const {
        return source_ && p==source_ && n==bytes_ && enabled.load(std::memory_order_relaxed) &&
            admitted.load(std::memory_order_relaxed) && !faulted.load(std::memory_order_relaxed) &&
            (!material_ || materialEnabled.load(std::memory_order_relaxed));
    }
    // Slot is the naked gate's incoming ESP (original API return slot).
    // frame is its incoming native EBP, NOT the C++ hook's EBP. Stack bounds are
    // read from the current TIB on each call; no foreign-thread memory is read.
    static Proof Inspect(const uintptr_t* slot,uintptr_t frame,const void* source,
                         size_t bytes,Stage stage,uintptr_t stackLow,uintptr_t stackHigh,
                         const Sites& sites=nativeSites,uintptr_t owner=0) {
        if(!enabled.load(std::memory_order_relaxed) || !admitted.load(std::memory_order_relaxed))return {};
        ++counts.attempts;
        const unsigned bank=stage==Stage::Vertex?0:1;
        const uintptr_t sp=reinterpret_cast<uintptr_t>(slot),p=reinterpret_cast<uintptr_t>(source);
        const uintptr_t aligned=frame&~uintptr_t(15);
        const size_t limit=bank==0?256*16:224*16;
        if(!source || !bytes || bytes%16 || bytes>limit || !stackLow || stackLow>=stackHigh ||
           sp<stackLow || sp>stackHigh || stackHigh-sp<12*sizeof(uintptr_t) || sp%4 ||
           frame<stackLow || frame>stackHigh || stackHigh-frame<2*sizeof(uintptr_t) ||
           aligned<0x794 || frame%4)return {};
        const uintptr_t first=aligned-0x794,last=aligned-4; // exclude cookie
        // Exact native callback ESP: allocation minus three saved registers.
        // This also proves that normal stack growth has already passed below
        // every source page; an automatic guard page cannot lie in this span.
        if(first<stackLow || first!=sp+12*sizeof(uintptr_t))return {};
        __try {
            if(slot[0]!=sites.apiReturn[bank] || slot[5]<sites.builderFirst[bank] ||
               slot[5]>=sites.builderEnd[bank] || slot[3]!=p || slot[4]!=bytes/16 ||
               slot[2]>=limit/16 || slot[4]>limit/16-slot[2] ||
               slot[6]!=slot[2] || slot[7]!=slot[3] || slot[8]!=slot[4])return {};
            const auto parent=reinterpret_cast<const uintptr_t*>(frame)[1];
            bool known=false;
            for(unsigned i=0;i<3;++i)known|=sites.parentReturn[bank][i] && parent==sites.parentReturn[bank][i];
            if(!known)return {};
            if(p>=first && p<=last && bytes<=last-p) {
                ++counts.proved;return Proof(source,bytes);
            }
            // EBX is assigned the current material owner once at builder entry,
            // preserved by both native forwarders, and never reassigned before
            // the epilogue. Admit only exact immediate-field submission sites.
            // No dynamic array, nested owner, page cache or cross-call pointer.
            if(materialEnabled.load(std::memory_order_relaxed) && owner && owner%4==0 &&
               owner<=UINTPTR_MAX-0x20) {
                for(unsigned i=0;i<fieldCount;++i) {
                    if(!sites.fieldReturn[bank][i] || slot[5]!=sites.fieldReturn[bank][i] ||
                       bytes!=fieldVectors[i]*16)continue;
                    const uintptr_t block=*reinterpret_cast<const uintptr_t*>(owner+0x1c);
                    if(block && block<=UINTPTR_MAX-0x5e4 && p==block+fieldOffset[i]) {
                        ++counts.proved;++counts.materialProved;return Proof(source,bytes,true);
                    }
                }
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {return {};}
        return {};
    }
    bool Material()const{return material_;}
};

inline bool Copy(const Proof& proof,void* destination,const void* source,size_t bytes) {
    if(!proof.Matches(source,bytes))return false;
    __try {std::memcpy(destination,source,bytes);}
    __except(EXCEPTION_EXECUTE_HANDLER) {
        // A violated ownership contract is sticky, never a permission to retry
        // the direct path. The caller retains the existing RPM/legacy fallback.
        ++counts.faults;faulted.store(true,std::memory_order_relaxed);
        admitted.store(false,std::memory_order_relaxed);return false;
    }
    ++counts.copies;counts.bytes+=bytes;if(proof.Material())++counts.materialCopies;return true;
}

inline Proof InspectCurrent(const uintptr_t* slot,uintptr_t frame,const void* source,size_t bytes,Stage stage,uintptr_t owner=0) {
#if defined(_M_IX86)
    const auto* tib=reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    return Proof::Inspect(slot,frame,source,bytes,stage,reinterpret_cast<uintptr_t>(tib->StackLimit),
        reinterpret_cast<uintptr_t>(tib->StackBase),nativeSites,owner);
#else
    return {};
#endif
}

inline bool AdmitImage(const char* sha,const unsigned char* image) {
    admitted.store(false,std::memory_order_relaxed);
    if(faulted.load(std::memory_order_relaxed) || !enabled.load(std::memory_order_relaxed) || !sha ||
       std::strcmp(sha,"d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1") ||
       reinterpret_cast<uintptr_t>(image)!=0x400000)return false;
    struct Fingerprint {uintptr_t address;size_t bytes;uint32_t hash;};
    constexpr Fingerprint required[]={{0xb74720,35,0x9e9cc9ec},{0x96e3b0,35,0xb213a2f0},
        {0x571670,0x20e0,0x4e52ac82},{0xad3380,0x20e0,0x180de9d4}};
    __try {
        for(const auto& site:required) {
            uint32_t hash=2166136261u;
            const auto* code=reinterpret_cast<const unsigned char*>(site.address);
            for(size_t i=0;i<site.bytes;++i)hash=(hash^code[i])*16777619u;
            if(hash!=site.hash)return false;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
    admitted.store(true,std::memory_order_relaxed);return true;
}
} // namespace d2vr_native_constants

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
// Local buffers, freshly bounded active caller-stack buffers and separately
// enabled exact fields of the active owner qualify. The naked API gate captures
// owner EBX before any C++ register reuse.
enum class Stage { Vertex, Pixel };
struct Sites {
    uintptr_t apiReturn[2];
    uintptr_t builderFirst[2],builderEnd[2];
    uintptr_t parentReturn[2][3];
    uintptr_t fieldReturn[2][10] = {};
    uintptr_t nestedReturn[2][4] = {};
};
inline constexpr Sites nativeSites{
    {0x00b74740,0x0096e3d0},
    {0x00571670,0x00ad3380},{0x00573750,0x00ad5460},
    {{0x0071f6db,0x0071f71f,0x008f8180},{0x0071f702,0x0071f736,0}},
    {{0x571730,0x57176a,0x571787,0x5717ab,0x5717d0,0x571d6f,0x571e44,0x571e8d,0x572e2e,0x571e68},
     {0xad3440,0xad347a,0xad3497,0xad34bb,0xad34e0,0xad3a7f,0xad3b54,0xad3b9d,0xad4b3e,0xad3b78}},
    {{0x571db2,0x571dea,0x571e1f,0x5723a1},
     {0xad3ac2,0xad3afa,0xad3b2f,0xad40b1}}
};
inline std::atomic<bool> enabled{false},admitted{false},faulted{false};
inline std::atomic<bool> materialEnabled{false};
// The common matrix callback may follow several native computations. Only its
// current owner's fixed 0x330 scratch matrix qualifies, never a returned foreign
// pointer. The full producer admission and fresh owner test remain mandatory.
inline constexpr size_t fieldOffset[]={0x10,0xd0,0xd0,0x110,0x220,0x4f0,0x470,0x260,0x580,0x330};
inline constexpr size_t fieldVectors[]={12,4,1,17,4,6,4,4,1,4};
inline constexpr size_t fieldCount=sizeof(fieldOffset)/sizeof(fieldOffset[0]);
inline constexpr size_t nestedOffset[]={0xe0,0x70,0,0xc0};
inline constexpr size_t nestedVectors[]={6,1,9,4};
struct Counts {uint64_t attempts=0,proved=0,copies=0,bytes=0,faults=0,materialProved=0,materialCopies=0,activeStackProved=0,activeStackCopies=0,nestedProved=0,nestedCopies=0;};
inline thread_local Counts counts;

class Proof {
    const void* source_=nullptr;
    size_t bytes_=0;
    bool material_=false;
    bool activeStack_=false;
    bool nested_=false;
    Proof(const void* p,size_t n,bool material=false,bool activeStack=false,bool nested=false)
        :source_(p),bytes_(n),material_(material),activeStack_(activeStack),nested_(nested){}
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
            // No dynamic array, page cache or cross-call pointer. The separate
            // nested-field sites below require their own exact producer path.
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
                for(unsigned i=0;i<4;++i) {
                    if(!sites.nestedReturn[bank][i] || slot[5]!=sites.nestedReturn[bank][i] ||
                       bytes!=nestedVectors[i]*16)continue;
                    // These pinned producer branches submit an immediate field
                    // of the CURRENT context's model/camera synchronously. Walk
                    // the exact native chain anew; neither an array nor a
                    // returned helper pointer grants ownership here.
                    const uintptr_t block=*reinterpret_cast<const uintptr_t*>(owner+0x1c);
                    if(!block || block%4 || block>UINTPTR_MAX-0x5dc)return {};
                    uintptr_t object=*reinterpret_cast<const uintptr_t*>(block+(i==3?0x5d8:0x5d4));
                    if(!object || object%4)return {};
                    if(i!=3) {
                        if(object>UINTPTR_MAX-0x158)return {};
                        const uintptr_t ref=*reinterpret_cast<const uintptr_t*>(object+0x154);
                        if(!ref || ref%4 || ref>UINTPTR_MAX-sizeof(uintptr_t))return {};
                        object=*reinterpret_cast<const uintptr_t*>(ref);
                    }
                    if(!object || object%4 || object>UINTPTR_MAX-nestedOffset[i]-bytes ||
                       p!=object+nestedOffset[i])return {};
                    ++counts.proved;++counts.materialProved;++counts.nestedProved;
                    return Proof(source,bytes,true,false,true);
                }
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {return {};}
        return {};
    }
    bool Material()const{return material_;}
    bool ActiveStack()const{return activeStack_;}
    bool Nested()const{return nested_;}
    // The pinned forwarders duplicate all three arguments without moving the
    // payload. Any source wholly ABOVE the current native call stack and BELOW
    // this thread's current StackBase is live until this synchronous call
    // returns, including an outer native caller's local buffer. The current ESP
    // is already below every source page: automatic stack guard growth cannot
    // lie in this span. This does not admit heap buffers, another thread's stack,
    // or a previous stack limit. As with compiler-owned locals, manually
    // protected active stack pages violate the native readable-buffer contract;
    // Copy's sticky exception path handles an unexpected violation.
    // This fallback deliberately grants no material/EBX ownership receipt.
    static Proof InspectActiveStack(const uintptr_t* slot,const void* source,size_t bytes,
                                    Stage stage,uintptr_t stackLow,uintptr_t stackHigh,
                                    const Sites& sites=nativeSites) {
        if(!enabled.load(std::memory_order_relaxed) || !admitted.load(std::memory_order_relaxed) ||
           faulted.load(std::memory_order_relaxed))return {};
        const unsigned bank=stage==Stage::Vertex?0:1;
        const uintptr_t sp=reinterpret_cast<uintptr_t>(slot),p=reinterpret_cast<uintptr_t>(source);
        const size_t limit=bank==0?256*16:224*16;
        if(!source || !bytes || bytes%16 || bytes>limit || p%4 ||
           !stackLow || stackLow>=stackHigh || sp<stackLow || sp>stackHigh || sp%4 ||
           stackHigh-sp<12*sizeof(uintptr_t) || p<sp+12*sizeof(uintptr_t) ||
           p>stackHigh || bytes>stackHigh-p)return {};
        __try {
            if(slot[0]!=sites.apiReturn[bank] || slot[3]!=p || slot[4]!=bytes/16 ||
               slot[2]>=limit/16 || slot[4]>limit/16-slot[2] ||
               slot[6]!=slot[2] || slot[7]!=slot[3] || slot[8]!=slot[4])return {};
        } __except(EXCEPTION_EXECUTE_HANDLER){return {};}
        ++counts.proved;++counts.activeStackProved;
        return Proof(source,bytes,false,true);
    }
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
    ++counts.copies;counts.bytes+=bytes;if(proof.Material())++counts.materialCopies;
    if(proof.ActiveStack())++counts.activeStackCopies;
    if(proof.Nested())++counts.nestedCopies;return true;
}

inline Proof InspectCurrent(const uintptr_t* slot,uintptr_t frame,const void* source,size_t bytes,Stage stage,
                            uintptr_t owner=0,const Sites& sites=nativeSites) {
#if defined(_M_IX86)
    const auto* tib=reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    const auto low=reinterpret_cast<uintptr_t>(tib->StackLimit),high=reinterpret_cast<uintptr_t>(tib->StackBase);
    const auto proof=Proof::Inspect(slot,frame,source,bytes,stage,low,high,sites,owner);
    return proof.Matches(source,bytes)?proof:Proof::InspectActiveStack(slot,source,bytes,stage,low,high,sites);
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

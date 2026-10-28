#pragma once
// Offline candidate infrastructure: copy only independently audited, digest-
// pinned x86 functions into owned RX storage. No hook, EXE write, debug-context
// operation, import edit, or native-instance publication occurs in this class.
#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include <memory>
#include "native_image_guard.h"
#pragma comment(lib,"bcrypt.lib")

namespace d2vr_native_clone {
struct Block {const void* source;size_t bytes;const char* sha256;};
struct Call {unsigned block;size_t offset;uintptr_t originalTarget;};
struct Override {unsigned block;size_t offset;const void* target;};
// A caller-audited whole-instruction span inside a digest-pinned copy only.
// Its adapter must replay the displaced ABI/instructions, including flags.
struct CallSpan {unsigned block;size_t offset;const unsigned char* expected;size_t bytes;const void* target;};

inline bool Digest(const void* data,size_t bytes,unsigned char out[32]) {
    if(!data || !bytes || bytes>MAXDWORD)return false;
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    bool ok=false;
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0 &&
       BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)>=0)
        ok=BCryptHashData(hash,static_cast<PUCHAR>(const_cast<void*>(data)),ULONG(bytes),0)>=0 &&
            BCryptFinishHash(hash,out,32,0)>=0;
    if(hash)BCryptDestroyHash(hash);
    if(alg)BCryptCloseAlgorithmProvider(alg,0);
    return ok;
}
inline bool MatchesDigest(const void* data,size_t bytes,const char* expected) {
    if(!expected || std::strlen(expected)!=64)return false;
    unsigned char digest[32]{};if(!Digest(data,bytes,digest))return false;
    constexpr char hex[]="0123456789abcdef";
    for(unsigned i=0;i<32;++i)
        if(expected[i*2]!=hex[digest[i]>>4] || expected[i*2+1]!=hex[digest[i]&15])return false;
    return true;
}

class Code {
    struct Free {void operator()(void* p)const{if(p)VirtualFree(p,0,MEM_RELEASE);}};
    std::unique_ptr<void,Free> memory_;
    size_t offsets_[8]{};
    unsigned blocks_=0;
    const char* status_="idle";
    bool Fail(const char* status){status_=status;return false;}
    static bool ImageSpan(const Block& block,const d2vr_native_image::Guard& guard) {
        const auto begin=reinterpret_cast<uintptr_t>(block.source);
        if(!guard.enabled || !guard.base || guard.bytes>(std::numeric_limits<uintptr_t>::max)()-guard.base ||
           begin<guard.base || begin-guard.base>=guard.bytes ||
           !block.bytes || block.bytes>guard.bytes-(begin-guard.base))return false;
        auto position=begin;const auto end=begin+block.bytes;
        while(position<end) {
            MEMORY_BASIC_INFORMATION page{};
            if(!VirtualQuery(reinterpret_cast<void*>(position),&page,sizeof(page)) ||
               page.Type!=MEM_IMAGE || page.State!=MEM_COMMIT ||
               page.AllocationBase!=reinterpret_cast<void*>(guard.base) ||
               !(page.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)) ||
               (page.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
            const auto next=reinterpret_cast<uintptr_t>(page.BaseAddress)+page.RegionSize;
            if(next<=position)return false;
            position=next;
        }
        return true;
    }
public:
    Code()=default;
    Code(const Code&)=delete;
    Code& operator=(const Code&)=delete;
    // This class never publishes Entry. A host which publishes it must retain
    // this object for the entire callable lifetime. Fixtures destroy at quiescence.
    bool Prepare(const Block* blocks,unsigned count,const Call* calls,size_t callCount,
                 const Override* replacement,const d2vr_native_image::Guard& guard,const CallSpan* span=nullptr) {
        static_assert(sizeof(void*)==4,"x86 only");
        if(memory_)return Fail("already-prepared");
        if(!blocks || !count || count>8 || (!calls && callCount) || callCount>4096)
            return Fail("plan-size");
        std::vector<unsigned char> snapshots[8];
        size_t offsets[8]{},total=0;
        try {
            for(unsigned i=0;i<count;++i) {
                if(blocks[i].bytes>16384 || !ImageSpan(blocks[i],guard))return Fail("source-image");
                const auto source=reinterpret_cast<uintptr_t>(blocks[i].source);
                for(unsigned j=0;j<i;++j) {
                    const auto previous=reinterpret_cast<uintptr_t>(blocks[j].source);
                    if(source<previous+blocks[j].bytes && previous<source+blocks[i].bytes)
                        return Fail("source-overlap");
                }
                snapshots[i].resize(blocks[i].bytes);SIZE_T read=0;
                if(!ReadProcessMemory(GetCurrentProcess(),blocks[i].source,snapshots[i].data(),blocks[i].bytes,&read) ||
                   read!=blocks[i].bytes)return Fail("source-read");
                if(!MatchesDigest(snapshots[i].data(),blocks[i].bytes,blocks[i].sha256))return Fail("source-digest");
                offsets[i]=total;total=(total+blocks[i].bytes+15u)&~size_t(15u);
            }
        } catch(...) {return Fail("snapshot-allocation");}
        if(!total || total>65536)return Fail("code-size");
        unsigned replacements=0;
        for(size_t i=0;i<callCount;++i) {
            const auto& call=calls[i];
            if(call.block>=count || blocks[call.block].bytes<5 || call.offset>blocks[call.block].bytes-5)
                return Fail("call-range");
            for(size_t j=0;j<i;++j)if(calls[j].block==call.block &&
                call.offset<calls[j].offset+5 && calls[j].offset<call.offset+5)return Fail("call-overlap");
            const auto* instruction=snapshots[call.block].data()+call.offset;
            uint32_t relative=0;std::memcpy(&relative,instruction+1,4);
            const uint32_t target=uint32_t(reinterpret_cast<uintptr_t>(blocks[call.block].source)+call.offset+5)+relative;
            if(instruction[0]!=0xe8 || target!=call.originalTarget)return Fail("call-contract");
            if(replacement && replacement->block==call.block && replacement->offset==call.offset)++replacements;
        }
        if(replacement && (!replacement->target || replacements!=1))return Fail("override-contract");
        if(span) {
            if(span->block>=count || !span->expected || !span->target || span->bytes<5 || span->bytes>16 ||
               blocks[span->block].bytes<span->bytes || span->offset>blocks[span->block].bytes-span->bytes)
                return Fail("span-range");
            if(std::memcmp(snapshots[span->block].data()+span->offset,span->expected,span->bytes))
                return Fail("span-contract");
            for(size_t i=0;i<callCount;++i)if(calls[i].block==span->block &&
                span->offset<calls[i].offset+5 && calls[i].offset<span->offset+span->bytes)
                return Fail("span-overlap");
        }
        std::unique_ptr<void,Free> code(VirtualAlloc(nullptr,total,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        if(!code || !guard.Allows(reinterpret_cast<uintptr_t>(code.get()),total))return Fail("owned-allocation");
        auto* output=static_cast<unsigned char*>(code.get());
        for(unsigned i=0;i<count;++i)std::memcpy(output+offsets[i],snapshots[i].data(),blocks[i].bytes);
        for(size_t i=0;i<callCount;++i) {
            const auto& call=calls[i];uintptr_t target=call.originalTarget;
            for(unsigned j=0;j<count;++j) {
                const auto original=reinterpret_cast<uintptr_t>(blocks[j].source);
                if(target>=original && target-original<blocks[j].bytes) {
                    target=reinterpret_cast<uintptr_t>(output)+offsets[j]+target-original;break;
                }
            }
            if(replacement && replacement->block==call.block && replacement->offset==call.offset)
                target=reinterpret_cast<uintptr_t>(replacement->target);
            auto* site=output+offsets[call.block]+call.offset;
            const uint32_t relative=uint32_t(target)-uint32_t(reinterpret_cast<uintptr_t>(site)+5);
            std::memcpy(site+1,&relative,4);
        }
        DWORD previous=0;
        if(span) {
            auto* site=output+offsets[span->block]+span->offset;
            std::memset(site,0x90,span->bytes);site[0]=0xe8;
            const uint32_t relative=uint32_t(reinterpret_cast<uintptr_t>(span->target))-
                uint32_t(reinterpret_cast<uintptr_t>(site)+5);
            std::memcpy(site+1,&relative,4);
        }
        if(!VirtualProtect(output,total,PAGE_EXECUTE_READ,&previous))return Fail("seal-rx");
        if(!FlushInstructionCache(GetCurrentProcess(),output,total))return Fail("flush-code");
        for(unsigned i=0;i<count;++i)offsets_[i]=offsets[i];
        blocks_=count;memory_=std::move(code);status_="prepared-unpublished";return true;
    }
    void* Entry(unsigned block)const {
        return memory_ && block<blocks_?static_cast<unsigned char*>(memory_.get())+offsets_[block]:nullptr;
    }
    const char* Status()const{return status_;}
};
}

#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "../src/native_view_code_clone.h"
#include "../third_party/minhook/src/hde/hde32.h"
#define CHECK(x) do{if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
using namespace d2vr_native_clone;
static uintptr_t downstreamCaller=0;
static unsigned downstreamCalls=0,originalCalls=0,replacements=0;
static int __declspec(noinline) __cdecl Input(int value){++originalCalls;return value+7;}
static int __declspec(noinline) __cdecl Replacement(int value){++replacements;return value+10;}
static int __declspec(noinline) __cdecl Downstream(int value) {
    downstreamCaller=reinterpret_cast<uintptr_t>(_ReturnAddress());++downstreamCalls;return value+100;
}
#define D2VR_NOPS10 __asm {nop} __asm {nop} __asm {nop} __asm {nop} __asm {nop} __asm {nop} __asm {nop} __asm {nop} __asm {nop} __asm {nop}
static int __declspec(naked) __cdecl Native(int) {
    __asm {
        push ebp
        mov ebp,esp
        cmp dword ptr [ebp+8],0
        je early
        push dword ptr [ebp+8]
        call Input
        add esp,4
    }
    // Force the early exit to a near conditional branch, like AD9267.
    D2VR_NOPS10 D2VR_NOPS10 D2VR_NOPS10 D2VR_NOPS10 D2VR_NOPS10 D2VR_NOPS10 D2VR_NOPS10
    D2VR_NOPS10 D2VR_NOPS10 D2VR_NOPS10 D2VR_NOPS10 D2VR_NOPS10 D2VR_NOPS10 D2VR_NOPS10
    __asm {
        // Exact marker at the native continuation, not copied into the prefix.
        _emit 0x0f
        _emit 0x1f
        _emit 0x40
        _emit 0x00
        push eax
        call Downstream
        add esp,4
        mov esp,ebp
        pop ebp
        ret
    early:
        mov eax,321
        mov esp,ebp
        pop ebp
        ret
    }
}
static std::string Hash(const void* bytes,size_t count) {
    unsigned char digest[32];CHECK(Digest(bytes,count,digest));std::string result;
    for(auto b:digest){char text[3];sprintf_s(text,"%02x",b);result+=text;}return result;
}
int main() {
    static_assert(sizeof(void*)==4,"x86");
    const auto source=reinterpret_cast<uintptr_t>(&Native);
    const auto* bytes=reinterpret_cast<const unsigned char*>(source);
    size_t size=0,cut=0;unsigned returns=0;std::vector<Call> calls;std::vector<PrefixExit> exits;
    uintptr_t expectedDownstream=0;std::vector<Call> fullCalls;
    while(size<512 && returns<2) {
        hde32s instruction{};const auto n=hde32_disasm(bytes+size,&instruction);CHECK(n && !(instruction.flags&F_ERROR));
        if(!memcmp(bytes+size,"\x0f\x1f\x40\x00",4))cut=size;
        if(instruction.opcode==0xe8) {
            uint32_t relative=0;memcpy(&relative,bytes+size+1,4);
            const auto target=uint32_t(source+size+5)+relative;
            fullCalls.push_back({0,size,target});
            if(!cut)calls.push_back({0,size,target});
            else {CHECK(target==reinterpret_cast<uintptr_t>(&Downstream));expectedDownstream=source+size+5;}
        }
        if(instruction.opcode==0x0f && instruction.opcode2==0x84) {
            uint32_t relative=0;memcpy(&relative,bytes+size+2,4);
            exits.push_back({size,uint32_t(source+size+6)+relative});
        }
        if(instruction.opcode==0xc3)++returns;
        size+=n;
    }
    CHECK(returns==2 && cut && calls.size()==1 && exits.size()==1 && expectedDownstream);
    const auto digest=Hash(bytes,size);
    Block block{bytes,size,digest.c_str()};Prefix prefix{cut,exits.data(),exits.size()};
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    const auto pe=reinterpret_cast<IMAGE_NT_HEADERS32*>(image+dos->e_lfanew);
    d2vr_native_image::Guard guard{true,image,pe->OptionalHeader.SizeOfImage};
    Override replacement{0,calls[0].offset,reinterpret_cast<void*>(&Replacement)};
    Code code;CHECK(code.Prepare(&block,1,calls.data(),calls.size(),&replacement,guard,nullptr,&prefix));
    using Invoke=int(__cdecl*)(int);const auto copied=reinterpret_cast<Invoke>(code.Entry(0));CHECK(copied);
    MEMORY_BASIC_INFORMATION page{};CHECK(VirtualQuery(code.Entry(0),&page,sizeof(page)));
    CHECK(page.Type==MEM_PRIVATE && page.Protect==PAGE_EXECUTE_READ);
    for(int i=1;i<=1000;++i) {
        CHECK(Native(i)==i+107 && downstreamCaller==expectedDownstream);
        CHECK(copied(i)==i+110 && downstreamCaller==expectedDownstream);
        CHECK(copied(0)==321); // untouched original native early epilogue
    }
    CHECK(originalCalls==1000 && replacements==1000 && downstreamCalls==2000);
    CHECK(Hash(bytes,size)==digest);
    Code unmodifiedPrefix,full;
    CHECK(unmodifiedPrefix.Prepare(&block,1,calls.data(),calls.size(),nullptr,guard,nullptr,&prefix));
    CHECK(full.Prepare(&block,1,fullCalls.data(),fullCalls.size(),nullptr,guard));
    for(int i=1;i<=1000;++i) {
        CHECK(reinterpret_cast<Invoke>(unmodifiedPrefix.Entry(0))(i)==i+107 && downstreamCaller==expectedDownstream);
        CHECK(reinterpret_cast<Invoke>(full.Entry(0))(i)==i+107);
        CHECK(downstreamCaller==reinterpret_cast<uintptr_t>(full.Entry(0))+expectedDownstream-source);
        CHECK(reinterpret_cast<Invoke>(full.Entry(0))(0)==321);
    }
    CHECK(Hash(bytes,size)==digest);
    auto reject=[&](const Prefix& plan,const std::vector<PrefixExit>& e,const std::vector<Call>& c) {
        Code invalid;Prefix p=plan;p.exits=e.data();p.exitCount=e.size();
        CHECK(!invalid.Prepare(&block,1,c.data(),c.size(),&replacement,guard,nullptr,&p));CHECK(!invalid.Entry(0));
    };
    auto wrong=exits;wrong[0].originalTarget++;reject(prefix,wrong,calls);
    wrong=exits;wrong[0].offset++;reject(prefix,wrong,calls);
    wrong=exits;wrong.push_back(wrong[0]);reject(prefix,wrong,calls);
    auto shortCopy=prefix;shortCopy.copiedBytes=5;reject(shortCopy,exits,calls);
    shortCopy=prefix;shortCopy.copiedBytes=size;reject(shortCopy,exits,calls);
    auto outside=calls;outside[0].offset=cut;reject(prefix,exits,outside);
    puts("PASS x86 actual prefix copier: 1000 overrides + 1000 original + 1000 unmodified prefix + 1000 full clone calls, 2000 early exits; downstream PCs/stack/native image unchanged, RX sealing, malformed plans rejected; no game/HMD claim");
}

#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <string>
#include <xmmintrin.h>
#include "../src/native_view_clone_plan.h"
#include "../src/native_view_owner_shim.h"
#include "../third_party/minhook/src/hde/hde32.h"
#define CHECK(x) do{if(!(x)){std::printf("FAIL %d %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace d2vr_native_clone;
struct Owner {unsigned value=0x12345678,visits=0;};
static Owner owner;
static volatile unsigned watched[3]{};
static unsigned callbacks=0;
static uintptr_t lastCaller=0;
static void* viewReference=&owner;
static unsigned viewGetterCalls=0,viewOwnerCalls=0;
static void __cdecl ObserveViewOwner(void* renderer,void* reference,void* manager) {
    const DWORD error=GetLastError();
    CHECK(renderer==&owner && manager==&owner && *static_cast<void**>(reference)==&viewReference);
    ++viewOwnerCalls;
    _mm_setcsr(_mm_getcsr()^0x8000u);
    __asm fldpi
    SetLastError(error);
}
D2VR_VIEW_OWNER_SHIM(ViewOwnerAdapter,ObserveViewOwner)
static void __declspec(naked) ViewGetter() {
    __asm inc viewGetterCalls
    __asm mov eax,[esp+4]
    __asm mov dword ptr [eax],offset viewReference
    __asm xor ecx,ecx
    __asm ret 4
}
static unsigned __declspec(naked) ViewCaller(void**) {
    __asm push ebx
    __asm mov ebx,ecx
    __asm push dword ptr [esp+8]
    __asm mov eax,offset ViewGetter
    __asm call eax
    __asm mov ecx,[eax]
    __asm cmp dword ptr [ecx],0
    __asm setnz al
    __asm movzx eax,al
    __asm pop ebx
    __asm ret 4
}
static unsigned __declspec(naked) Leaf(unsigned) {
    __asm mov eax,[ecx]
    __asm add eax,[esp+4]
    __asm inc dword ptr [ecx+4]
    __asm ret 4
}
static unsigned __declspec(naked) Inner(unsigned) {
    __asm push dword ptr [esp+4]
    __asm call Leaf
    __asm ret 4
}
static unsigned __declspec(naked) Outer(unsigned) {
    __asm push dword ptr [esp+4]
    __asm call Inner
    __asm ret 4
}
static void __fastcall Observe(Owner* selected,uintptr_t caller) {
    const DWORD error=GetLastError();
    CHECK(selected==&owner);++callbacks;lastCaller=caller;
    _mm_setcsr(_mm_getcsr()^0x8000u);
    __asm fld1
    __asm fldpi
    SetLastError(error);
}
static void* originalLeaf=reinterpret_cast<void*>(&Leaf);
static unsigned __declspec(naked) Hook(unsigned) {
    __asm pushfd
    __asm pushad
    __asm mov eax,esp
    __asm sub esp,528
    __asm and esp,0xfffffff0
    __asm mov [esp+512],eax
    __asm fxsave [esp]
    __asm fninit
    __asm mov edx,[eax+36]
    __asm call Observe
    __asm fxrstor [esp]
    __asm mov esp,[esp+512]
    __asm popad
    __asm popfd
    __asm jmp dword ptr [originalLeaf]
}
static size_t FunctionSize(const void* code,std::vector<Call>& calls,unsigned block) {
    const auto* p=static_cast<const unsigned char*>(code);size_t position=0;
    while(position<128) {
        hde32s instruction{};const auto length=hde32_disasm(p+position,&instruction);
        CHECK(length && !(instruction.flags&F_ERROR));
        if(instruction.opcode==0xe8) {
            uint32_t relative=0;memcpy(&relative,p+position+1,4);
            calls.push_back({block,position,uint32_t(reinterpret_cast<uintptr_t>(p)+position+5)+relative});
        }
        position+=length;
        if(instruction.opcode==0xc2 || instruction.opcode==0xc3)return position;
    }
    CHECK(false);return 0;
}
static std::string Hash(const void* code,size_t size) {
    unsigned char digest[32]{};CHECK(Digest(code,size,digest));std::string result;
    for(auto byte:digest){char text[3];sprintf_s(text,"%02x",byte);result+=text;}
    return result;
}
struct DebugWork {DWORD thread;bool install=false;CONTEXT context{};bool ok=false;const CONTEXT* restore=nullptr;};
static DWORD WINAPI DebugWorker(void* argument) {
    auto& work=*static_cast<DebugWork*>(argument);
    HANDLE thread=OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_SUSPEND_RESUME,FALSE,work.thread);
    if(!thread)return 0;
    if(SuspendThread(thread)!=0){CloseHandle(thread);return 0;}
    work.context.ContextFlags=CONTEXT_DEBUG_REGISTERS;
    if(GetThreadContext(thread,&work.context)) {
        if(work.restore) {
            work.context.Dr0=work.restore->Dr0;work.context.Dr1=work.restore->Dr1;
            work.context.Dr2=work.restore->Dr2;work.context.Dr3=work.restore->Dr3;
            work.context.Dr6=work.restore->Dr6;work.context.Dr7=work.restore->Dr7;
            work.ok=SetThreadContext(thread,&work.context)!=FALSE;
        } else if(work.install) {
            work.context.Dr0=reinterpret_cast<DWORD>(&watched[0]);
            work.context.Dr1=reinterpret_cast<DWORD>(&watched[1]);
            work.context.Dr2=reinterpret_cast<DWORD>(&watched[2]);
            work.context.Dr3=reinterpret_cast<DWORD>(&owner);
            work.context.Dr7=0xffff0015u;
            work.ok=SetThreadContext(thread,&work.context)!=FALSE;
        } else work.ok=true;
    }
    ResumeThread(thread);CloseHandle(thread);return 0;
}
static CONTEXT Debug(bool install=false,const CONTEXT* restore=nullptr) {
    DebugWork work{GetCurrentThreadId(),install};
    work.restore=restore;
    HANDLE worker=CreateThread(nullptr,0,DebugWorker,&work,0,nullptr);CHECK(worker);
    CHECK(WaitForSingleObject(worker,10000)==WAIT_OBJECT_0 && work.ok);CloseHandle(worker);return work.context;
}
int main() {
    CHECK(sizeof(viewBlocks)/sizeof(viewBlocks[0])==2 && sizeof(viewCalls)/sizeof(viewCalls[0])==131);
    auto image=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS32*>(image+reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew);
    const d2vr_native_image::Guard guard{true,reinterpret_cast<uintptr_t>(image),pe->OptionalHeader.SizeOfImage};
    std::vector<Call> calls;
    const auto outerSize=FunctionSize(reinterpret_cast<void*>(&Outer),calls,0);
    const auto innerSize=FunctionSize(reinterpret_cast<void*>(&Inner),calls,1);
    const auto outerHash=Hash(reinterpret_cast<void*>(&Outer),outerSize),innerHash=Hash(reinterpret_cast<void*>(&Inner),innerSize);
    const Block blocks[]={{reinterpret_cast<void*>(&Outer),outerSize,outerHash.c_str()},
                          {reinterpret_cast<void*>(&Inner),innerSize,innerHash.c_str()}};
    CHECK(calls.size()==2 && calls[0].originalTarget==reinterpret_cast<uintptr_t>(&Inner) &&
        calls[1].originalTarget==reinterpret_cast<uintptr_t>(&Leaf));
    const Override replacement{1,calls[1].offset,reinterpret_cast<void*>(&Hook)};
    using Fn=unsigned(__thiscall*)(Owner*,unsigned);
    CHECK(reinterpret_cast<Fn>(&Outer)(&owner,7)==owner.value+7 && owner.visits==1 && !callbacks);
    Code plain;CHECK(plain.Prepare(blocks,2,calls.data(),calls.size(),nullptr,guard));
    CHECK(reinterpret_cast<Fn>(plain.Entry(0))(&owner,8)==owner.value+8 && owner.visits==2 && !callbacks);
    for(unsigned i=0;i<2;++i) {
        uint32_t relative=0;auto* site=static_cast<unsigned char*>(plain.Entry(i))+calls[i].offset;
        memcpy(&relative,site+1,4);
        const uint32_t destination=uint32_t(reinterpret_cast<uintptr_t>(site)+5)+relative;
        CHECK(destination==(i?reinterpret_cast<uintptr_t>(&Leaf):reinterpret_cast<uintptr_t>(plain.Entry(1))));
    }
    CHECK(!plain.Entry(2));
    MEMORY_BASIC_INFORMATION page{};CHECK(VirtualQuery(plain.Entry(0),&page,sizeof(page)));
    CHECK(page.Type==MEM_PRIVATE && page.Protect==PAGE_EXECUTE_READ);
    // Simulate the game's three active data watches plus disabled DR3 object
    // storage in THIS fixture process only. Neither prepared clone touches it.
    const auto initialDebug=Debug();Debug(true);const auto before=Debug();
    Code observed;CHECK(observed.Prepare(blocks,2,calls.data(),calls.size(),&replacement,guard));
    __declspec(align(16)) unsigned char floatingBefore[512]{},floatingAfter[512]{};
    __asm fld1
    __asm fldpi
    _fxsave(floatingBefore);SetLastError(0x12345678);
    for(unsigned i=0;i<10000;++i)CHECK(reinterpret_cast<Fn>(observed.Entry(0))(&owner,i)==owner.value+i);
    const auto error=GetLastError();_fxsave(floatingAfter);__asm fninit
    CHECK(error==0x12345678 && !memcmp(floatingBefore,floatingAfter,sizeof(floatingBefore)));
    CHECK(callbacks==10000 && owner.visits==10002);
    CHECK(lastCaller==reinterpret_cast<uintptr_t>(observed.Entry(1))+calls[1].offset+5);
    const auto after=Debug();
    CHECK(before.Dr0==after.Dr0 && before.Dr1==after.Dr1 && before.Dr2==after.Dr2 &&
        before.Dr3==after.Dr3 && before.Dr7==after.Dr7);
    CHECK(Hash(reinterpret_cast<void*>(&Outer),outerSize)==outerHash && Hash(reinterpret_cast<void*>(&Inner),innerSize)==innerHash);
    Code rejected;
    CHECK(!rejected.Prepare(blocks,2,calls.data(),calls.size(),&replacement,{}));
    CHECK(!rejected.Entry(0) && !strcmp(rejected.Status(),"source-image"));
    auto badBlocks=std::array<Block,2>{blocks[0],blocks[1]};badBlocks[1].sha256=outerHash.c_str();
    CHECK(!rejected.Prepare(badBlocks.data(),2,calls.data(),calls.size(),&replacement,guard));
    CHECK(!strcmp(rejected.Status(),"source-digest"));
    badBlocks[1]=blocks[0];
    CHECK(!rejected.Prepare(badBlocks.data(),2,calls.data(),calls.size(),&replacement,guard));
    CHECK(!strcmp(rejected.Status(),"source-overlap"));
    auto badCalls=calls;badCalls[1].originalTarget++;
    CHECK(!rejected.Prepare(blocks,2,badCalls.data(),badCalls.size(),&replacement,guard));
    CHECK(!strcmp(rejected.Status(),"call-contract"));
    badCalls=calls;badCalls.push_back(calls[0]);
    CHECK(!rejected.Prepare(blocks,2,badCalls.data(),badCalls.size(),&replacement,guard));
    CHECK(!strcmp(rejected.Status(),"call-overlap"));
    auto missing=replacement;missing.offset++;
    CHECK(!rejected.Prepare(blocks,2,calls.data(),calls.size(),&missing,guard));
    CHECK(!strcmp(rejected.Status(),"override-contract"));
    CHECK(!plain.Prepare(blocks,2,calls.data(),calls.size(),nullptr,guard));
    CHECK(!strcmp(plain.Status(),"already-prepared"));
    std::vector<Call> viewCallsFixture;
    const auto viewBytes=FunctionSize(reinterpret_cast<void*>(&ViewCaller),viewCallsFixture,0);
    CHECK(viewCallsFixture.empty());
    const auto viewHash=Hash(reinterpret_cast<void*>(&ViewCaller),viewBytes);
    const Block viewBlock{reinterpret_cast<void*>(&ViewCaller),viewBytes,viewHash.c_str()};
    const unsigned char spanBytes[]={0xff,0xd0,0x8b,0x08,0x83,0x39,0};
    size_t spanOffset=0;
    while(spanOffset+sizeof(spanBytes)<=viewBytes &&
          memcmp(static_cast<const unsigned char*>(viewBlock.source)+spanOffset,spanBytes,sizeof(spanBytes)))++spanOffset;
    CHECK(spanOffset+sizeof(spanBytes)<=viewBytes);
    CallSpan span{0,spanOffset,spanBytes,sizeof(spanBytes),reinterpret_cast<void*>(&ViewOwnerAdapter)};
    Code viewCode;CHECK(viewCode.Prepare(&viewBlock,1,nullptr,0,nullptr,guard,&span));
    using ViewFn=unsigned(__thiscall*)(Owner*,void**);
    for(unsigned i=0;i<1000;++i) {
        viewReference=i%2?&owner:nullptr;void* reference=nullptr;
        SetLastError(0x87654321);_fxsave(floatingBefore);
        CHECK(reinterpret_cast<ViewFn>(viewCode.Entry(0))(&owner,&reference)==(i%2));
        const auto viewError=GetLastError();_fxsave(floatingAfter);
        CHECK(reference==&viewReference && viewError==0x87654321);
        CHECK(!memcmp(floatingBefore,floatingAfter,sizeof(floatingBefore)));
    }
    CHECK(viewGetterCalls==1000 && viewOwnerCalls==1000);
    CHECK(Hash(viewBlock.source,viewBytes)==viewHash);
    Code refusedSpan;auto badSpan=span;badSpan.bytes=4;
    CHECK(!refusedSpan.Prepare(&viewBlock,1,nullptr,0,nullptr,guard,&badSpan));
    CHECK(!strcmp(refusedSpan.Status(),"span-range"));
    const auto* overlapExpected=static_cast<const unsigned char*>(blocks[0].source)+calls[0].offset;
    const CallSpan overlapSpan{0,calls[0].offset,overlapExpected,5,reinterpret_cast<void*>(&ViewOwnerAdapter)};
    CHECK(!refusedSpan.Prepare(blocks,2,calls.data(),calls.size(),nullptr,guard,&overlapSpan));
    CHECK(!strcmp(refusedSpan.Status(),"span-overlap"));
    badSpan=span;badSpan.offset++;
    CHECK(!refusedSpan.Prepare(&viewBlock,1,nullptr,0,nullptr,guard,&badSpan));
    CHECK(!strcmp(refusedSpan.Status(),"span-contract"));
    badSpan=span;badSpan.target=nullptr;
    CHECK(!refusedSpan.Prepare(&viewBlock,1,nullptr,0,nullptr,guard,&badSpan));
    CHECK(!strcmp(refusedSpan.Status(),"span-range"));
    Debug(false,&initialDebug);
    puts("PASS live-reference span adapter: 1000 original getters/observations, empty/nonempty CMP flags, out-reference, stack cleanup, x87/SSE/LastError, unchanged source and span refusals");
    puts("PASS x86 owned function clone: cross-block/external calls,10000 exact callbacks,unchanged source bytes,x87/SSE/LastError,active foreign data watches+DR3 preserved,RW-to-RX sealing,digest/plan refusals; own fixture only, NOT game compatibility");
    return 0;
}

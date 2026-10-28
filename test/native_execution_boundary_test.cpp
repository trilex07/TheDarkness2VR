#define NOMINMAX
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <xmmintrin.h>
#include "../src/native_execution_boundary.h"
#define CHECK(x) do {if(!(x)){std::printf("FAIL line=%d %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Owner {unsigned value=0x12345678,visits=0;};
static Owner owner;
static unsigned callbacks=0,observedFlags=0;
static unsigned __declspec(naked) Point() {__asm mov eax,[ecx] __asm ret}
static unsigned __declspec(naked) __cdecl Invoke(Owner*) {
    __asm push ebx
    __asm push esi
    __asm push edi
    __asm mov ecx,[esp+16]
    __asm mov ebx,0x11223344
    __asm mov esi,0x55667788
    __asm mov edi,0x99aabbcc
    __asm stc
    __asm call Point
    __asm pushfd
    __asm pop edx
    __asm mov dword ptr [observedFlags],edx
    __asm pop edi
    __asm pop esi
    __asm pop ebx
    __asm ret
}
static void AtPoint(const CONTEXT& context) {
    CHECK(context.Eip==reinterpret_cast<DWORD>(&Point));
    CHECK(context.Ecx==reinterpret_cast<DWORD>(&owner));
    CHECK(context.Ebx==0x11223344 && context.Esi==0x55667788 && context.Edi==0x99aabbcc);
    CHECK(context.EFlags&1);
    ++callbacks;++owner.visits;
    SetLastError(0xdeadbeef);
    _mm_setcsr(_mm_getcsr()^0x8000u);
    __asm fld1
    __asm fldpi
}
static DWORD WINAPI OtherThread(void* boundary) {
    CHECK(!static_cast<d2vr_native_execution::Boundary*>(boundary)->Stop());
    for(unsigned i=0;i<100;++i)CHECK(Invoke(&owner)==owner.value);
    return 0;
}
static bool UnmatchedException() {
    __try {RaiseException(EXCEPTION_SINGLE_STEP,0,0,nullptr);}
    __except(GetExceptionCode()==EXCEPTION_SINGLE_STEP?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH) {return true;}
    return false;
}
struct DebugSetup {HANDLE thread;DWORD value;bool ok=false;};
static DWORD WINAPI SetUnusedSlot(void* parameter) {
    auto& setup=*static_cast<DebugSetup*>(parameter);
    CHECK(SuspendThread(setup.thread)==0);
    CONTEXT context{};context.ContextFlags=CONTEXT_DEBUG_REGISTERS;
    if(GetThreadContext(setup.thread,&context)) {
        context.Dr1=setup.value;setup.ok=SetThreadContext(setup.thread,&context)!=FALSE;
    }
    CHECK(ResumeThread(setup.thread)==1);return 0;
}
static void UnusedSlot(DWORD value) {
    DebugSetup setup{OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_SUSPEND_RESUME,FALSE,GetCurrentThreadId()),value};
    CHECK(setup.thread);
    HANDLE worker=CreateThread(nullptr,0,SetUnusedSlot,&setup,0,nullptr);CHECK(worker);
    CHECK(WaitForSingleObject(worker,10000)==WAIT_OBJECT_0 && setup.ok);
    CloseHandle(worker);CloseHandle(setup.thread);
}
static unsigned TextHash() {
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS32*>(base+reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    auto section=IMAGE_FIRST_SECTION(pe);unsigned hash=2166136261u;
    for(unsigned i=0;i<pe->FileHeader.NumberOfSections;++i)if(section[i].Characteristics&IMAGE_SCN_CNT_CODE)
        for(unsigned j=0;j<section[i].Misc.VirtualSize;++j)hash=(hash^base[section[i].VirtualAddress+j])*16777619u;
    return hash;
}
int main() {
    auto base=GetModuleHandleW(nullptr);
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS32*>(reinterpret_cast<char*>(base)+reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const d2vr_native_image::Guard guard{true,reinterpret_cast<uintptr_t>(base),pe->OptionalHeader.SizeOfImage};
    const unsigned original=TextHash();
    d2vr_native_execution::Boundary boundary,second;
    CHECK(!boundary.Start(&owner,AtPoint,guard));
    CHECK(!boundary.Start(reinterpret_cast<void*>(&Point),AtPoint,{}));
    UnusedSlot(reinterpret_cast<DWORD>(&Point));
    CHECK(!boundary.Start(reinterpret_cast<void*>(&Point),AtPoint,guard));
    CHECK(!boundary.Installed());
    UnusedSlot(0);
    CHECK(boundary.Start(reinterpret_cast<void*>(&Point),AtPoint,guard));
    CHECK(boundary.Installed());
    CHECK(!second.Start(reinterpret_cast<void*>(&Point),AtPoint,guard));
    __declspec(align(16)) unsigned char before[512]{},after[512]{};
    __asm fld1
    __asm fldpi
    _fxsave(before);
    SetLastError(0x13572468);
    CHECK(Invoke(&owner)==owner.value);
    const DWORD error=GetLastError();
    _fxsave(after);
    CHECK(error==0x13572468 && (observedFlags&1));
    CHECK(!std::memcmp(before,after,sizeof(before)));
    __asm fninit
    CHECK(callbacks==1 && owner.visits==1);
    CHECK(UnmatchedException() && callbacks==1);
    HANDLE other=CreateThread(nullptr,0,OtherThread,&boundary,0,nullptr);CHECK(other);
    CHECK(WaitForSingleObject(other,10000)==WAIT_OBJECT_0);CloseHandle(other);
    CHECK(callbacks==1);
    LARGE_INTEGER begin{},end{},frequency{};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&begin);
    for(unsigned i=0;i<10000;++i)CHECK(Invoke(&owner)==owner.value);
    QueryPerformanceCounter(&end);
    const double microseconds=double(end.QuadPart-begin.QuadPart)*1e6/double(frequency.QuadPart)/10000;
    CHECK(callbacks==10001 && owner.visits==10001);
    UnusedSlot(reinterpret_cast<DWORD>(&Point));
    CHECK(boundary.Stop() && !boundary.Installed());
    CHECK(Invoke(&owner)==owner.value && callbacks==10001);
    CHECK(!boundary.Start(reinterpret_cast<void*>(&Point),AtPoint,guard)); // Stop preserved foreign slot1.
    UnusedSlot(0);
    for(unsigned i=0;i<20;++i) {
        CHECK(boundary.Start(reinterpret_cast<void*>(&Point),AtPoint,guard));
        CHECK(Invoke(&owner)==owner.value);
        CHECK(boundary.Stop());
    }
    CHECK(callbacks==10021 && original==TextHash());
    std::printf("PASS x86 execution boundary: 10000 exact instruction callbacks, ECX/EBX/ESI/EDI/EAX/flags/LastError/x87/SSE, occupied-slot refusal, unmatched exception passthrough, other thread bypass/Stop refusal, 20 rearm cycles, Stop and unchanged EXE text; synthetic mean %.3fus/hit (not game evidence)\n",microseconds);
    return 0;
}

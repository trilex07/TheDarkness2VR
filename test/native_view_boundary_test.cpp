#include <windows.h>
#include <intrin.h>
#include <xmmintrin.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <thread>
#include <vector>
#include "../src/native_view_boundary.h"
#include "../src/native_view_witness.h"
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static volatile LONG callbacks=0;
static void* expectedOwner=reinterpret_cast<void*>(0x12345678);
static void Observe(void* owner) {
    CHECK(owner==expectedOwner);InterlockedIncrement(&callbacks);
    SetLastError(999);
    _mm_setcsr(_mm_getcsr()^0x6000);
    __asm fld1 // deliberately leave a dirty x87 stack; interception must restore it
}
static uint64_t TextHash(HMODULE module) {
    auto image=reinterpret_cast<unsigned char*>(module);
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS32*>(image+reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew);
    auto section=IMAGE_FIRST_SECTION(pe);uint64_t hash=14695981039346656037ull;
    for(unsigned n=0;n<pe->FileHeader.NumberOfSections;++n)if(section[n].Characteristics&IMAGE_SCN_MEM_EXECUTE)
        for(unsigned i=0;i<section[n].Misc.VirtualSize;++i)hash=(hash^image[section[n].VirtualAddress+i])*1099511628211ull;
    return hash;
}
int main(int argc,char** argv) {
    d2vr_native_view::ProjectionWitness witness;
    const float inverse[16]={1,0,0,0,0,2,0,0,0,0,0,-10,0,0,1,10};
    witness.Begin(10,1,2,3,inverse);
    CHECK(!witness.CanCover(11,1,2,3));
    CHECK(!witness.Observe(9,true,inverse) && !witness.Observe(10,false,inverse));
    float wrong[16];memcpy(wrong,inverse,sizeof(wrong));wrong[5]=3;
    CHECK(!witness.Observe(10,true,wrong));
    CHECK(witness.Observe(10,true,inverse));
    CHECK(witness.CanCover(11,1,2,3));
    CHECK(!witness.CanCover(10,1,2,3) && !witness.CanCover(12,1,2,3));
    CHECK(!witness.CanCover(11,4,2,3) && !witness.CanCover(11,1,4,3) && !witness.CanCover(11,1,2,4));
    witness.Begin(11,1,2,3,inverse);CHECK(witness.Observe(11,true,inverse));
    CHECK(witness.CanCover(12,1,2,3));
    d2vr_native_view::ProjectionWitness early;
    CHECK(!early.Observe(20,true,inverse));
    early.Begin(20,1,2,3,inverse);
    CHECK(early.CanCover(21,1,2,3));
    early.Begin(21,1,2,3,inverse);
    CHECK(!early.CanCover(22,1,2,3)); // previous-frame upload cannot corroborate
    CHECK(!early.Observe(21,false,inverse));
    CHECK(!early.Observe(21,true,wrong));
    CHECK(!early.CanCover(22,1,2,3));
    CHECK(early.Observe(21,true,inverse));
    CHECK(early.CanCover(22,1,2,3));
    d2vr_native_view::ProjectionWitness mismatch;
    CHECK(!mismatch.Observe(30,true,wrong));
    mismatch.Begin(30,1,2,3,inverse);
    CHECK(!mismatch.CanCover(31,1,2,3));
    CHECK(argc==2);auto fixture=LoadLibraryA(argv[1]);CHECK(fixture);
    using Invoke=BOOL(__cdecl*)(LARGE_INTEGER*,void*);
    auto known=reinterpret_cast<Invoke>(GetProcAddress(fixture,"InvokeKnown"));CHECK(known);
    auto other=reinterpret_cast<Invoke>(GetProcAddress(fixture,"InvokeOther"));CHECK(other);
    auto caller=reinterpret_cast<uintptr_t(__cdecl*)()>(GetProcAddress(fixture,"FixtureCaller"));CHECK(caller);
    auto calls=reinterpret_cast<LONG(__cdecl*)()>(GetProcAddress(fixture,"FixtureCalls"));CHECK(calls);
    auto target=GetProcAddress(fixture,"_FixtureQuery@4");CHECK(target);
    LARGE_INTEGER output{};CHECK(known(&output,expectedOwner));const auto selected=caller();
    auto image=GetModuleHandleW(nullptr);
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS32*>(reinterpret_cast<char*>(image)+reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew);
    d2vr_native_image::Guard guard{true,reinterpret_cast<uintptr_t>(image),pe->OptionalHeader.SizeOfImage};
    const auto originalText=TextHash(image);
    d2vr_native_view::Boundary gate;
    CHECK(!gate.Start(target,selected,Observe,{}));
    CHECK(!gate.Start(reinterpret_cast<void*>(&Observe),selected,Observe,guard));
    CHECK(gate.Start(target,selected,Observe,guard));CHECK(gate.Installed());
    unsigned short controlBefore=0,statusBefore=0,controlAfter=0,statusAfter=0;
    __asm fnstcw controlBefore
    __asm fnstsw statusBefore
    const auto mxcsr=_mm_getcsr();
    CHECK(known(&output,expectedOwner));CHECK(GetLastError()==0x13572468);
    __asm fnstcw controlAfter
    __asm fnstsw statusAfter
    CHECK(controlBefore==controlAfter && statusBefore==statusAfter && _mm_getcsr()==mxcsr);
    CHECK(output.QuadPart==0x123456789abcdefLL && callbacks==1 && calls()==2);
    CHECK(!known(nullptr,expectedOwner));CHECK(GetLastError()==0x13572468 && callbacks==1 && calls()==3);
    CHECK(other(&output,reinterpret_cast<void*>(0xdeadbeef)));CHECK(callbacks==1 && calls()==4);
    std::vector<std::thread> threads;
    for(int t=0;t<4;++t)threads.emplace_back([=]{for(int i=0;i<1000;++i){LARGE_INTEGER value{};
        CHECK(known(&value,expectedOwner));CHECK(value.QuadPart==0x123456789abcdefLL && GetLastError()==0x13572468);
        CHECK(other(&value,nullptr));CHECK(GetLastError()==0x13572468);}});
    for(auto& thread:threads)thread.join();
    CHECK(callbacks==4001 && calls()==8004 && TextHash(image)==originalText);
    puts("PASS x86 external boundary: exact ESI/caller, API once, BOOL/output/LastError, x87/SSE, concurrent bypass, EXE text unchanged");
    return 0;
}

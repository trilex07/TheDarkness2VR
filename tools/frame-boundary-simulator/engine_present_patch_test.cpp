#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../../src/diagnostic_present_patch.h"
#include "../../src/capture_present_witness.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace d2vr_present_patch;
struct FakeMemory {
    unsigned char data[5]; int flushFailures=0, restoreFailures=0;bool deny=false;
    FakeMemory(){memcpy(data,original,5);}
    unsigned char* bytes(){return data;}
    bool writable(){return !deny;}
    bool flush(){if(flushFailures){--flushFailures;return false;}return true;}
    bool restore(){if(restoreFailures){--restoreFailures;return false;}return true;}
};
struct RealMemory {
    unsigned char* data;DWORD old=0;
    unsigned char* bytes(){return data;}
    bool writable(){return VirtualProtect(data,5,PAGE_EXECUTE_READWRITE,&old)!=FALSE;}
    bool flush(){return FlushInstructionCache(GetCurrentProcess(),data,5)!=FALSE;}
    bool restore(){DWORD unused;return VirtualProtect(data,5,old,&unused)!=FALSE;}
};
static Dispatch dispatch;
static long desired=0;static int calls=0;static bool recurse=false;
using Fn=long (__stdcall*)(unsigned,unsigned,unsigned,unsigned,unsigned,unsigned);
static long __stdcall Wrapper(unsigned,unsigned,unsigned,unsigned,unsigned,unsigned);
static long __stdcall Original(unsigned a,unsigned b,unsigned c,unsigned d,unsigned e,unsigned f) {
    assert(a==11 && b==22 && c==33 && d==44 && e==55 && f==66);++calls;
    if(recurse) return Wrapper(a,b,c,d,e,f);
    return desired;
}
static long __stdcall Wrapper(unsigned a,unsigned b,unsigned c,unsigned d,unsigned e,unsigned f) {
    if(!dispatch.Enter()) return static_cast<long>(0x8876086c);
    ++dispatch.forwarded;long hr=Original(a,b,c,d,e,f);dispatch.Returned(static_cast<int32_t>(hr));
    dispatch.Leave();return hr;
}
int main(){
    float native[16]={.864337f,0,0,0,0,.57735f,0,0,0,0,0,-10.525f,0,0,1,10.526f};
    float uploaded[16];memcpy(uploaded,native,sizeof(native));uploaded[0]*=2;
    assert(d2vr_capture::ProjectionUpload(native,uploaded,8,4,8,0));
    assert(!d2vr_capture::ProjectionUpload(native,uploaded,8,3,8,0));
    assert(!d2vr_capture::ProjectionUpload(native,uploaded,9,4,8,0));
    assert(!d2vr_capture::ProjectionUpload(native,uploaded,8,4,-1,0));
    assert(!d2vr_capture::ProjectionUpload(native,uploaded,8,4,8,-1));
    assert(!d2vr_capture::ProjectionUpload(native,uploaded,224,4,224,0));
    uploaded[0]=NAN;assert(!d2vr_capture::ProjectionUpload(native,uploaded,8,4,8,0));
    static_assert(sizeof(void*)==4,"Real x86 stdcall test required");
    const char* sha="d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1";
    assert(Identity(sha,0x14c,0x4f68a875,0xdec000,0x400000,0x10b));
    assert(!Identity("unknown",0x14c,0x4f68a875,0xdec000,0x400000,0x10b));
    assert(!Identity(sha,0x8664,0x4f68a875,0xdec000,0x400000,0x10b));
    for(int failure=0;failure<4;++failure){
        FakeMemory m;Patch p;
        if(failure==0)m.deny=true;
        if(failure==1)m.flushFailures=1;
        if(failure==2)m.restoreFailures=1;
        if(failure==3)m.flushFailures=2;
        assert(!p.Install(m,0xa94052,0x50000000));
        assert(!memcmp(m.data,original,5));assert(!p.installed);
        assert(p.rollbackUncertain==(failure==3));
    }
    {FakeMemory m;Patch p;m.data[0]=0xcc;assert(!p.Install(m,1,2));assert(m.data[0]==0xcc);}
    {FakeMemory m;Patch p;assert(p.Install(m,0xa94052,0x50000000));
     assert(!p.Install(m,0xa94052,0x50000000));m.data[1]^=1;
     assert(!p.Remove(m));assert(p.installed);}
    // Execute the exact replaced instruction shape on a private synthetic page.
    // Pushes six args before MOV EAX,[ECX+0C]; CALL EAX, just like the game.
    void* vt[4]={nullptr,nullptr,nullptr,reinterpret_cast<void*>(&Original)};
    std::vector<unsigned char> code;
    auto imm=[&](unsigned char op,uint32_t value){code.push_back(op);auto b=reinterpret_cast<unsigned char*>(&value);code.insert(code.end(),b,b+4);};
    for(unsigned value:{66u,55u,44u,33u,22u,11u})imm(0x68,value);
    imm(0xb9,reinterpret_cast<uint32_t>(vt));
    size_t offset=code.size();code.insert(code.end(),original,original+5);code.push_back(0xc3);
    auto page=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));assert(page);
    memcpy(page,code.data(),code.size());DWORD old;assert(VirtualProtect(page,4096,PAGE_EXECUTE_READ,&old));
    assert(FlushInstructionCache(GetCurrentProcess(),page,code.size()));
    auto run=reinterpret_cast<long (__cdecl*)()>(page);
    desired=123;assert(run()==123 && calls==1);
    RealMemory memory{page+offset};Patch patch;
    assert(patch.Install(memory,reinterpret_cast<uint32_t>(page+offset),reinterpret_cast<uint32_t>(&Wrapper)));
    for(long hr:{0L,static_cast<long>(0x88760868),static_cast<long>(0x08760878)}) {
        desired=hr;int prior=calls;assert(run()==hr && calls==prior+1);
    }
    assert(dispatch.entries==3 && dispatch.forwarded==3 && dispatch.returned==3);
    assert(dispatch.ok==1 && dispatch.statuses==1 && dispatch.errors==1 && dispatch.fault);
    // An isolated error at entry 17 must not disappear between 1000-call logs.
    Dispatch status;status.entries=17;
    assert(!status.Returned(0));
    assert(status.Returned(0x08760878) && !status.fault);
    assert(status.Returned(static_cast<int32_t>(0x88760868)) && status.fault);
    assert(status.Returned(0) && status.fault); // Successful retry cannot rearm capture.
    assert(status.returned==4 && status.ok==2 && status.statuses==1 && status.errors==1);
    recurse=true;int prior=calls;assert(run()==static_cast<long>(0x8876086c) && calls==prior+1);
    assert(dispatch.recursive==1 && dispatch.fault && !dispatch.active);
    recurse=false;assert(patch.Remove(memory));assert(!memcmp(memory.bytes(),original,5));
    desired=456;assert(run()==456);assert(VirtualFree(page,0,MEM_RELEASE));
    d2vr_capture::PresentWitness witness;
    assert(witness.Enter("SwapChainPresent",1,2,3,10));witness.Leave();
    assert(witness.InCaptureWindow(2,0,11,3,10));
    assert(!witness.Matches(2,0,11,3));
    assert(witness.Enter("SwapChainPresent",1,2,3,11));assert(witness.Matches(2,0,11,3));
    witness.Reset();assert(!witness.Matches(2,0,11,3));
    puts("PASS: exact x86 call-site/stdcall arguments and HR, once-only forwarding, recursive cycle rejection, identity/bytes, rollback failures, two-phase witness");
}

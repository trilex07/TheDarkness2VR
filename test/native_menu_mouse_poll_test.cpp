#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include "../src/native_menu_mouse_poll.h"
using namespace d2vr_menu_native;
static unsigned checks,nativeCalls,dispatchCalls;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL %d %s\n",__LINE__,#x);std::exit(1);}}while(0)
static uintptr_t seenOwner,seenCaller,expectedCaller;
static void* expectedDevice;static MouseEvent* expectedEvents;static uint32_t* expectedCount;
static HRESULT nativeResult=S_OK;
static HRESULT STDMETHODCALLTYPE Native(void* device,uint32_t bytes,void* events,uint32_t* count,uint32_t flags){
    ++nativeCalls;CHECK(device==expectedDevice&&bytes==20&&events==expectedEvents&&count==expectedCount&&flags==0);
    CHECK(*count==256);*count=2;SetLastError(0xabcdef01);return nativeResult;
}
static HRESULT STDMETHODCALLTYPE Dispatch(void* device,uint32_t bytes,void* events,uint32_t* count,uint32_t flags,uintptr_t caller,uintptr_t owner){
    ++dispatchCalls;seenOwner=owner;seenCaller=caller;
    return Native(device,bytes,events,count,flags);
}
D2VR_MOUSE_DATA_GATE(Gateway,Dispatch)
// The driver double has no context knowledge. Only the actual source gateway
// can capture ESI before Dispatch's optimized prologue reuses registers.
// Taking the assembly return-label address disables global optimization only
// in this harness function; production Dispatch and Gateway stay unchanged.
#pragma warning(push)
#pragma warning(disable:4740)
static __declspec(noinline) HRESULT Invoke(uintptr_t owner,uint32_t& before,uint32_t& after){
    HRESULT result;
    __asm {
        push esi
        push ebx
        push edi
        mov esi,owner
        mov ebx,0x13572468
        mov edi,0x24681357
        mov eax,before
        mov [eax],esp
        mov eax,offset returned
        mov expectedCaller,eax
        push 0
        push expectedCount
        push expectedEvents
        push 20
        push expectedDevice
        call Gateway
    returned:
        mov result,eax
        mov eax,after
        mov [eax],esp
        cmp ebx,0x13572468
        jne failed
        cmp edi,0x24681357
        jne failed
        cmp esi,owner
        je done
    failed:
        mov result,0x80004005
    done:
        pop edi
        pop ebx
        pop esi
    }
    return result;
}
#pragma warning(pop)
int main(){
    const uint32_t owner=0x100000,window=0x1234;
    std::unordered_map<uint32_t,uint32_t> memory={{owner,0xFC00C8},{owner+0x1A4,0x200000},
        {owner+0x1A8,0x300000},{owner+0x1BC,256},{owner+0x1C0,20},{owner+0x1C4,owner+0x1F4},
        {owner+0x1C8,owner+0x208},{owner+0x1E8,window},{owner+0x1CC,0}};
    const auto read=[&](uint32_t a,uint32_t& v){auto it=memory.find(a);if(it==memory.end())return false;v=it->second;return true;};
    MousePollCall call{0x9A5E70,owner,0x200000,20,0x300000,0x400000,0,256,7};
    MousePollView good,actual;CHECK(AdmitMousePoll(call,window,read,good));
    CHECK(good.owner==owner&&good.immediate==owner+0x1F4&&good.events==call.events&&good.capacity==256);
    for(auto& field:memory){const uint32_t saved=field.second;field.second=saved^4;actual=good;
        CHECK(!AdmitMousePoll(call,window,read,actual));CHECK(actual.owner==good.owner&&actual.immediate==good.immediate);
        field.second=saved;
    }
    const auto fields=memory; // Iterate a stable copy while removing live fields.
    for(const auto& field:fields){memory.erase(field.first);actual=good;CHECK(!AdmitMousePoll(call,window,read,actual));
        CHECK(actual.owner==good.owner);memory.insert(field);}
    for(unsigned kind=0;kind<15;++kind){auto bad=call;
        switch(kind){case 0:bad.caller++;break;case 1:bad.owner=0;break;case 2:bad.owner++;break;
        case 3:bad.owner=0xfffffffcu;break;case 4:bad.device=0;break;case 5:bad.eventBytes=16;break;
        case 6:bad.events=0;break;case 7:bad.events=0xfffffffcu;break;case 8:bad.countAddress=0;break;
        case 9:bad.countAddress++;break;case 10:bad.flags=1;break;case 11:bad.capacityAtEntry=0;break;
        case 12:bad.capacityAtEntry=257;break;case 13:bad.returned=257;break;case 14:bad.device++;break;}
        actual=good;CHECK(!AdmitMousePoll(bad,window,read,actual));CHECK(actual.owner==good.owner&&actual.countAddress==good.countAddress);
    }
    CHECK(!AdmitMousePoll(call,0,read,actual));CHECK(!AdmitMousePoll(call,window+1,read,actual));
    MouseEvent events[256]{};uint32_t count;
    expectedDevice=reinterpret_cast<void*>(0x123400);expectedEvents=events;expectedCount=&count;
    for(HRESULT status:{S_OK,S_FALSE,HRESULT(0x8007001eu)}){
        nativeResult=status;
        for(unsigned i=0;i<1000;++i){count=256;uint32_t before=0,after=0;
            CHECK(Invoke(0x654320,before,after)==status);CHECK(before==after);
            CHECK(count==2&&seenOwner==0x654320&&seenCaller==expectedCaller);CHECK(GetLastError()==0xabcdef01);
        }
    }
    CHECK(nativeCalls==3000&&dispatchCalls==3000);
    std::printf("PASS actual x86 native mouse poll gate: %u checks; fresh ESI/caller, stdcall stack, HRESULT/LastError, exact args/original once; context refusal retains output\n",checks);
}

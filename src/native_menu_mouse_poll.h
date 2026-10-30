#pragma once
#include <cstdint>
#include "native_menu_mouse_merge.h"
namespace d2vr_menu_native {
// A descriptor of the CURRENT GetDeviceData call, not an object cache. The
// owner/caller are captured before the C++ prologue by D2VR_MOUSE_DATA_GATE.
struct MousePollCall {
    uint32_t caller=0,owner=0,device=0,eventBytes=0,events=0,countAddress=0,flags=0;
    uint32_t capacityAtEntry=0,returned=0;
};
struct MousePollView {
    uint32_t owner=0,immediate=0,events=0,countAddress=0,capacity=0;
};
// Pin the complete 9A5D90 method and 821400 constructor before invoking this
// admission. A caller address alone is not image admission. Only use the view
// synchronously inside that native API call, before returning to 9A5E70.
// Do not call arbitrary virtual methods or retain the returned addresses.
template<class Read> bool AdmitMousePoll(const MousePollCall& call,uint32_t window,
                                        Read read,MousePollView& output) {
    if(call.caller!=0x9A5E70 || call.eventBytes!=20 || call.flags || !window ||
       call.owner<0x10000 || (call.owner&3) || call.owner>UINT32_MAX-0x244 ||
       call.device<0x10000 || (call.device&3) || call.events<0x10000 ||
       (call.events&3) || call.countAddress<0x10000 || (call.countAddress&3) ||
       call.countAddress>UINT32_MAX-3 || !call.capacityAtEntry ||
       call.capacityAtEntry>256 || call.returned>call.capacityAtEntry ||
       call.events>UINT32_MAX-call.capacityAtEntry*20)return false;
    uint32_t table=0,device=0,events=0,capacity=0,bytes=0,current=0,previous=0,hwnd=0,index=0;
    if(!read(call.owner,table) || table!=0xFC00C8 ||
       !read(call.owner+0x1A4,device) || device!=call.device ||
       !read(call.owner+0x1A8,events) || events!=call.events ||
       !read(call.owner+0x1BC,capacity) || capacity!=call.capacityAtEntry ||
       !read(call.owner+0x1C0,bytes) || bytes!=20 ||
       !read(call.owner+0x1C4,current) || current!=call.owner+0x1F4 ||
       !read(call.owner+0x1C8,previous) || previous!=call.owner+0x208 ||
       !read(call.owner+0x1E8,hwnd) || hwnd!=window ||
       !read(call.owner+0x1CC,index) || index)return false;
    output={call.owner,current,events,call.countAddress,capacity};return true;
}
}

// Public x86 GetDeviceData ABI is stdcall(this, eventBytes, events, count, flags).
// Impl adds the unmodified return address and ESI as its final two arguments.
// Its RET 28 removes only our copied args; RET 20 consumes the public API args.
// Original nonvolatile registers remain owned by the compiler/real native API.
#if defined(_MSC_VER) && defined(_M_IX86)
#define D2VR_MOUSE_DATA_GATE(Name,Impl) \
    static __declspec(naked) HRESULT STDMETHODCALLTYPE Name( \
        void*,uint32_t,void*,uint32_t*,uint32_t) { \
        __asm mov eax,esp \
        __asm push esi \
        __asm push dword ptr [eax] \
        __asm push dword ptr [eax+20] \
        __asm push dword ptr [eax+16] \
        __asm push dword ptr [eax+12] \
        __asm push dword ptr [eax+8] \
        __asm push dword ptr [eax+4] \
        __asm call Impl \
        __asm ret 20 \
    }
#endif

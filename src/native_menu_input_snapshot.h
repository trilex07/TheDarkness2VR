#pragma once
#include <cstdint>

namespace d2vr_menu_native {
// Pinned D46 native contracts, observed without retaining or mutating objects.
// A stable pair of snapshots is not a lifetime lease or input admission.
struct Snapshot {
    uint32_t framework=0,clientRef=0,client=0,inputRef=0,input=0;
    uint32_t mouseRef=0,mouse=0,device=0,deviceTable=0,getState=0,getData=0;
    bool operator==(const Snapshot& b)const {
        return framework==b.framework && clientRef==b.clientRef && client==b.client &&
            inputRef==b.inputRef && input==b.input && mouseRef==b.mouseRef && mouse==b.mouse &&
            device==b.device && deviceTable==b.deviceTable && getState==b.getState && getData==b.getData;
    }
};
inline bool Pointer(uint32_t p){return p>=0x10000 && !(p&3) && p<=UINT32_MAX-4;}
template<class Read> bool Observe(uint32_t hwnd,uint32_t framework,Read read,Snapshot& output) {
    Snapshot s;s.framework=framework;
    const auto field=[&](uint32_t base,uint32_t offset,uint32_t& value) {
        return Pointer(base) && offset<=UINT32_MAX-base-3 && read(base+offset,value);
    };
    uint32_t table=0,mirror=0;
    if(!hwnd || !field(framework,0,table) || table!=0xFC8930 ||
       !field(framework,0xF8,mirror) || mirror!=hwnd ||
       !field(framework,0xA8,s.clientRef) || !field(s.clientRef,0,s.client) ||
       !field(s.client,0x374,s.inputRef) || !field(s.inputRef,0,s.input) ||
       !field(s.input,0,table) || table!=0xFBFCD0 ||
       !field(s.input,0xA44,s.mouseRef) || !field(s.mouseRef,0,s.mouse) ||
       !field(s.mouse,0,table) || table!=0xFC00C8 ||
       !field(s.mouse,0x1A4,s.device) || !field(s.device,0,s.deviceTable) ||
       !field(s.deviceTable,0x24,s.getState) || !field(s.deviceTable,0x28,s.getData) ||
       !s.getState || !s.getData)return false;
    output=s;return true;
}
}

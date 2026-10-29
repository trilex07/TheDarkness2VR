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
struct PauseSnapshot {
    uint32_t manager=0,rulesRef=0,rules=0,table=0,menuRef=0,menu=0;
    uint8_t suppressed=0;
    bool showing=false;
    bool operator==(const PauseSnapshot& b)const {
        return manager==b.manager && rulesRef==b.rulesRef && rules==b.rules &&
            table==b.table && menuRef==b.menuRef && menu==b.menu &&
            suppressed==b.suppressed && showing==b.showing;
    }
};
// Read-only equivalent of pinned A4D340, reached through RegionMgr::GetGameRules.
// The method may be inherited by a derived class; require its exact slot value,
// never assume all GameRules subclasses use this predicate. No lifetime lease.
template<class Read> bool ObservePause(uint32_t manager,Read read,PauseSnapshot& output) {
    PauseSnapshot s;s.manager=manager;
    const auto field=[&](uint32_t base,uint32_t offset,uint32_t& value) {
        return Pointer(base) && offset<=UINT32_MAX-base-3 && read(base+offset,value);
    };
    uint32_t table=0,getter=0,flags=0;
    if(!field(manager,0,table) || table!=0xFC7D00 ||
       !field(table,0x13C,getter) || getter!=0x490D30 ||
       !field(manager,0xBC,s.rulesRef) || !field(s.rulesRef,0,s.rules) ||
       !field(s.rules,0,s.table) || !field(s.table,0x80,getter) || getter!=0xA4D340 ||
       !field(s.rules,0x198,flags))return false;
    s.suppressed=uint8_t(flags);
    // Preserve the native short circuit: the menu reference need not be read
    // while the low byte at +198 is nonzero. Neighboring bytes are unrelated.
    if(!s.suppressed) {
        if(!field(s.rules,0xEC,s.menuRef) || !field(s.menuRef,0,s.menu))return false;
        s.showing=s.menu!=0;
    }
    output=s;return true;
}
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

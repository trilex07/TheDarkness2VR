#pragma once
#include <cstdint>

namespace d2vr_menu_native {
// Pinned D46 native contracts, observed without retaining or mutating objects.
// A stable pair of snapshots is not a lifetime lease or input admission.
struct Snapshot {
    uint32_t framework=0,clientRef=0,client=0,inputRef=0,input=0,inputTable=0;
    uint32_t mouseRef=0,mouse=0,device=0,deviceTable=0,getState=0,getData=0;
    bool operator==(const Snapshot& b)const {
        return framework==b.framework && clientRef==b.clientRef && client==b.client &&
            inputRef==b.inputRef && input==b.input && inputTable==b.inputTable && mouseRef==b.mouseRef && mouse==b.mouse &&
            device==b.device && deviceTable==b.deviceTable && getState==b.getState && getData==b.getData;
    }
};
// Both pinned Windows classes use +A44 as the same intrusive mouse reference.
// FBFAD8 is constructed by 4CE810 and initializes it through 8460E0;
// FBFCD0's 479EB0 initializer calls that same base initializer first.
inline bool InputTable(uint32_t table){return table==0xFBFAD8 || table==0xFBFCD0;}
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
// Borrowed FlashInstanceImpl only: caller must own the object on its native
// thread for the entire read. This observation never establishes that lease.
struct MovieSnapshot {
    uint32_t movie=0,container=0,managerRef=0,manager=0,topEnd=0,topRef=0,ownRef=0;
    bool visible=false,topmost=false;
    bool operator==(const MovieSnapshot& b)const {
        return movie==b.movie && container==b.container && managerRef==b.managerRef &&
            manager==b.manager && topEnd==b.topEnd && topRef==b.topRef && ownRef==b.ownRef &&
            visible==b.visible && topmost==b.topmost;
    }
};
// Read equivalents of IsVisible (76D5C0), IsTopmost (6E4250), and the concrete
// manager's top-reference getter (A0B120). Pin those complete methods first.
// Distinct intrusive references whose objects are both null compare equal in
// the native method. Expose that result faithfully; it is NOT input admission.
template<class Read> bool ObserveMovie(uint32_t movie,Read read,MovieSnapshot& output) {
    MovieSnapshot s;s.movie=movie;
    const auto field=[&](uint32_t base,uint32_t offset,uint32_t& value) {
        return Pointer(base) && offset<=UINT32_MAX-base-3 && read(base+offset,value);
    };
    uint32_t table=0,method=0,flags=0;
    if(!field(movie,0,table) || table!=0xFDA6E8 ||
       !field(table,0xFC,method) || method!=0x76D5C0 ||
       !field(table,0x104,method) || method!=0x6E4250 ||
       !field(movie,0x78,flags))return false;
    s.visible=uint8_t(flags>>8)!=0;
    if(!field(movie,0x44,s.container) || !field(s.container,0x44,s.managerRef) ||
       !field(s.managerRef,0,s.manager) || !field(s.manager,0,table) || table!=0xFDA948 ||
       !field(table,0xA8,method) || method!=0xA0B120 ||
       !field(s.manager,0x5C,s.topEnd) || !Pointer(s.topEnd) || s.topEnd<0x10004 ||
       !read(s.topEnd-4,s.topRef) || !Pointer(s.topRef) ||
       !field(movie,0xC,s.ownRef) || !Pointer(s.ownRef))return false;
    if(s.topRef==s.ownRef)s.topmost=true;
    else {
        uint32_t top=0,own=0;
        if(!field(s.topRef,0,top) || !field(s.ownRef,0,own))return false;
        s.topmost=(top|own)==0;
    }
    output=s;return true;
}
// Root used by C98BD0's singleton creation/read path and cleared by BB87C0.
// Read-only observation only; never call that factory merely to find a movie.
struct TopMovieSnapshot {
    uint32_t managerRef=0,manager=0,begin=0,end=0,topRef=0,topMovie=0;
    MovieSnapshot movie;
    bool cursorRequested=false,hasMovie=false;
    bool operator==(const TopMovieSnapshot& b)const {
        return managerRef==b.managerRef && manager==b.manager && begin==b.begin &&
            end==b.end && topRef==b.topRef && topMovie==b.topMovie && movie==b.movie &&
            cursorRequested==b.cursorRequested && hasMovie==b.hasMovie;
    }
};
template<class Read> bool ObserveTopMovie(Read read,TopMovieSnapshot& output) {
    TopMovieSnapshot s;
    const auto field=[&](uint32_t base,uint32_t offset,uint32_t& value) {
        return Pointer(base) && offset<=UINT32_MAX-base-3 && read(base+offset,value);
    };
    uint32_t table=0,method=0,flags=0;
    if(!read(0x10B389C,s.managerRef) || !field(s.managerRef,0,s.manager) ||
       !field(s.manager,0,table) || table!=0xFDA948 ||
       !field(table,0xA8,method) || method!=0xA0B120 ||
       !field(s.manager,0x58,s.begin) || !field(s.manager,0x5C,s.end) ||
       (s.begin && !Pointer(s.begin)) || s.end<s.begin || ((s.end-s.begin)&3))return false;
    if(s.begin!=s.end) {
        if(!Pointer(s.end) || !read(s.end-4,s.topRef) ||
           !field(s.topRef,0,s.topMovie) || !Pointer(s.topMovie) ||
           !ObserveMovie(s.topMovie,read,s.movie) || s.movie.manager!=s.manager ||
           !field(s.topMovie,0,table) || !field(table,0x130,method) || method!=0xB64910 ||
           !field(s.topMovie,0xF4,flags))return false;
        // B64910 returns this exact byte. It requests native mouse visibility;
        // neither this byte nor topmost/visible proves an input lifetime lease.
        s.cursorRequested=uint8_t(flags)!=0;s.hasMovie=true;
    }
    output=s;return true;
}
// A refusal receipt identifies the first failed read/type/identity check. It
// conveys no admission and never publishes a partially resolved receiver.
struct Refusal {
    const char* stage="none";
    uint32_t address=0,observed=0,expected=0;
    bool readable=false;
};
// ownedFrameworkTable must come from our pinned immutable framework clone,
// never from an observed unknown instance. All other graph/type checks remain.
template<class Read> bool Observe(uint32_t hwnd,uint32_t framework,Read read,Snapshot& output,
                                  uint32_t ownedFrameworkTable=0,Refusal* refusal=nullptr) {
    Snapshot s;s.framework=framework;
    Refusal failure;
    const auto field=[&](const char* stage,uint32_t base,uint32_t offset,uint32_t& value,
                         uint32_t expected=0) {
        failure={stage,0,0,expected,false};
        if(!Pointer(base) || offset>UINT32_MAX-base-3)return false;
        failure.address=base+offset;
        if(!read(failure.address,value))return false;
        failure.readable=true;failure.observed=value;
        return true;
    };
    uint32_t table=0,mirror=0;
    failure.stage="window";
    if(!hwnd || !field("framework-type",framework,0,table,0xFC8930) ||
       (table!=0xFC8930 && (!ownedFrameworkTable || table!=ownedFrameworkTable)) ||
       !field("window-mirror",framework,0xF8,mirror,hwnd) || mirror!=hwnd ||
       !field("client-reference",framework,0xA8,s.clientRef) ||
       !field("client",s.clientRef,0,s.client) ||
       !field("input-reference",s.client,0x374,s.inputRef) ||
       !field("input",s.inputRef,0,s.input) ||
       !field("input-type",s.input,0,s.inputTable,0xFBFCD0) || !InputTable(s.inputTable) ||
       !field("mouse-reference",s.input,0xA44,s.mouseRef) ||
       !field("mouse",s.mouseRef,0,s.mouse) ||
       !field("mouse-type",s.mouse,0,table,0xFC00C8) || table!=0xFC00C8 ||
       !field("direct-input-device",s.mouse,0x1A4,s.device) ||
       !field("direct-input-table",s.device,0,s.deviceTable) ||
       !field("get-device-state",s.deviceTable,0x24,s.getState) || !s.getState ||
       !field("get-device-data",s.deviceTable,0x28,s.getData) || !s.getData) {
        if(refusal)*refusal=failure;
        return false;
    }
    output=s;if(refusal)*refusal={};return true;
}
}

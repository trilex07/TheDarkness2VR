#include "native_action_movies.h"
static std::atomic<uint64_t> g_nativeActionMovies{0},g_nativeVrMenu{0};
static unsigned NativeVrMenuFlags(){const auto v=g_nativeVrMenu.load(std::memory_order_acquire);const auto now=GetTickCount64();return now>=(v>>8) && now-(v>>8)<150 ? unsigned(v&255):0;}
static bool NativeVrMainMenu(){return (NativeVrMenuFlags()&d2vr_action_movies::Main)!=0;}
static bool NativeMenuPresentationActive(){return NativeVrMenuFlags()!=0 || MenuConfirmActive();}
static unsigned NativeActionMovies() {
    const auto value=g_nativeActionMovies.load(std::memory_order_acquire);
    const auto now=GetTickCount64();
    return now>=(value>>8) && now-(value>>8)<150 ? unsigned(value&255):0;
}
static void ObserveNativeActionMovies() {
#ifdef D2VR_MATCHED_CAPTURE
    if(!g_nativePlayerOwnerAdmitted || !g_nativeImageGuard.enabled || !g_imagePreservingInput ||
       !g_trackedFrame.open || GetCurrentThreadId()!=g_renderThreadId)return;
    DWORD process=0;
    if(GetWindowThreadProcessId(g_renderSizeHwnd,&process)!=GetCurrentThreadId() || process!=GetCurrentProcessId())return;
    static uint64_t frame=0;if(frame==g_trackedFrame.id)return;frame=g_trackedFrame.id;
    // These are the actual native definition getter and concrete metadata types.
    static int contract=0;
    if(!contract) {
        contract=-1;
        if(d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0x8E4FC0),18,"bb77a59d47af5ca8e2d0670a5d9192108d460b536c61fc42e050d42298291e55") &&
           d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0xFDAFB8),168,"7b4701b997e666947a3e7ca7b0dab90b8e0a839053ac749c2850d147352f50ba") &&
           d2vr_native_clone::MatchesDigest(reinterpret_cast<void*>(0xF6B40C),48,"79b573835eec01de2f7e874a897c27d79c0b5da3945658332d28be020ccbcc1c"))contract=1;
        Log("[ActionMovies] contract=%d; retained definitions, exact basename, conservative input protection",contract);
    }
    if(contract!=1)return;
    void* managerRef=nullptr;BYTE* manager=nullptr;unsigned flags=0,menu=0;
    void* movieRefs[64]{};void* definitions[64]{};unsigned count=0;
    __try {
        managerRef=*reinterpret_cast<void**>(0x10B389C);
        manager=static_cast<BYTE*>(RetainWorldMarkerReference(managerRef));
        if(manager && *reinterpret_cast<DWORD*>(manager)==0xFDA948) {
            auto begin=*reinterpret_cast<void***>(manager+0x58),end=*reinterpret_cast<void***>(manager+0x5C);
            if(end>=begin && end-begin<=64)for(auto at=begin;at!=end;++at) {
                auto movie=static_cast<BYTE*>(RetainWorldMarkerReference(*at));
                if(!movie)continue;
                const auto slot=count++;movieRefs[slot]=*at;
                if((*reinterpret_cast<DWORD*>(movie)==0xFDA6E8 || g_worldMarkerMovieTable.Owns(movie)) && movie[0x79]) {
                    auto ref=*reinterpret_cast<void**>(movie+0x34);
                    if(RetainWorldMarkerReference(ref))definitions[slot]=ref;
                }
            }
            const auto read=[](uint32_t address,void* output,size_t length) {
                __try {std::memcpy(output,reinterpret_cast<const void*>(address),length);return true;}
                __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
            };
            for(unsigned i=0;i<count;++i)if(definitions[i]) {
                flags|=d2vr_action_movies::InspectDefinition(*static_cast<uint32_t*>(definitions[i]),read);
                menu|=d2vr_action_movies::InspectDefinition(*static_cast<uint32_t*>(definitions[i]),read,true);
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {flags=menu=0;}
    for(unsigned i=0;i<count;++i) {
        if(definitions[i])ReleaseSelectedLocalPlayer(uint32_t(reinterpret_cast<uintptr_t>(definitions[i])));
        ReleaseSelectedLocalPlayer(uint32_t(reinterpret_cast<uintptr_t>(movieRefs[i])));
    }
    if(manager)ReleaseSelectedLocalPlayer(uint32_t(reinterpret_cast<uintptr_t>(managerRef)));
    g_nativeVrMenu.store((GetTickCount64()<<8)|menu,std::memory_order_release);
    static unsigned previousMenu=0;
    if(previousMenu!=menu){previousMenu=menu;Log("[NativeMenu] flags=%u; retained visible movie definitions",menu);}
    const auto previous=NativeActionMovies();
    g_nativeActionMovies.store((GetTickCount64()<<8)|flags,std::memory_order_release);
    if(previous!=flags)Log("[ActionMovies] flags=%u previous=%u; input-only protection, no camera override",flags,previous);
#endif
}

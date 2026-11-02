#include <windows.h>
#include <intrin.h>
#include <cassert>
#include <cstring>
#include <cstdio>
#include "../../src/native_hands_layout.h"
namespace d2vr_input {enum class GrabFeedback {Unknown,Empty,Busy};}
struct GrabSample {d2vr_input::GrabFeedback grab;bool alternateHeldTarget;};
static bool GrabAdmitted(bool protectedRoute,GrabSample sample) {
#include "hands_grab_actual.inl"
    return true;
}
static bool cinematic=true,available=false;
static void* g_nativeHandsSelectedResource=nullptr;
static SRWLOCK g_nativeHandsLock=SRWLOCK_INIT;
static unsigned nativeCalls=0,applyCalls=0;
static bool ProtectedHandsResourceOwned(void* p){return p && p==g_nativeHandsSelectedResource;}
static bool CinemaSceneThisFrame(){return cinematic;}
static bool ProtectedHandsAvailable(){return available;}
static bool ApplyNativeHandsChecked(void*,DWORD,void*,bool){++applyCalls;return true;}
#include "cinematic_restore_actual.inl"
#include "cinematic_pack_actual.inl"
static void __fastcall NativePack(void*,void*,void*){++nativeCalls;}
alignas(16) static BYTE renderer[0x600]{},resource[0x600]{},skeleton[0x100]{},records[130*12]{};
alignas(16) static float palette[130][16]{};
static void Prepare(unsigned boneCount=130) {
    static void* resourceRef=resource;static void* skeletonRef=skeleton;
    *reinterpret_cast<void***>(renderer+0x5bc)=&resourceRef;
    *reinterpret_cast<void***>(resource+0x5e0)=&skeletonRef;
    *reinterpret_cast<float**>(renderer+0x3f4)=palette[0];
    *reinterpret_cast<float**>(renderer+0x3f8)=palette[0]+boneCount*16;
    *reinterpret_cast<DWORD*>(skeleton)=0xFC2338;
    *reinterpret_cast<BYTE**>(skeleton+0x90)=records;
    *reinterpret_cast<BYTE**>(skeleton+0x94)=records+boneCount*12;
    *reinterpret_cast<DWORD*>(renderer+0x424)=0;*reinterpret_cast<DWORD*>(renderer+0x448)=0;
    for(unsigned b=0;b<130;++b)for(unsigned f=0;f<16;++f)palette[b][f]=10000.f+b*16+f;
    auto& cache=g_nativeHandsCache[0];cache={};cache.renderer=renderer;cache.resource=resource;
    cache.skeleton=skeleton;cache.palette=palette[0];
    for(unsigned b=0;b<44;++b)for(unsigned f=0;f<16;++f) {
        cache.original[b][f]=20000.f+b*16+f;
        cache.applied[b][f]=palette[b+2][f];
    }
    cache.appliedValid[0]=cache.appliedValid[1]=true;
    renderer[0x580]=renderer[0x581]=0;
}
int main() {
    assert(d2vr_hands::HumanPaletteLayout(47,47*16));
    assert(d2vr_hands::HumanPaletteLayout(130,130*16));
    assert(!d2vr_hands::HumanPaletteLayout(47,130*16));
    assert(!d2vr_hands::HumanPaletteLayout(46,46*16));
    assert(!d2vr_hands::HumanPaletteLayout(48,48*16));
    using G=d2vr_input::GrabFeedback;
    assert(GrabAdmitted(true,{G::Unknown,false})); // peaceful human arms
    assert(GrabAdmitted(true,{G::Busy,true})); // owned human bones only
    assert(!GrabAdmitted(false,{G::Unknown,false}));
    assert(!GrabAdmitted(false,{G::Busy,false}));
    assert(!GrabAdmitted(false,{G::Empty,true}));
    assert(GrabAdmitted(false,{G::Empty,false}));
    g_nativeHandsSelectedResource=resource;
    g_protectedHandsPackOriginal=reinterpret_cast<ProtectedHandsPackMethod>(&NativePack);
    void* rendererRef=renderer;
    // Production packer must retire our old controller arms even with no
    // tracking / unavailable hands. Preserve the rest of the full body.
    Prepare();ProtectedHandsPackHook(resource,nullptr,&rendererRef);
    assert(nativeCalls==1 && applyCalls==0);
    assert(!memcmp(palette[2],g_nativeHandsCache[0].original,44*64));
    assert(palette[0][0]==10000 && palette[46][0]==10000+46*16);
    assert(renderer[0x580] && renderer[0x581]);
    // Native animation has already replaced one arm: NEVER restore stale data.
    Prepare();palette[2][0]=333;
    ProtectedHandsPackHook(resource,nullptr,&rendererRef);
    assert(nativeCalls==2 && palette[2][0]==333);
    assert(!memcmp(palette[24],g_nativeHandsCache[0].original+22,22*64));
    // Gameplay unavailable: no controller pose, but native pack still once.
    cinematic=false;Prepare();ProtectedHandsPackHook(resource,nullptr,&rendererRef);
    assert(nativeCalls==3 && applyCalls==0 && palette[2][0]==10000+2*16);
    available=true;ProtectedHandsPackHook(resource,nullptr,&rendererRef);
    assert(nativeCalls==4 && applyCalls==1);
    cinematic=true;Prepare(47);ProtectedHandsPackHook(resource,nullptr,&rendererRef);
    assert(nativeCalls==5 && applyCalls==1);
    assert(!memcmp(palette[2],g_nativeHandsCache[0].original,44*64));
    assert(palette[0][0]==10000 && palette[46][0]==10000+46*16);
    // A different skeleton/palette size is not authority for arm writes.
    Prepare(46);ProtectedHandsPackHook(resource,nullptr,&rendererRef);
    assert(palette[2][0]==10000+2*16);
    puts("PASS actual x86 pack/restore: 47/130 bone retirement, unknown layout rejection, native body/newer animation preserved, gameplay resume");
}

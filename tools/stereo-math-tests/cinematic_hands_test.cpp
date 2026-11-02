#include <windows.h>
#include <intrin.h>
#include <cassert>
#include <cstring>
#include <cstdio>
static bool cinematic=true,available=false;
static void* g_nativeHandsSelectedResource=nullptr;
static SRWLOCK g_nativeHandsLock=SRWLOCK_INIT;
static unsigned nativeCalls=0,applyCalls=0;
static bool ProtectedHandsResourceOwned(void* p){return p && p==g_nativeHandsSelectedResource;}
static bool NativeCinematicPlaying(){return cinematic;}
static bool ProtectedHandsAvailable(){return available;}
static bool ApplyNativeHandsChecked(void*,DWORD,void*,bool){++applyCalls;return true;}
#include "cinematic_restore_actual.inl"
#include "cinematic_pack_actual.inl"
static void __fastcall NativePack(void*,void*,void*){++nativeCalls;}
alignas(16) static BYTE renderer[0x600]{},resource[0x600]{},skeleton[16]{};
alignas(16) static float palette[130][16]{};
static void Prepare() {
    static void* resourceRef=resource;static void* skeletonRef=skeleton;
    *reinterpret_cast<void***>(renderer+0x5bc)=&resourceRef;
    *reinterpret_cast<void***>(resource+0x5e0)=&skeletonRef;
    *reinterpret_cast<float**>(renderer+0x3f4)=palette[0];
    *reinterpret_cast<float**>(renderer+0x3f8)=palette[0]+130*16;
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
    puts("PASS actual x86 pack/restore: cinematic tracking-loss retirement, native body preserved, native newer arm preserved, gameplay resume");
}

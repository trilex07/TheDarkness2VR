#include "image_present_fixture.h"
#include <cassert>
#include <cstring>
struct Pair;
struct Object {void** vt;Pair* pair;};
struct Pair {Object base,ex,privateObject;LONG refs;bool alive;};
static void* deviceVT[180];static void* baseVT[180];static void* exVT[180];static void* privateVT[3];
static Object device{deviceVT,nullptr};static Pair pool;
static FixtureStats stats{};static HRESULT desired=S_OK;static bool cycle=false;
static const RECT* expectedA=nullptr;static const RECT* expectedB=nullptr;
static HWND expectedWindow=nullptr;static const RGNDATA* expectedDirty=nullptr;static DWORD expectedFlags=0;
static ULONG STDMETHODCALLTYPE Add(Object* o) {++stats.adds;return o->pair?++o->pair->refs:2;}
static ULONG STDMETHODCALLTYPE Release(Object* o) {
    ++stats.releases;if(!o->pair) return 1;
    assert(o->pair->alive && o->pair->refs>0);
    const auto n=--o->pair->refs;if(!n){o->pair->alive=false;++stats.destroyed;}return n;
}
static __declspec(noinline) HRESULT STDMETHODCALLTYPE Query(Object* o,REFIID id,void** out) {
    ++stats.queries;if(!out) return E_POINTER;
    *out=nullptr;
    if(!o->pair || !o->pair->alive) return E_NOINTERFACE;
    if(IsEqualGUID(id,__uuidof(IDirect3DSwapChain9Ex))) *out=&o->pair->ex;
    else if(IsEqualGUID(id,__uuidof(IDirect3DSwapChain9)) || IsEqualGUID(id,__uuidof(IUnknown))) *out=&o->pair->base;
    else if(IsEqualGUID(id,privateFixtureIID)) *out=&o->pair->privateObject;
    else return E_NOINTERFACE;
    Add(static_cast<Object*>(*out));return S_OK;
}
static unsigned STDMETHODCALLTYPE PrivateDevice(Object* o) {assert(o==&device);++stats.privateDevice;return 0x156;}
static unsigned STDMETHODCALLTYPE PrivateChain(Object* o) {assert(o->pair && o->pair->alive);++stats.privateChain;return 0x37;}
static HRESULT STDMETHODCALLTYPE GetDevice(Object*,IDirect3DDevice9** out) {if(!out)return E_POINTER;Add(&device);*out=reinterpret_cast<IDirect3DDevice9*>(&device);return S_OK;}
static __declspec(noinline) HRESULT STDMETHODCALLTYPE NativePresent(Object* o,const RECT* a,const RECT* b,HWND w,const RGNDATA* r,DWORD f) {
    assert(o->pair && o->pair->alive);++stats.native;
    assert(a==expectedA && b==expectedB && w==expectedWindow && r==expectedDirty && f==expectedFlags);
    using Private=unsigned(STDMETHODCALLTYPE*)(Object*);
    assert(reinterpret_cast<Private>(o->vt[37])(o)==0x37);
    if(cycle) {
        using Present=HRESULT(STDMETHODCALLTYPE*)(Object*,const RECT*,const RECT*,HWND,const RGNDATA*,DWORD);
        return reinterpret_cast<Present>(o->vt[3])(o,a,b,w,r,f);
    }
    return desired;
}
static __declspec(noinline) HRESULT STDMETHODCALLTYPE OverlayPresent(Object* o,const RECT* a,const RECT* b,HWND w,const RGNDATA* r,DWORD f) {
    ++stats.overlay;return NativePresent(o,a,b,w,r,f);
}
// A pre-existing inline JMP, as opposed to a guessed original-function pointer.
static __declspec(naked) HRESULT STDMETHODCALLTYPE OverlayEntry(Object*,const RECT*,const RECT*,HWND,const RGNDATA*,DWORD) {
    __asm {jmp OverlayPresent}
}
static __declspec(noinline) HRESULT STDMETHODCALLTYPE CreateAdditional(Object* o,D3DPRESENT_PARAMETERS* pp,IDirect3DSwapChain9** out) {
    using Private=unsigned(STDMETHODCALLTYPE*)(Object*);
    assert(reinterpret_cast<Private>(o->vt[156])(o)==0x156);
    assert(pp && pp->BackBufferWidth==171 && pp->BackBufferHeight==113 && out);
    assert(!pool.alive);++stats.creates;
    pool={{baseVT,&pool},{exVT,&pool},{privateVT,&pool},1,true};
    *out=reinterpret_cast<IDirect3DSwapChain9*>(&pool.base);return S_OK;
}
extern "C" __declspec(dllexport) IDirect3DDevice9* __cdecl FixtureDevice() {
    deviceVT[0]=baseVT[0]=exVT[0]=privateVT[0]=reinterpret_cast<void*>(&Query);
    deviceVT[1]=baseVT[1]=exVT[1]=privateVT[1]=reinterpret_cast<void*>(&Add);
    deviceVT[2]=baseVT[2]=exVT[2]=privateVT[2]=reinterpret_cast<void*>(&Release);
    deviceVT[13]=reinterpret_cast<void*>(&CreateAdditional);deviceVT[156]=reinterpret_cast<void*>(&PrivateDevice);
    baseVT[3]=reinterpret_cast<void*>(&OverlayEntry);exVT[3]=reinterpret_cast<void*>(&NativePresent);
    baseVT[8]=exVT[8]=reinterpret_cast<void*>(&GetDevice);
    baseVT[37]=exVT[37]=reinterpret_cast<void*>(&PrivateChain);
    return reinterpret_cast<IDirect3DDevice9*>(&device);
}
extern "C" __declspec(dllexport) const FixtureStats* __cdecl FixtureCounters(){return &stats;}
extern "C" __declspec(dllexport) void __cdecl FixtureExpect(const RECT* a,const RECT* b,HWND w,const RGNDATA* r,DWORD flags,HRESULT hr) {
    expectedA=a;expectedB=b;expectedWindow=w;expectedDirty=r;expectedFlags=flags;desired=hr;
}
extern "C" __declspec(dllexport) void __cdecl FixtureCycle(){cycle=true;}
extern "C" __declspec(dllexport) void __cdecl FixtureCreateTarget(void* p){deviceVT[13]=p;}

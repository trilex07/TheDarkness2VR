#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../../src/image_present_observer.h"
#include "../../src/capture_present_witness.h"
#include "../../src/frame_boundary.h"
#include "image_present_fixture.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <thread>
static d2vr_image_present::Observer observer;
static d2vr_frame::BoundaryMachine boundary;
static d2vr_capture::PresentWitness witness;
static unsigned rendered=0,accepted=0;static uint64_t frame=0,clockMs=1;
static IDirect3DDevice9* borrowed=nullptr;
static bool qualifies(d2vr_image_present::Method,uintptr_t caller){return caller!=0;}
static void before(IDirect3DSwapChain9* sc,uintptr_t caller,bool native,bool ready) {
    assert(native);if(!ready) return;
    assert(observer.InPresentation() && observer.Ready());
    assert(sc->GetDevice(&borrowed)==S_OK && borrowed);
    d2vr_frame::Event event{};event.type=d2vr_frame::EventType::SwapChainPresent;
    event.timestampMs=clockMs++;event.threadId=GetCurrentThreadId();
    auto decision=boundary.OnEvent(event);assert(decision.requestVrFrame);++frame;
    assert(witness.Enter("SwapChainPresent",reinterpret_cast<uintptr_t>(borrowed),caller,event.threadId,frame));
    ++rendered;
}
static void after(IDirect3DSwapChain9*,uintptr_t,bool native,bool ready,HRESULT hr) {
    assert(native);
    if(ready) {
        assert(witness.active && witness.frame==frame);
        if(hr==S_OK && !observer.Failed()) ++accepted;
        witness.Leave();borrowed->Release();borrowed=nullptr;
    }
}
static std::vector<unsigned char> TextSnapshot() {
    auto image=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS*>(image+reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew);
    auto section=IMAGE_FIRST_SECTION(pe);
    for(unsigned i=0;i<pe->FileHeader.NumberOfSections;++i,++section)
        if(!std::memcmp(section->Name,".text",5)) return {image+section->VirtualAddress,image+section->VirtualAddress+section->Misc.VirtualSize};
    assert(false);return {};
}
static HRESULT STDMETHODCALLTYPE ForbiddenMainCreate(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*,IDirect3DSwapChain9**){assert(false);return E_FAIL;}
int main(int argc,char** argv) {
    assert(argc>=2);
    const auto text=TextSnapshot();
    auto dll=LoadLibraryA(argv[1]);assert(dll);
    auto deviceFn=reinterpret_cast<IDirect3DDevice9*(__cdecl*)()>(GetProcAddress(dll,"FixtureDevice"));
    auto counters=reinterpret_cast<const FixtureStats*(__cdecl*)()>(GetProcAddress(dll,"FixtureCounters"));
    auto expect=reinterpret_cast<void(__cdecl*)(const RECT*,const RECT*,HWND,const RGNDATA*,DWORD,HRESULT)>(GetProcAddress(dll,"FixtureExpect"));
    assert(deviceFn && counters && expect);auto device=deviceFn();
    auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS*>(image+reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew);
    d2vr_native_image::Guard guard{true,image,pe->OptionalHeader.SizeOfImage};
    if(argc>2 && !std::strcmp(argv[2],"deny")) {
        auto change=reinterpret_cast<void(__cdecl*)(void*)>(GetProcAddress(dll,"FixtureCreateTarget"));assert(change);
        change(reinterpret_cast<void*>(&ForbiddenMainCreate));
        assert(!observer.Start(device,guard,{qualifies,before,after,nullptr}) && observer.Failed());
        assert(TextSnapshot()==text);
        std::puts("PASS refuses target in main EXE before MinHook changes bytes");return 0;
    }
    D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=171;pp.BackBufferHeight=113;
    // Snapshot every fixture-owned slot BEFORE any observer hook is installed.
    IDirect3DSwapChain9* originalBase=nullptr;IDirect3DSwapChain9Ex* originalEx=nullptr;
    assert(device->CreateAdditionalSwapChain(&pp,&originalBase)==S_OK);
    assert(originalBase->QueryInterface(__uuidof(IDirect3DSwapChain9Ex),reinterpret_cast<void**>(&originalEx))==S_OK);
    auto originalBaseVT=*reinterpret_cast<void***>(originalBase),originalExVT=*reinterpret_cast<void***>(originalEx);
    const std::vector<void*> originalBaseTable(originalBaseVT,originalBaseVT+180),originalExTable(originalExVT,originalExVT+180);
    auto vt=*reinterpret_cast<void***>(device);std::vector<void*> deviceTable(vt,vt+180);
    assert(observer.Start(device,guard,{qualifies,before,after,nullptr}));
    assert(!observer.Ready());assert(*reinterpret_cast<void***>(device)==vt);
    assert(originalEx->Release()==1 && originalBase->Release()==0);
    RECT a{1,2,31,47},b{4,5,57,63};RGNDATA dirty{};const auto window=reinterpret_cast<HWND>(0x1234);
    void* reused=nullptr;
    for(unsigned run=0;run<1000;++run) {
        IDirect3DSwapChain9* base=nullptr;
        assert(device->CreateAdditionalSwapChain(&pp,&base)==S_OK && base);
        if(reused) assert(base==reused);reused=base;
        IDirect3DSwapChain9Ex* ex=nullptr;
        assert(base->QueryInterface(__uuidof(IDirect3DSwapChain9Ex),reinterpret_cast<void**>(&ex))==S_OK && ex && ex!=base);
        auto baseVT=*reinterpret_cast<void***>(base);auto exVT=*reinterpret_cast<void***>(ex);
        assert(baseVT==originalBaseVT && exVT==originalExVT);
        IUnknown* privateObject=nullptr;
        assert(base->QueryInterface(privateFixtureIID,reinterpret_cast<void**>(&privateObject))==S_OK && privateObject);
        privateObject->Release(); // exactly three private slots; no Present inspection
        for(HRESULT hr:{S_OK,S_OK,static_cast<HRESULT>(0x08760878),S_OK,S_OK,D3DERR_DEVICELOST}) {
            expect(&a,&b,window,&dirty,0x13579,hr);
            const auto prior=counters()->native;
            // Base entry is already a JMP to an overlay; it calls the separately
            // intercepted Ex implementation. Each native leaf runs exactly once.
            assert(base->Present(&a,&b,window,&dirty,0x13579)==hr);
            assert(counters()->native==prior+1);
        }
        assert(!observer.Ready());
        observer.ResetBegin();witness.Reset();
        observer.ResetEnd(false);witness.ResetResult(false);assert(!observer.Ready());
        // Retained Ex interface after ResetEx: old object remains forwarded.
        observer.ResetEnd(true);witness.ResetResult(true);
        expect(nullptr,nullptr,nullptr,nullptr,0,S_OK);
        assert(ex->Present(nullptr,nullptr,nullptr,nullptr,0)==S_OK);
        assert(observer.Ready());
        assert(ex->Present(nullptr,nullptr,nullptr,nullptr,0)==S_OK);
        assert(*reinterpret_cast<void***>(base)==baseVT && *reinterpret_cast<void***>(ex)==exVT);
        assert(!std::memcmp(baseVT,originalBaseTable.data(),180*sizeof(void*)));
        assert(!std::memcmp(exVT,originalExTable.data(),180*sizeof(void*)));
        assert(ex->Release()==1 && base->Release()==0);
        assert(counters()->destroyed==run+2);
    }
    assert(!std::memcmp(vt,deviceTable.data(),180*sizeof(void*)));
    assert(TextSnapshot()==text && !observer.Failed());
    assert(rendered>0 && accepted>0 && accepted<rendered && observer.recursive==0);
    // A later overlay cycle must never call a saved original repeatedly.
    IDirect3DSwapChain9* last=nullptr;assert(device->CreateAdditionalSwapChain(&pp,&last)==S_OK);
    if(argc>2 && !std::strcmp(argv[2],"hr-error")) {
        expect(nullptr,nullptr,nullptr,nullptr,0,D3DERR_INVALIDCALL);
        assert(last->Present(nullptr,nullptr,nullptr,nullptr,0)==D3DERR_INVALIDCALL);
        assert(observer.Failed() && !observer.Ready());
        last->Release();assert(TextSnapshot()==text);
        std::puts("PASS unexpected Present failure preserves HRESULT and closes XR admission");return 0;
    }
    if(argc>2 && !std::strcmp(argv[2],"thread-change")) {
        expect(nullptr,nullptr,nullptr,nullptr,0,S_OK);
        assert(last->Present(nullptr,nullptr,nullptr,nullptr,0)==S_OK && observer.Ready());
        std::thread changed([&]{assert(last->Present(nullptr,nullptr,nullptr,nullptr,0)==S_OK);});changed.join();
        assert(observer.Failed() && !observer.Ready());
        last->Release();assert(TextSnapshot()==text);
        std::puts("PASS Present thread change without Reset preserves HRESULT and closes XR admission");return 0;
    }
    auto cycle=reinterpret_cast<void(__cdecl*)()>(GetProcAddress(dll,"FixtureCycle"));assert(cycle);cycle();
    const auto prior=counters()->native;
    assert(last->Present(nullptr,nullptr,nullptr,nullptr,0)==D3DERR_INVALIDCALL);
    assert(counters()->native==prior+1 && observer.recursive==1 && observer.Failed() && !observer.Ready());
    last->Release();assert(TextSnapshot()==text);
    std::printf("PASS x86 native DLL entry/trampoline ABI: native=%u frames=%u accepted=%u; base/Ex/private slots, 1000 release/recreate cycles, Reset, statuses, recursive overlay rejection; EXE text unchanged\n",counters()->native,rendered,accepted);
}

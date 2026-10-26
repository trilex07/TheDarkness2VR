#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../../src/image_present_observer.h"
#include "../../src/capture_present_witness.h"
#include "../../src/frame_boundary.h"
#include "image_present_fixture.h"
#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <cstring>
#include <vector>
#include <thread>
static d2vr_image_present::Observer observer;
static d2vr_frame::BoundaryMachine boundary;
static d2vr_capture::PresentWitness witness;
static unsigned rendered=0,accepted=0;static uint64_t frame=0,clockMs=1;
static IDirect3DDevice9* borrowed=nullptr;
static const FixtureStats* stats=nullptr;
static bool timingEnabled=true,forwardActive=false,expectedQualified=true;
static bool createCallerQualified=true,presentCallerQualified=true;
static std::vector<std::string> receipts;
static unsigned expectedReceiptNative=0,expectedReceiptCreates=0;
static void receiptLog(const char* format,...) {
    char line[2048];va_list args;va_start(args,format);vsnprintf(line,sizeof(line),format,args);va_end(args);
    if(std::strstr(line,"raw-present returned")) {
        // Receipts cannot substitute entry for a successful original return.
        assert(!forwardActive && stats->native==expectedReceiptNative);
        assert(observer.forwarded.load()==observer.returned.load());
        receipts.emplace_back(line);
    } else if(std::strstr(line,"raw-create returned")) {
        assert(stats->creates==expectedReceiptCreates);receipts.emplace_back(line);
    }
}
static unsigned receiptCount(const char* kind,const char* fields) {
    unsigned n=0;for(const auto& line:receipts)
        if(line.find(kind)!=std::string::npos && line.find(fields)!=std::string::npos) ++n;
    return n;
}
static unsigned phase=0,forwardStarts=0,forwardEnds=0,priorNative=0;
static void forwardBegin() {
    assert(timingEnabled && phase==1 && !forwardActive);
    phase=2;forwardActive=true;++forwardStarts;priorNative=stats->native;
}
static void forwardEnd() {
    assert(timingEnabled && phase==2 && forwardActive);
    // The original native leaf ran once INSIDE this span. Overlay nesting must
    // not open a second span or count callback work as native forwarding.
    assert(stats->native==priorNative+1);
    phase=3;forwardActive=false;++forwardEnds;
}
static bool qualifies(d2vr_image_present::Method method,uintptr_t caller) {
    return caller!=0 && (method==d2vr_image_present::Method::Create?createCallerQualified:presentCallerQualified);
}
static void before(IDirect3DSwapChain9* sc,uintptr_t caller,bool native,bool ready) {
    assert(native==expectedQualified && phase==0 && !forwardActive);phase=1;
    if(!ready) return;
    assert(observer.InPresentation() && observer.Ready());
    assert(sc->GetDevice(&borrowed)==S_OK && borrowed);
    d2vr_frame::Event event{};event.type=d2vr_frame::EventType::SwapChainPresent;
    event.timestampMs=clockMs++;event.threadId=GetCurrentThreadId();
    auto decision=boundary.OnEvent(event);assert(decision.requestVrFrame);++frame;
    assert(witness.Enter("SwapChainPresent",reinterpret_cast<uintptr_t>(borrowed),caller,event.threadId,frame));
    ++rendered;
}
static void after(IDirect3DSwapChain9*,uintptr_t,bool native,bool ready,HRESULT hr) {
    assert(native==expectedQualified && !forwardActive && phase==((timingEnabled && native)?3u:1u));
    phase=0;
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
    assert(deviceFn && counters && expect);auto device=deviceFn();stats=counters();
    timingEnabled=!(argc>2 && !std::strcmp(argv[2],"timing-disabled"));
    const bool receiptTest=argc>2 && !std::strcmp(argv[2],"unqualified-create");
    d2vr_image_present::Callbacks callbacks{qualifies,before,after,receiptTest?receiptLog:nullptr};
    if(timingEnabled) {callbacks.forwardBegin=forwardBegin;callbacks.forwardEnd=forwardEnd;}
    auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS*>(image+reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew);
    d2vr_native_image::Guard guard{true,image,pe->OptionalHeader.SizeOfImage};
    if(argc>2 && !std::strcmp(argv[2],"deny")) {
        auto change=reinterpret_cast<void(__cdecl*)(void*)>(GetProcAddress(dll,"FixtureCreateTarget"));assert(change);
        change(reinterpret_cast<void*>(&ForbiddenMainCreate));
        assert(!observer.Start(device,guard,callbacks) && observer.Failed());
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
    assert(observer.Start(device,guard,callbacks));
    assert(!observer.Ready());assert(*reinterpret_cast<void***>(device)==vt);
    assert(originalEx->Release()==1 && originalBase->Release()==0);
    RECT a{1,2,31,47},b{4,5,57,63};RGNDATA dirty{};const auto window=reinterpret_cast<HWND>(0x1234);
    if(receiptTest) {
        // The typed API can succeed for an untrusted caller. Observation alone
        // must not grant native-camera/XR ownership, even to a later native caller.
        createCallerQualified=false;presentCallerQualified=false;expectedQualified=false;
        IDirect3DSwapChain9* base=nullptr;IDirect3DSwapChain9Ex* ex=nullptr;
        expectedReceiptCreates=stats->creates+1;
        assert(device->CreateAdditionalSwapChain(&pp,&base)==S_OK && base);
        assert(base->QueryInterface(__uuidof(IDirect3DSwapChain9Ex),reinterpret_cast<void**>(&ex))==S_OK && ex!=base);
        const auto positive=static_cast<HRESULT>(0x08760878);
        auto present=[&](HRESULT hr) {
            expect(&a,&b,window,&dirty,0x13579,hr);
            const auto prior=stats->native;expectedReceiptNative=prior+1;
            assert(base->Present(&a,&b,window,&dirty,0x13579)==hr);
            assert(stats->native==prior+1 && !observer.Failed());
        };
        present(positive); // first raw outer call is unrelated (e.g. an overlay)
        assert(receiptCount("raw-present","callerQualifies=0 created=0 qualified=0")==1);
        presentCallerQualified=true;
        present(S_OK); // verified native caller, but no verified acquisition
        assert(receiptCount("raw-present","callerQualifies=1 created=0 qualified=0")==1);
        const auto receiptSize=receipts.size();
        for(unsigned i=0;i<20;++i) present(i%2?S_OK:positive);
        assert(receipts.size()==receiptSize && !observer.Ready());
        assert(rendered==0 && accepted==0 && observer.nativePresents==0 && forwardStarts==0);
        assert(ex->Release()==1 && base->Release()==0);
        // A subsequent properly qualified acquisition recovers existing policy.
        createCallerQualified=true;expectedQualified=true;
        expectedReceiptCreates=stats->creates+1;
        assert(device->CreateAdditionalSwapChain(&pp,&base)==S_OK && base && !observer.Ready());
        assert(base->QueryInterface(__uuidof(IDirect3DSwapChain9Ex),reinterpret_cast<void**>(&ex))==S_OK);
        present(S_OK);assert(observer.Ready() && rendered==0 && observer.nativePresents==1);
        present(S_OK);assert(rendered==1 && accepted==1 && observer.nativePresents==2);
        assert(receiptCount("raw-create","callerQualifies=0 observed=1 created=0")==1);
        assert(receiptCount("raw-create","callerQualifies=1 observed=1 created=1")==1);
        assert(receiptCount("raw-present","callerQualifies=1 created=1 qualified=1")==1);
        assert(receipts.size()==5 && forwardStarts==2 && forwardEnds==2 && observer.recursive==0);
        assert(*reinterpret_cast<void***>(base)==originalBaseVT && *reinterpret_cast<void***>(ex)==originalExVT);
        assert(!std::memcmp(originalBaseVT,originalBaseTable.data(),180*sizeof(void*)));
        assert(!std::memcmp(originalExVT,originalExTable.data(),180*sizeof(void*)));
        assert(!std::memcmp(vt,deviceTable.data(),180*sizeof(void*)));
        assert(ex->Release()==1 && base->Release()==0 && stats->destroyed==3);
        // Repeated acquisitions sharing the same implementation remain bounded,
        // including switching back to an unrelated Create caller after recovery.
        for(bool trusted:{true,false}) {
            createCallerQualified=trusted;
            assert(device->CreateAdditionalSwapChain(&pp,&base)==S_OK && base);
            present(S_OK);present(S_OK);
            assert(receipts.size()==5 && base->Release()==0);
        }
        assert(TextSnapshot()==text && !forwardActive && phase==0);
        std::puts("PASS bounded post-return receipts: unqualified Create/native Present rejected, original ABI/HRESULT/ownership retained, later qualified acquisition recovers XR admission");
        return 0;
    }
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
    assert(!forwardActive && phase==0 && forwardStarts==forwardEnds);
    assert(forwardStarts==(timingEnabled?observer.nativePresents.load():0));
    // A later overlay cycle must never call a saved original repeatedly.
    IDirect3DSwapChain9* last=nullptr;assert(device->CreateAdditionalSwapChain(&pp,&last)==S_OK);
    // A non-engine Present is forwarded but cannot emit authoritative native
    // timing or mutate the frame boundary. The fixture still verifies all args.
    const auto priorStarts=forwardStarts,priorFrames=rendered;
    const auto priorQualified=observer.nativePresents.load();
    expectedQualified=false;presentCallerQualified=false;expect(nullptr,nullptr,nullptr,nullptr,0,S_OK);
    assert(last->Present(nullptr,nullptr,nullptr,nullptr,0)==S_OK);
    assert(forwardStarts==priorStarts && rendered==priorFrames);
    assert(observer.nativePresents.load()==priorQualified);
    expectedQualified=true;presentCallerQualified=true;
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
    assert(!forwardActive && phase==0 && forwardStarts==forwardEnds);
    assert(forwardStarts==(timingEnabled?observer.nativePresents.load():0));
    std::printf("PASS x86 native DLL entry/trampoline ABI: native=%u frames=%u accepted=%u; base/Ex/private slots, 1000 release/recreate cycles, Reset, statuses, recursive overlay rejection; EXE text unchanged\n",counters()->native,rendered,accepted);
}

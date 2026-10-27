#include "../../src/protected_source_size.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <initializer_list>
#include <vector>
using namespace d2vr_source_quality;
static std::vector<unsigned char> Text() {
    const auto image=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    const auto pe=reinterpret_cast<IMAGE_NT_HEADERS*>(image+reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew);
    auto section=IMAGE_FIRST_SECTION(pe);
    for(unsigned i=0;i<pe->FileHeader.NumberOfSections;++i,++section)
        if(!std::memcmp(section->Name,".text",5)) return {image+section->VirtualAddress,image+section->VirtualAddress+section->Misc.VirtualSize};
    assert(false);return {};
}
int main(int argc,char** argv) {
    const auto originalText=Text();
    assert(Parse(0)==Mode::Off && Parse(3)==Mode::High && Parse(4)==Mode::Exact && Parse(5)==Mode::Off);
    assert(Parse((std::numeric_limits<unsigned>::max)())==Mode::Off);
    const auto native=Choose(Mode::Native,1920,1071),quality=Choose(Mode::Quality,1920,1071),high=Choose(Mode::High,1920,1071);
    assert(native.width==1920 && native.height==1071);
    assert(quality.width==2160 && quality.height==1204);
    assert(high.width==2400 && high.height==1338);
    assert(!Choose(Mode::Off,1920,1071).width);
    assert(!Choose(Mode::High,3840,2160).width); // refuse excessive cost, no silent resize
    assert(!Choose(Mode::High,0,1071).width && !Choose(Mode::High,1920,0).width);
    assert(!Choose(Mode::Quality,0xffffffff,0xffffffff).width);
    const auto exact=Choose(Mode::Exact,1920,1071,{2676,2728});
    assert(exact.width==2676 && exact.height==2728);
    assert(!Choose(Mode::Exact,1920,1071).width);
    assert(!Choose(Mode::Exact,1920,1071,{3840,3840}).width);
    assert(!Choose(Mode::Exact,1920,1071,{0xffffffff,2728}).width);
    assert(!Choose(Mode::Exact,1920,1071,{2676,0}).width);
    for(unsigned w=640;w<=3840;w+=31) for(unsigned h=480;h<=3840;h+=29) {
        for(auto mode:{Mode::Native,Mode::Quality,Mode::High}) {
            const auto size=Choose(mode,w,h);
            if(!size.width) continue;
            assert(size.width>=w && size.height>=h && size.width<=3840 && size.height<=3840);
            assert(uint64_t(size.width)*size.height<=8294400);
            const double relative=(double(size.width)/size.height)/(double(w)/h);
            assert(relative>.996 && relative<1.004);
        }
    }
    assert(Eligible(true,true,1,1,4,4));
    assert(!Eligible(false,true,1,1,4,4) && !Eligible(true,false,1,1,4,4));
    assert(!Eligible(true,true,2,1,4,4) && !Eligible(true,true,1,1,5,4));
    assert(!Eligible(true,true,0,0,4,4) && !Eligible(true,true,1,1,0,0));
    assert(argc>=2 && argc<=4);
    const bool exactMode=argc>=3 && !std::strcmp(argv[2],"exact");
    const bool deferred=argc==4 && !std::strcmp(argv[3],"deferred");
    const auto selected=exactMode?Mode::Exact:Mode::Quality;
    const LONG expectedW=exactMode?2676:2160,expectedH=exactMode?2728:1204;
    HMODULE fixture=LoadLibraryA(argv[1]);assert(fixture);
    using Query=BOOL(WINAPI*)(HWND,LPRECT);
    auto query=reinterpret_cast<Query>(GetProcAddress(fixture,"_SizeQuery@8"));
    auto invoke=reinterpret_cast<Query>(GetProcAddress(fixture,"_SizeInvoke@8"));
    auto other=reinterpret_cast<Query>(GetProcAddress(fixture,"_OtherInvoke@8"));
    auto caller=reinterpret_cast<uintptr_t(WINAPI*)()>(GetProcAddress(fixture,"_SizeCaller@0"));
    auto calls=reinterpret_cast<unsigned(WINAPI*)()>(GetProcAddress(fixture,"_SizeCalls@0"));
    assert(query && invoke && other && caller && calls);
    RECT rect{};HWND window=reinterpret_cast<HWND>(1);
    assert(invoke(window,&rect));const auto nativeCaller=caller();
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS*>(image+reinterpret_cast<const IMAGE_DOS_HEADER*>(image)->e_lfanew);
    d2vr_native_image::Guard guard{true,image,pe->OptionalHeader.SizeOfImage};
    ClientSize blocked;
    assert(!blocked.Start(reinterpret_cast<void*>(&main),guard,Mode::Quality,window,nativeCaller));
    ClientSize qualityHook;
    assert(!qualityHook.Start(query,guard,Mode::Off,window,nativeCaller));
    assert(!qualityHook.Start(query,guard,Mode::Exact,window,nativeCaller,nullptr,{3840,3840}));
    assert(!qualityHook.Start(query,guard,selected,nullptr,nativeCaller,nullptr,{2676,2728}));
    assert(qualityHook.Start(query,guard,selected,deferred?nullptr:window,nativeCaller,nullptr,{2676,2728},deferred));
    if(deferred) {
        assert(invoke(window,&rect) && rect.right==1920 && qualityHook.requests==0); // invalid HWND
        window=CreateWindowExW(0,L"STATIC",L"source sizing fixture",WS_POPUP,0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        assert(window);
        assert(other(window,&rect) && rect.right==1920 && qualityHook.requests==0); // wrong caller must not bind
        assert(!invoke(window,nullptr) && qualityHook.requests==0);
    }
    auto before=calls();assert(invoke(window,&rect));
    assert(calls()==before+1 && GetLastError()==0x1234);
    assert(rect.left==0 && rect.top==0 && rect.right==expectedW && rect.bottom==expectedH && qualityHook.requests==1);
    before=calls();assert(other(window,&rect));
    assert(calls()==before+1 && rect.right==1920 && rect.bottom==1071 && qualityHook.requests==1);
    before=calls();assert(!invoke(reinterpret_cast<HWND>(2),&rect));
    assert(calls()==before+1 && GetLastError()==0x1234 && rect.right==1920 && qualityHook.requests==1);
    before=calls();assert(!invoke(window,nullptr));
    assert(calls()==before+1 && GetLastError()==0x1234 && qualityHook.requests==1);
    qualityHook.Window(reinterpret_cast<HWND>(3));
    assert(invoke(window,&rect) && rect.right==1920 && qualityHook.requests==1);
    qualityHook.Window(window);
    for(unsigned i=0;i<1000;++i) {assert(invoke(window,&rect));assert(rect.right==expectedW && rect.bottom==expectedH);}
    assert(qualityHook.requests==1001);
    assert(Text()==originalText);
    if(deferred) assert(DestroyWindow(window));
    std::printf("PASS source quality mode=%s: bounded/aspect/exact sizing, native ABI, exact caller/window, failure/null, one forward, last error, image denial\n",exactMode?"Exact2676x2728":"Quality");
}

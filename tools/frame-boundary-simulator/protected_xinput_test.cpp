// Own-process system-XInput detour test. No game, Steam, OpenXR or user input.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <xinput.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../src/protected_xinput.h"

static d2vr_protected_input::ExternalXInput hook;
static unsigned checks=0,entries=0,nativeCalls=0;
static void Check(bool value) {++checks;if(!value){std::printf("FAIL check %u\n",checks);std::exit(1);}}
static DWORD WINAPI Replacement(DWORD user,XINPUT_STATE* state) {
    ++entries;
    if(user==0 && state) {
        state->dwPacketNumber=827;state->Gamepad.sThumbLX=-12345;
        state->Gamepad.wButtons=XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_LEFT_SHOULDER;
        return ERROR_SUCCESS;
    }
    ++nativeCalls;return hook.Native()(user,state);
}
static uint64_t TextHash(HMODULE module) {
    auto image=reinterpret_cast<const BYTE*>(module);
    auto pe=reinterpret_cast<const IMAGE_NT_HEADERS*>(image+reinterpret_cast<const IMAGE_DOS_HEADER*>(image)->e_lfanew);
    auto section=IMAGE_FIRST_SECTION(pe);uint64_t hash=1469598103934665603ull;
    for(unsigned n=0;n<pe->FileHeader.NumberOfSections;++n) if(section[n].Characteristics&IMAGE_SCN_MEM_EXECUTE)
        for(DWORD i=0;i<section[n].Misc.VirtualSize;++i) {hash^=image[section[n].VirtualAddress+i];hash*=1099511628211ull;}
    return hash;
}
int main() {
    using namespace d2vr_protected_input;
    for(unsigned bits=0;bits<16;++bits)
        Check(Requested((bits&1)!=0,(bits&2)!=0,(bits&4)!=0,(bits&8)!=0)==(bits==15));
    Grips grips;
    Check(grips.Update(.6f,.6f)==0);
    Check(grips.Update(.8f,.2f)==XINPUT_GAMEPAD_LEFT_SHOULDER);
    Check(grips.Update(.5f,.8f)==(XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_RIGHT_SHOULDER));
    Check(grips.Update(.1f,.5f)==XINPUT_GAMEPAD_RIGHT_SHOULDER);
    Check(grips.Update(.1f,.1f)==0);
    grips.Update(.9f,.9f);grips={};Check(grips.Update(.5f,.5f)==0);
    Check(grips.Update(.9f,.9f)==(XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_RIGHT_SHOULDER));
    Check(grips.Update(.9f,.9f,false,true)==XINPUT_GAMEPAD_RIGHT_SHOULDER);
    Check(grips.Update(.9f,.9f,true,true)==XINPUT_GAMEPAD_RIGHT_SHOULDER);
    Check(grips.Update(.1f,.9f)==XINPUT_GAMEPAD_RIGHT_SHOULDER);
    Check(grips.Update(.9f,.9f)==(XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_RIGHT_SHOULDER));
    Check(grips.Update(.9f,.9f,true,false)==XINPUT_GAMEPAD_LEFT_SHOULDER);
    Check(grips.Update(.9f,.9f)==XINPUT_GAMEPAD_LEFT_SHOULDER);
    Check(grips.Update(.9f,.1f)==XINPUT_GAMEPAD_LEFT_SHOULDER);
    Check(grips.Update(.9f,.9f)==(XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_RIGHT_SHOULDER));
    auto main=GetModuleHandleW(nullptr);auto image=reinterpret_cast<const BYTE*>(main);
    const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS*>(image+reinterpret_cast<const IMAGE_DOS_HEADER*>(image)->e_lfanew);
    d2vr_native_image::Guard guard{true,reinterpret_cast<uintptr_t>(main),pe->OptionalHeader.SizeOfImage};
    Check(!guard.Allows(reinterpret_cast<uintptr_t>(&Replacement),8));
    Check(!hook.Start(&Replacement,{}));
    const auto before=TextHash(main);
    Check(hook.Start(&Replacement,guard));Check(hook.Ready() && hook.Native());
    WCHAR path[MAX_PATH]{};GetSystemDirectoryW(path,MAX_PATH);wcscat_s(path,L"\\xinput1_3.dll");
    auto system=GetModuleHandleW(path);Check(system!=nullptr);
    auto get=reinterpret_cast<GetState>(GetProcAddress(system,"XInputGetState"));
    Check(get && get==reinterpret_cast<GetState>(GetProcAddress(system,MAKEINTRESOURCEA(2))));
    XINPUT_STATE value{};
    for(unsigned i=0;i<10000;++i) {
        Check(get(0,&value)==ERROR_SUCCESS && value.dwPacketNumber==827 && value.Gamepad.sThumbLX==-12345);
        Check(value.Gamepad.wButtons==(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_LEFT_SHOULDER));
    }
    // Unowned ports keep the original status and state. Port9 cannot be a real
    // attached controller, so the forwarding comparison is deterministic.
    XINPUT_STATE expected{},actual{};
    const auto expectedStatus=hook.Native()(9,&expected);
    Check(get(9,&actual)==expectedStatus && !std::memcmp(&actual,&expected,sizeof actual));
    Check(entries==10001 && nativeCalls==1);
    Check(hook.Start(&Replacement,guard)); // idempotent, no second detour
    Check(before==TextHash(main));
    std::printf("Protected XInput system export/ABI/forwarding/EXE-code preservation: %u checks PASS\n",checks);
}

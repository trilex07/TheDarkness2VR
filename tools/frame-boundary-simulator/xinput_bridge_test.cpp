// Compiles the real bridge/installer bodies, extracted by build-xinput-bridge.ps1.
// Only the import-table writes and physical controller provider are mocked.
#define NOMINMAX
#include <windows.h>
#include <Xinput.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../src/vr_input.h"
#include "../../src/native_aim_math.h"

using PFN_XInputGetState = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
PFN_XInputGetState g_origXInputGetState = nullptr;
volatile LONG g_xinputHooked=0, g_xinputAttempted=0, g_xinputRemapLogged=0;
volatile LONG g_cachedXrPadValid=0, g_headStickX=0, g_headStickY=0;
bool g_restoredTrackedGameplay=false, g_remapPhysical=false, g_enableVRGamepad=true;
bool g_headGeometry=true, g_in3DGameplayMode=true;
int g_headLookMode=0;
XINPUT_STATE g_cachedXrPad{}, physical[4]{};
bool connected[4]{}, delayResult=true, standardResult=false;
unsigned delayCalls=0, standardCalls=0, resetAimCalls=0, checks=0;
static void Check(bool value) {
    ++checks;
    if(!value) { std::fprintf(stderr,"FAIL check %u\n",checks); std::exit(1); }
}
static void Log(const char*, ...) {}
static void ResetNativeAim() { ++resetAimCalls; }
static void RequestRecenter(const char*) {}
static DWORD WINAPI PhysicalState(DWORD port, XINPUT_STATE* state) {
    if(port>=4 || !connected[port]) return ERROR_DEVICE_NOT_CONNECTED;
    *state=physical[port]; return ERROR_SUCCESS;
}
static bool HookDelayIATOrdinal(HMODULE module,const char* dll,WORD ordinal,void* hook,void** original) {
    ++delayCalls;
    Check(module && !_stricmp(dll,"XINPUT1_3.dll") && ordinal==2 && hook && original);
    if(delayResult) *original=reinterpret_cast<void*>(&PhysicalState);
    return delayResult;
}
static bool HookStandardIATOrdinal(HMODULE,const char*,WORD,void*,void** original) {
    ++standardCalls;
    if(standardResult) *original=reinterpret_cast<void*>(&PhysicalState);
    return standardResult;
}
// Synthetic native objects only; never read or patch the installed game.
struct NativeWeaponPose { bool valid=false; ULONGLONG time=0; float local[16]{}; };
struct NativeGameplaySample { DWORD camera=0, inventory=0, entities[2]{}; };
static NativeGameplaySample gameplay;
static NativeWeaponPose g_nativeWeaponPoses[2];
static SRWLOCK g_nativeWeaponLock=SRWLOCK_INIT;
static bool g_nativeWeaponPose=true, g_nativeWeaponInstalled=true;
static unsigned ownerReads=0, gameplayCopies=0;
static bool CopyNativeGameplay(NativeGameplaySample& out) { ++gameplayCopies; out=gameplay; return true; }
static bool NativeAimOwnerMatches(void*,DWORD) { ++ownerReads; return true; }
#include "d2vr_xinput_bridge_under_test.inl"

static void TestWeaponNullModel() {
    // Reproduce the runtime trigger: an empty secondary model slot plus a valid
    // tracked off-hand pose. The old function selected that slot and read [0].
    for(auto& pose:g_nativeWeaponPoses) {
        pose.valid=true; pose.time=GetTickCount64();
        pose.local[0]=pose.local[5]=pose.local[10]=pose.local[15]=1;
    }
    float world[16]; for(auto& v:world) v=123;
    Check(!TryNativeWeaponMatrix(nullptr,world));
    Check(gameplayCopies==0 && ownerReads==0);
    Check(std::all_of(world,world+16,[](float v){return v==123;}));
    Check(!TryNativeWeaponMatrix(reinterpret_cast<void*>(1),nullptr));
    Check(gameplayCopies==0 && ownerReads==0);

    // A real, non-empty synthetic primary gun must still produce its pose.
    alignas(16) BYTE camera[0x100]{},avatar[0x640]{},inventory[0xD0]{},gun[0x990]{},model[0x190]{};
    BYTE *avatarPtr=avatar,*inventoryPtr=inventory,*gunPtr=gun,*modelPtr=model;
    *reinterpret_cast<BYTE***>(camera+0x30)=&avatarPtr;
    *reinterpret_cast<BYTE***>(avatar+0x630)=&inventoryPtr;
    *reinterpret_cast<BYTE***>(inventory+0xB0)=&gunPtr;
    *reinterpret_cast<BYTE***>(gun+0x980)=&avatarPtr;
    *reinterpret_cast<BYTE***>(gun+0x12C)=&modelPtr;
    *reinterpret_cast<DWORD*>(gun)=0x1010E30;
    *reinterpret_cast<DWORD*>(model)=0xFD6BC0;
    auto base=reinterpret_cast<float*>(camera+0x80);
    base[0]=base[5]=base[10]=base[15]=1;
    gameplay.camera=static_cast<DWORD>(reinterpret_cast<uintptr_t>(camera));
    gameplay.inventory=static_cast<DWORD>(reinterpret_cast<uintptr_t>(inventory));
    gameplay.entities[0]=static_cast<DWORD>(reinterpret_cast<uintptr_t>(model));
    g_nativeWeaponPoses[1].time=GetTickCount64();
    Check(TryNativeWeaponMatrix(model,world));
    Check(world[0]==1 && world[5]==1 && world[10]==1 && world[15]==1);
    const auto reads=ownerReads;
    Check(!TryNativeWeaponMatrix(nullptr,world));
    Check(ownerReads==reads); // empty secondary slot cannot re-enter owner reads
}

static void ResetInstall() {
    g_xinputHooked=g_xinputAttempted=g_xinputRemapLogged=0;
    g_origXInputGetState=nullptr; delayCalls=standardCalls=0;
    delayResult=true; standardResult=false; g_restoredTrackedGameplay=false;
}
int main() {
    ResetInstall();
#ifdef D2VR_MATCHED_CAPTURE
    // DllMain/factory calls before the SHA/site-verified playable opt-in neither
    // write imports nor consume the later one-shot installation attempt.
    TryInstallXInputHook(); TryInstallXInputHook();
    Check(!g_xinputHooked && !g_xinputAttempted && !delayCalls && !standardCalls);
    g_restoredTrackedGameplay=true;
#endif
    TryInstallXInputHook();
    Check(g_xinputHooked==1 && g_xinputAttempted==1 && delayCalls==1 && !standardCalls);
    Check(g_origXInputGetState==&PhysicalState);
    TryInstallXInputHook(); Check(delayCalls==1 && !standardCalls);

    ResetInstall(); g_restoredTrackedGameplay=true; delayResult=false; standardResult=true;
    TryInstallXInputHook(); Check(g_xinputHooked==1 && delayCalls==1 && standardCalls==1);
    ResetInstall(); g_restoredTrackedGameplay=true; delayResult=false;
    TryInstallXInputHook(); TryInstallXInputHook();
    Check(!g_xinputHooked && g_xinputAttempted==1 && delayCalls==1 && standardCalls==1);

    ResetInstall(); g_restoredTrackedGameplay=true; TryInstallXInputHook();
    g_cachedXrPadValid=1;
    g_cachedXrPad.dwPacketNumber=17;
    g_cachedXrPad.Gamepad.sThumbLX=12000; g_cachedXrPad.Gamepad.sThumbLY=24000;
    g_cachedXrPad.Gamepad.sThumbRX=-16000; g_cachedXrPad.Gamepad.sThumbRY=9000;
    g_cachedXrPad.Gamepad.bRightTrigger=200;
    g_cachedXrPad.Gamepad.wButtons=XINPUT_GAMEPAD_Y|XINPUT_GAMEPAD_RIGHT_SHOULDER;
    XINPUT_STATE actual{};
    Check(Hook_XInputGetState(0,&actual)==ERROR_SUCCESS);
    Check(std::memcmp(&actual,&g_cachedXrPad,sizeof actual)==0);
    Check(Hook_XInputGetState(1,&actual)==ERROR_DEVICE_NOT_CONNECTED);
    Check(Hook_XInputGetState(0,nullptr)==ERROR_DEVICE_NOT_CONNECTED);

    // Physical input still takes priority; idle physical input allows VR.
    connected[0]=true; physical[0].Gamepad.sThumbLX=-24000;
    Check(Hook_XInputGetState(0,&actual)==ERROR_SUCCESS);
    Check(actual.Gamepad.sThumbLX==-24000 && resetAimCalls==1);
    physical[0]={}; Check(Hook_XInputGetState(0,&actual)==ERROR_SUCCESS);
    Check(actual.Gamepad.sThumbLY==24000 && actual.Gamepad.sThumbRX==-16000);
    connected[0]=false; g_cachedXrPadValid=0;
    Check(Hook_XInputGetState(0,&actual)==ERROR_DEVICE_NOT_CONNECTED);
    g_cachedXrPadValid=1; g_enableVRGamepad=false;
    Check(Hook_XInputGetState(0,&actual)==ERROR_DEVICE_NOT_CONNECTED);
    TestWeaponNullModel();
    std::printf("XInput bridge and null-model regression (%s): %u checks PASS\n",
#ifdef D2VR_MATCHED_CAPTURE
        "diagnostic opt-in",
#else
        "production",
#endif
        checks);
}

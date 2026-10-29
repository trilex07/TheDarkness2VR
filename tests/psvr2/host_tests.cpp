#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define D2VR_MATCHED_CAPTURE
#include <windows.h>
#include <xinput.h>
#include <openxr/openxr.h>
#include <cstdio>
#include <cstdlib>
#include "../../src/vr_input.h"
#include "../../src/psvr2/HapticDelivery.h"
#include "../../src/psvr2/ShotEvents.h"
using namespace d2vr::psvr2;
static unsigned checks=0,applies[2]{},stops[2]{},starts=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
namespace d2vr::psvr2 {
enum class ClientStartResult{Disabled,Running};
static TriggerCommand commands[2];
static ClientStartResult ClientStart(){++starts;return ClientStartResult::Running;}
static bool ClientRunning(){return true;}
static void Submit(const TriggerCommand& c){commands[c.hand==TriggerHand::Left?0:1]=c;}
static void SubmitBothOff(TriggerOffReason r){Submit(offCommand(TriggerHand::Left,r));Submit(offCommand(TriggerHand::Right,r));}
}
static ULONGLONG clockMs=1000;
static ULONGLONG Clock(){return clockMs;}
static void Log(const char*,...){}
static XrSession g_xrSession=(XrSession)1;
static XrSessionState g_xrSessionState=XR_SESSION_STATE_FOCUSED;
static XrAction g_actHaptic[2]={(XrAction)1,(XrAction)2};
static XrPath g_pathHandLeft=1,g_pathHandRight=2;
static bool g_gripHapticOn[2]{};
static HapticDelivery g_hapticDelivery[2];
static ULONGLONG g_hapticRetry[2]{};
static unsigned g_hapticEpoch=0;
static XrResult applyResult=XR_SUCCESS;
static XrResult stopResult=XR_SUCCESS;
XrResult XRAPI_CALL xrApplyHapticFeedback(XrSession,const XrHapticActionInfo* i,const XrHapticBaseHeader*) {
    ++applies[i->subactionPath==1?0:1];return applyResult;
}
XrResult XRAPI_CALL xrStopHapticFeedback(XrSession,const XrHapticActionInfo* i) {
    ++stops[i->subactionPath==1?0:1];return stopResult;
}
static bool g_adaptiveTriggers=true,g_in3DGameplayMode=true,g_tentacleBridge=true;
static struct{bool paused=false;}g_protectedLeftTentacle,g_protectedRightTentacle;
static struct{bool open=false;bool Open()const{return open;}}g_vrSettings;
static struct{bool aimValid=true,gripValid=true;struct{d2vr_input::Vec3 velocity{};}motion;}g_hands[2];
static d2vr_input::Tentacles g_tentacles;
static struct{ULONGLONG reloadUntil=0,reloadUntilByHand[2]{};bool grabbed=false;}g_gestures;
struct NativeGameplaySample{unsigned char firearm[2]{1,1},demonHold=0,shotOwnedMask=3;};
static NativeGameplaySample sample;
static bool CopyNativeGameplay(NativeGameplaySample& out){out=sample;return true;}
#define GetTickCount64 Clock
#include "actual_haptics.inl"
#undef GetTickCount64
int main(){
    XINPUT_GAMEPAD pad{};pad.bLeftTrigger=pad.bRightTrigger=255;
    PublishAdaptiveTriggers(&pad,true);
    CHECK(applies[0]==0 && applies[1]==0); // held trigger alone is not a native shot
    noteShot(0);PublishAdaptiveTriggers(&pad,true);
    CHECK(applies[0]==1 && applies[1]==0);
    for(unsigned i=0;i<100;++i)PublishAdaptiveTriggers(&pad,true);
    CHECK(applies[0]==1); // 100 polls do not restart the same shot
    clockMs+=61;PublishAdaptiveTriggers(&pad,true);
    CHECK(applies[0]==1 && stops[0]==0); // expired pulse needs no redundant Stop
    noteShot(1);PublishAdaptiveTriggers(&pad,true);
    CHECK(applies[1]==1);
    g_protectedRightTentacle.paused=true;PublishAdaptiveTriggers(&pad,true);
    CHECK(stops[1]==1 && commands[1].effect==TriggerEffect::Off && commands[0].effect==TriggerEffect::Off);
    g_protectedRightTentacle.paused=false;
    noteShot(0);noteShot(1);PublishAdaptiveTriggers(&pad,true);
    CHECK(applies[0]==2 && applies[1]==2);
    g_hands[0].aimValid=g_hands[0].gripValid=false;
    PublishAdaptiveTriggers(&pad,true);
    CHECK(stops[0]==1 && commands[0].effect==TriggerEffect::Off);
    CHECK(commands[1].effect!=TriggerEffect::Off);
    PublishAdaptiveTriggers(nullptr,false);
    CHECK(stops[1]==2 && commands[1].effect==TriggerEffect::Off);
    const auto stopped=stops[1];PublishAdaptiveTriggers(nullptr,false);CHECK(stops[1]==stopped);
    g_hands[0].aimValid=g_hands[0].gripValid=true;
    pad.bLeftTrigger=pad.bRightTrigger=0;pad.wButtons=XINPUT_GAMEPAD_RIGHT_SHOULDER;
    PublishAdaptiveTriggers(&pad,true);const auto strike=applies[1];CHECK(strike==3);
    for(unsigned i=0;i<100;++i)PublishAdaptiveTriggers(&pad,true);
    CHECK(applies[1]==strike);
    pad.wButtons=0;PublishAdaptiveTriggers(&pad,true);CHECK(stops[1]==stopped+1);
    pad.wButtons=XINPUT_GAMEPAD_RIGHT_SHOULDER;PublishAdaptiveTriggers(&pad,true);CHECK(applies[1]==strike+1);
    g_adaptiveTriggers=false;PublishAdaptiveTriggers(&pad,true);
    CHECK(commands[1].effect==TriggerEffect::Off && !g_gripHapticOn[1]);
    g_adaptiveTriggers=true;g_vrSettings.open=true;noteShot(1);PublishAdaptiveTriggers(&pad,true);
    CHECK(commands[1].effect==TriggerEffect::Off && !g_gripHapticOn[1]);
    // Failed submission is retried at a bounded rate; non-success positive XR
    // results must never be acknowledged as delivered.
    g_vrSettings.open=false;pad.wButtons=0;PublishAdaptiveTriggers(&pad,true);
    applyResult=XR_SESSION_NOT_FOCUSED;pad.wButtons=XINPUT_GAMEPAD_RIGHT_SHOULDER;
    const auto before=applies[1];PublishAdaptiveTriggers(&pad,true);
    CHECK(applies[1]==before+1 && !g_gripHapticOn[1]);
    for(unsigned i=0;i<100;++i)PublishAdaptiveTriggers(&pad,true);
    CHECK(applies[1]==before+1);
    clockMs+=100;applyResult=XR_SUCCESS;PublishAdaptiveTriggers(&pad,true);
    CHECK(applies[1]==before+2 && g_gripHapticOn[1]);
    stopResult=XR_ERROR_RUNTIME_FAILURE;StopGripHaptics();
    CHECK(g_gripHapticOn[1]);const auto failedStop=stops[1];
    for(unsigned i=0;i<100;++i)StopGripHaptics();
    CHECK(stops[1]==failedStop);
    clockMs+=10;stopResult=XR_SUCCESS;StopGripHaptics();
    CHECK(stops[1]==failedStop+1 && !g_gripHapticOn[1]);
    printf("PASS actual haptic host: %u checks; independent shots, dedupe, pause, focus, disconnect, settings, disable, retry\n",checks);
}

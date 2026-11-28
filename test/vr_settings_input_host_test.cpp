#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define D2VR_MATCHED_CAPTURE
#include <windows.h>
#include <xinput.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../src/vr_settings.h"
#include "../src/vr_input.h"
#include "../src/vr_gameplay_options.h"
static unsigned checks,resets,keyCalls;
static void Log(const char*,...){}
alignas(8) static volatile LONGLONG g_objectiveHudUntil=0;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL %d %s\n",__LINE__,#x);std::exit(1);}}while(0)
static int g_vrSettingsToggleKey=VK_INSERT;
static bool keyDown=false,g_haveViews=true;
static XrSessionState g_xrSessionState=XR_SESSION_STATE_FOCUSED;
static XrSpace g_xrSpace=(XrSpace)3;
static XrPosef g_eyePose[2]={{{0,0,0,1},{0,1.6f,0}},{{0,0,0,1},{0,1.6f,0}}};
static struct Hand {XrPosef aim{{0,0,0,1},{0,1.6f,0}};bool aimValid=true;float squeeze=0;} g_hands[2];
static struct {uint64_t id=1;bool renderTrackingValid=true;} g_trackedFrame;
static d2vr_settings::Panel g_vrSettings;
static d2vr_settings::ToggleAction g_vrSettingsToggle;
static bool g_in3DGameplayMode=false;
static bool NativeVrMainMenu(){return false;}
static bool MenuConfirmActive(){return false;}
static unsigned NativeActionMovies(){return 0;}
static bool jackieAllowed=false,g_headGeometry=true,g_headGeometryReferenceValid=true;
static bool NativeJackieInputAllowed(){return jackieAllowed;}
static bool CinemaSceneThisFrame(){return false;}
static XrPosef g_headGeometryReference=g_eyePose[0];
static struct {bool open=false;} g_wheelGate;
struct NativeGameplaySample {bool nativeTraversal=false;};
static bool ownerFresh=true,nativeTraversal=false;
static bool g_nativePlayerOwnerAdmitted=true;
static bool CopyNativeGameplay(NativeGameplaySample& out){out.nativeTraversal=nativeTraversal;return ownerFresh;}
static struct {void SyncNativePause(bool){}} g_protectedLeftTentacle,g_protectedRightTentacle;
static bool NativeHeldActionContext(){return false;}
static bool g_nativeCinematicPlaying=false,circleActive=false,circleDown=false;
static d2vr_input::ObjectiveButton g_objectiveButton;
static void ResetWeaponWheelInput(){}
static void ApplyVrSettingsAction(d2vr_settings::Action){}
static auto g_adaptiveFPSMode=d2vr_adaptive::Mode::Off;
static XrPosef HeadCenterPose(const XrPosef& a,const XrPosef&){return a;}
static SHORT StickShort(float value){return SHORT(value*32767);}
static XINPUT_GAMEPAD routed{};
static SHORT KeyState(int key){++keyCalls;CHECK(key==VK_INSERT);return keyDown?SHORT(-32768):0;}
static void ResetPoseInput(){++resets;for(auto& h:g_hands)h.aimValid=false;}
static bool Actual(XINPUT_STATE* out,XINPUT_GAMEPAD gp,float lt=0,float rt=0,
                   float lx=0,float ly=0,float rx=0,float ry=0,
                   bool triangleDown=false,bool menuDown=false,bool viewDown=false,bool l3=false,bool r3=false){
    const bool haveL=true;
#define GetAsyncKeyState KeyState
#include "actual_vr_settings_input.inl"
#undef GetAsyncKeyState
    routed=gp;
    return false; // host continues its existing gameplay route
}
static XINPUT_GAMEPAD physical{};
static XINPUT_STATE output{};
static bool Poll(bool key,float rt=0,bool l3=false,bool r3=false){
    keyDown=key;g_trackedFrame.id++;
    for(auto& hand:g_hands)hand.aimValid=true; // LocateTrackedHands, before route
    output.Gamepad=physical;
    return Actual(&output,physical,0,rt,0,0,0,0,false,false,false,l3,r3);
}
static bool Zero(const XINPUT_GAMEPAD& pad){const XINPUT_GAMEPAD zero{};return !memcmp(&pad,&zero,sizeof(pad));}
int main(){
    physical.wButtons=XINPUT_GAMEPAD_A;physical.bRightTrigger=255;
    const auto saved=physical;
    CHECK(!Poll(false));CHECK(!g_vrSettings.Open());
    CHECK(Poll(true,1));CHECK(g_vrSettings.Open());CHECK(!g_vrSettings.Presented());
    CHECK(Zero(output.Gamepad));CHECK(!memcmp(&physical,&saved,sizeof(saved)));
    CHECK(!g_hands[1].aimValid&&resets==1);
    g_vrSettings.Submitted(true);
    CHECK(Poll(false,1));CHECK(Zero(output.Gamepad));
    CHECK(Poll(true,1));CHECK(!g_vrSettings.Open());CHECK(Zero(output.Gamepad));
    // A held physical/XR action and stick chord cannot escape on menu close.
    CHECK(Poll(false,1,true,true));CHECK(Zero(output.Gamepad));
    physical={};CHECK(Poll(false,0,true,true));CHECK(Zero(output.Gamepad));
    CHECK(!Poll(false));CHECK(!g_vrSettings.OwnInput(true));
    CHECK(Poll(true));CHECK(g_vrSettings.Open());
    g_vrSettings.Submitted(false);
    CHECK(Poll(false,1));CHECK(!g_vrSettings.Open());CHECK(Zero(output.Gamepad));
    CHECK(!Poll(false));
    // An unfocused session cannot open the panel; fresh key release is needed.
    g_xrSessionState=XR_SESSION_STATE_VISIBLE;CHECK(!Poll(true));CHECK(!g_vrSettings.Open());
    g_xrSessionState=XR_SESSION_STATE_FOCUSED;CHECK(!Poll(true));
    CHECK(!Poll(false));CHECK(Poll(true));CHECK(g_vrSettings.Open());
    g_vrSettings.Close();CHECK(!Poll(false));
    const auto before=keyCalls;g_vrSettingsToggleKey=0;
    CHECK(!Poll(true));CHECK(keyCalls==before);CHECK(!g_vrSettings.Open());
    CHECK(!Poll(false));
    g_in3DGameplayMode=jackieAllowed=g_vrSettings.Options().headRelativeMovement=true;
    g_eyePose[0].orientation={0,1,0,0}; // looking back from the ladder
    XINPUT_GAMEPAD stick{};stick.sThumbLY=32767;stick.wButtons=XINPUT_GAMEPAD_A;
    CHECK(!Actual(&output,stick,0,0,0,1));
    CHECK(routed.sThumbLY==-32767);
    nativeTraversal=true;
    for(float vertical:{1.f,-1.f,0.f}){
        stick.sThumbLX=StickShort(.25f);stick.sThumbLY=StickShort(vertical);
        CHECK(!Actual(&output,stick,0,0,.25f,vertical));
        CHECK(!memcmp(&routed,&stick,sizeof(stick)));
    }
    ownerFresh=false;
    CHECK(!Actual(&output,stick,0,0,.25f,0));CHECK(routed.sThumbLX==stick.sThumbLX);
    ownerFresh=true;nativeTraversal=false;
    CHECK(!Actual(&output,stick,0,0,.25f,0));CHECK(routed.sThumbLX==-stick.sThumbLX);
    g_nativePlayerOwnerAdmitted=false;ownerFresh=false;
    CHECK(!Actual(&output,stick,0,0,.25f,0));CHECK(routed.sThumbLX==-stick.sThumbLX);
    std::printf("PASS actual VR settings host routing %u checks\n",checks);
}

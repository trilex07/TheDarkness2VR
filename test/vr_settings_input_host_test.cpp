#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <xinput.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../src/vr_settings.h"
static unsigned checks,resets,keyCalls;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL %d %s\n",__LINE__,#x);std::exit(1);}}while(0)
static int g_vrSettingsToggleKey=VK_INSERT;
static bool keyDown=false,g_haveViews=true;
static XrSessionState g_xrSessionState=XR_SESSION_STATE_FOCUSED;
static XrSpace g_xrSpace=(XrSpace)3;
static XrPosef g_eyePose[2]={{{0,0,0,1},{0,1.6f,0}},{{0,0,0,1},{0,1.6f,0}}};
static struct Hand {XrPosef aim{{0,0,0,1},{0,1.6f,0}};bool aimValid=true;float squeeze=0;} g_hands[2];
static struct {uint64_t id=1;} g_trackedFrame;
static d2vr_settings::Panel g_vrSettings;
static d2vr_settings::ToggleAction g_vrSettingsToggle;
static auto g_adaptiveFPSMode=d2vr_adaptive::Mode::Off;
static XrPosef HeadCenterPose(const XrPosef& a,const XrPosef&){return a;}
static SHORT KeyState(int key){++keyCalls;CHECK(key==VK_INSERT);return keyDown?SHORT(-32768):0;}
static void ResetPoseInput(){++resets;for(auto& h:g_hands)h.aimValid=false;}
static bool Actual(XINPUT_STATE* out,XINPUT_GAMEPAD gp,float lt=0,float rt=0,
                   float lx=0,float ly=0,float rx=0,float ry=0,
                   bool triangleDown=false,bool menuDown=false,bool viewDown=false,bool l3=false,bool r3=false){
#define GetAsyncKeyState KeyState
#include "actual_vr_settings_input.inl"
#undef GetAsyncKeyState
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
    std::printf("PASS actual VR settings host routing %u checks\n",checks);
}

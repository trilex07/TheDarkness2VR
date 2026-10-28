// Compiles actual production gesture bodies and existing Tentacles/DwellZone.
#define NOMINMAX
#include <windows.h>
#include <xinput.h>
#include <openxr/openxr.h>
#include <cstdio>
#include <cstdlib>
#include "../../src/vr_input.h"
static unsigned checks=0;
static void Check(bool value){++checks;if(!value){std::printf("FAIL check %u\n",checks);std::exit(1);}}
static void Log(const char*,...){}
static bool g_protectedGestureActions=true,g_gestureBridge=false,g_tentacleBridge=false;
static bool g_haveViews=true,g_in3DGameplayMode=true,g_headGeometryReferenceValid=false;
static XrPosef g_eyePose[2]{},g_headGeometryReference{};
struct Frame {struct State {XrTime predictedDisplayTime=0;} state;XrViewStateFlags flags=0;} g_trackedFrame;
struct Hand {XrPosef grip{};bool valid=true,gripValid=true;float squeeze=0;d2vr_input::Motion motion;} g_hands[2];
struct Gestures {d2vr_input::DwellZone objective,reload[2];d2vr_input::Slash slash;
    ULONGLONG objectiveUntil=0,reloadUntil=0,slashUntil=0;bool grabbed=false;} g_gestures;
static d2vr_input::Tentacles g_protectedRightTentacle;
static ULONGLONG clockMs=1000;
static ULONGLONG Clock(){return clockMs;}
#define GetTickCount64 Clock
#include "d2vr_protected_gestures_under_test.inl"
#undef GetTickCount64
static double timeNow=1;
static void Reset(){
    g_gestures={};g_protectedRightTentacle={};g_hands[0]={};g_hands[1]={};
    g_headGeometryReferenceValid=false;g_headGeometryReference={};g_headGeometryReference.orientation.w=1;
    g_haveViews=g_in3DGameplayMode=g_protectedGestureActions=true;
    g_trackedFrame.flags=XR_VIEW_STATE_POSITION_TRACKED_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
    for(auto& p:g_eyePose){p={};p.orientation.w=1;p.position.y=1.6f;}
    g_hands[0].grip.position={-.7f,1.1f,-.5f};g_hands[1].grip.position={.7f,1.1f,-.5f};
    timeNow=1;clockMs=1000;
}
static XINPUT_GAMEPAD Step(double dt=.01f,WORD buttons=0){
    timeNow+=dt;clockMs=ULONGLONG(timeNow*1000+.001);
    g_trackedFrame.state.predictedDisplayTime=XrTime(timeNow*1e9);
    XINPUT_GAMEPAD gp{};gp.wButtons=buttons;gp.sThumbLX=1234;gp.sThumbRX=4321;
    ApplyProtectedGestureActions(gp);gp.wButtons|=GestureButtons(gp);return gp;
}
int main(){
    Reset();Step();g_hands[1].squeeze=.9f;
    auto gp=Step(.01f,XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_RIGHT_SHOULDER|XINPUT_GAMEPAD_A);
    Check(!(gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER));
    Check((gp.wButtons&(XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_A))==(XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_A));
    Check(gp.sThumbLX==1234 && gp.sThumbRX==4321); // grip alone never attacks
    g_hands[1].motion.velocity={3,0,0};Step();gp=Step();
    Check((gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER) && gp.sThumbRX==0);
    gp=Step(.04);Check((gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER) && gp.sThumbRX>32000);
    g_hands[1].squeeze=0;Step();Step(.1);gp=Step(.04);Check(!(gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER));
    // Continuous same-direction movement cannot create another attack.
    for(unsigned i=0;i<40;++i){gp=Step();Check(!(gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER));}
    // A deliberate release/new grip can attack in the opposite direction.
    g_hands[1].squeeze=.9f;g_hands[1].motion.velocity={-3,0,0};Step();Step();gp=Step();
    Check(gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER);
    gp=Step(.04);Check(gp.sThumbRX < -32000);
    // Focus or tracking loss cancels the current pulse; held grip cannot fire
    // on reacquisition until released and rearmed.
    g_haveViews=false;gp=Step();Check(!(gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER));
    g_haveViews=true;
    for(unsigned i=0;i<40;++i){gp=Step();Check(!(gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER));}
    g_hands[1].squeeze=0;Step();g_hands[1].squeeze=.9f;Step();Step();gp=Step();
    Check(gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER);
    g_hands[1].valid=g_hands[1].gripValid=false;gp=Step();Check(!(gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER));
    // A slash consumes grip motion, never the separate aiming pose. Losing
    // aim tracking cannot disable a still tracked right grip or its stroke.
    Reset();g_hands[1].valid=false;Step();g_hands[1].squeeze=.9f;
    g_hands[1].motion.velocity={0,3,0};Step();Step();gp=Step();
    Check(gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER);
    gp=Step(.04);Check(gp.sThumbRY>32000);
    g_hands[1].gripValid=false;gp=Step();Check(!(gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER));
    // Left tracking loss does not disable a right vertical gesture. Test both
    // directions through the actual production gesture-to-stick mapping.
    for(float direction : {-1.f,1.f}) {
        Reset();g_hands[0].valid=false;Step();
        g_hands[1].squeeze=.9f;g_hands[1].motion.velocity={0,3*direction,0};
        Step();Step();gp=Step();Check(gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER);
        gp=Step(.04);Check(gp.sThumbRX==0 && gp.sThumbRY*direction>32000);
    }
    // The visible tentacle follows the current head orientation. A pad stroke
    // in that visual plane must keep its direction after turning/tilting the
    // head, independently of the level's old recenter reference.
    const float s=std::sqrt(.5f);
    const XrQuaternionf orientations[]={{0,s,0,s},{0,-s,0,s},{0,1,0,0},{s,0,0,s},{0,0,s,s}};
    for(const auto& orientation:orientations)for(unsigned axis=0;axis<2;++axis)for(float direction:{-1.f,1.f}) {
        Reset();g_headGeometryReferenceValid=true;g_headGeometryReference={};g_headGeometryReference.orientation.w=1;
        g_hands[0].valid=false;
        for(auto& eye:g_eyePose)eye.orientation=orientation;
        Step();g_hands[1].squeeze=.9f;
        const auto velocity=RotVec(orientation,axis==0?3*direction:0,axis==1?3*direction:0,0);
        g_hands[1].motion.velocity={velocity.x,velocity.y,velocity.z};
        Step();Step();gp=Step();Check(gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER);
        gp=Step(.04);
        Check(axis==0 ? (gp.sThumbRX*direction>32000 && std::abs(gp.sThumbRY)<2) :
                        (gp.sThumbRY*direction>32000 && std::abs(gp.sThumbRX)<2));
    }
    for(const auto& orientation:orientations) {
        Reset();Step();g_hands[1].squeeze=.9f;
        for(auto& eye:g_eyePose)eye.orientation=orientation;
        // Turning/tilting the head with a stationary pad cannot attack.
        for(unsigned i=0;i<40;++i)Check(!(Step().wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER));
        const auto forward=RotVec(orientation,0,0,3);
        g_hands[1].motion.velocity={forward.x,forward.y,forward.z};
        // Depth-only pad thrusts remain excluded from the stock planar slash.
        for(unsigned i=0;i<40;++i)Check(!(Step().wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER));
    }
    // Pause is latched until the next Start edge; no stroke leaks through.
    Reset();Step();gp=Step(.01,XINPUT_GAMEPAD_START);Check(g_protectedRightTentacle.paused);
    g_hands[1].squeeze=.9f;g_hands[1].motion.velocity={4,0,0};
    for(unsigned i=0;i<40;++i){gp=Step();Check(!(gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER));}
    Step(.01,XINPUT_GAMEPAD_START);Check(!g_protectedRightTentacle.paused);
    for(unsigned i=0;i<10;++i){gp=Step();Check(!(gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER));}
    // Objective uses a left-face dwell, reload uses the existing hip dwell.
    Reset();Step();g_hands[0].grip.position={-.18f,1.58f,-.08f};bool objective=false;
    for(unsigned i=0;i<30;++i)objective|=(Step().wButtons&XINPUT_GAMEPAD_BACK)!=0;
    Check(objective);
    for(unsigned i=0;i<30;++i)Step();Check(!(Step().wButtons&XINPUT_GAMEPAD_BACK));
    g_haveViews=false;Step();g_haveViews=true;
    for(unsigned i=0;i<40;++i)Check(!(Step().wButtons&XINPUT_GAMEPAD_BACK));
    Reset();Step();g_hands[1].grip.position={.23f,.85f,0};bool reload=false;
    for(unsigned i=0;i<25;++i)reload|=(Step().wButtons&XINPUT_GAMEPAD_X)!=0;
    Check(reload);
    Step(.01,XINPUT_GAMEPAD_START);
    for(unsigned i=0;i<40;++i)Check(!(Step().wButtons&(XINPUT_GAMEPAD_X|XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_RIGHT_SHOULDER)));
    // OFF mode leaves raw input intact; no native hook function exists here.
    Reset();g_protectedGestureActions=false;gp=Step(.01,XINPUT_GAMEPAD_RIGHT_SHOULDER);
    Check((gp.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER) && gp.sThumbRX==4321);
    std::printf("Protected gesture actual-body release/rearm/focus/pause/objective/reload: %u checks PASS\n",checks);
}

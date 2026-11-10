#define D2VR_MATCHED_CAPTURE
#include "../src/double_shake_reload.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
struct XINPUT_GAMEPAD {unsigned wButtons=0;unsigned char bLeftTrigger=0,bRightTrigger=0;};
static struct Hand {bool gripValid=true,aimValid=true;float squeeze=0;d2vr_input::Motion motion;} g_hands[2];
static struct {d2vr_input::DoubleShakeReload doubleShake[2];unsigned long long doubleShakeUntilByHand[2]{};} g_gestures;
static bool g_doubleShakeReload=true,g_protectedGestureActions=true,g_imagePreservingInput=true;
static constexpr int XR_SESSION_STATE_FOCUSED=5;
static int g_xrSessionState=XR_SESSION_STATE_FOCUSED;
static struct {bool open=false;} g_wheelGate;
static struct {bool down=false,consumed=false;} g_heartPull;
static int g_twoHandSupportIntent=0;
static int InterlockedCompareExchange(int* p,int,int){return *p;}
static bool menu=false,cinema=false,fresh=true;
static bool MenuConfirmActive(){return menu;}
static bool CinemaSceneThisFrame(){return cinema;}
struct NativeGameplaySample {
    d2vr_input::GrabFeedback grab=d2vr_input::GrabFeedback::Empty;
    bool alternateHeldTarget=false;unsigned demonHold=0;
    unsigned char firearm[2]{1,1};
};
static NativeGameplaySample sample;
static bool CopyNativeGameplay(NativeGameplaySample& out){out=sample;return fresh;}
static bool g_inputContext=true;
#include "actual_shake_wrapper.inl"
static XINPUT_GAMEPAD gp;
static double time=0;
static bool enabled=true;
static void Reset(){
    g_gestures={};g_hands[0]={};g_hands[1]={};gp={};sample={};
    g_doubleShakeReload=g_protectedGestureActions=g_imagePreservingInput=fresh=enabled=g_inputContext=true;
    g_xrSessionState=5;g_wheelGate.open=g_heartPull.down=g_heartPull.consumed=menu=cinema=false;
    g_twoHandSupportIntent=0;time=0;
}
static bool Step(int h,float velocity,int axis=1){
    time+=.01;auto& motion=g_hands[h].motion;motion.valid=true;motion.lastTime=time;
    motion.velocity={};if(axis==0)motion.velocity.x=velocity;else if(axis==1)motion.velocity.y=velocity;else motion.velocity.z=velocity;
    return DoubleShakeReloadGesture(h,enabled,gp,time);
}
static unsigned Pattern(int h,int axis=1){
    unsigned fired=0;Step(h,0,axis);
    for(float v:{1.4f,-1.4f,1.4f,-1.4f})for(int i=0;i<7;++i)fired+=Step(h,v,axis);
    return fired;
}
int main(){
    for(int h:{0,1})for(int axis:{0,1,2}){Reset();CHECK(Pattern(h,axis)==1);CHECK(Pattern(h,axis)==0);}
    // Different hands cannot combine incomplete gestures.
    Reset();Step(0,0);Step(1,0);
    for(int h:{0,1})for(float v:{1.4f,-1.4f})for(int i=0;i<6;++i)CHECK(!Step(h,v));
    // Each production ownership/tracking gate cancels the in-progress recognizer.
    for(int gate=0;gate<21;++gate){
        Reset();Step(0,0);for(int i=0;i<7;++i)CHECK(!Step(0,1.4f));
        switch(gate){
        case 0:g_doubleShakeReload=false;break;case 1:enabled=false;break;
        case 2:g_protectedGestureActions=false;break;case 3:g_imagePreservingInput=false;break;
        case 4:g_xrSessionState=3;break;case 5:menu=true;break;case 6:g_wheelGate.open=true;break;
        case 7:cinema=true;break;case 8:g_heartPull.down=true;break;case 9:g_heartPull.consumed=true;break;
        case 10:g_twoHandSupportIntent=1;break;case 11:g_hands[0].gripValid=false;break;
        case 12:g_hands[0].aimValid=false;break;case 13:g_hands[0].squeeze=.3f;break;
        case 14:gp.bRightTrigger=30;break;case 15:gp.wButtons=1;break;case 16:fresh=false;break;
        case 17:sample.grab=d2vr_input::GrabFeedback::Busy;break;
        case 18:sample.alternateHeldTarget=true;break;case 19:sample.firearm[1]=0;break;case 20:g_inputContext=false;break;
        }
        g_gestures.doubleShakeUntilByHand[0]=999;
        CHECK(!Step(0,-1.4f));CHECK(!g_gestures.doubleShake[0].armed);
        CHECK(g_gestures.doubleShakeUntilByHand[0]==0);
    }
    Reset();sample.grab=d2vr_input::GrabFeedback::Unknown;CHECK(Pattern(0)==0);
    Reset();sample.demonHold=1;CHECK(Pattern(1)==0);
    Reset();sample.firearm[0]=0;CHECK(Pattern(1)==0);CHECK(Pattern(0)==1);
    Reset();sample.firearm[1]=0;CHECK(Pattern(0)==0);CHECK(Pattern(1)==1);
    Reset();Step(0,0);g_hands[0].motion.lastTime=time-.15;
    CHECK(!DoubleShakeReloadGesture(0,true,gp,time));CHECK(!g_gestures.doubleShake[0].armed);
    Reset();Step(0,0);g_hands[0].motion.valid=false;
    CHECK(!DoubleShakeReloadGesture(0,true,gp,time));CHECK(!g_gestures.doubleShake[0].armed);
    Reset();for(int i=0;i<200;++i)CHECK(!Step(0,1.4f)); // no neutral arming
    Reset();Step(0,0);for(int i=0;i<200;++i)CHECK(!Step(0,1.4f)); // no alternation
    Reset();Step(0,0);for(int i=0;i<100;++i)CHECK(!Step(0,i%2?1.4f:-1.4f)); // sub35ms noise
    Reset();Step(0,0);CHECK(!Step(0,std::numeric_limits<float>::quiet_NaN()));
    Reset();Step(0,0);CHECK(!Step(0,std::numeric_limits<float>::infinity()));
    Reset();Step(0,0);time+=.2;CHECK(!Step(0,1.4f));CHECK(!g_gestures.doubleShake[0].armed);
    Reset();Step(0,0);time=-1;CHECK(!Step(0,1.4f));CHECK(!g_gestures.doubleShake[0].armed);
    Reset();Step(0,0);for(int i=0;i<7;++i)Step(0,1.4f);
    for(int i=0;i<100;++i)CHECK(!DoubleShakeReloadGesture(0,true,gp,time)); // same sample
    printf("PASS actual double-shake ownership and sequence %u checks\n",checks);
}

// Execute the actual LocateTrackedHands body against independently failing XR actions.
#define NOMINMAX
#include <windows.h>
#include <openxr/openxr.h>
#include <cstdio>
#include <cstdlib>
#include "../../src/vr_input.h"
static unsigned checks=0;
static void Check(bool v){++checks;if(!v){std::printf("FAIL %u\n",checks);std::exit(1);}}
static void Log(const char*,...){}
template<class T> static T Handle(unsigned v){return (T)(uintptr_t)v;}
static constexpr XrSpaceLocationFlags required=XR_SPACE_LOCATION_POSITION_VALID_BIT |
    XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT |
    XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
struct Action {bool active=true;XrResult state=XR_SUCCESS,locate=XR_SUCCESS;
    XrSpaceLocationFlags flags=required;XrPosef pose{{0,0,0,1},{0,0,0}};} actions[4];
static unsigned locateCalls[4]{};
static bool g_haveViews=true;
static XrSession g_xrSession{};static XrSpace g_xrSpace{};static XrInstance g_xrInstance{};
static XrAction g_actGripPose[2]{Handle<XrAction>(1),Handle<XrAction>(3)};
static XrAction g_actAimPose[2]{Handle<XrAction>(2),Handle<XrAction>(4)},g_actSqueeze[2]{};
static XrSpace g_handGripSpace[2]{Handle<XrSpace>(1),Handle<XrSpace>(3)};
static XrSpace g_handAimSpace[2]{Handle<XrSpace>(2),Handle<XrSpace>(4)};
static XrPath g_pathHandLeft=1,g_pathHandRight=2;
struct Frame {XrFrameState state{XR_TYPE_FRAME_STATE};uint64_t id=1;} g_trackedFrame;
static bool ReadFloat(XrAction,XrPath path,float* out){*out=path==1?.8f:.9f;return true;}
static XrResult FakeState(XrSession,const XrActionStateGetInfo* info,XrActionStatePose* out){
    auto& a=actions[(uintptr_t)info->action-1];out->isActive=a.active;return a.state;
}
static XrResult FakeLocate(XrSpace space,XrSpace,XrTime time,XrSpaceLocation* out){
    Check(time==g_trackedFrame.state.predictedDisplayTime);
    auto index=(uintptr_t)space-1;++locateCalls[index];auto& a=actions[index];
    out->locationFlags=a.flags;out->pose=a.pose;return a.locate;
}
static XrResult FakeProfile(XrSession,XrPath,XrInteractionProfileState*){return XR_ERROR_RUNTIME_FAILURE;}
static XrResult FakePath(XrInstance,XrPath,uint32_t,uint32_t*,char*){return XR_ERROR_RUNTIME_FAILURE;}
#define xrGetActionStatePose FakeState
#define xrLocateSpace FakeLocate
#define xrGetCurrentInteractionProfile FakeProfile
#define xrPathToString FakePath
#include "d2vr_tracked_hands_under_test.inl"
static void Step(){g_trackedFrame.state.predictedDisplayTime+=10000000;LocateTrackedHands();}
static void Reset(){for(auto& a:actions)a={};for(auto& h:g_hands)h={};
    for(auto& c:locateCalls)c=0;g_haveViews=true;g_trackedFrame.state.predictedDisplayTime=1000000000;}
int main(){
    for(unsigned bits=0;bits<16;++bits){
        Reset();for(unsigned i=0;i<4;++i)actions[i].active=(bits&(1u<<i))!=0;Step();
        for(unsigned h=0;h<2;++h){
            const bool grip=(bits&(1u<<(h*2)))!=0,aim=(bits&(2u<<(h*2)))!=0;
            Check(g_hands[h].gripValid==grip);Check(g_hands[h].aimValid==aim);
            Check(g_hands[h].valid==(grip&&aim));Check(g_hands[h].motion.valid==grip);
            Check(locateCalls[h*2]==unsigned(grip));Check(locateCalls[h*2+1]==unsigned(aim));
        }
    }
    for(unsigned failed=0;failed<4;++failed)for(unsigned mode=0;mode<6;++mode){
        Reset();Step();
        auto& a=actions[failed];
        if(mode==0)a.state=XR_ERROR_RUNTIME_FAILURE;
        else if(mode==1)a.locate=XR_ERROR_RUNTIME_FAILURE;
        else a.flags &= ~(XrSpaceLocationFlags(1)<<(mode-2));
        for(unsigned i=0;i<4;++i)actions[i].pose.position.x+=.01f;
        Step();
        for(unsigned h=0;h<2;++h){
            Check(g_hands[h].gripValid==(failed!=h*2));
            Check(g_hands[h].aimValid==(failed!=h*2+1));
            Check(g_hands[h].valid==(failed/2!=h));
            Check(std::fabs(g_hands[h].motion.velocity.x-(failed==h*2?0.f:1.f))<.001f);
        }
        a={};Step();
        Check(g_hands[failed/2].valid);
        if(failed%2==0)Check(d2vr_input::Length(g_hands[failed/2].motion.velocity)==0);
    }
    Reset();Step();g_haveViews=false;
    for(auto& c:locateCalls)c=0;Step();
    for(unsigned h=0;h<2;++h){Check(!g_hands[h].valid&&!g_hands[h].gripValid&&!g_hands[h].aimValid);
        Check(!g_hands[h].motion.valid);Check(!locateCalls[2*h]&&!locateCalls[2*h+1]);}
    g_haveViews=true;Step();for(auto& h:g_hands)Check(d2vr_input::Length(h.motion.velocity)==0);
    std::printf("Tracked hands actual-body action isolation/failure/reacquisition: %u checks PASS\n",checks);
}

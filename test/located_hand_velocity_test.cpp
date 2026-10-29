// Actual LocateTrackedHands body with a deterministic OpenXR runtime boundary.
#define NOMINMAX
#include <windows.h>
#include <openxr/openxr.h>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include "../src/vr_input.h"
static unsigned checks=0,locates[2][2]{};
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static bool Near(float a,float b){return std::fabs(a-b)<.00002f;}
template<class T>static T Handle(unsigned value){return (T)uintptr_t(value);}
struct TrackedHand {
    bool valid=false,gripValid=false,aimValid=false;float squeeze=0;
    XrSpaceLocationFlags flags=0;XrPosef grip{},aim{};d2vr_input::Motion motion;
} g_hands[2];
static XrAction g_actSqueeze[2]={Handle<XrAction>(1),Handle<XrAction>(2)};
static XrAction g_actGripPose[2]={Handle<XrAction>(3),Handle<XrAction>(4)};
static XrAction g_actAimPose[2]={Handle<XrAction>(5),Handle<XrAction>(6)};
static XrSpace g_handGripSpace[2]={Handle<XrSpace>(7),Handle<XrSpace>(8)};
static XrSpace g_handAimSpace[2]={Handle<XrSpace>(9),Handle<XrSpace>(10)};
static XrSpace g_xrSpace=Handle<XrSpace>(11);
static XrSession g_xrSession=Handle<XrSession>(12);
static XrInstance g_xrInstance=Handle<XrInstance>(13);
static XrPath g_pathHandLeft=14,g_pathHandRight=15;
static bool g_haveViews=true;
static struct {uint64_t id=1;XrFrameState state{XR_TYPE_FRAME_STATE};} g_trackedFrame;
static constexpr XrSpaceLocationFlags tracked=XR_SPACE_LOCATION_POSITION_VALID_BIT|
    XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_TRACKED_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
struct RuntimeHand {
    bool active[2]={true,true},fail[2]{};
    XrSpaceLocationFlags flags[2]={tracked,tracked};
    XrSpaceVelocityFlags velocityFlags=XR_SPACE_VELOCITY_LINEAR_VALID_BIT;
    XrVector3f position{},velocity{2,3,4};
} runtime[2];
static void Log(const char*,...){}
static bool ReadFloat(XrAction,XrPath,float* value){*value=0;return true;}
static XrResult GetPose(XrSession,const XrActionStateGetInfo* info,XrActionStatePose* out){
    for(unsigned h=0;h<2;++h)for(unsigned kind=0;kind<2;++kind)
        if(info->action==(kind?g_actAimPose[h]:g_actGripPose[h])){
            out->isActive=runtime[h].active[kind];return XR_SUCCESS;
        }
    CHECK(false);return XR_ERROR_HANDLE_INVALID;
}
static XrResult Locate(XrSpace space,XrSpace base,XrTime time,XrSpaceLocation* out){
    CHECK(base==g_xrSpace && time==g_trackedFrame.state.predictedDisplayTime);
    for(unsigned h=0;h<2;++h)for(unsigned kind=0;kind<2;++kind)
        if(space==(kind?g_handAimSpace[h]:g_handGripSpace[h])){
            ++locates[h][kind];CHECK(out->type==XR_TYPE_SPACE_LOCATION);
            // Production must request velocity on the already existing grip
            // locate only, with fresh output storage, not on an extra call.
            if(!kind && out->next){
                auto* v=static_cast<XrSpaceVelocity*>(out->next);
                CHECK(v->type==XR_TYPE_SPACE_VELOCITY && !v->next && !v->velocityFlags);
                v->velocityFlags=runtime[h].velocityFlags;v->linearVelocity=runtime[h].velocity;
            } else if(kind)CHECK(!out->next);
            out->locationFlags=runtime[h].flags[kind];
            out->pose={{0,0,0,1},runtime[h].position};
            return runtime[h].fail[kind]?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;
        }
    CHECK(false);return XR_ERROR_HANDLE_INVALID;
}
static XrResult Profile(XrSession,XrPath,XrInteractionProfileState*){return XR_ERROR_HANDLE_INVALID;}
static XrResult PathString(XrInstance,XrPath,uint32_t,uint32_t*,char*){return XR_ERROR_HANDLE_INVALID;}
#define xrLocateSpace Locate
#define xrGetActionStatePose GetPose
#define xrGetCurrentInteractionProfile Profile
#define xrPathToString PathString
#include "actual_located_hands.inl"
static void Reset(){
    for(unsigned h=0;h<2;++h){runtime[h]={};g_hands[h]={};locates[h][0]=locates[h][1]=0;}
    g_haveViews=true;g_trackedFrame.state.predictedDisplayTime=1000000000;
}
static void Step(double dt=.02){g_trackedFrame.state.predictedDisplayTime+=XrTime(dt*1e9);LocateTrackedHands();}
static void Zero(d2vr_input::Vec3 v){CHECK(v.x==0 && v.y==0 && v.z==0);}
int main(){
    Reset();Step();Zero(g_hands[0].motion.velocity);
    // Identical predicted positions can accompany nonzero instantaneous speed:
    // a runtime estimate must not be replaced by their zero finite difference.
    runtime[1].velocity={-4,1,2};Step();
    CHECK(g_hands[0].motion.velocity.x==2 && g_hands[0].motion.velocity.y==3 && g_hands[0].motion.velocity.z==4);
    CHECK(g_hands[1].motion.velocity.x==-4 && g_hands[1].motion.velocity.y==1);
    for(unsigned h=0;h<2;++h)CHECK(locates[h][0]==2 && locates[h][1]==2);
    // Missing LINEAR_VALID, angular-only, invalid numeric velocity: exact old
    // position-based fallback; output from the preceding locate cannot leak.
    for(auto flags:{XrSpaceVelocityFlags(0),XrSpaceVelocityFlags(XR_SPACE_VELOCITY_ANGULAR_VALID_BIT)}){
        Reset();runtime[0].velocityFlags=flags;Step();runtime[0].position={.02f,.04f,-.06f};Step();
        const auto v=g_hands[0].motion.velocity;CHECK(Near(v.x,1)&&Near(v.y,2)&&Near(v.z,-3));
    }
    Reset();Step();runtime[0].velocityFlags=0;Step();Zero(g_hands[0].motion.velocity);
    for(float bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),std::numeric_limits<float>::max()}){
        Reset();Step();runtime[0].velocity={bad,bad,bad};runtime[0].position.x=.02f;Step();
        CHECK(Near(g_hands[0].motion.velocity.x,1));
    }
    // Aim failure does not cancel grip motion; one hand cannot cancel its peer.
    Reset();Step();runtime[0].fail[1]=true;Step();
    CHECK(!g_hands[0].valid && g_hands[0].gripValid && g_hands[0].motion.velocity.x==2);
    CHECK(g_hands[1].valid && g_hands[1].motion.velocity.x==2);
    for(unsigned mode=0;mode<4;++mode){
        Reset();Step();
        if(mode==0)runtime[0].active[0]=false;
        if(mode==1)runtime[0].fail[0]=true;
        if(mode==2)runtime[0].flags[0]=XR_SPACE_LOCATION_POSITION_VALID_BIT;
        if(mode==3)g_haveViews=false;
        Step();Zero(g_hands[0].motion.velocity);
        runtime[0]={};g_haveViews=true;Step();Zero(g_hands[0].motion.velocity);Step();CHECK(g_hands[0].motion.velocity.x==2);
    }
    for(double gap:{.001,.101,-.01}){Reset();Step();Step(gap);Zero(g_hands[0].motion.velocity);}
    Reset();Step();runtime[0].position.x=.6f;Step();Zero(g_hands[0].motion.velocity);
    for(float bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        Reset();Step();runtime[0].position.x=bad;Step();Zero(g_hands[0].motion.velocity);
        runtime[0].position.x=0;Step();Zero(g_hands[0].motion.velocity);Step();CHECK(g_hands[0].motion.velocity.x==2);
    }
    // A known accelerating trajectory, including the reported slow frame range.
    // Compare to its exact instantaneous derivative, not to headset performance.
    for(double hz:{40.,45.,72.,90.,120.}){
        d2vr_input::Motion actual,fallback;actual.Update(true,{},1);fallback.Update(true,{},1);
        const double dt=1/hz;
        for(unsigned n=1;n<=10;++n){
            const float t=float(n*dt);const d2vr_input::Vec3 p{t*t,0,0},v{2*t,0,0};
            actual.Update(true,p,1+n*dt,&v);fallback.Update(true,p,1+n*dt);
            CHECK(Near(actual.velocity.x,2*t));CHECK(Near(fallback.velocity.x,2*t-float(dt)));
        }
    }
    for(double bad:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){
        d2vr_input::Motion motion;const d2vr_input::Vec3 v{3,0,0};
        motion.Update(true,{},1,&v);motion.Update(true,{},bad,&v);Zero(motion.velocity);
        motion.Update(true,{},2,&v);Zero(motion.velocity);
        motion.Update(true,{},2.02,&v);CHECK(motion.velocity.x==3);
    }
    // Actual located velocity -> existing powered stroke -> release direction.
    // Use constant z speed with a runtime-provided x component; a position-only
    // consumer would lose that component. This is a boundary fixture, not a
    // simulation of the runtime's prediction estimator or native enemy physics.
    for(double hz:{40.,45.,72.,90.,120.}){
        Reset();d2vr_input::Tentacles tentacle;const double dt=1/hz;
        auto gesture=[&](float grip){
            const auto& p=g_hands[0].grip.position;
            return tentacle.Update(true,false,g_hands[0].gripValid,false,grip,0,
                {p.x,p.y,p.z},g_hands[0].motion.velocity,{},
                g_trackedFrame.state.predictedDisplayTime*1e-9,d2vr_input::GrabFeedback::Busy,{0,0,-1});
        };
        Step(dt);gesture(0);Step(dt);gesture(1);
        for(unsigned n=1;n<=unsigned(std::ceil(.25*hz));++n){
            runtime[0].position.z=-3*float(n*dt);runtime[0].velocity={1,0,-3};Step(dt);
            CHECK(!gesture(1).throwObject);
        }
        Step(dt);auto released=gesture(0);CHECK(released.throwObject);
        d2vr_input::Vec3 direction{};
        CHECK(d2vr_input::ThrowDirection(released.throwVelocity,released.throwAim,0,direction));
        CHECK(Near(direction.x,1/std::sqrt(10.f)) && Near(direction.z,-3/std::sqrt(10.f)));
        for(unsigned n=0;n<10;++n){Step(dt);CHECK(!gesture(0).throwObject);}
    }
    puts("PASS actual LocateTrackedHands and Motion: runtime velocity, no extra locates, per-hand/aim independence, fallback, recovery/jump/gap guards, exact accelerating trajectory at 40/45/72/90/120 Hz");
    printf("%u checks; mocked OpenXR boundary, no physical throw or FPS claim\n",checks);
}

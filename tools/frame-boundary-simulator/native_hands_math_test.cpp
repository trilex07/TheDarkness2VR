#include "../../src/native_aim_math.h"
#include "../../src/native_hands_math.h"
#include "../../src/vr_input.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>

using d2vr_hands::V;
static unsigned checks=0;
static void Check(bool ok,const char* what,unsigned iteration) {
    ++checks;
    if(!ok) { std::fprintf(stderr,"FAIL %s case=%u check=%u\n",what,iteration,checks);std::exit(1); }
}
static float Range(std::mt19937& rng,float lo,float hi) {
    return std::uniform_real_distribution<float>(lo,hi)(rng);
}
static V Unit(std::mt19937& rng) {
    V v{};
    do {v={Range(rng,-1,1),Range(rng,-1,1),Range(rng,-1,1)};}
    while(d2vr_hands::Length(v)<.05f);
    return v*(1/d2vr_hands::Length(v));
}
static void Matrix(float m[16],V p,float yaw=0) {
    const float c=std::cos(yaw),s=std::sin(yaw);
    const float value[16]={c,0,-s,0, 0,1,0,0, s,0,c,0, p.x,p.y,p.z,1};
    std::memcpy(m,value,sizeof(value));
}
static V Transform(V v,const float m[16]) {
    return {v.x*m[0]+v.y*m[4]+v.z*m[8]+m[12],
            v.x*m[1]+v.y*m[5]+v.z*m[9]+m[13],
            v.x*m[2]+v.y*m[6]+v.z*m[10]+m[14]};
}
static float Distance(V a,V b) {return d2vr_hands::Length(a-b);}
static bool Near(V a,V b,float tolerance=.0008f) {return Distance(a,b)<tolerance;}

int main(int argc,char** argv) {
    // Fixed seed makes every failed geometry reproducible. Check independent
    // position, length, scope and rigid-finger invariants, not implementation
    // branch counts.
    const uint32_t seed=argc>1 ? (uint32_t)std::strtoul(argv[1],nullptr,0):0xD2A11725u;
    std::mt19937 rng(seed);
    // Free -> pistol support -> left grip -> ordinary-prop grab -> release.
    // The native grab must receive a pristine left arm, and a changed native
    // pose must never be overwritten by the cache cleanup.
    {
        float pristine[130][16];for(auto& bone:pristine) Matrix(bone,{0,0,0});
        Matrix(pristine[3],{0,0,0});Matrix(pristine[4],{.3f,0,0});
        Matrix(pristine[5],{.6f,0,0});Matrix(pristine[6],{.64f,0,0});
        Matrix(pristine[22],{.3f,0,0});Matrix(pristine[23],{0,0,0});
        float palette[130][16];std::memcpy(palette,pristine,sizeof(palette));
        d2vr_input::HandSupport support;
        Check(!support.Update(0,true,false,.1f),"support dwell",0);
        Check(support.Update(.13,true,false,.1f),"support engaged",0);
        float supportTarget[16];Matrix(supportTarget,{.35f,.25f,0});
        Check(d2vr_hands::PoseArm(palette,3,4,5,22,23,supportTarget),"support wrist pose",0);
        float applied[22][16],original[22][16];
        std::memcpy(applied,palette+2,sizeof(applied));
        std::memcpy(original,pristine+2,sizeof(original));
        float mixed[130][16];std::memcpy(mixed,palette,sizeof(mixed));
        Matrix(mixed[27],{.1f,.4f,0}); // native changed only the other arm
        Check(d2vr_hands::RebaseArm(mixed[2],palette[2],applied[0],original[0]),
              "left support rebases independently of right arm",0);
        Check(std::memcmp(mixed+2,pristine+2,sizeof(original))==0,
              "left source remains native after right arm update",0);
        Check(Near(d2vr_hands::Position(mixed[27]),{.1f,.4f,0}),
              "right native edit remains untouched",0);
        Check(!support.Update(.14,false,false,.1f),"left grip releases support",0);
        Check(!d2vr_input::CanRestoreSupportDuringGrab(d2vr_input::GrabFeedback::Busy,0,false),
              "acquisition without attached prop cannot edit palette",0);
        Check(!d2vr_input::CanRestoreSupportDuringGrab(d2vr_input::GrabFeedback::Busy,1,true),
              "enemy grab cannot edit palette",0);
        Check(d2vr_input::CanRestoreSupportDuringGrab(d2vr_input::GrabFeedback::Busy,1,false),
              "ordinary attached prop may release owned support",0);
        Check(d2vr_hands::RestoreOwnedArm(palette[2],applied[0],original[0]),
              "busy prop restores owned support pose",0);
        Check(std::memcmp(palette,pristine,sizeof(palette))==0,
              "native grab receives original palette",0);
        Matrix(palette[5],{.46f,.1f,0}); // native grab animation owns this wrist
        float nativeEdited[130][16];std::memcpy(nativeEdited,palette,sizeof(palette));
        Check(!d2vr_hands::RestoreOwnedArm(palette[2],applied[0],original[0]),
              "changed native grab palette rejected",0);
        Check(std::memcmp(palette,nativeEdited,sizeof(palette))==0,
              "busy native animation untouched",0);
        std::memcpy(palette,pristine,sizeof(palette));
        float freeTarget[16];Matrix(freeTarget,{.37f,-.20f,0});
        Check(d2vr_hands::PoseArm(palette,3,4,5,22,23,freeTarget),
              "controller pose resumes after grab",0);
        Check(Near(d2vr_hands::Position(palette[5]),d2vr_hands::Position(freeTarget)),
              "released left wrist follows controller",0);
    }
    constexpr unsigned reachableCases=20000;
    for(unsigned k=0;k<reachableCases;++k) {
        float bones[130][16];
        for(auto& bone:bones) Matrix(bone,{0,0,0});
        const V shoulder={Range(rng,-.8f,.8f),Range(rng,-.8f,.8f),Range(rng,-.8f,.8f)};
        const float upper=Range(rng,.22f,.48f),lower=Range(rng,.22f,.48f);
        const V elbow=shoulder+Unit(rng)*upper;
        const V wrist=elbow+Unit(rng)*lower;
        const V fingertip=wrist+Unit(rng)*.045f;
        Matrix(bones[25],shoulder);Matrix(bones[26],elbow);
        Matrix(bones[27],wrist);Matrix(bones[28],fingertip);
        Matrix(bones[44],elbow);Matrix(bones[45],shoulder);
        const V leftShoulder={Range(rng,-.8f,.8f),Range(rng,-.8f,.8f),Range(rng,-.8f,.8f)};
        const float leftUpper=Range(rng,.22f,.48f),leftLower=Range(rng,.22f,.48f);
        const V leftElbow=leftShoulder+Unit(rng)*leftUpper;
        const V leftWrist=leftElbow+Unit(rng)*leftLower;
        Matrix(bones[3],leftShoulder);Matrix(bones[4],leftElbow);
        Matrix(bones[5],leftWrist);Matrix(bones[6],leftWrist+V{.045f,0,0});
        Matrix(bones[22],leftElbow);Matrix(bones[23],leftShoulder);
        float original[130][16];std::memcpy(original,bones,sizeof(bones));
        const float minDist=std::max(std::fabs(upper-lower)+.02f,.04f);
        const float maxDist=upper+lower-.02f;
        const V targetPosition=shoulder+Unit(rng)*Range(rng,minDist,maxDist);
        float target[16];Matrix(target,targetPosition,Range(rng,-3.0f,3.0f));
        Check(d2vr_hands::PoseArm(bones,25,26,27,44,45,target),"reachable solve",k);
        Check(Near(d2vr_hands::Position(bones[27]),targetPosition),"wrist target",k);
        Check(std::fabs(Distance(d2vr_hands::Position(bones[25]),d2vr_hands::Position(bones[26]))-upper)<.001f,
              "upper length",k);
        Check(std::fabs(Distance(d2vr_hands::Position(bones[26]),d2vr_hands::Position(bones[27]))-lower)<.001f,
              "lower length",k);
        Check(std::fabs(Distance(d2vr_hands::Position(bones[28]),d2vr_hands::Position(bones[27]))-.045f)<.001f,
              "finger rigid with wrist",k);
        Check(Near(d2vr_hands::Position(bones[44]),d2vr_hands::Position(bones[26])),"lower twist follows elbow",k);
        Check(Near(d2vr_hands::Position(bones[45]),d2vr_hands::Position(bones[25])),"upper twist follows shoulder",k);
        for(int i=0;i<130;++i) {
            if(i<25 || i>45) Check(std::memcmp(bones[i],original[i],sizeof(bones[i]))==0,
                                   "other arm/tentacle/world bone untouched",k);
            else Check(d2vr_hands::Valid(bones[i]),"finite rigid arm bone",k);
        }
        float rightAfter[21][16];std::memcpy(rightAfter,bones+25,sizeof(rightAfter));
        const float leftMin=std::max(std::fabs(leftUpper-leftLower)+.02f,.04f);
        const V leftRequested=leftShoulder+Unit(rng)*Range(rng,leftMin,leftUpper+leftLower-.02f);
        float leftTarget[16];Matrix(leftTarget,leftRequested,Range(rng,-3.0f,3.0f));
        Check(d2vr_hands::PoseArm(bones,3,4,5,22,23,leftTarget),"left reachable solve",k);
        Check(Near(d2vr_hands::Position(bones[5]),leftRequested),"left wrist target",k);
        Check(std::fabs(Distance(d2vr_hands::Position(bones[3]),d2vr_hands::Position(bones[4]))-leftUpper)<.001f,
              "left upper length",k);
        Check(std::fabs(Distance(d2vr_hands::Position(bones[4]),d2vr_hands::Position(bones[5]))-leftLower)<.001f,
              "left lower length",k);
        Check(std::memcmp(rightAfter,bones+25,sizeof(rightAfter))==0,
              "left solve preserves right arm",k);
        for(int i=47;i<120;++i)
            Check(std::memcmp(bones[i],original[i],sizeof(bones[i]))==0,
                  "left solve preserves demon tentacles",k);
    }
    constexpr unsigned stretchedCases=4096;
    for(unsigned k=0;k<stretchedCases;++k) {
        float bones[130][16];for(auto& bone:bones) Matrix(bone,{0,0,0});
        const V shoulder={Range(rng,-.5f,.5f),Range(rng,-.5f,.5f),Range(rng,-.5f,.5f)};
        const float upper=Range(rng,.25f,.4f),lower=Range(rng,.25f,.4f);
        const V elbow=shoulder+Unit(rng)*upper,wrist=elbow+Unit(rng)*lower;
        Matrix(bones[25],shoulder);Matrix(bones[26],elbow);Matrix(bones[27],wrist);
        Matrix(bones[44],elbow);Matrix(bones[45],shoulder);
        const V direction=Unit(rng);
        const float beyond=Range(rng,.05f,.5f);
        const V requested=shoulder+direction*(upper+lower+beyond);
        float target[16];Matrix(target,requested);
        Check(d2vr_hands::PoseArm(bones,25,26,27,44,45,target),"stretch solve",k);
        Check(Near(d2vr_hands::Position(bones[27]),requested),"stretched wrist target",k);
        Check(Near(d2vr_hands::Position(bones[25]),shoulder+direction*(beyond+.001f)),
              "limited shoulder translation",k);
        Check(std::fabs(Distance(d2vr_hands::Position(bones[25]),d2vr_hands::Position(bones[26]))-upper)<.001f,
              "stretched upper length",k);
        Check(std::fabs(Distance(d2vr_hands::Position(bones[26]),d2vr_hands::Position(bones[27]))-lower)<.001f,
              "stretched lower length",k);
    }
    constexpr unsigned insideCases=2048;
    for(unsigned k=0;k<insideCases;++k) {
        float bones[130][16];for(auto& bone:bones) Matrix(bone,{0,0,0});
        const float upper=Range(rng,.33f,.48f),lower=Range(rng,.10f,.18f);
        const V elbow={upper,0,0},wrist={upper+lower,0,0};
        Matrix(bones[25],{0,0,0});Matrix(bones[26],elbow);
        Matrix(bones[27],wrist);Matrix(bones[44],elbow);
        float target[16];Matrix(target,Unit(rng)*Range(rng,.01f,upper-lower-.01f));
        float original[130][16];std::memcpy(original,bones,sizeof(bones));
        Check(!d2vr_hands::PoseArm(bones,25,26,27,44,45,target),"unreachable folded arm rejected",k);
        Check(std::memcmp(bones,original,sizeof(bones))==0,"folded arm no mutation",k);
    }
    constexpr unsigned oppositeCases=2048;
    for(unsigned k=0;k<oppositeCases;++k) {
        const V a={Range(rng,-1,1),Range(rng,-1,1),Range(rng,-1,1)};
        const V delta=Unit(rng)*Range(rng,.2f,.8f);
        const V b=a+delta,na={Range(rng,-1,1),Range(rng,-1,1),Range(rng,-1,1)};
        const V nb=na-delta;
        float map[16];
        Check(d2vr_hands::SegmentDelta(a,b,na,nb,map),"opposite segment accepted",k);
        Check(Near(Transform(a,map),na),"opposite segment start",k);
        Check(Near(Transform(b,map),nb),"opposite segment end",k);
        Check(d2vr_hands::Valid(map),"opposite segment rigid",k);
    }
    constexpr unsigned invalidCases=2048;
    for(unsigned k=0;k<invalidCases;++k) {
        float bones[130][16];for(auto& bone:bones) Matrix(bone,{0,0,0});
        Matrix(bones[25],{0,0,0});Matrix(bones[26],{.3f,0,0});
        Matrix(bones[27],{.6f,0,0});Matrix(bones[44],{.3f,0,0});
        float target[16];Matrix(target,{.4f,.2f,0});
        switch(k%4) {
            case 0:target[8]=std::numeric_limits<float>::quiet_NaN();break;
            case 1:target[15]=0;break;
            case 2:target[12]=3;break;
            default:Matrix(bones[26],{0,0,0});break;
        }
        float original[130][16];std::memcpy(original,bones,sizeof(bones));
        Check(!d2vr_hands::PoseArm(bones,25,26,27,44,45,target),"invalid input rejected",k);
        Check(std::memcmp(bones,original,sizeof(bones))==0,"invalid input no mutation",k);
    }
    std::printf("native hands geometry: PASS seed=%08X cases=%u checks=%u\n",
        seed,reachableCases*2+stretchedCases+insideCases+oppositeCases+invalidCases,checks);
}

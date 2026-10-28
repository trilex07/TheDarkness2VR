#define NOMINMAX
#include <windows.h>
#include <xinput.h>
#include <openxr/openxr.h>
#include <cstdio>
#include <cstdlib>
#include "../src/vr_input.h"
#include "../src/native_aim_math.h"
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL line%d %s\n",__LINE__,#x);exit(1);}}while(0)
static void Log(const char*,...){}
static ULONGLONG clockMs=1000;
static ULONGLONG Clock(){return clockMs;}
static bool g_protectedLeftThrow=true,g_protectedLeftAim=true,g_protectedGestureActions=true;
static bool g_imagePreservingInput=true,g_protectedThrowPrivateReady=true,g_haveViews=true,g_in3DGameplayMode=true;
static bool g_headGeometry=true,g_headGeometryReferenceValid=true,g_nativeLeftAim=false,g_nativeThrowInstalled=false;
static struct {bool enabled=true;} g_nativeImageGuard;
static float g_throwAimAssist=.2f;
static DWORD g_renderThreadId=GetCurrentThreadId();
static d2vr_input::Tentacles g_protectedLeftTentacle,g_protectedRightTentacle;
static DWORD g_protectedThrowInputOwner[3]{};
static XrPosef g_headGeometryReference{{0,0,0,1},{}};
static struct {bool open=true;XrViewStateFlags flags=3|12;struct {XrTime predictedDisplayTime=1000000000;} state;} g_trackedFrame;
static struct {bool valid=true;float squeeze=0;XrPosef grip{{0,0,0,1},{}},aim{{0,0,0,1},{}};
    struct {d2vr_input::Vec3 velocity{};} motion;} g_hands[2];
static __declspec(align(16)) BYTE camera[0xB00],avatar[0x2000],inventory[0x600],weapon[0xB00],parts[0xC0],fire[0xD00],entity[0x100];
static void* refs[32];static unsigned nextRef=0;
static DWORD Ptr(const void* p){return DWORD(reinterpret_cast<uintptr_t>(p));}
static void Word(BYTE* p,unsigned o,DWORD v){*reinterpret_cast<DWORD*>(p+o)=v;}
static void Ref(BYTE* p,unsigned o,void* value){refs[nextRef]=value;*reinterpret_cast<void***>(p+o)=&refs[nextRef++];}
static void SetRef(BYTE* p,unsigned o,void* value){**reinterpret_cast<void***>(p+o)=value;}
static bool ProtectedAimCameraOwned(void* p){return p==camera&&*reinterpret_cast<DWORD*>(camera)==0x20000010;}
static bool ProtectedThrowFireOwned(void* p){return p==fire&&*reinterpret_cast<DWORD*>(fire)==0x20000000;}
struct NativeGameplaySample {DWORD camera=0,inventory=0,grabFire=0,grabEntity=0;
    bool alternateHeldTarget=false;d2vr_input::GrabFeedback grab=d2vr_input::GrabFeedback::Busy;};
static NativeGameplaySample gameplay;
static bool copyOK=true;
static bool CopyNativeGameplay(NativeGameplaySample& sample){sample=gameplay;return copyOK;}
#include "d2vr_protected_throw_types.inl"
static NativeThrowToken g_nativeThrowToken;
static ULONGLONG g_nativeThrowSequence=0;
static SRWLOCK g_nativeThrowLock=SRWLOCK_INIT;
#define GetTickCount64 Clock
#include "d2vr_protected_throw_under_test.inl"
#undef GetTickCount64
static void Reset() {
    clockMs=1000;nextRef=0;copyOK=true;
    memset(camera,0,sizeof(camera));memset(avatar,0,sizeof(avatar));memset(inventory,0,sizeof(inventory));
    memset(weapon,0,sizeof(weapon));memset(parts,0,sizeof(parts));memset(fire,0,sizeof(fire));memset(entity,0,sizeof(entity));
    Word(camera,0,0x20000010);Word(avatar,0,0x1001630);Word(inventory,0,0xFFCA10);Word(weapon,0,0xFD6E00);
    Word(fire,0,0x20000000);Word(fire,4,0x11126F0);
    Ref(camera,0x30,avatar);Ref(avatar,0x62C,camera);Ref(avatar,0x630,inventory);Ref(inventory,0x2A8,avatar);
    Ref(inventory,0x20C+6*4,weapon);Ref(weapon,0x980,avatar);Word(weapon,0xA64,Ptr(parts));
    Ref(parts,0xC,fire);Ref(fire,0x4C,weapon);Ref(fire,0x580,entity);Ref(fire,0x584,nullptr);Ref(fire,0x590,nullptr);
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,10,20,30,1};memcpy(camera+0x80,identity,64);
    gameplay={Ptr(camera),Ptr(inventory),Ptr(fire),Ptr(entity)};
    g_protectedLeftThrow=g_protectedLeftAim=g_protectedGestureActions=g_imagePreservingInput=true;
    g_protectedThrowPrivateReady=g_haveViews=g_in3DGameplayMode=g_headGeometry=g_headGeometryReferenceValid=true;
    g_nativeLeftAim=g_nativeThrowInstalled=false;g_nativeImageGuard.enabled=true;g_throwAimAssist=.2f;
    ResetProtectedLeftThrow();g_protectedRightTentacle={};g_headGeometryReference={{0,0,0,1},{}};
    g_trackedFrame={};g_renderThreadId=GetCurrentThreadId();
    for(auto& hand:g_hands)hand={};g_hands[1].valid=false;
    ResetNativeThrow();
}
static void Arm() {PublishNativeThrow(true,false,true,{2,0,0},{0,0,-1},true);CHECK(g_nativeThrowToken.until==1650);}
static void RejectedVelocity(float* velocity) {
    float before[4];memcpy(before,velocity,16);
    CHECK(!ResolveProtectedThrowVelocity(fire,velocity));CHECK(!memcmp(before,velocity,16));
}
template<class F> static void Reject(F change) {
    Reset();Arm();change();__declspec(align(16)) float v[4]={3,4,0,.75f};RejectedVelocity(v);
}
static XINPUT_GAMEPAD Step(unsigned ms,float grip,float x,float velocity) {
    clockMs=1000+ms;g_trackedFrame.state.predictedDisplayTime=clockMs*1000000;
    g_hands[0].squeeze=grip;g_hands[0].grip.position.x=x;g_hands[0].motion.velocity={velocity,0,0};
    XINPUT_GAMEPAD gp{};gp.wButtons=XINPUT_GAMEPAD_RIGHT_SHOULDER|XINPUT_GAMEPAD_A;
    gp.sThumbRX=12345;gp.sThumbRY=-5432;ApplyProtectedLeftThrow(gp);
    CHECK((gp.wButtons&(XINPUT_GAMEPAD_RIGHT_SHOULDER|XINPUT_GAMEPAD_A))==(XINPUT_GAMEPAD_RIGHT_SHOULDER|XINPUT_GAMEPAD_A));
    CHECK(gp.sThumbRX==12345&&gp.sThumbRY==-5432);return gp;
}
int main() {
    Reset();Arm();__declspec(align(16)) float velocity[4]={3,4,0,.75f};
    BYTE savedCamera[sizeof(camera)],savedFire[sizeof(fire)];memcpy(savedCamera,camera,sizeof(camera));memcpy(savedFire,fire,sizeof(fire));
    CHECK(ResolveProtectedThrowVelocity(fire,velocity));
    CHECK(std::fabs(velocity[0]-4.8507125f)<.00001f&&velocity[1]==0&&std::fabs(velocity[2]-1.2126781f)<.00001f);
    CHECK(std::fabs(d2vr_input::Length({velocity[0],velocity[1],velocity[2]})-5)<.00001f&&velocity[3]==.75f);
    CHECK(!g_nativeThrowToken.until);RejectedVelocity(velocity);
    CHECK(!memcmp(savedCamera,camera,sizeof(camera))&&!memcmp(savedFire,fire,sizeof(fire)));
    Reset();Arm();reinterpret_cast<float*>(camera+0x80)[0]=1.02f;
    float scaled[4]={3,4,0,.25f};CHECK(ResolveProtectedThrowVelocity(fire,scaled));
    CHECK(std::fabs(d2vr_input::Length({scaled[0],scaled[1],scaled[2]})-5)<.00001f&&scaled[3]==.25f);
    Reset();g_headGeometryReference.orientation={0,.70710678f,0,.70710678f};
    PublishNativeThrow(true,false,true,{2,0,0},{1,0,0},true);
    float turned[4]={3,4,0,.25f};CHECK(ResolveProtectedThrowVelocity(fire,turned));
    CHECK(std::fabs(turned[0])<.00001f&&std::fabs(turned[1])<.00001f&&std::fabs(turned[2]+5)<.00001f);
    Reject([]{g_protectedLeftThrow=false;});Reject([]{g_protectedLeftAim=false;});Reject([]{g_protectedGestureActions=false;});
    Reject([]{g_nativeImageGuard.enabled=false;});Reject([]{g_protectedThrowPrivateReady=false;});
    Reject([]{g_trackedFrame.open=false;});Reject([]{g_trackedFrame.flags=0;});Reject([]{g_haveViews=false;});
    Reject([]{g_in3DGameplayMode=false;});Reject([]{g_protectedRightTentacle.paused=true;});
    Reject([]{g_protectedLeftTentacle.paused=true;});Reject([]{g_hands[0].valid=false;});
    Reject([]{g_headGeometryReferenceValid=false;});Reject([]{g_renderThreadId+=1;});Reject([]{clockMs+=650;});
    Reject([]{copyOK=false;});Reject([]{gameplay.camera+=4;});Reject([]{gameplay.inventory+=4;});
    Reject([]{Word(camera,0,0x1234);});Reject([]{Word(inventory,0,0x1234);});
    Reject([]{SetRef(avatar,0x62C,nullptr);});Reject([]{Word(weapon,0xA78,4);});Reject([]{Word(weapon,0xA64,0);});
    Reject([]{Word(fire,0,0x1004FE8);});Reject([]{Word(fire,4,0x1234);});Reject([]{SetRef(fire,0x4C,nullptr);});
    Reject([]{SetRef(fire,0x580,nullptr);});Reject([]{SetRef(fire,0x584,entity);});Reject([]{SetRef(fire,0x590,entity);});
    Reject([]{entity[0x24]=1;});
    Reset();Arm();float zero[4]{};RejectedVelocity(zero);CHECK(g_nativeThrowToken.until);
    float nan[4]={NAN,4,0,.5f};RejectedVelocity(nan);
    Reset();PublishNativeThrow(true,false,true,{2,0,0},{0,0,-1});CHECK(!g_nativeThrowToken.until);
    Reset();Arm();PublishNativeThrow(false,false,false,{});CHECK(!g_nativeThrowToken.until);
    Reset();Arm();PublishNativeThrow(true,true,false,{}, {},true);CHECK(!g_nativeThrowToken.until);
    Reset();gameplay.grab=d2vr_input::GrabFeedback::Empty;
    CHECK(!(Step(0,0,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    CHECK(Step(20,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER);
    gameplay.grab=d2vr_input::GrabFeedback::Busy;
    CHECK(!(Step(160,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));CHECK(ProtectedLeftAimHeld());
    Step(180,1,.18f,2);Step(200,1,.22f,2);Step(220,1,.26f,2);Step(240,1,.30f,2);
    CHECK(Step(260,0,.34f,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER);CHECK(g_nativeThrowToken.until==1910);
    CHECK(!ProtectedLeftAimHeld());CHECK(ResolveProtectedThrowVelocity(fire,velocity));
    Step(400,0,.34f,0);CHECK(!g_nativeThrowToken.until);
    Reset();Step(0,0,0,0);Step(20,1,0,0);Step(60,1,.2f,2);
    gameplay.inventory+=4;Step(80,1,.24f,2);CHECK(!ProtectedLeftAimHeld());CHECK(!g_nativeThrowToken.until);
    CHECK(!(Step(100,0,.28f,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    Reset();Step(0,0,0,0);Step(20,1,0,0);g_protectedRightTentacle.paused=true;
    CHECK(!(Step(40,1,.2f,2).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));CHECK(!g_nativeThrowToken.until);
    printf("PASS protected throw actual bodies: %u checks; live owner/entity/branch gates,one-shot token,native speed/W preserved,left stroke+aim,neutral LB gap,right inputs independent\n",checks);
}

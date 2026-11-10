#define NOMINMAX
#define D2VR_MATCHED_CAPTURE
#include <windows.h>
#include <xinput.h>
#include <openxr/openxr.h>
#include <cstdio>
#include <cstdlib>
#include "../src/vr_input.h"
#include "../src/native_aim_math.h"
#include "../src/throw_target_math.h"
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL line%d %s\n",__LINE__,#x);exit(1);}}while(0)
static void Log(const char*,...){}
static ULONGLONG clockMs=1000;
static ULONGLONG Clock(){return clockMs;}
static bool g_protectedLeftThrow=true,g_protectedLeftAim=true,g_protectedGestureActions=true;
static bool g_deferredGripRetry=false;
static d2vr_input::GripRetryContext retryContext{true};
static bool g_imagePreservingInput=true,g_protectedThrowPrivateReady=true,g_haveViews=true,g_in3DGameplayMode=true;
static bool g_headGeometry=true,g_headGeometryReferenceValid=true,g_nativeLeftAim=false,g_nativeThrowInstalled=false;
static struct {bool enabled=true;} g_nativeImageGuard;
static float g_throwAimAssist=.2f,g_gameUnitsPerMeter=1.f;
static DWORD g_renderThreadId=GetCurrentThreadId();
static d2vr_input::Tentacles g_protectedLeftTentacle,g_protectedRightTentacle;
static DWORD g_protectedThrowInputOwner[3]{};
static XrPosef g_headGeometryReference{{0,0,0,1},{}};
static struct {bool open=true;XrViewStateFlags flags=3|12;struct {XrTime predictedDisplayTime=1000000000;} state;} g_trackedFrame;
static struct {bool valid=true,aimValid=false,gripValid=true;float squeeze=0;XrPosef grip{{0,0,0,1},{}},aim{{0,0,0,1},{}};
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
    unsigned char demonHold=0;bool alternateHeldTarget=false;d2vr_input::GrabFeedback grab=d2vr_input::GrabFeedback::Busy;
    ULONGLONG time=1000;};
// Production dependencies added after this fixture was written: no cinema scene,
// gaze throw off (controller aim path), camera stabilizer off (native 0x80 matrix).
static bool cinemaScene=false;
static bool CinemaSceneThisFrame(){return cinemaScene;}
static bool g_gazeThrow=false;static float g_gazeThrowAssist=.85f;
static bool testGaze=false;
static bool TentacleGazeWorldRay(XrVector3f& o,XrVector3f& d){o={};d={0,0,-1};return testGaze;}
static const float* StabilizedCameraWorld(const void* p,float (&)[16]){return reinterpret_cast<const float*>(static_cast<const BYTE*>(p)+0x80);}
static NativeGameplaySample g_nativeGameplay;
static auto& gameplay=g_nativeGameplay;
static SRWLOCK g_nativeGameplayLock=SRWLOCK_INIT;
#define GetTickCount64 Clock
#include "d2vr_native_sample_freshness.inl"
#undef GetTickCount64
static bool copyOK=true;
static bool freshSample=true;
static bool CopyNativeGameplay(NativeGameplaySample& sample){return copyOK && CopyNativeGameplayFresh(sample);}
#include "d2vr_protected_throw_types.inl"
static thread_local ThrowTargetScope* g_throwTargetScope=nullptr;
static bool g_throwTargetAssist=false;
using ThrowTargetTypeCheck=bool(__thiscall*)(void*,const void*);
static ThrowTargetTypeCheck g_throwTargetTypeCheck=nullptr;
using ThrowTargetFilter=bool(__thiscall*)(void*,void*,void*,int,int,unsigned,const float*);
static ThrowTargetFilter g_throwTargetFilter=nullptr;
using ThrowTargetSort=void(__cdecl*)(BYTE*,BYTE*,int,unsigned);
static ThrowTargetSort g_throwTargetSort=nullptr;
using ThrowTargetQuery=void*(__thiscall*)(void*,void*,void*,float,float,unsigned,void*,unsigned);
static ThrowTargetQuery g_throwTargetQuery=nullptr;
static void* g_throwTargetTrajectory=nullptr;
static struct {void* entry=nullptr;void* Entry(unsigned){return entry;}} g_throwTargetCode;
static NativeThrowToken g_nativeThrowToken;
static ULONGLONG g_nativeThrowSequence=0;
static SRWLOCK g_nativeThrowLock=SRWLOCK_INIT;
#define GetTickCount64 Clock
#include "d2vr_protected_throw_under_test.inl"
#undef GetTickCount64
static void Reset() {
    g_deferredGripRetry=false;retryContext={true};
    freshSample=true;
    clockMs=1000;nextRef=0;copyOK=true;testGaze=false;g_gazeThrow=false;
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
    if(freshSample)gameplay.time=clockMs;
    g_hands[0].squeeze=grip;g_hands[0].grip.position.x=x;g_hands[0].motion.velocity={velocity,0,0};
    const WORD untouched=XINPUT_GAMEPAD_RIGHT_SHOULDER|XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_B|XINPUT_GAMEPAD_X|XINPUT_GAMEPAD_Y;
    XINPUT_GAMEPAD gp{};gp.wButtons=untouched;
    gp.sThumbRX=12345;gp.sThumbRY=-5432;ApplyProtectedLeftThrow(gp,retryContext);
    CHECK((gp.wButtons&untouched)==untouched);
    CHECK(gp.sThumbRX==12345&&gp.sThumbRY==-5432);return gp;
}
static const void* CharacterRef(){return *reinterpret_cast<void**>(fire+0x584);}
using CharacterConsumer=void(__thiscall*)(void*,const float*,unsigned);
static CharacterConsumer characterConsumer=nullptr;
struct BodyRef {void* object;unsigned strong=1,weak=0;};
struct Physics {void** table;};
static Physics physics;
static unsigned bodyCalls=0;static const float* expectedVelocity=nullptr;
static void __fastcall BodyVelocity(Physics* self,void*,BodyRef* body,const float* velocity,unsigned mode){
    CHECK(self==&physics && body->object && body->strong==2);
    CHECK(velocity==expectedVelocity && mode==2);
    --body->strong;++bodyCalls;
}
static Physics* __fastcall PhysicsForCharacter(void* self,void*){CHECK(self==avatar);return &physics;}
static void LoadCharacterConsumer(const char* path){
    // The build script verifies the full EXE/consumer hashes before producing
    // this private92-byte fixture. Only its backend getter is redirected here.
    HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    CHECK(file!=INVALID_HANDLE_VALUE && GetFileSize(file,nullptr)==0x5C);
    auto code=static_cast<BYTE*>(VirtualAlloc(nullptr,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));CHECK(code);
    DWORD bytes=0;CHECK(ReadFile(file,code,0x5C,&bytes,nullptr) && bytes==0x5C);CloseHandle(file);
    CHECK(code[0x29]==0xE8);
    const auto displacement=DWORD(reinterpret_cast<uintptr_t>(&PhysicsForCharacter))-DWORD(reinterpret_cast<uintptr_t>(code+0x2E));
    memcpy(code+0x2A,&displacement,4);
    CHECK(code[0x59]==0xC2 && code[0x5A]==8 && code[0x5B]==0);
    DWORD old=0;CHECK(VirtualProtect(code,0x1000,PAGE_EXECUTE_READ,&old));CHECK(FlushInstructionCache(GetCurrentProcess(),code,0x5C));
    characterConsumer=reinterpret_cast<CharacterConsumer>(code);
}
static void ConsumeCharacterVelocity(const float* velocity){
    void* table[50]{};table[0xC4/4]=reinterpret_cast<void*>(&BodyVelocity);physics.table=table;
    BodyRef a{entity},empty{nullptr},b{fire};BodyRef* bodies[]={&a,&empty,&b};
    Word(avatar,0x4F0,Ptr(bodies));Word(avatar,0x4F4,Ptr(bodies+3));
    bodyCalls=0;expectedVelocity=velocity;
    characterConsumer(avatar,velocity,2);
    if(bodyCalls!=2 || a.strong!=1 || empty.strong!=1 || b.strong!=1)
        printf("Native fanout calls=%u refs=%u,%u,%u\n",bodyCalls,a.strong,empty.strong,b.strong);
    CHECK(bodyCalls==2 && a.strong==1 && empty.strong==1 && b.strong==1);
    Word(avatar,0x4F0,0);Word(avatar,0x4F4,0);
}
static void CharacterCases(){
    for(unsigned flag=0;flag<2;++flag){
        Reset();gameplay.alternateHeldTarget=true;SetRef(fire,0x584,avatar);entity[0x24]=BYTE(flag);Arm();
        BYTE beforeFire[sizeof(fire)],beforeCamera[sizeof(camera)];
        memcpy(beforeFire,fire,sizeof(fire));memcpy(beforeCamera,camera,sizeof(camera));
        float velocity[4]={3,4,0,.75f};
        // A direct/legacy call without the native retained receiver cannot
        // authorize a character. Its token remains available to the real site.
        RejectedVelocity(velocity);CHECK(g_nativeThrowToken.until);
        CHECK(ResolveProtectedThrowVelocity(fire,velocity,CharacterRef()));
        CHECK(std::fabs(d2vr_input::Length({velocity[0],velocity[1],velocity[2]})-5)<.00001f);
        CHECK(velocity[0]>4.8f && velocity[1]==0 && velocity[2]>1.2f && velocity[3]==.75f);
        ConsumeCharacterVelocity(velocity);
        CHECK(!g_nativeThrowToken.until);
        CHECK(!memcmp(beforeFire,fire,sizeof(fire)) && !memcmp(beforeCamera,camera,sizeof(camera)));
        const float sent[4]={velocity[0],velocity[1],velocity[2],velocity[3]};
        CHECK(!ResolveProtectedThrowVelocity(fire,velocity,CharacterRef()));CHECK(!memcmp(sent,velocity,16));
    }
    for(unsigned refusal=0;refusal<9;++refusal){
        Reset();gameplay.alternateHeldTarget=true;SetRef(fire,0x584,avatar);Arm();
        const void* reference=CharacterRef();void* other=entity;
        if(refusal==0)SetRef(fire,0x584,entity);
        if(refusal==1)SetRef(fire,0x584,nullptr);
        if(refusal==2)SetRef(fire,0x590,entity);
        if(refusal==3)SetRef(fire,0x580,avatar);
        if(refusal==4)reference=&other; // native dispatch target differs from fire
        if(refusal==5)reference=reinterpret_cast<const void*>(1);
        if(refusal==6)clockMs+=650;
        if(refusal==7)copyOK=false;
        if(refusal==8)g_protectedLeftThrow=false;
        float velocity[4]={3,4,0,.75f},before[4];memcpy(before,velocity,16);
        CHECK(!ResolveProtectedThrowVelocity(fire,velocity,reference));CHECK(!memcmp(before,velocity,16));
    }
    Reset();gameplay.alternateHeldTarget=true;SetRef(fire,0x584,avatar);SetRef(fire,0x590,entity);
    PublishNativeThrow(true,false,true,{2,0,0},{0,0,-1},true);CHECK(!g_nativeThrowToken.until);
    Reset();gameplay.alternateHeldTarget=true;SetRef(fire,0x584,avatar);
    g_nativeLeftAim=g_nativeThrowInstalled=true;
    PublishNativeThrow(true,false,true,{2,0,0},{0,0,-1},false);CHECK(!g_nativeThrowToken.until);
    // Actual recognizer -> token -> native retained-receiver consumption.
    Reset();gameplay.grab=d2vr_input::GrabFeedback::Empty;
    Step(0,0,0,0);Step(20,1,0,0);
    gameplay.grab=d2vr_input::GrabFeedback::Busy;gameplay.alternateHeldTarget=true;SetRef(fire,0x584,avatar);
    Step(160,1,0,0);Step(180,1,.18f,2);Step(200,1,.22f,2);Step(220,1,.26f,2);Step(240,1,.30f,2);
    CHECK(Step(260,0,.34f,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER);CHECK(g_nativeThrowToken.until==1910);
    float velocity[4]={3,4,0,.5f};CHECK(ResolveProtectedThrowVelocity(fire,velocity,CharacterRef()));
    CHECK(!(Step(400,0,.34f,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));CHECK(!g_nativeThrowToken.until);
}
static bool targetFound=false,queryScoped=false,queryThrows=false;
static unsigned sorted=0;
static void __cdecl FakeSort(BYTE* first,BYTE* last,int,unsigned) {
    ++sorted;CHECK(last-first==0x60);
    CHECK(*reinterpret_cast<float*>(first)>*reinterpret_cast<float*>(first+0x30));
}
static void* __fastcall FakeQuery(void* self,void*,void* result,void*,float cone,float,unsigned,void*,unsigned) {
    CHECK(self==fire);queryScoped=g_throwTargetScope!=nullptr;
    if(queryScoped) {
        CHECK(cone==12);float ray[16];CHECK(ResolveThrowTargetRay(camera,nullptr,ray));
        CHECK(!ResolveThrowTargetRay(entity,nullptr,ray));
        CHECK(std::fabs(ray[8]-g_nativeThrowToken.direction[0])<1e-5f &&
              std::fabs(ray[10]-g_nativeThrowToken.direction[2])<1e-5f);
        if(queryThrows)RaiseException(0xE0001234,0,0,nullptr);
    }
    *static_cast<void**>(result)=targetFound ? refs[0] : nullptr;
    return result;
}
static bool testNpc=true,testFilter=true;static unsigned filterCalls=0;
static bool __fastcall FakeType(void* object,void*,const void* type){CHECK(object==camera || object==avatar);CHECK(type==reinterpret_cast<void*>(0x1110508));return testNpc;}
static bool __fastcall FakeFilter(void*,void*,void*,void*,int,int,unsigned,const float*){++filterCalls;return testFilter;}
static DWORD trajectoryArgs[5]{};
static __declspec(naked) void FakeTrajectory() {
    __asm {
        mov trajectoryArgs[0],ecx
        mov trajectoryArgs[4],esi
        mov eax,[esp+4]
        mov trajectoryArgs[8],eax
        mov eax,[esp+8]
        mov trajectoryArgs[12],eax
        mov eax,[esp+12]
        mov trajectoryArgs[16],eax
        mov eax,12340001h
        push 247h
        popfd
        ret
    }
}
static void TrajectoryCases() {
    Reset();Arm();g_nativeThrowToken.targetSelected=true;
    g_throwTargetTrajectory=reinterpret_cast<void*>(FakeTrajectory);
    DWORD before=0,after=0,result=0,flags=0;
    SetLastError(0x56781234);
    __asm {
        push edi
        push esi
        mov edi,offset fire
        mov esi,offset entity
        mov ecx,offset avatar
        mov before,esp
        push 33
        push 22
        push 11
        call ThrowTargetTrajectory
        lea esp,[esp+12]
        mov after,esp
        mov result,eax
        pushfd
        pop flags
        pop esi
        pop edi
    }
    CHECK(before==after && result==0x12340001 && (flags&0x8d5)==(0x247&0x8d5));
    CHECK(trajectoryArgs[0]==Ptr(avatar) && trajectoryArgs[1]==Ptr(entity));
    CHECK(trajectoryArgs[2]==11 && trajectoryArgs[3]==22 && trajectoryArgs[4]==33);
    CHECK(g_nativeThrowToken.targetApplied && GetLastError()==0x56781234);
}
static void TargetCases() {
    const float origin[3]{},axis[3]={0,0,1},nearSide[3]={1,0,3},farCenter[3]={0,0,100};
    CHECK(d2vr_throw_target::Score(origin,axis,farCenter)<d2vr_throw_target::Score(origin,axis,nearSide));
    const float behind[3]={0,0,-2},nan[3]={NAN,0,2};
    CHECK(d2vr_throw_target::Score(origin,axis,behind)>1e10f);
    CHECK(d2vr_throw_target::Score(origin,axis,nan)>1e10f);
    Reset();Arm();g_throwTargetAssist=true;g_throwTargetQuery=reinterpret_cast<ThrowTargetQuery>(FakeQuery);
    g_throwTargetCode.entry=reinterpret_cast<void*>(FakeQuery);g_throwTargetSort=FakeSort;
    g_throwTargetTypeCheck=reinterpret_cast<ThrowTargetTypeCheck>(FakeType);
    g_throwTargetFilter=reinterpret_cast<ThrowTargetFilter>(FakeFilter);
    void* result=nullptr;targetFound=true;
    QueryThrowTarget(fire,nullptr,&result,nullptr,45,100,1,nullptr,1);
    CHECK(queryScoped && !g_throwTargetScope && g_nativeThrowToken.targetSelected);
    CHECK(!g_nativeThrowToken.targetApplied);
    MarkThrowTargetTrajectory(fire,0);CHECK(!g_nativeThrowToken.targetApplied);
    MarkThrowTargetTrajectory(entity,1);CHECK(!g_nativeThrowToken.targetApplied);
    MarkThrowTargetTrajectory(fire,1);CHECK(g_nativeThrowToken.targetApplied);
    float v[4]={3,4,0,.75f};CHECK(ResolveProtectedThrowVelocity(fire,v));
    CHECK(v[0]==3 && v[1]==4 && v[2]==0 && v[3]==.75f);
    Reset();Arm();targetFound=false;
    QueryThrowTarget(fire,nullptr,&result,nullptr,45,100,1,nullptr,1);
    CHECK(queryScoped && !g_nativeThrowToken.targetApplied);
    CHECK(ResolveProtectedThrowVelocity(fire,v));CHECK(std::fabs(v[0])<1e-5f && v[2]>4.99f);
    Reset();Arm();clockMs+=650;
    QueryThrowTarget(fire,nullptr,&result,nullptr,45,100,1,nullptr,1);CHECK(!queryScoped);
    Reset();Arm();g_throwTargetAssist=false;
    QueryThrowTarget(fire,nullptr,&result,nullptr,45,100,1,nullptr,1);CHECK(!queryScoped);g_throwTargetAssist=true;
    Reset();Arm();queryThrows=true;bool caught=false;
    __try {QueryThrowTarget(fire,nullptr,&result,nullptr,45,100,1,nullptr,1);}
    __except(EXCEPTION_EXECUTE_HANDLER) {caught=true;}
    CHECK(caught && !g_throwTargetScope);queryThrows=false;
    ThrowTargetScope scope;scope.ray[10]=1;g_throwTargetScope=&scope;
    __declspec(align(16)) BYTE records[0x60]{};
    reinterpret_cast<float*>(records+0x10)[0]=1;reinterpret_cast<float*>(records+0x10)[2]=3;
    reinterpret_cast<float*>(records+0x40)[2]=100;
    SortThrowTargets(records,records+sizeof(records),1,0);CHECK(sorted==1);
    void* npc=camera;
    CHECK(FilterThrowTarget(fire,nullptr,nullptr,&npc,0,0,1,nullptr));
    testNpc=false;CHECK(!FilterThrowTarget(fire,nullptr,nullptr,&npc,0,0,1,nullptr));
    testNpc=true;testFilter=false;CHECK(!FilterThrowTarget(fire,nullptr,nullptr,&npc,0,0,1,nullptr));
    CHECK(filterCalls==3);testFilter=true;
    CHECK(!SkipThrowProjectile(camera,nullptr,nullptr));
    g_throwTargetScope=nullptr;g_throwTargetAssist=false;
}
static void QueueRetry(bool enabled=true) {
    Reset();g_deferredGripRetry=enabled;gameplay.grab=d2vr_input::GrabFeedback::Empty;
    Step(0,0,0,0);CHECK(Step(20,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER);
    Step(40,0,0,0);Step(140,0,0,0);
    CHECK(!(Step(210,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
}
static void HoldToGrace() {
    for(unsigned ms=310;ms<=610;ms+=100)CHECK(!(Step(ms,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    CHECK(!(Step(660,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
}
static void DeferredRetryCases() {
    QueueRetry();HoldToGrace();
    CHECK(Step(680,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER);
    for(unsigned ms=820;ms<=1520;ms+=100)CHECK(!(Step(ms,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    QueueRetry(false);HoldToGrace();CHECK(!(Step(680,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    // Continuous original hold never acquired a second deliberate edge.
    Reset();g_deferredGripRetry=true;gameplay.grab=d2vr_input::GrabFeedback::Empty;
    Step(0,0,0,0);Step(20,1,0,0);
    for(unsigned ms=160;ms<=1460;ms+=100)CHECK(!(Step(ms,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    // Every conflicting owner cancels the pending edge rather than pausing it.
    for(int owner=0;owner<5;++owner) {
        QueueRetry();
        if(owner==0)retryContext.focused=false;
        if(owner==1)retryContext.menu=true;
        if(owner==2)retryContext.wheel=true;
        if(owner==3)retryContext.heart=true;
        if(owner==4)retryContext.support=true;
        CHECK(!(Step(310,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
        retryContext={true};
        for(unsigned ms=410;ms<=710;ms+=100)CHECK(!(Step(ms,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    }
    for(int reason=0;reason<8;++reason) {
        QueueRetry();
        if(reason==0)gameplay.grab=d2vr_input::GrabFeedback::Busy;
        if(reason==1)gameplay.grab=d2vr_input::GrabFeedback::Unknown;
        if(reason==2)g_hands[0].valid=false;
        if(reason==3)copyOK=false;
        if(reason==4)g_in3DGameplayMode=false;
        if(reason==5)g_deferredGripRetry=false;
        if(reason==6)gameplay.inventory+=4;
        if(reason==7)g_trackedFrame.flags=0;
        CHECK(!(Step(310,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
        gameplay.grab=d2vr_input::GrabFeedback::Empty;g_hands[0].valid=true;
        copyOK=true;g_in3DGameplayMode=true;g_deferredGripRetry=true;g_trackedFrame.flags=3|12;
        if(reason==6)gameplay.inventory-=4;
        for(unsigned ms=410;ms<=710;ms+=100)CHECK(!(Step(ms,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    }
    QueueRetry();
    for(unsigned ms=310;ms<=710;ms+=100)CHECK(!(Step(ms,0,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    QueueRetry();CHECK(!(Step(290,0,.3f,2).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    CHECK(!g_nativeThrowToken.until); // cancelling a pending retry cannot become a throw
    // Actual CopyNativeGameplay body: sample goes stale while input polling
    // itself remains timely. A later fresh sample cannot resurrect the edge.
    QueueRetry();freshSample=false;Step(310,1,0,0);
    CHECK(CopyNativeGameplayFresh(gameplay)); // 100ms old
    CHECK(!(Step(361,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER)); // 151ms old
    CHECK(!CopyNativeGameplayFresh(gameplay));
    freshSample=true;
    for(unsigned ms=461;ms<=761;ms+=100)CHECK(!(Step(ms,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    // Producer contract is verified in the builder: valid=gripValid&&aimValid.
    for(int invalid=0;invalid<2;++invalid) {
        QueueRetry();g_hands[0].gripValid=invalid!=0;g_hands[0].aimValid=invalid!=1;
        g_hands[0].valid=g_hands[0].gripValid && g_hands[0].aimValid;
        CHECK(!(Step(310,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
        g_hands[0].gripValid=g_hands[0].aimValid=g_hands[0].valid=true;
        for(unsigned ms=410;ms<=710;ms+=100)CHECK(!(Step(ms,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    }
    QueueRetry();g_protectedLeftTentacle.throwAt=1.31;
    Step(310,1,0,0);CHECK(!g_protectedLeftTentacle.deferredGrab);
    for(unsigned ms=450;ms<=750;ms+=100)CHECK(!(Step(ms,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    QueueRetry();CHECK(!(Step(500,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER)); // stale clock
    Step(600,1,0,0);CHECK(!(Step(700,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
}
int main(int argc,char** argv) {
    CHECK(argc==2);LoadCharacterConsumer(argv[1]);
    Reset();gameplay.alternateHeldTarget=true;SetRef(fire,0x584,avatar);
    PublishNativeThrow(true,false,true,{2,0,0},{0,0,-1},true);
    CHECK(g_nativeThrowToken.until==1650); // owned native character release
    Reset();CHECK(!NativeHeldActionContext());gameplay.alternateHeldTarget=true;
    CHECK(NativeHeldActionContext());g_hands[0].valid=false;g_hands[1].valid=false;
    CHECK(NativeHeldActionContext()); // face-button action needs no controller pose
    g_haveViews=false;CHECK(!NativeHeldActionContext());g_haveViews=true;
    g_in3DGameplayMode=false;CHECK(!NativeHeldActionContext());g_in3DGameplayMode=true;
    g_imagePreservingInput=false;CHECK(!NativeHeldActionContext());g_imagePreservingInput=true;
    copyOK=false;CHECK(!NativeHeldActionContext());copyOK=true;
    gameplay.camera+=4;CHECK(!NativeHeldActionContext());gameplay.camera-=4;
    Word(camera,0,0xffc7c0);CHECK(!NativeHeldActionContext());
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
    // Acquisition can enter the native enemy/weapon branch after the grab
    // pulse. Its stock LB release must remain reachable from a left stroke;
    // an alternate feedback flag alone cannot authorize velocity steering.
    // The fresh native character branch is exercised separately below.
    Reset();gameplay.grab=d2vr_input::GrabFeedback::Empty;
    Step(0,0,0,0);CHECK(Step(20,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER);
    gameplay.alternateHeldTarget=true;gameplay.grab=d2vr_input::GrabFeedback::Busy;
    CHECK(!(Step(160,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    Step(180,1,.18f,2);Step(200,1,.22f,2);Step(220,1,.26f,2);Step(240,1,.30f,2);
    CHECK(Step(260,0,.34f,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER);
    CHECK(!g_nativeThrowToken.until);
    CHECK(!(Step(400,0,.34f,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    CHECK(!(Step(520,0,.34f,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    CHECK(!(Step(620,0,.34f,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    CHECK(!(Step(720,0,.34f,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    CHECK(!(Step(820,0,.34f,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    // Attaching to an already held enemy still requires a deliberate stroke;
    // gripping alone is not a second native press or a throw.
    Reset();gameplay.alternateHeldTarget=true;gameplay.grab=d2vr_input::GrabFeedback::Busy;
    Step(0,0,0,0);CHECK(!(Step(20,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    CHECK(!(Step(160,1,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    CHECK(!(Step(180,0,0,0).wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER));
    CHECK(!g_nativeThrowToken.until);
    // The stabilized gaze consumer must steer the actual native release body,
    // retaining speed and the native W component; fallback stays controller-led.
    Reset();testGaze=true;g_gazeThrow=true;g_gazeThrowAssist=1;
    PublishNativeThrow(true,false,true,{2,0,0},{1,0,0},true);
    float gazeVelocity[4]={3,4,0,.25f};
    CHECK(ResolveProtectedThrowVelocity(fire,gazeVelocity));
    CHECK(std::fabs(gazeVelocity[0])<1e-5f && std::fabs(gazeVelocity[2]-5)<1e-5f && gazeVelocity[3]==.25f);
    Reset();g_gazeThrow=true;testGaze=false;
    PublishNativeThrow(true,false,true,{2,0,0},{1,0,0},true);
    CHECK(g_nativeThrowToken.direction[0]>.99f);
    Reset();g_hands[0].aimValid=true;g_throwAimAssist=1;
    PublishNativeThrow(true,false,true,{2,0,0},{1,0,0},true);
    CHECK(std::fabs(g_nativeThrowToken.direction[0])<1e-5f && g_nativeThrowToken.direction[2]>.999f);
    TrajectoryCases();
    TargetCases();
    Reset();g_hands[0].aim.position={.2f,.4f,-.1f};Arm();
    float ray[16];g_gameUnitsPerMeter=100;
    CHECK(BuildThrowTargetRay(g_nativeThrowToken,ray));
    CHECK(std::fabs(ray[12]-30)<1e-4f && std::fabs(ray[13]-60)<1e-4f && std::fabs(ray[14]-40)<1e-4f);
    g_gameUnitsPerMeter=NAN;CHECK(!BuildThrowTargetRay(g_nativeThrowToken,ray));g_gameUnitsPerMeter=1;
    g_nativeThrowToken.camera=1;CHECK(!BuildThrowTargetRay(g_nativeThrowToken,ray));
    CharacterCases();
    DeferredRetryCases();
    CHECK(VirtualFree(reinterpret_cast<void*>(characterConsumer),0,MEM_RELEASE));
    printf("PASS protected throw actual bodies: %u checks; live owner/entity/character/retained-receiver gates,one-shot token,native speed/W preserved,left stroke+aim,enemy release direction,neutral LB gap,right inputs independent\n",checks);
}

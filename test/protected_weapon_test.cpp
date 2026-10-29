#define NOMINMAX
#define D2VR_MATCHED_CAPTURE
#include <windows.h>
#include <openxr/openxr.h>
#include <intrin.h>
#include <xmmintrin.h>
#include <cstdio>
#include <cstdlib>
#include "../src/vr_input.h"
#include "../src/native_aim_math.h"
#include "../src/stereo_math.h"
#include "../src/native_view_instance.h"
#include "../src/native_weapon_bridge.h"
#include "../src/psvr2/IpcClient.h"
#include "../src/native_weapon_prefix_plan.h"
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL line%d %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned logCalls=0;
static void Log(const char*,...){++logCalls;}
static ULONGLONG clockMs=1000;
static ULONGLONG Clock(){return clockMs;}
static bool g_protectedTrackedHands=true,g_nativePlayerOwnerAdmitted=true;
static bool g_imagePreservingInput=true,g_protectedGestureActions=true,g_haveViews=true,g_in3DGameplayMode=true;
static bool g_headGeometry=true,g_headGeometryReferenceValid=true;
static float g_gameUnitsPerMeter=1;
static DWORD g_renderThreadId=GetCurrentThreadId();
static d2vr_native_image::Guard g_nativeImageGuard;
static struct {uint64_t id=20;bool open=true;XrViewStateFlags flags=15;} g_trackedFrame;
static XrPosef g_headGeometryReference{{0,0,0,1},{}};
static d2vr_input::Tentacles g_protectedLeftTentacle,g_protectedRightTentacle;
static struct {bool aimValid=true;XrPosef aim{{0,0,0,1},{}};} g_hands[2];
static SRWLOCK g_nativeGameplayLock=SRWLOCK_INIT;
static PVOID volatile g_nativeHandsSelectedResource=nullptr;
static __declspec(align(16)) BYTE camera[0x200],avatar[0x2000],inventory[0x400];
static __declspec(align(16)) BYTE guns[2][0xB00],parts[2][192];
static BYTE (*models)[0x500]=nullptr;static BYTE (*fires)[0x500]=nullptr;
static BYTE tentacle[0xB00],grab[0x600],grabParts[192];
static void* references[100];static unsigned nextRef=0;
static void* modelTable[131];static void* fireTable[33];
static DWORD Ptr(const void* p){return DWORD(reinterpret_cast<uintptr_t>(p));}
static void Word(BYTE* p,unsigned o,DWORD v){memcpy(p+o,&v,4);}
static void Ref(BYTE* p,unsigned o,void* value){references[nextRef]=value;*reinterpret_cast<void***>(p+o)=&references[nextRef++];}
static bool ProtectedAimCameraOwned(void* p){return p==camera;}
static bool ProtectedHandsResourceOwned(void*){return false;}
static bool ProtectedThrowFireOwned(void*){return false;}
static bool ProtectedWeaponModelOwned(void*);
static bool ProtectedWeaponFireOwned(void*);
#define GetTickCount64 Clock
#include "actual_weapon_owner.inl"
#include "../src/protected_weapon.inl"
#undef GetTickCount64
struct __declspec(align(16)) State {DWORD regs[9];DWORD padding[3];BYTE floating[512];};
using Invoke=uintptr_t(__cdecl*)(void*,void*,void*,const float*,State*);
using InvokeModel=uintptr_t(__cdecl*)(void*,const float*,float*,State*);
static Invoke invoke=nullptr;
static void Matrix(float* m,float x=0,float y=0,float z=0){memset(m,0,64);m[0]=m[5]=m[10]=m[15]=1;m[12]=x;m[13]=y;m[14]=z;}
static bool Near(float a,float b){return std::fabs(a-b)<.0001f;}
static void Reset(){
    nextRef=0;clockMs=1000;memset(camera,0,sizeof(camera));memset(avatar,0,sizeof(avatar));
    memset(inventory,0,sizeof(inventory));memset(guns,0,sizeof(guns));memset(models,0,2*sizeof(*models));
    memset(fires,0,2*sizeof(*fires));memset(parts,0,sizeof(parts));memset(tentacle,0,sizeof(tentacle));
    memset(grab,0,sizeof(grab));memset(grabParts,0,sizeof(grabParts));
    Word(camera,0,0xFFC7C0);Word(avatar,0,0x1001630);Word(inventory,0,0xFFCA10);
    Ref(camera,0x30,avatar);Ref(avatar,0x62C,camera);Ref(avatar,0x630,inventory);Ref(inventory,0x2A8,avatar);
    Ref(inventory,0x20C+6*4,tentacle);Word(tentacle,0,0xFD6E00);Ref(tentacle,0x980,avatar);
    Word(tentacle,0xA64,Ptr(grabParts));Ref(grabParts,0xC,grab);Word(grab,0,0x1004FE8);Word(grab,4,0x11126F0);
    Ref(grab,0x4C,tentacle);Ref(grab,0x558,nullptr);Ref(grab,0x580,nullptr);Ref(grab,0x584,nullptr);Ref(grab,0x590,nullptr);
    for(unsigned slot=0;slot<2;++slot) {
        Ref(inventory,0xB0+12*slot,guns[slot]);Word(guns[slot],0,0x1010E30);
        Ref(guns[slot],0x980,avatar);Ref(guns[slot],0x12C,models[slot]);Word(guns[slot],0xA64,Ptr(parts[slot]));
        Ref(parts[slot],0xC,fires[slot]);Ref(fires[slot],0x4C,guns[slot]);Word(fires[slot],4,0x11193F0);
        *reinterpret_cast<void***>(models[slot])=modelTable;*reinterpret_cast<void***>(fires[slot])=fireTable;
        CHECK(g_protectedWeaponModelTable.Publish(models[slot],g_nativeImageGuard));
        CHECK(g_protectedWeaponFireTable.Publish(fires[slot],g_nativeImageGuard));
    }
    Matrix(reinterpret_cast<float*>(camera+0x80),10,20,30);
    g_nativeGameplay={};g_protectedTrackedHands=g_haveViews=g_in3DGameplayMode=g_imagePreservingInput=g_protectedGestureActions=true;
    g_nativePlayerOwnerAdmitted=g_nativeImageGuard.enabled=g_headGeometry=g_headGeometryReferenceValid=true;
    g_trackedFrame.id=20;g_trackedFrame.open=true;g_trackedFrame.flags=15;
    g_protectedRightTentacle.paused=g_protectedLeftTentacle.paused=false;g_renderThreadId=GetCurrentThreadId();
    g_gameUnitsPerMeter=1;g_headGeometryReference={{0,0,0,1},{}};
    g_hands[0].aim={{0,0,0,1},{-.4f,.2f,-.3f}};g_hands[1].aim={{0,0,0,1},{.5f,.3f,-.4f}};
    g_hands[0].aimValid=g_hands[1].aimValid=true;CHECK(CaptureNativeGameplay(camera));
    CHECK(g_nativeGameplay.entities[0]==Ptr(models[0]) && g_nativeGameplay.entities[1]==Ptr(models[1]));
}
template<class Mutation> static void Reject(Mutation change) {
    Reset();change();alignas(16) float out[16],before[16];Matrix(out,7,8,9);memcpy(before,out,64);
    CHECK(!ResolveProtectedWeaponModel(reinterpret_cast<float*>(models[0]+0x140),out));
    CHECK(!memcmp(out,before,64));
    CHECK(!ResolveProtectedWeaponFire(fires[0],inventory+0xB0,out));CHECK(!memcmp(out,before,64));
}
static void GrabFeedbackCases(){
    using d2vr_input::GrabFeedback;
    Reset();g_nativeGameplay.grab=GrabFeedback::Unknown;CHECK(!NativeTentacleHeadAttachmentAllowed());
    Reset();clockMs+=150;CHECK(!NativeTentacleHeadAttachmentAllowed());
    for(unsigned field:{0x558u,0x580u,0x584u,0x590u}){
        Reset();CHECK(g_nativeGameplay.grab==GrabFeedback::Empty && !g_nativeGameplay.alternateHeldTarget);
        CHECK(NativeTentacleHeadAttachmentAllowed());
        Ref(grab,field,avatar);CHECK(CaptureNativeGameplay(camera));
        CHECK(g_nativeGameplay.grab==GrabFeedback::Busy);
        CHECK(!NativeTentacleHeadAttachmentAllowed());
        CHECK(g_nativeGameplay.alternateHeldTarget==(field==0x584 || field==0x590));
        // Drive the real recognizer from the real native sampler, rather than
        // mocking an already-consistent Busy/alternate pair.
        d2vr_input::Tentacles gesture;
        auto step=[&](double time,float grip,float x,float speed){
            return gesture.Update(true,false,true,false,grip,0,{x,0,0},{speed,0,0},{},time,
                g_nativeGameplay.grab,{0,0,-1});
        };
        CHECK(!step(0,0,0,0).leftButton);
        const auto hold=step(.02,1,0,0);CHECK(!hold.grab && !hold.leftButton);
        // A held target stays held beyond the empty/acquisition grace period.
        for(unsigned i=2;i<=40;++i)CHECK(!step(i*.02,1,0,0).leftButton);
        CHECK(!step(.82,1,.18f,2).leftButton);CHECK(!step(.84,1,.22f,2).leftButton);
        CHECK(!step(.86,1,.26f,2).leftButton);CHECK(!step(.88,1,.30f,2).leftButton);
        const auto release=step(.90,0,.34f,0);CHECK(release.throwObject && release.leftButton);
        CHECK(!step(1.04,0,.34f,0).leftButton);
        Ref(grab,field,nullptr);CHECK(CaptureNativeGameplay(camera));
        CHECK(g_nativeGameplay.grab==GrabFeedback::Empty && !g_nativeGameplay.alternateHeldTarget);
    }
    // The optional character reference itself may be null, as in the native
    // no-target path. It is not a corrupt entire gameplay sample.
    Reset();Word(grab,0x584,0);CHECK(CaptureNativeGameplay(camera));
    CHECK(g_nativeGameplay.grab==GrabFeedback::Empty && !g_nativeGameplay.alternateHeldTarget);
}
#include "native_pistol_fire_body.inl"
int main(int argc,char** argv) {
    CHECK(argc==2||argc==3);const auto module=LoadLibraryA(argv[1]);CHECK(module);
    const auto original=reinterpret_cast<d2vr_weapon_bridge::Fire>(GetProcAddress(module,"@OriginalFire@28"));
    const auto compare=reinterpret_cast<d2vr_weapon_bridge::Compare>(GetProcAddress(module,"OriginalCompare"));
    invoke=reinterpret_cast<Invoke>(GetProcAddress(module,"InvokeFire"));
    const auto result=reinterpret_cast<const void*(__cdecl*)(unsigned)>(GetProcAddress(module,"Result"));
    const auto invokeModel=reinterpret_cast<InvokeModel>(GetProcAddress(module,"InvokeModel"));
    CHECK(original && compare && invoke && result && invokeModel);
    models=static_cast<BYTE(*)[0x500]>(VirtualAlloc(nullptr,0xA00,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    fires=static_cast<BYTE(*)[0x500]>(VirtualAlloc(nullptr,0xA00,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));CHECK(models && fires);
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS32*>(reinterpret_cast<const BYTE*>(module)+dos->e_lfanew);
    g_nativeImageGuard={true,reinterpret_cast<uintptr_t>(module),pe->OptionalHeader.SizeOfImage};
    CHECK(d2vr_weapon_bridge::Bridge::Prepare(compare,original,ResolveProtectedWeaponModel,ResolveProtectedWeaponFire,g_nativeImageGuard,ReportProtectedWeaponShot));
    CHECK(g_protectedWeaponModelTable.Prepare(modelTable,131,0xF0/4,d2vr_weapon_bridge::Bridge::ModelEntry(),g_nativeImageGuard));
    CHECK(g_protectedWeaponFireTable.Prepare(fireTable,33,0x64/4,d2vr_weapon_bridge::Bridge::FireEntry(),g_nativeImageGuard));
    g_protectedWeaponReady=true;
    Reset();Word(guns[0],0xA64,1);CHECK(CaptureNativeGameplay(camera));
    CHECK(g_nativeGameplay.camera==Ptr(camera) && !(g_nativeGameplay.shotOwnedMask&1));
    CHECK(g_nativeGameplay.grab==d2vr_input::GrabFeedback::Empty);
    // Fresh native receiver ownership and result gate the event, independently.
    for(unsigned slot=0;slot<2;++slot) {
        Reset();const auto hand=1-slot;
        const auto before=d2vr::psvr2::shotSerial[hand].load();
        const auto peer=d2vr::psvr2::shotSerial[slot].load();
        ReportProtectedWeaponShot(fires[slot],inventory+0xB0+12*slot,false);
        CHECK(d2vr::psvr2::shotSerial[hand].load()==before);
        ReportProtectedWeaponShot(fires[slot],inventory+0xB0+12*(1-slot),true);
        CHECK(d2vr::psvr2::shotSerial[hand].load()==before);
        ReportProtectedWeaponShot(fires[slot],inventory+0xB0+12*slot,true);
        CHECK(d2vr::psvr2::shotSerial[hand].load()==before+1);
        CHECK(d2vr::psvr2::shotSerial[slot].load()==peer);
        g_protectedRightTentacle.paused=true;
        ReportProtectedWeaponShot(fires[slot],inventory+0xB0+12*slot,true);
        CHECK(d2vr::psvr2::shotSerial[hand].load()==before+1);
    }
    for(unsigned slot=0;slot<2;++slot) {
        Reset();Ref(guns[slot],0x12C,nullptr);
        const auto logged=logCalls;
        PrepareProtectedWeaponForCamera(camera);CHECK(logCalls==logged);
        CHECK(ProtectedWeaponFireOwned(fires[slot]));
        Ref(fires[slot],0x4C,guns[1-slot]);
        *reinterpret_cast<void***>(fires[slot])=fireTable;
        PrepareProtectedWeaponForCamera(camera);CHECK(!ProtectedWeaponFireOwned(fires[slot]));
    }
    GrabFeedbackCases();
    for(unsigned slot=0;slot<2;++slot) {
        Reset();g_hands[slot].aimValid=false; // peer loss must not gate this slot's 1-slot controller
        alignas(16) float model[16],shot[16];
        CHECK(ResolveProtectedWeaponModel(reinterpret_cast<float*>(models[slot]+0x140),model));
        CHECK(ResolveProtectedWeaponFire(fires[slot],inventory+0xB0+12*slot,shot));CHECK(!memcmp(model,shot,64));
        CHECK(Near(shot[12],slot==0?10.5f:9.6f) && Near(shot[14],slot==0?30.4f:30.3f));
    }
    Reset();g_gameUnitsPerMeter=100;alignas(16) float world[16];
    // An equipped Gun descendant still owns the exact pinned model/fire
    // classes. A different concrete gun vptr must not strand the left pistol.
    Reset();DWORD derivedType[4]={0,0,0,0x1119658};
    Word(guns[1],0,0x1010E34);Word(guns[1],4,Ptr(derivedType));
    CHECK(CaptureNativeGameplay(camera));
    CHECK(ResolveProtectedWeaponModel(reinterpret_cast<float*>(models[1]+0x140),world));
    CHECK(Near(world[12],9.6f));
    CHECK(ResolveProtectedWeaponFire(fires[1],inventory+0xBC,world));
    XrPosef leftPose{};CHECK(CopyProtectedWeaponHandPose(Ptr(camera),0,leftPose));
    CHECK(Near(leftPose.position.x,-.4f));
    derivedType[3]=0;CHECK(!ResolveProtectedWeaponFire(fires[1],inventory+0xBC,world));
    derivedType[3]=Ptr(derivedType);CHECK(!ProtectedGunType(guns[1])); // bounded malformed chain
    Word(guns[1],4,1);CHECK(!ProtectedGunType(guns[1]));
    Reset();g_gameUnitsPerMeter=100;
    CHECK(ResolveProtectedWeaponFire(fires[0],inventory+0xB0,world));CHECK(Near(world[12],60) && Near(world[14],70));
    Reset();g_hands[1].aim.orientation={0,.70710678f,0,.70710678f};
    CHECK(ResolveProtectedWeaponFire(fires[0],inventory+0xB0,world));CHECK(std::fabs(world[8])>.99f && std::fabs(world[10])<.001f);
    Reset();g_headGeometryReference.position={2,3,4};g_hands[1].aim.position={2.5f,3.3f,3.6f};
    CHECK(ResolveProtectedWeaponFire(fires[0],inventory+0xB0,world));CHECK(Near(world[12],10.5f) && Near(world[13],20.3f) && Near(world[14],30.4f));
    Reset();auto base=reinterpret_cast<float*>(camera+0x80);
    base[0]=0;base[2]=1;base[8]=-1;base[10]=0;
    CHECK(ResolveProtectedWeaponFire(fires[0],inventory+0xB0,world));CHECK(Near(world[12],9.6f) && Near(world[14],30.5f));
    // A nullable +584 alternate-character field is valid when empty. Conversely,
    // A held target is separate from the still-owned equipped pistol.
    Reset();Word(grab,0x584,0);CHECK(ResolveProtectedWeaponFire(fires[0],inventory+0xB0,world));
    Reset();Ref(grab,0x580,avatar);CHECK(g_nativeGameplay.grab==d2vr_input::GrabFeedback::Empty);
    CHECK(ResolveProtectedWeaponFire(fires[0],inventory+0xB0,world));
    Reset();CHECK(!ResolveProtectedWeaponFire(fires[0],inventory+0xBC,world));
    Reject([]{g_hands[1].aimValid=false;});Reject([]{clockMs=1150;});Reject([]{g_trackedFrame.open=false;});
    Reject([]{g_trackedFrame.flags=0;});Reject([]{g_renderThreadId=0;});Reject([]{g_protectedTrackedHands=false;});
    Reject([]{g_headGeometryReferenceValid=false;});Reject([]{g_protectedRightTentacle.paused=true;});
    Reject([]{g_gameUnitsPerMeter=0;});Reject([]{g_hands[1].aim.position.x=4;});Reject([]{g_hands[1].aim.orientation.w=NAN;});
    Reject([]{Word(guns[0],0xA78,4);});Reject([]{Ref(guns[0],0x980,nullptr);});Reject([]{Word(fires[0],4,0);});
    Reject([]{Ref(fires[0],0x4C,guns[1]);});Reject([]{Ref(inventory,0xB0,guns[1]);});
    for(unsigned field:{0x558u,0x580u,0x584u,0x590u})for(unsigned slot=0;slot<2;++slot) {
        Reset();Ref(grab,field,avatar);CHECK(CaptureNativeGameplay(camera));
        float model[16],shot[16];
        CHECK(ResolveProtectedWeaponModel(reinterpret_cast<float*>(models[slot]+0x140),model));
        CHECK(ResolveProtectedWeaponFire(fires[slot],inventory+0xB0+12*slot,shot));
        CHECK(!memcmp(model,shot,64));
        XrPosef pose{};CHECK(CopyProtectedWeaponHandPose(Ptr(camera),1-slot,pose));
        CHECK(!memcmp(&pose,&g_hands[1-slot].aim,sizeof(pose)));
        Ref(guns[slot],0x980,nullptr);
        CHECK(!ResolveProtectedWeaponFire(fires[slot],inventory+0xB0+12*slot,shot));
        CHECK(!CopyProtectedWeaponHandPose(Ptr(camera),1-slot,pose));
    }
    // Live secondary gun: +12C is empty, while the reciprocal fire action is
    // still owned. Its ray must follow LEFT aim even if right tracking is lost.
    for(unsigned slot=0;slot<2;++slot)for(unsigned missing=0;missing<3;++missing) {
        Reset();
        if(missing==0)Ref(guns[slot],0x12C,nullptr);
        if(missing==1)*reinterpret_cast<void***>(models[slot])=modelTable;
        if(missing==2)Word(models[slot],0,0x12345678);
        g_hands[slot].aimValid=false;
        g_hands[1-slot].aim.orientation={0,.70710678f,0,.70710678f};
        float shot[16],unchanged[16];Matrix(shot,7,8,9);memcpy(unchanged,shot,64);
        CHECK(!ResolveProtectedWeaponModel(reinterpret_cast<float*>(models[slot]+0x140),shot));
        CHECK(!memcmp(shot,unchanged,64));
        CHECK(ResolveProtectedWeaponFire(fires[slot],inventory+0xB0+12*slot,shot));
        CHECK(std::fabs(shot[8])>.99f && std::fabs(shot[10])<.001f);
        CHECK(Near(shot[12],slot==0?10.5f:9.6f));
        XrPosef pose{};CHECK(CopyProtectedWeaponHandPose(Ptr(camera),1-slot,pose));
        CHECK(!memcmp(&pose,&g_hands[1-slot].aim,sizeof(pose)));
        // Missing model relaxes no gun, receiver, slot or tracking ownership.
        CHECK(!ResolveProtectedWeaponFire(fires[slot],inventory+0xB0+12*(1-slot),shot));
        Ref(fires[slot],0x4C,guns[1-slot]);
        CHECK(!ResolveProtectedWeaponFire(fires[slot],inventory+0xB0+12*slot,shot));
    }
    // Distinguish publication from the actual final-consumer admission.
    // Receipts never relax the ownership/context contract or write on refusal.
    for(unsigned slot=0;slot<2;++slot) {
        Reset();unsigned stage=0;float ray[16],before[16];Matrix(ray,7,8,9);memcpy(before,ray,64);
        CHECK(ResolveProtectedWeapon(nullptr,fires[slot],inventory+0xB0+12*slot,ray,&stage));
        CHECK(stage==32+slot);
        g_trackedFrame.open=false;memcpy(ray,before,64);
        CHECK(!ResolveProtectedWeapon(nullptr,fires[slot],inventory+0xB0+12*slot,ray,&stage));
        CHECK(stage==1 && !memcmp(ray,before,64));g_trackedFrame.open=true;
        CHECK(!ResolveProtectedWeapon(nullptr,fires[slot],inventory+0xB0+12*(1-slot),ray,&stage));
        CHECK(stage==6+16*slot && !memcmp(ray,before,64));
        g_hands[1-slot].aimValid=false;
        CHECK(!ResolveProtectedWeapon(nullptr,fires[slot],inventory+0xB0+12*slot,ray,&stage));
        CHECK(stage==7+16*slot && !memcmp(ray,before,64));
    }
    // Actual bridge: unchanged and tracked matrices, five arguments, native
    // receiver, stack cleanup, result registers/flags, FPU and LastError.
    alignas(16) BYTE nativeIncoming[512];State nativeState{},bridgeState{};
    alignas(16) float stock[16];Matrix(stock,7,8,9);
    for(unsigned tracked=0;tracked<2;++tracked)for(unsigned n=0;n<1000;++n) {
        Reset();SetLastError(0x12345678);
        CHECK(invoke(reinterpret_cast<void*>(original),fires[0],inventory+0xB0,stock,&nativeState)==0x5a5a1234);
        memcpy(nativeIncoming,result(0),512);CHECK(GetLastError()==0x87654321);
        g_protectedTrackedHands=tracked!=0;SetLastError(0x12345678);
        CHECK(invoke(d2vr_weapon_bridge::Bridge::FireEntry(),fires[0],inventory+0xB0,stock,&bridgeState)==0x5a5a1234);
        CHECK(GetLastError()==0x87654321);
        // FXSAVE's saved instruction/data PCs legitimately differ after math;
        // compare register contents, tag/control/status and MXCSR separately.
        CHECK(!memcmp(nativeIncoming,result(0),8));
        CHECK(!memcmp(nativeIncoming+24,static_cast<const BYTE*>(result(0))+24,8));
        CHECK(!memcmp(nativeIncoming+32,static_cast<const BYTE*>(result(0))+32,256));
        for(unsigned index:{0u,1u,2u,3u,4u,7u,8u})CHECK(nativeState.regs[index]==bridgeState.regs[index]);
        const auto args=static_cast<const uintptr_t*>(result(2));CHECK(args[0]==Ptr(fires[0]) && args[1]==Ptr(inventory+0xB0));
        CHECK(args[2]==0x11111111 && args[3]==0x22222222 && args[4]==0x33333333);
        CHECK(args[5]==0x12345678);
        const auto matrix=static_cast<const float*>(result(1));CHECK(Near(matrix[12],tracked?10.5f:7.f));
        CHECK(!d2vr_weapon_bridge::detail::resolving);
    }
    CHECK(*static_cast<const unsigned*>(result(3))==4000);
    for(unsigned tracked=0;tracked<2;++tracked)for(unsigned n=0;n<1000;++n) {
        Reset();Matrix(world,7,8,9);SetLastError(0x1234);
        CHECK(invokeModel(reinterpret_cast<void*>(compare),reinterpret_cast<float*>(models[0]+0x140),world,&nativeState)==0x5a5a1234);
        g_protectedTrackedHands=tracked!=0;SetLastError(0x1234);
        CHECK(invokeModel(d2vr_weapon_bridge::Bridge::ModelEntry(),reinterpret_cast<float*>(models[0]+0x140),world,&bridgeState)==0x5a5a1234);
        CHECK(GetLastError()==0x1234 && Near(world[12],tracked?10.5f:7.f));
        CHECK(!memcmp(nativeState.regs,bridgeState.regs,sizeof(nativeState.regs)));
        CHECK(!memcmp(nativeState.floating,bridgeState.floating,8));
        CHECK(!memcmp(nativeState.floating+24,bridgeState.floating+24,264));
    }
    CHECK(*static_cast<const unsigned*>(result(4))==4000);
    for(unsigned i=0;i<131;++i)CHECK(modelTable[i]==nullptr &&
        (i==0xF0/4 || g_protectedWeaponModelTable.Table()[i]==modelTable[i]));
    for(unsigned i=0;i<33;++i)CHECK(fireTable[i]==nullptr &&
        (i==0x64/4 || g_protectedWeaponFireTable.Table()[i]==fireTable[i]));
    if(argc==3)NativePistolBody(argv[2],original,result);
    for(unsigned slot=0;slot<2;++slot) {
        Reset();Ref(guns[slot],0x12C,nullptr);
        g_hands[slot].aimValid=false;
        g_hands[1-slot].aim.orientation={0,.70710678f,0,.70710678f};
        CHECK(invoke(d2vr_weapon_bridge::Bridge::FireEntry(),fires[slot],inventory+0xB0+12*slot,stock,&bridgeState)==0x5a5a1234);
        const auto matrix=static_cast<const float*>(result(1));
        CHECK(std::fabs(matrix[8])>.99f && std::fabs(matrix[10])<.001f);
        CHECK(Near(matrix[12],slot==0?10.5f:9.6f));
        const auto args=static_cast<const uintptr_t*>(result(2));CHECK(args[0]==Ptr(fires[slot]) && args[1]==Ptr(inventory+0xB0+12*slot));
    }
    printf("PASS protected weapon actual source: %u checks; independent aims, scaled translation/rotation, fresh equipped/held ownership refusals, model recognition, 2000 fire + 2000 model native-vs-bridge comparisons, ABI/FP/LastError, original once, source tables unchanged; no HMD/FPS claim\n",checks);
}

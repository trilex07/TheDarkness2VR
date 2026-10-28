#define NOMINMAX
#include <windows.h>
#include <xinput.h>
#include <openxr/openxr.h>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "../src/native_aim_math.h"
static unsigned checks=0,captures=0;
static void Check(bool v){++checks;if(!v){std::printf("FAIL %u\n",checks);std::exit(1);}}
static void Log(const char*,...){}
static ULONGLONG clockMs=1000;
static ULONGLONG Clock(){return clockMs;}
static bool g_protectedLeftAim=true,g_imagePreservingInput=true,g_protectedGestureActions=true;
static bool g_protectedAimPrivateReady=true,g_haveViews=true,g_in3DGameplayMode=true;
static bool g_headGeometry=true,g_headGeometryReferenceValid=true;
static float g_gameUnitsPerMeter=1;
static struct {bool enabled=true;} g_nativeImageGuard;
static struct {bool paused=false;} g_protectedRightTentacle;
static struct {bool open=true;XrViewStateFlags flags=XR_VIEW_STATE_POSITION_TRACKED_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;} g_trackedFrame;
static struct {bool valid=true;XrPosef aim{{0,0,0,1},{-.3f,1.3f,-.4f}};} g_hands[2];
static XrPosef g_headGeometryReference{{0,0,0,1},{0,1.6f,0}};
static DWORD g_renderThreadId=GetCurrentThreadId();
struct NativeGameplaySample {DWORD camera=0x1000,inventory=0x2000,grabFire=0x3000;bool alternateHeldTarget=false;};
static NativeGameplaySample gameplay;
static bool owned=true,ownerMatches=true,copyOK=true,captureOK=true,alternateAtCapture=false;
static bool ProtectedAimCameraOwned(void* c){return g_protectedAimPrivateReady&&owned&&c==reinterpret_cast<void*>(0x1000);}
static bool NativeAimOwnerMatches(void*){return ownerMatches;}
static bool CopyNativeGameplay(NativeGameplaySample& out){out=gameplay;return copyOK;}
static bool CaptureNativeGameplay(void*){++captures;gameplay.alternateHeldTarget=alternateAtCapture;return captureOK;}
static SRWLOCK g_protectedAimRayLock=SRWLOCK_INIT;
#define GetTickCount64 Clock
#include "d2vr_protected_left_aim_under_test.inl"
#undef GetTickCount64
static XINPUT_GAMEPAD grip;
static void Reset(){
    g_protectedLeftAim=g_imagePreservingInput=g_protectedGestureActions=g_protectedAimPrivateReady=true;
    g_haveViews=g_in3DGameplayMode=g_headGeometry=g_headGeometryReferenceValid=true;
    g_gameUnitsPerMeter=1;
    g_nativeImageGuard.enabled=true;g_protectedRightTentacle.paused=false;
    g_trackedFrame.open=true;g_trackedFrame.flags=XR_VIEW_STATE_POSITION_TRACKED_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
    for(auto& h:g_hands)h={};g_headGeometryReference={{0,0,0,1},{0,1.6f,0}};
    owned=ownerMatches=copyOK=captureOK=true;alternateAtCapture=false;gameplay={};
    captures=0;clockMs=1000;g_renderThreadId=GetCurrentThreadId();
    grip={};grip.wButtons=XINPUT_GAMEPAD_LEFT_SHOULDER;ResetProtectedLeftAim();
}
int main(){
    const float base[16]={1,0,0,0,0,1,0,0,0,0,1,0,10,20,30,1};
    float output[16]{};
    Reset();PublishProtectedLeftAim(grip);Check(g_protectedAimRay.ray.valid&&g_protectedAimRay.camera==0x1000);
    Check(ResolveProtectedLeftAim(reinterpret_cast<void*>(0x1000),base,output));
    Check(std::fabs(output[12]-9.7f)<.00001f&&std::fabs(output[13]-19.7f)<.00001f&&std::fabs(output[14]-30.4f)<.00001f);
    Check(output[8]==0&&output[9]==0&&output[10]==1);
    for(unsigned i=0;i<16;++i)if(i<8||i==11||i==15)Check(output[i]==base[i]);
    g_hands[1].valid=false;PublishProtectedLeftAim(grip);Check(g_protectedAimRay.ray.valid);
    const auto rejectPublish=[&](auto change){Reset();PublishProtectedLeftAim(grip);Check(g_protectedAimRay.ray.valid);
        change();PublishProtectedLeftAim(grip);Check(!g_protectedAimRay.ray.valid&&!g_protectedAimRay.camera);};
    rejectPublish([]{g_protectedLeftAim=false;});rejectPublish([]{g_imagePreservingInput=false;});
    rejectPublish([]{g_protectedGestureActions=false;});rejectPublish([]{g_protectedAimPrivateReady=false;});
    rejectPublish([]{g_nativeImageGuard.enabled=false;});rejectPublish([]{g_haveViews=false;});
    rejectPublish([]{g_in3DGameplayMode=false;});rejectPublish([]{g_protectedRightTentacle.paused=true;});
    rejectPublish([]{g_headGeometry=false;});rejectPublish([]{g_headGeometryReferenceValid=false;});
    rejectPublish([]{g_hands[0].valid=false;});rejectPublish([]{grip.wButtons=0;});
    rejectPublish([]{grip.bLeftTrigger=30;});rejectPublish([]{grip.bRightTrigger=30;});
    rejectPublish([]{copyOK=false;});rejectPublish([]{gameplay.grabFire=0;});
    rejectPublish([]{gameplay.alternateHeldTarget=true;});rejectPublish([]{owned=false;});
    rejectPublish([]{g_trackedFrame.flags&=~XR_VIEW_STATE_POSITION_TRACKED_BIT;});
    rejectPublish([]{g_trackedFrame.flags&=~XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;});
    const auto rejectConsume=[&](auto change){Reset();PublishProtectedLeftAim(grip);change();
        for(auto& v:output)v=-123;
        Check(!ResolveProtectedLeftAim(reinterpret_cast<void*>(0x1000),base,output));
        for(auto v:output)Check(v==-123);};
    rejectConsume([]{clockMs+=100;});rejectConsume([]{clockMs-=1;});
    rejectConsume([]{g_trackedFrame.open=false;});rejectConsume([]{g_haveViews=false;});
    rejectConsume([]{g_trackedFrame.flags=0;});rejectConsume([]{g_hands[0].valid=false;});
    rejectConsume([]{g_headGeometryReferenceValid=false;});rejectConsume([]{g_protectedRightTentacle.paused=true;});
    rejectConsume([]{owned=false;});rejectConsume([]{ownerMatches=false;});
    rejectConsume([]{g_renderThreadId+=1;});rejectConsume([]{captureOK=false;});
    rejectConsume([]{alternateAtCapture=true;});rejectConsume([]{copyOK=false;});
    rejectConsume([]{gameplay.camera=0x1100;});rejectConsume([]{gameplay.grabFire=0;});
    rejectConsume([]{g_gameUnitsPerMeter=0;});rejectConsume([]{g_gameUnitsPerMeter=-1;});
    rejectConsume([]{g_gameUnitsPerMeter=5001;});rejectConsume([]{g_gameUnitsPerMeter=NAN;});
    rejectConsume([]{g_gameUnitsPerMeter=INFINITY;});
    for(float scale:{.01f,.5f,1.f,2.f,100.f,5000.f}) {
        Reset();g_gameUnitsPerMeter=scale;PublishProtectedLeftAim(grip);
        Check(ResolveProtectedLeftAim(reinterpret_cast<void*>(0x1000),base,output));
        Check(std::fabs(output[12]-(10-.3f*scale))<.001f);
        Check(std::fabs(output[13]-(20-.3f*scale))<.001f);
        Check(std::fabs(output[14]-(30+.4f*scale))<.001f);
        Check(output[8]==0&&output[9]==0&&output[10]==1);
    }
    Reset();PublishProtectedLeftAim(grip);clockMs+=99;
    Check(ResolveProtectedLeftAim(reinterpret_cast<void*>(0x1000),base,output));
    ResetProtectedLeftAim();Check(!ResolveProtectedLeftAim(reinterpret_cast<void*>(0x1000),base,output));
    // Reference yaw cancels recenter orientation; current gaze cannot steer
    // this controller ray. Left orientation and position supply both outputs.
    for(float yaw:{-3.14159265f,-1.5707963f,0.f,1.5707963f,3.14159265f}){
        Reset();XrQuaternionf q{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
        g_headGeometryReference.orientation=q;g_hands[0].aim.orientation=q;
        auto p=RotVec(q,-.3f,-.3f,-.4f);
        g_hands[0].aim.position={p.x,p.y+1.6f,p.z};PublishProtectedLeftAim(grip);
        Check(ResolveProtectedLeftAim(reinterpret_cast<void*>(0x1000),base,output));
        Check(std::fabs(output[12]-9.7f)<.00001f&&std::fabs(output[13]-19.7f)<.00001f&&std::fabs(output[14]-30.4f)<.00001f);
        Check(std::fabs(output[8])<.00001f&&std::fabs(output[9])<.00001f&&std::fabs(output[10]-1)<.00001f);
    }
    std::printf("Protected left aim actual-body pose/owner/trigger/pause/fresh-branch/recenter gates: %u checks PASS\n",checks);
}

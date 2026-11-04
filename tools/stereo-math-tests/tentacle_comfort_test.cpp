#define NOMINMAX
#include <windows.h>
#include <xinput.h>
#include <openxr/openxr.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../src/native_aim_math.h"
#define D2VR_MATCHED_CAPTURE
static unsigned checks=0;
static void Check(bool v,const char* text){++checks;if(!v){std::printf("FAIL %s\n",text);std::exit(1);}}
static bool Near(float a,float b){return std::fabs(a-b)<1e-4f;}
static DWORD clockMs=1000;
static DWORD Clock(){return clockMs;}
static ULONGLONG Clock64(){return clockMs;}
#define GetTickCount Clock
#define GetTickCount64 Clock64
static void Log(const char*,...){}
static bool cinema=false,jackie=true,gaze=true,digest=true;
static unsigned movies=0;
static bool CinemaSceneThisFrame(){return cinema;}
static bool NativeJackieInputAllowed(){return jackie;}
static unsigned NativeActionMovies(){return movies;}
static bool g_protectedLeftAim=true,g_imagePreservingInput=true,g_protectedGestureActions=true;
static bool g_protectedAimPrivateReady=true,g_haveViews=true,g_in3DGameplayMode=true;
static bool g_headGeometry=true,g_headGeometryReferenceValid=true,g_gazeGrab=true;
static bool g_dialogueCameraComfort=true,g_cameraNativeNoShake=true;
static float g_gameUnitsPerMeter=10;
static DWORD g_renderThreadId=GetCurrentThreadId();
static struct Guard {bool enabled=true;} g_nativeImageGuard;
static struct Frame {bool open=true;XrViewStateFlags flags=15;} g_trackedFrame;
static struct Tentacle {bool paused=false;} g_protectedRightTentacle;
static struct Hand {bool valid=true,aimValid=true;XrPosef aim{{0,0,0,1},{-.3f,1.2f,-.4f}};} g_hands[2];
static XrPosef g_headGeometryReference{{0,0,0,1},{0,1.7f,0}};
struct NativeAimSnapshot {bool valid=false;ULONGLONG time=0;float origin[3]{},direction[3]{};};
struct ProtectedLeftRay {NativeAimSnapshot ray;DWORD camera=0;};
static ProtectedLeftRay g_protectedAimRay;
static SRWLOCK g_protectedAimRayLock=SRWLOCK_INIT;
struct NativeGameplaySample {DWORD camera=0,grabFire=1;bool alternateHeldTarget=false;};
static NativeGameplaySample owner;
static bool fresh=true;
static bool CopyNativeGameplay(NativeGameplaySample& s){s=owner;return fresh;}
static bool CaptureNativeGameplay(void*){return fresh;}
static bool ProtectedAimCameraOwned(void* p){return DWORD(uintptr_t(p))==owner.camera;}
static bool NativeAimOwnerMatches(void* p){return ProtectedAimCameraOwned(p);}
static bool GazeWorldRay(XrVector3f& p,XrVector3f& d){p={0,1.7f,0};d={.6f,0,-.8f};return gaze;}
static void ResetProtectedLeftAim(){g_protectedAimRay={};}
namespace d2vr_native_clone {static bool MatchesDigest(void*,size_t,const char*){return digest;}}
#include "d2vr-tentacle-aim-under-test.inl"
#include "../../src/native_camera_level.inl"
#undef GetTickCount
#undef GetTickCount64
static unsigned setters=0;
static void __fastcall SetAngles(void* p,void*,const float* a){++setters;std::memcpy(static_cast<BYTE*>(p)+0x110,a,12);}
int main(){
 alignas(16) BYTE camera[0x200]{},avatar[0x700]{};
 void* cameraPointer=camera;*reinterpret_cast<void***>(avatar+0x62c)=&cameraPointer;
 owner.camera=DWORD(uintptr_t(camera));
 XINPUT_GAMEPAD gp{}; // no LB: acquire the query ray BEFORE the grip edge
 g_hands[0].valid=g_hands[0].aimValid=false;
 PublishProtectedLeftAim(gp);
 Check(g_protectedAimRay.ray.valid,"valid gaze works without controller tracking");
 Check(Near(g_protectedAimRay.ray.direction[0],.6f) && Near(g_protectedAimRay.ray.direction[2],.8f),"gaze axis conversion");
 float base[16]={1,0,0,0,0,1,0,0,0,0,1,0,100,2,5,1},out[16]{};
 Check(ResolveProtectedLeftAim(camera,base,out),"native acquisition accepts fresh gaze without hand");
 Check(Near(out[8],.6f) && Near(out[10],.8f) && Near(out[12],100),"consumed gaze ray world composition");
 gaze=false;PublishProtectedLeftAim(gp);Check(!g_protectedAimRay.ray.valid,"gaze loss with absent hand cancels ray");
 g_hands[0].aimValid=true;g_hands[0].aim.orientation={0,.382683432f,0,.923879533f};
 PublishProtectedLeftAim(gp);Check(g_protectedAimRay.ray.valid,"controller fallback before grip");
 Check(ResolveProtectedLeftAim(camera,base,out),"controller ray consumed");
 Check(Near(out[12],97) && Near(out[13],-3) && Near(out[14],9),"controller origin scaled in metres");
 Check(out[8]<-.7f && out[10]>.7f,"left controller direction, not head forward");
 owner.alternateHeldTarget=true;PublishProtectedLeftAim(gp);Check(!g_protectedAimRay.ray.valid,"held character keeps native branch");owner.alternateHeldTarget=false;
 for(unsigned gate=0;gate<4;++gate){
  cinema=gate==0;movies=gate==1;jackie=gate!=2;g_protectedRightTentacle.paused=gate==3;
  PublishProtectedLeftAim(gp);Check(!g_protectedAimRay.ray.valid,"cinema/QTE/alternate avatar/pause reject acquisition");
 } cinema=false;movies=0;jackie=true;g_protectedRightTentacle.paused=false;
 PublishProtectedLeftAim(gp);clockMs+=101;Check(!ResolveProtectedLeftAim(camera,base,out),"stale ray rejected");
 // Execute the actual queued-leveling code with an ABI-correct setter fixture.
 Check(VirtualAlloc(reinterpret_cast<void*>(0x9d9000),4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE)!=nullptr,"setter fixture address");
 auto b=reinterpret_cast<BYTE*>(0x9d9120);b[0]=0xe9;*reinterpret_cast<int32_t*>(b+1)=int32_t(uintptr_t(SetAngles)-0x9d9125);
 float angles[3]={-130,-10.3f,4};std::memcpy(camera+0x110,angles,12);
 RequestNativeCameraLevel();Check(setters==0,"input callback only queues, never mutates camera");
 ApplyNativeCameraLevel(DWORD(uintptr_t(avatar)));float result[3];std::memcpy(result,camera+0x110,12);
 Check(setters==1 && result[0]==-130 && result[1]==0 && result[2]==0,"native leveling preserves yaw and clears pitch/roll");
 ApplyNativeCameraLevel(DWORD(uintptr_t(avatar)));Check(setters==1,"one-shot per chord");
 RequestNativeCameraLevel();clockMs+=501;ApplyNativeCameraLevel(DWORD(uintptr_t(avatar)));Check(setters==1,"expired request cannot affect later scene");
 RequestNativeCameraLevel();digest=false;ApplyNativeCameraLevel(DWORD(uintptr_t(avatar)));Check(setters==1,"unverified native setter rejected");digest=true;
 owner.camera+=16;ApplyNativeCameraLevel(DWORD(uintptr_t(avatar)));Check(setters==1,"owner switch rejected");
 std::printf("PASS %u actual-source tentacle targeting and queued camera leveling checks\n",checks);
}

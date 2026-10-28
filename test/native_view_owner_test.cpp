#define NOMINMAX
#define D2VR_MATCHED_CAPTURE
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../src/native_player_camera_owner.h"
static unsigned checks=0,captures=0,aims=0,throws=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %u line%d\n",checks,__LINE__);exit(1);}}while(0)
static bool g_nativePlayerOwnerAdmitted=true,g_imagePreservingInput=true,g_protectedGestureActions=true;
static bool g_haveViews=true,g_in3DGameplayMode=true;
static int g_submitMode=4,g_stereoMode=1;
static DWORD g_renderThreadId=0;
static struct {bool enabled=true;} g_nativeImageGuard;
static struct {uint64_t id=17;bool open=true;} g_trackedFrame;
static uint64_t g_nativePlayerViewFrame=0;
static uintptr_t g_nativePlayerViewRenderer=0;
static uint32_t g_nativePlayerViewCamera=0;
static unsigned char renderer[0x500],avatar[0x700],camera[0x80],inventory[0x300],selected[4];
static void* blocks[6]{};
static void* avatarBlock=avatar;
static bool readCamera=true,captureOK=true;
struct NativeCameraFields{};
static DWORD ProtectedAimCameraTable(){return 0x77770000;}
static bool NativeAimOwnerMatches(void* p){return p==camera;}
static bool CaptureNativeGameplay(void* p){CHECK(p==camera);++captures;return captureOK;}
static bool PrepareProtectedAimCamera(void* p){CHECK(p==camera);++aims;return true;}
static void PrepareProtectedThrowForCamera(void* p){CHECK(p==camera);++throws;}
static bool ReadSelectedRenderCamera(void* c,void* r,NativeCameraFields&){return readCamera && c==selected && r==renderer;}
template<class... T> static void Log(const char*,T...){}
#include "d2vr_native_view_owner_under_test.inl"
static void Word(void* p,unsigned offset,uint32_t value){memcpy(static_cast<unsigned char*>(p)+offset,&value,4);}
static void Ref(void* p,unsigned offset,unsigned block,void* object){blocks[block]=object;Word(p,offset,uint32_t(uintptr_t(&blocks[block])));}
static void Reset(){
    g_nativePlayerViewFrame=0;g_nativePlayerViewRenderer=0;captures=aims=throws=0;
    g_nativePlayerOwnerAdmitted=g_imagePreservingInput=g_protectedGestureActions=true;
    g_haveViews=g_in3DGameplayMode=g_nativeImageGuard.enabled=g_trackedFrame.open=readCamera=captureOK=true;
    g_submitMode=4;g_stereoMode=1;g_renderThreadId=GetCurrentThreadId();g_trackedFrame.id=17;
    memset(renderer,0,sizeof(renderer));memset(avatar,0,sizeof(avatar));memset(camera,0,sizeof(camera));memset(inventory,0,sizeof(inventory));
    avatarBlock=avatar;Word(avatar,0,0x1001630);Word(camera,0,0xffc7c0);Word(inventory,0,0xffca10);
    Ref(renderer,0x3f4,0,selected);Ref(avatar,0x62c,1,camera);Ref(avatar,0x630,2,inventory);
    Ref(camera,0x30,3,avatar);Ref(inventory,0x2a8,4,avatar);
}
static void Observe(){void* reference=&avatarBlock;SetLastError(0x87654321);ObserveNativeViewOwner(renderer,&reference);CHECK(GetLastError()==0x87654321);}
template<class F> static void Reject(F change){Reset();change();Observe();CHECK(!g_nativePlayerViewFrame && !g_nativePlayerViewRenderer && !aims && !throws);}
int main(){
    Reset();Observe();CHECK(captures==1 && aims==1 && throws==1);
    CHECK(g_nativePlayerViewFrame==17 && g_nativePlayerViewRenderer==uintptr_t(renderer));
    // No renderer+460 override exists in this actual callback fixture.
    Reject([]{g_nativePlayerOwnerAdmitted=false;});Reject([]{g_imagePreservingInput=false;});
    Reject([]{g_protectedGestureActions=false;});Reject([]{g_nativeImageGuard.enabled=false;});
    Reject([]{g_trackedFrame.open=false;});Reject([]{g_haveViews=false;});Reject([]{g_in3DGameplayMode=false;});
    Reject([]{g_trackedFrame.id=0;});
    Reject([]{g_submitMode=0;});Reject([]{g_stereoMode=0;});Reject([]{readCamera=false;});
    Reject([]{avatarBlock=nullptr;});Reject([]{Word(avatar,0,0x100c918);});
    Reject([]{blocks[1]=nullptr;});Reject([]{blocks[3]=inventory;});Reject([]{blocks[4]=camera;});
    Reject([]{captureOK=false;});
    Reset();Observe();readCamera=false;Observe();CHECK(!g_nativePlayerViewFrame && !g_nativePlayerViewRenderer);
    Reset();g_renderThreadId++;Observe();CHECK(!captures && !aims && !throws);
    Reset();Word(camera,0,ProtectedAimCameraTable());Observe();CHECK(aims==1 && throws==1);
    printf("PASS actual native view owner callback: %u checks, live reference without FOV override, graph/type/context refusals, invalidation, LastError\n",checks);
}

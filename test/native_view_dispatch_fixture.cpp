#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static unsigned currentThread=7,g_renderThreadId=7,observations=0,copies=0,reads=0,publications=0;
static int g_nativeViewBoundaryMode=2,g_submitMode=4,g_stereoMode=1;
static bool g_haveViews=true,g_in3DGameplayMode=true,readOK=true;
static bool g_nativeTranslatedViewPrepared=true;
static unsigned long long g_nativeCullingAppliedFrame=0;
using DWORD=unsigned;
static DWORD lastError=0x12345678;
static DWORD GetLastError(){return lastError;}
static void SetLastError(DWORD value){lastError=value;}
static struct {bool open=true;unsigned long long id=1;} g_trackedFrame;
enum class NativeCullingReason {Context,Camera};
static NativeCullingReason g_nativeCullingReason=NativeCullingReason::Context;
static void Log(const char*,...) {}
struct CONTEXT {unsigned Eip=0x98618E,Edi=0x200003f4,Ecx=0x30000000;};
struct NativeCameraFields {};
static unsigned GetCurrentThreadId(){return currentThread;}
static void ObserveNativeCameraBoundary(void*,bool render){CHECK(!render);++observations;}
static void PublishTranslatedNativeView(void*){++publications;}
static bool requireCover=false,covered=false;
static unsigned coverCalls=0;
static void __fastcall CoverSelectedRenderCamera(void* camera,void* renderer) {
    CHECK(camera==reinterpret_cast<void*>(0x30000000));
    CHECK(renderer==reinterpret_cast<void*>(0x20000000));
    covered=true;++coverCalls;SetLastError(456);
}
static bool ReadSelectedRenderCamera(void* camera,void* renderer,NativeCameraFields&) {
    CHECK(camera==reinterpret_cast<void*>(0x30000000));
    CHECK(renderer==reinterpret_cast<void*>(0x20000000));++reads;if(requireCover)CHECK(covered);return readOK;
}
static void ObserveNativeSelectedCamera(void* id,void* renderer,void* camera,const NativeCameraFields&,bool before) {
    CHECK(id==reinterpret_cast<void*>(0x98618E) && before);
    CHECK(renderer==reinterpret_cast<void*>(0x20000000));
    CHECK(camera==reinterpret_cast<void*>(0x30000000));++copies;
    g_nativeCullingAppliedFrame=1;
    SetLastError(123);
}
@TRANSLATED@
@COVER@
@IMPLEMENTATION@
int main() {
    void* view=reinterpret_cast<void*>(0x1234);
    CONTEXT context;
    for(int i=0;i<10000;++i)ObserveNativeViewBoundary(view);
    CHECK(observations==0);
    for(int i=0;i<10000;++i)ObserveNativeFrustumCopy(context);
    CHECK(copies==10000 && reads==10000);
    CHECK(g_nativeCullingAppliedFrame==1);
    readOK=false;ObserveNativeFrustumCopy(context);CHECK(copies==10000 && reads==10001 && !g_nativeCullingAppliedFrame);readOK=true;
    context.Eip++;ObserveNativeFrustumCopy(context);context.Eip--;
    context.Edi=0x3f3;ObserveNativeFrustumCopy(context);context.Edi=0x200003f4;
    g_trackedFrame.open=false;ObserveNativeFrustumCopy(context);g_trackedFrame.open=true;
    g_haveViews=false;ObserveNativeFrustumCopy(context);g_haveViews=true;
    g_in3DGameplayMode=false;ObserveNativeFrustumCopy(context);g_in3DGameplayMode=true;
    g_submitMode=0;ObserveNativeFrustumCopy(context);g_submitMode=4;
    g_stereoMode=0;ObserveNativeFrustumCopy(context);g_stereoMode=1;
    currentThread=8;ObserveNativeFrustumCopy(context);currentThread=7;
    g_nativeViewBoundaryMode=1;ObserveNativeViewBoundary(view);
    ObserveNativeFrustumCopy(context);
    CHECK(observations==1 && copies==10000 && reads==10001);
    currentThread=8;ObserveNativeViewBoundary(view);
    CHECK(observations==1);
    g_nativeViewBoundaryMode=3;ObserveNativeViewBoundary(view);CHECK(!publications);
    currentThread=7;
    for(unsigned i=0;i<10000;++i)ObserveNativeViewBoundary(view);
    CHECK(publications==10000 && observations==1 && copies==10000);
    auto camera=reinterpret_cast<void*>(0x30000000),renderer=reinterpret_cast<void*>(0x20000000);
    SetLastError(0x12345678);
    for(unsigned i=0;i<10000;++i)ObserveTranslatedFrustum(camera,renderer);
    CHECK(copies==20000 && reads==20001 && GetLastError()==0x12345678);
    auto refused=[&]{const auto count=copies,read=reads;ObserveTranslatedFrustum(camera,renderer);CHECK(copies==count && reads==read);};
    g_nativeTranslatedViewPrepared=false;refused();g_nativeTranslatedViewPrepared=true;
    g_nativeViewBoundaryMode=2;refused();g_nativeViewBoundaryMode=3;
    currentThread=8;refused();currentThread=7;
    g_trackedFrame.open=false;refused();g_trackedFrame.open=true;
    g_haveViews=false;refused();g_haveViews=true;
    g_in3DGameplayMode=false;refused();g_in3DGameplayMode=true;
    g_submitMode=0;refused();g_submitMode=4;
    g_stereoMode=0;refused();g_stereoMode=1;
    g_nativeCullingAppliedFrame=1;
    readOK=false;ObserveTranslatedFrustum(camera,renderer);CHECK(copies==20000 && reads==20002 && !g_nativeCullingAppliedFrame);
    CHECK(GetLastError()==0x12345678);
    std::puts("PASS actual-source phase routing:10000 hardware+10000 translated final-copy callbacks,10000 mode3 QPC publications,ECX camera/owner,LastError,mode/thread/gameplay/owner-read rejection");
    readOK=true;requireCover=true;
    for(unsigned i=0;i<10000;++i) {
        covered=false;const auto previous=copies;
        CoverTranslatedRenderCamera(camera,renderer);
        CHECK(covered && coverCalls==i+1 && copies==previous+1);
        CHECK(GetLastError()==0x12345678);
    }
    std::puts("PASS 10000 camera covers before frustum callback, owner/camera and LastError preserved");
    return 0;
}

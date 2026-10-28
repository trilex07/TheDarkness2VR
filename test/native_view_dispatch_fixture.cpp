#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static unsigned currentThread=7,g_renderThreadId=7,observations=0,copies=0,reads=0;
static int g_nativeViewBoundaryMode=2,g_submitMode=4,g_stereoMode=1;
static bool g_haveViews=true,g_in3DGameplayMode=true,readOK=true;
static struct {bool open=true;} g_trackedFrame;
struct CONTEXT {unsigned Eip=0x98618E,Edi=0x200003f4,Ecx=0x30000000;};
struct NativeCameraFields {};
static unsigned GetCurrentThreadId(){return currentThread;}
static void ObserveNativeCameraBoundary(void*,bool render){CHECK(!render);++observations;}
static bool ReadSelectedRenderCamera(void* camera,void* renderer,NativeCameraFields&) {
    CHECK(camera==reinterpret_cast<void*>(0x30000000));
    CHECK(renderer==reinterpret_cast<void*>(0x20000000));++reads;return readOK;
}
static void ObserveNativeSelectedCamera(void* id,void* renderer,void* camera,const NativeCameraFields&,bool before) {
    CHECK(id==reinterpret_cast<void*>(0x98618E) && before);
    CHECK(renderer==reinterpret_cast<void*>(0x20000000));
    CHECK(camera==reinterpret_cast<void*>(0x30000000));++copies;
}
@IMPLEMENTATION@
int main() {
    void* view=reinterpret_cast<void*>(0x1234);
    CONTEXT context;
    for(int i=0;i<10000;++i)ObserveNativeViewBoundary(view);
    CHECK(observations==0);
    for(int i=0;i<10000;++i)ObserveNativeFrustumCopy(context);
    CHECK(copies==10000 && reads==10000);
    readOK=false;ObserveNativeFrustumCopy(context);CHECK(copies==10000 && reads==10001);readOK=true;
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
    std::puts("PASS actual-source phase routing: 10000 final-copy callbacks, EDI owner/ECX camera, QPC observation-only, PC/thread/gameplay/owner-read rejection");
    return 0;
}

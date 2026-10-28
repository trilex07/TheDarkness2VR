#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static unsigned currentThread=7,g_renderThreadId=7,prepares=0,observations=0;
static int g_nativeViewBoundaryMode=2,g_nativeImageGuard=1;
static bool prepareOK=true;
static unsigned GetCurrentThreadId(){return currentThread;}
static bool PrepareNativeRenderViewTable(){++prepares;return prepareOK;}
static void ObserveNativeCameraBoundary(void*,bool render){CHECK(!render);++observations;}
static void Log(const char*,...){ }
struct Table {
    bool owns=false,publishOK=true;
    unsigned publications=0;
    bool Owns(void*)const{return owns;}
    bool Publish(void*,int){++publications;return publishOK;}
} g_nativeRenderViewTable;
@IMPLEMENTATION@
int main() {
    void* view=reinterpret_cast<void*>(0x1234);
    ObserveNativeViewBoundary(view);
    CHECK(prepares==1 && g_nativeRenderViewTable.publications==1 && observations==0);
    g_nativeRenderViewTable.owns=true;
    for(int i=0;i<10000;++i)ObserveNativeViewBoundary(view);
    CHECK(prepares==1 && g_nativeRenderViewTable.publications==1 && observations==0);
    g_nativeRenderViewTable.owns=false;g_nativeRenderViewTable.publishOK=false;
    ObserveNativeViewBoundary(view);
    CHECK(prepares==2 && g_nativeRenderViewTable.publications==2 && observations==1);
    prepareOK=false;ObserveNativeViewBoundary(view);
    CHECK(prepares==3 && g_nativeRenderViewTable.publications==2 && observations==2);
    g_nativeViewBoundaryMode=1;ObserveNativeViewBoundary(view);
    CHECK(prepares==3 && g_nativeRenderViewTable.publications==2 && observations==3);
    currentThread=8;ObserveNativeViewBoundary(view);
    CHECK(prepares==3 && observations==3);
    std::puts("PASS actual-source QPC discovery: render witness isolation, 10000 unchanged instances without republish, failed admission observation-only, observe mode and foreign thread");
}

#include "../src/cinematic_theater.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static uint64_t now=1000;
static uint64_t GetTickCount64(){return now;}
static unsigned thread=1,g_renderThreadId=1;
static unsigned GetCurrentThreadId(){return thread;}
static bool native=false,g_cinematicTheater=false;
static int g_submitMode=4,g_stereoMode=1;
static struct {bool open=true;} g_trackedFrame;
static bool NativeCinematicPlaying(){return native;}
static std::atomic<unsigned> g_vrCinemaMode{0};
static std::atomic<uint64_t> g_cinemaOverrideRequest{0};
// Exact production functions, extracted by build_vr_settings.ps1.
#include "actual_cinema_control.inl"
int main(){
    using namespace d2vr_cinema;
    CHECK(!CinemaEnabled() && !CinemaSceneThisFrame());
    g_cinematicTheater=true;CHECK(CinemaEnabled());
    CHECK(!CinemaSceneThisFrame());native=true;CHECK(CinemaSceneThisFrame());
    for(unsigned preference:{0u,1u,2u,3u,~0u})for(bool cinematic:{false,true}) {
        native=cinematic;g_vrCinemaMode=preference;
        CHECK(CinemaSceneThisFrame()==(preference==1?false:preference==2?true:cinematic));
    }
    g_cinematicTheater=false;g_vrCinemaMode=2;native=false;
    for(uint64_t time:{1000ull,61001ull,3600000ull,86400000ull}) {
        now=time;CHECK(CinemaEnabled() && CinemaSceneThisFrame());
        CHECK(Select(CinemaEnabled(),true,CinemaSceneThisFrame(),false,CinemaOverride())==Mode::Scene3D);
    }
    // Existing chord overrides temporarily, then returns to the saved choice.
    now=1000;g_cinemaOverrideRequest=RequestOpposite(Mode::Scene3D,now);
    CHECK(!CinemaSceneThisFrame());now=60999;CHECK(!CinemaSceneThisFrame());
    now=61000;CHECK(CinemaSceneThisFrame());
    g_vrCinemaMode=1;g_cinemaOverrideRequest=RequestOpposite(Mode::Gameplay,now);
    CHECK(CinemaSceneThisFrame());now+=60000;CHECK(!CinemaSceneThisFrame());
    CHECK(Select(CinemaEnabled(),true,false,true,CinemaOverride())==Mode::Gameplay);
    g_vrCinemaMode=2;g_cinemaOverrideRequest=0;
    thread=2;CHECK(!CinemaSceneThisFrame());thread=1;
    g_trackedFrame.open=false;CHECK(!CinemaSceneThisFrame());g_trackedFrame.open=true;
    g_submitMode=3;CHECK(!CinemaSceneThisFrame());g_submitMode=4;
    g_stereoMode=0;CHECK(!CinemaSceneThisFrame());g_stereoMode=1;
    g_vrCinemaMode=0;CHECK(!CinemaEnabled());
    // Demonstrate accepted temporary-only policy cannot meet persistence.
    CHECK(TemporaryOverride(RequestOpposite(Mode::Gameplay,1000),61000)==Override::Auto);
    CHECK(ResolveOverride(0,86400000,2)==Override::Scene3D);
    printf("PASS actual persistent cinema policy %u checks\n",checks);
}

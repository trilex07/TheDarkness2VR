#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <xinput.h>
#include "../src/controller_menu_pointer.h"
#include "../src/vr_settings.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL %d %s\n",__LINE__,#x);std::exit(1);}}while(0)
static bool ready=false,nativeMenu=true,g_in3DGameplayMode=false,g_inputContext=false;
static LONG g_nativeRegionManager=0;
static bool ControllerMenuConfirmReady(){return ready;}
static bool MenuConfirmActive(){return nativeMenu;}
static void Log(const char*,...){}
#include "d2vr_menu_confirm_under_test.inl"
static XINPUT_GAMEPAD Confirm(BYTE trigger,WORD buttons=0){
    XINPUT_GAMEPAD gp{};gp.bRightTrigger=trigger;gp.wButtons=buttons;
    ApplyMenuTriggerConfirm(gp);return gp;
}
int main(){
    using namespace d2vr_menu;
    Surface s{1,12,(XrSpace)1,{{0,0,0,1},{0,0,-2}},4,2,{0,0,2000,1000}};
    XrPosef aim{{0,0,0,1},{.2f,0,0}};Hit hit;
    CHECK(ControllerHit(s,aim,true,s.space,hit));CHECK(hit.x==1100&&hit.y==500);
    CHECK(CursorReady(hit,1100,500,.01));CHECK(!CursorReady(hit,1000,500,.01));
    CHECK(!CursorReady(hit,1100,500,.151));CHECK(!CursorReady(hit,1100,500,-.001));
    CHECK(!ControllerHit(s,aim,false,s.space,hit));CHECK(!ControllerHit(s,aim,true,(XrSpace)2,hit));
    auto outside=aim;outside.position.x=3;CHECK(!ControllerHit(s,outside,true,s.space,hit));
    auto behind=aim;behind.position.z=-3;CHECK(!ControllerHit(s,behind,true,s.space,hit));
    auto bad=s;bad.frame=0;CHECK(!ControllerHit(bad,aim,true,s.space,hit));
    bad=s;bad.width=0;CHECK(!ControllerHit(bad,aim,true,s.space,hit));
    // Main-menu quad and enlarged pause-menu projection use their own extents.
    for(float width:{1.f,2.f,4.f})for(float height:{1.f,2.f,3.f}){
        s.width=width;s.height=height;
        for(int i=-4;i<=4;++i){
            aim.position={width*i*.1f,height*i*.05f,0};
            CHECK(ControllerHit(s,aim,true,s.space,hit));
            CHECK(std::abs(hit.u-(.5+i*.1))<1e-6);CHECK(std::abs(hit.v-(.5-i*.05))<1e-6);
            Aim ray;ray.frame=s.frame;ray.space=s.space;ray.tracked=ray.menu=true;ray.pose=aim;
            XrPosef beam{};XrExtent2Df size{};
            if(i)CHECK(d2vr_settings::Laser(s,ray,{{0,0,0,1},{0,0,0}},beam,size));
        }
    }
    Confirm(0);auto gp=Confirm(255);CHECK(!(gp.wButtons&XINPUT_GAMEPAD_A));CHECK(!gp.bRightTrigger);
    ready=true;gp=Confirm(255);CHECK(gp.wButtons&XINPUT_GAMEPAD_A);CHECK(!gp.bRightTrigger);
    ready=false;gp=Confirm(255);CHECK(gp.wButtons&XINPUT_GAMEPAD_A); // one continuous native hold
    gp=Confirm(0);CHECK(!(gp.wButtons&XINPUT_GAMEPAD_A));
    gp=Confirm(255);CHECK(!(gp.wButtons&XINPUT_GAMEPAD_A));
    Confirm(0);nativeMenu=false;gp=Confirm(255);CHECK(gp.bRightTrigger==255);CHECK(!(gp.wButtons&XINPUT_GAMEPAD_A));
    Confirm(0);nativeMenu=true;ready=true;gp=Confirm(255,XINPUT_GAMEPAD_B);CHECK(gp.wButtons&XINPUT_GAMEPAD_A);CHECK(gp.wButtons&XINPUT_GAMEPAD_B);
    gp=Confirm(0,XINPUT_GAMEPAD_A);CHECK(gp.wButtons&XINPUT_GAMEPAD_A); // physical A untouched
    std::printf("PASS controller menu projection/readiness/actual confirm %u checks\n",checks);
}

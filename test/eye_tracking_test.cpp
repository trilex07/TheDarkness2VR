// Host test: gaze seqlock (bridge -> DLL), foveated NIS geometry, gaze -> menu quad.
// Windows: test/build_eye_tracking.ps1 (MSVC /W4 /WX). Linux: g++ -std=c++17.
#include <cmath>
#include <cstdio>
#include "../src/psvr2/GazeAbi.h"
#include "../src/psvr2/GazeShare.h"
#include "../src/foveated_sharpen.h"
#include "../src/gaze_menu.h"
#include "../src/gaze_target_filter.h"
static int g_fail=0, g_checks=0;
#define CHECK(c) do{ ++g_checks; if(!(c)){ ++g_fail; std::printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#c);} }while(0)
static void Seqlock() {
    using namespace d2vr::psvr2; GazeShared s{}; s.magic=kGazeMagic; s.version=kGazeVersion; GazeSample o;
    CHECK(!GazeRead(s,o));
    GazeSample v; v.frameCounter=7; v.dirCombined[2]=1; GazeWrite(s,v);
    CHECK(GazeRead(s,o) && o.frameCounter==7 && o.dirCombined[2]==1);
    s.seq.store(s.seq.load()+1); CHECK(!GazeRead(s,o)); s.seq.store(s.seq.load()+1);
    s.magic=0; CHECK(!GazeRead(s,o));
    CHECK(sizeof(gaze_abi::Status)==0x148);
}
static void Fovea() {
    using namespace d2vr_fovea; float x,y;
    const float L=-0.95f,R=0.8f,U=0.9f,D=-0.95f;
    CHECK(GazePixel(0,0,-1,L,R,U,D,2000,2000,x,y));
    CHECK(std::fabs(x-(0-std::tan(L))/(std::tan(R)-std::tan(L))*2000)<.01f);
    CHECK(GazePixel(-0.3f,0,-1,L,R,U,D,2000,2000,x,y) && x<1000);
    float xu,yu; CHECK(GazePixel(0,.3f,-1,L,R,U,D,2000,2000,xu,yu) && yu<y);
    CHECK(!GazePixel(0,0,1,L,R,U,D,2000,2000,x,y));
    CHECK(!GazePixel(-5,0,-1,L,R,U,D,2000,2000,x,y));
    auto r=Around(1000,1000,2000,1800,.25f); CHECK(r.w==448 && r.h==448 && r.x%8==0 && r.x+r.w<=2000);
    r=Around(5,5,2000,1800,.25f); CHECK(r.x==0 && r.y==0);
    r=Around(1999,1799,2000,1800,.25f); CHECK(r.x+r.w<=2000 && r.y+r.h<=1800);
    r=Around(10,10,20,20,.25f); CHECK(r.w==20 && r.x==0);
    CHECK(Around(0,0,0,0,.25f).Empty());
    auto g=Grade(.3f,1.2f); CHECK(g.fovea==1 && g.periphery==.3f && std::fabs(g.transition-.65f)<1e-6f);
}
static void Menu() {
    using namespace d2vr_gaze_menu; float u,v;
    CHECK(!PadConfirmContext(false,true)); // Flash cursor over active gameplay: R2 fires
    CHECK(PadConfirmContext(true,true));   // pause over world: R2 confirms
    CHECK(PadConfirmContext(false,false)); // cinema/menu without world: R2 confirms
    Quat id{0,0,0,1}; V3 c{0,1.5f,-2};
    CHECK(HitQuad({0,1.5f,0},{0,0,-1},c,id,2,1.125f,u,v) && std::fabs(u-.5f)<1e-4f && std::fabs(v-.5f)<1e-4f);
    CHECK(HitQuad({0,1.5f,0},{-0.2f,0.1f,-1},c,id,2,1.125f,u,v) && u<.5f && v<.5f);
    CHECK(!HitQuad({0,1.5f,0},{0,0,1},c,id,2,1.125f,u,v));
    CHECK(!HitQuad({0,1.5f,0},{-2,0,-1},c,id,2,1.125f,u,v));
    const float s=std::sqrt(.5f); Quat yaw{0,s,0,s}; V3 c2{-2,1.5f,0};
    CHECK(HitQuad({0,1.5f,0},{-1,0,0},c2,yaw,2,1,u,v) && std::fabs(u-.5f)<1e-3f);
    CHECK(HitQuad({0,1.5f,0},{-1,0,-0.2f},c2,yaw,2,1,u,v) && u>.5f);
    CursorFilter cf; CHECK(cf.Changed(10,10,3)); CHECK(!cf.Changed(11,12,3)); CHECK(cf.Changed(14,10,3));
}
static void TargetStability() {
    using namespace d2vr_gaze_target;
    Filter f; Direction o;
    CHECK(!f.Update(false,0,1000,{},o));
    CHECK(f.Update(true,1000,1000,{0,0,-1},o));
    // Repeated consumer polls must not prolong an old sample or re-filter it.
    CHECK(f.Update(true,1000,1090,{0,0,-1},o));
    CHECK(f.Update(false,0,1120,{},o) && o.z==-1); // blink: no controller jump
    CHECK(!f.Update(false,0,1161,{},o));
    CHECK(f.Update(true,1200,1200,{0,0,-1},o));
    CHECK(f.Update(true,1210,1210,{.003f,0,-1},o) && o.x==0); // fixation noise
    CHECK(f.Update(true,1220,1220,{.5f,0,-.8660254f},o) && o.x==0); // spike
    CHECK(f.Update(true,1230,1230,{0,0,-1},o) && o.x==0);
    // A deliberate new fixation becomes responsive after a bounded confirmation.
    CHECK(f.Update(true,1240,1240,{.5f,0,-.8660254f},o) && o.x==0);
    CHECK(f.Update(true,1250,1250,{.5f,0,-.8660254f},o) && o.x==0);
    CHECK(f.Update(true,1260,1260,{.5f,0,-.8660254f},o) && o.x>.49f);
    CHECK(std::fabs(Dot(o,o)-1)<1e-5f);
    CHECK(f.Update(true,1260,1270,{-.5f,0,-.8660254f},o) && o.x>.49f); // duplicate
    CHECK(!f.Update(false,0,900,{},o)); // clock reset clears retained gaze
    CHECK(!f.Update(true,1001,1000,{0,0,-1},o)); // future sample
    CHECK(!f.Update(true,800,1000,{0,0,-1},o)); // stale sample
    CHECK(!f.Update(true,1000,1000,{NAN,0,-1},o));
    CHECK(!f.Update(true,1000,1000,{0,0,0},o));
    CHECK(f.Update(true,1300,1300,{0,0,-1},o));
    CHECK(f.Update(true,1310,1310,{.05f,0,-.998749f},o) && o.x>0 && o.x<.05f);
    f.Reset(); CHECK(!f.Update(false,0,1400,{},o));
}
int main() {
    Seqlock(); Fovea(); Menu(); TargetStability();
    std::printf("%s: %d checks, %d failed\n",g_fail?"FAIL":"PASS",g_checks,g_fail);
    return g_fail?1:0;
}

#include "../src/vr_menu_pointer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
using namespace d2vr_menu;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL line%d %s\n",__LINE__,#x);std::exit(1);}}while(0)
static Surface surface;
static Aim aim;
static void Reset() {
    surface={};surface.identity=1;surface.frame=1;surface.space=(XrSpace)2;
    surface.pose={{0,0,0,1},{0,0,-2}};surface.width=2;surface.height=1;
    surface.cursor={100,50,1920,1080};
    aim={};aim.frame=1;aim.space=surface.space;aim.menu=aim.tracked=true;aim.pose.orientation.w=1;
}
static bool Near(double a,double b,double tolerance=1e-5){return std::fabs(a-b)<tolerance;}
template<class Mutation>static void Refuse(Mutation change) {
    Reset();change();Hit hit;hit.x=13;hit.y=17;hit.u=.125;Hit before=hit;
    CHECK(!Project(surface,aim,hit));CHECK(!memcmp(&hit,&before,sizeof(hit)));
}
static XrVector3f Transform(double x,double y,double z,double angle,XrVector3f value) {
    // Independent Rodrigues oracle, rather than the implementation quaternion
    // rotation. All quantities here describe the submitted surface itself.
    const double c=std::cos(angle),s=std::sin(angle),dot=x*value.x+y*value.y+z*value.z;
    return {float(value.x*c+(y*value.z-z*value.y)*s+x*dot*(1-c)),
            float(value.y*c+(z*value.x-x*value.z)*s+y*dot*(1-c)),
            float(value.z*c+(x*value.y-y*value.x)*s+z*dot*(1-c))};
}
static XrQuaternionf Toward(XrVector3f origin,XrVector3f target) {
    double x=target.x-origin.x,y=target.y-origin.y,z=target.z-origin.z;
    const double length=std::sqrt(x*x+y*y+z*z);x/=length;y/=length;z/=length;
    double qx=y,qy=-x,qw=1-z,n=std::sqrt(qx*qx+qy*qy+qw*qw);
    CHECK(n>.01);return {float(qx/n),float(qy/n),0,float(qw/n)};
}
int main() {
    Reset();Hit hit;CHECK(Project(surface,aim,hit));
    CHECK(hit.inside && hit.x==1060 && hit.y==590 && Near(hit.distance,2));
    CHECK(Near(hit.point.x,0)&&Near(hit.point.y,0)&&Near(hit.point.z,-2));
    for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y) {
        Reset();aim.pose.position={float(x),float(y)*.5f,0};CHECK(Project(surface,aim,hit));
        CHECK(hit.inside);CHECK(hit.x==(x<0?100:x>0?2019:1060));CHECK(hit.y==(y>0?50:y<0?1129:590));
    }
    Reset();aim.pose.position.x=1.25f;CHECK(Project(surface,aim,hit));
    CHECK(!hit.inside && hit.x==2260); // controller translation is not ignored
    Refuse([]{aim.pose.position.z=-3;});
    Refuse([]{aim.pose.orientation={0,1,0,0};});
    Refuse([]{aim.pose.orientation={0,.70710678f,0,.70710678f};});
    Refuse([]{aim.tracked=false;});Refuse([]{aim.menu=false;});
    Refuse([]{aim.frame=0;});Refuse([]{aim.frame=2;});Refuse([]{aim.space=(XrSpace)3;});
    Refuse([]{surface.identity=0;});Refuse([]{surface.space=XR_NULL_HANDLE;aim.space=XR_NULL_HANDLE;});
    Refuse([]{surface.width=0;});Refuse([]{surface.height=-1;});
    Refuse([]{surface.width=NAN;});Refuse([]{surface.height=INFINITY;});
    Refuse([]{surface.cursor.width=0;});Refuse([]{surface.cursor.x=INT32_MAX;});
    Refuse([]{surface.pose.orientation.w=0;});Refuse([]{aim.pose.orientation.w=2;});
    Refuse([]{aim.pose.position.x=NAN;});Refuse([]{surface.pose.position.y=INFINITY;});
    Refuse([]{surface.pose.orientation.x=NAN;});
    std::mt19937 random(0x5d2a19);std::uniform_real_distribution<double> unit(-1,1);
    for(unsigned iteration=0;iteration<20000;++iteration) {
        Reset();double x=unit(random),y=unit(random),z=unit(random);
        const double norm=std::sqrt(x*x+y*y+z*z);if(norm<.01){--iteration;continue;}x/=norm;y/=norm;z/=norm;
        const double angle=unit(random)*3;
        surface.pose.orientation={float(x*std::sin(angle/2)),float(y*std::sin(angle/2)),float(z*std::sin(angle/2)),float(std::cos(angle/2))};
        surface.pose.position={float(unit(random)*3),float(unit(random)*3),float(unit(random)*3)};
        surface.width=float(1+std::fabs(unit(random))*3);surface.height=float(.5+std::fabs(unit(random))*2);
        const double u=.5+unit(random)*.45,v=.5+unit(random)*.45;
        auto target=Transform(x,y,z,angle,{float((u-.5)*surface.width),float((.5-v)*surface.height),0});
        target.x+=surface.pose.position.x;target.y+=surface.pose.position.y;target.z+=surface.pose.position.z;
        auto origin=Transform(x,y,z,angle,{float(unit(random)),float(unit(random)),float(1+std::fabs(unit(random))*3)});
        origin.x+=surface.pose.position.x;origin.y+=surface.pose.position.y;origin.z+=surface.pose.position.z;
        aim.pose.position=origin;aim.pose.orientation=Toward(origin,target);
        CHECK(Project(surface,aim,hit));CHECK(hit.inside);
        CHECK(Near(hit.u,u)&&Near(hit.v,v));
        CHECK(Near(hit.point.x,target.x)&&Near(hit.point.y,target.y)&&Near(hit.point.z,target.z));
        CHECK(std::abs(hit.x-int(std::floor(100+u*1920)))<=1);
        CHECK(std::abs(hit.y-int(std::floor(50+v*1080)))<=1);
    }
    Reset();Capture capture;
    auto step=[&](float trigger,bool inside=true) {
        ++surface.frame;aim.frame=surface.frame;aim.trigger=trigger;aim.pose.position.x=inside?0:1.25f;
        return capture.Update(surface,aim);
    };
    CHECK(!step(1).press);CHECK(!step(0).press);
    auto a=step(1);CHECK(a.press && a.captured && a.move && a.visible);
    a=capture.Update(surface,aim);CHECK(!a.press && !a.release && !a.move);
    a=step(.5f);CHECK(a.captured && !a.press && !a.release);
    a=step(1,false);CHECK(a.captured && a.move && !a.hit.inside && a.hit.x==2260);
    a=step(0,false);CHECK(a.release && a.move && !a.captured && a.surface==1);
    a=step(1,false);CHECK(!a.press);a=step(1);CHECK(!a.press); // no click on re-entry
    step(0);CHECK(step(1).press);
    aim.tracked=false;a=capture.Update(surface,aim);CHECK(a.release && a.surface==1);
    CHECK(!capture.Update(surface,aim).release);
    aim.tracked=true;CHECK(!step(1).press);step(0);CHECK(step(1).press);
    surface.identity=2;a=step(1);CHECK(a.release && a.surface==1 && !a.press);
    CHECK(!step(1).press);step(0);CHECK(step(1).press);
    aim.menu=false;a=capture.Update(surface,aim);CHECK(a.release && !a.visible && !a.move);
    aim.menu=true;step(0);CHECK(step(1).press);
    aim.trigger=NAN;a=capture.Update(surface,aim);CHECK(a.release);CHECK(!capture.Reset().release);
    step(0);CHECK(step(1).press);a=capture.Reset();CHECK(a.release);CHECK(!capture.Reset().release);
    // A reference-space replacement invalidates an in-flight native drag,
    // even if the producer keeps the same surface identity and both new poses
    // agree on the replacement space. Held trigger cannot reacquire the menu.
    step(0);CHECK(step(1).press);
    const auto oldSpace=surface.space;
    surface.space=aim.space=(XrSpace)0x9876;
    a=step(1);CHECK(a.release && !a.press && !a.captured && !a.visible);
    CHECK(!step(1).press);step(0);CHECK(step(1).press);
    surface.space=aim.space=oldSpace;
    a=capture.Update(surface,aim);CHECK(a.release && !a.captured); // same frame, new space
    CHECK(!capture.Update(surface,aim).release);
    CHECK(!step(1).press);step(0);CHECK(step(1).press);
    capture.Reset();
    for(unsigned cycle=0;cycle<10000;++cycle) {
        step(0);CHECK(step(1).press);CHECK(!step(1).press);
        CHECK(step(0).release);CHECK(!step(0).release);
    }
    Reset();Navigation navigation;
    const auto nav=[&](bool back,float scroll,double dt=1.0/90,bool captured=false) {
        ++surface.frame;aim.frame=surface.frame;aim.back=back;aim.scrollY=scroll;aim.time+=dt;
        return navigation.Update(surface,aim,captured);
    };
    CHECK(!nav(true,0).back);CHECK(!nav(false,0).back);CHECK(nav(true,0).back);
    CHECK(!nav(true,0).back);CHECK(!navigation.Update(surface,aim).back);
    // Back works even when the controller points away from the menu plane.
    nav(false,0);aim.pose.orientation={0,1,0,0};CHECK(nav(true,0).back);
    CHECK(!nav(true,1).scroll);aim.pose.orientation={0,0,0,1};
    aim.tracked=false;CHECK(!nav(false,0).back);aim.tracked=true;
    CHECK(!nav(true,0).back);nav(false,0);CHECK(nav(true,0).back);
    ++surface.identity;CHECK(!nav(true,0).back);nav(false,0);CHECK(nav(true,0).back);
    nav(false,0);CHECK(!nav(true,0,1).back); // stale time must not create a new click
    nav(false,0);CHECK(nav(true,0).back);
    aim.menu=false;CHECK(!nav(false,0).back);aim.menu=true;CHECK(!nav(true,0).back);
    nav(false,0);CHECK(nav(true,0).back);
    for(const double fps:{36.,45.,60.,72.,90.,120.}) {
        Reset();navigation.Reset();nav(false,0,1.0/fps);
        int sum=0;
        for(int f=0;f<int(fps*10);++f)sum+=nav(false,1,1.0/fps).scroll;
        CHECK(sum>=79 && sum<=80); // same real duration, independent of frame cap
        CHECK(!nav(false,1,2).scroll); // no burst on resume
        aim.pose.position.x=4;
        for(int f=0;f<100;++f)CHECK(!nav(false,1,1.0/fps).scroll);
        sum=0;
        for(int f=0;f<int(fps);++f)sum+=nav(false,1,1.0/fps,true).scroll;
        CHECK(sum>=7 && sum<=8); // capture retains an outside slider interaction
        nav(false,0);aim.pose.position.x=0;
        for(int f=0;f<100;++f)CHECK(!nav(false,.24f,1.0/fps).scroll);
        sum=0;
        for(int f=0;f<int(fps);++f)sum+=nav(false,-1,1.0/fps).scroll;
        CHECK(sum>=-8 && sum<=-7);
    }
    Reset();navigation.Reset();nav(false,0);nav(false,1,.1);
    CHECK(!nav(false,-1,.1).scroll);CHECK(nav(false,-1,.1).scroll==-1);
    aim.scrollY=NAN;CHECK(!navigation.Update(surface,aim).scroll);
    CHECK(!nav(true,0).back);nav(false,0);CHECK(nav(true,0).back);
    nav(false,0,10);CHECK(nav(true,0).back);
    CHECK(!nav(true,0,-1).back);nav(false,0);CHECK(nav(true,0).back);
    aim.time=-1;CHECK(!nav(false,0).back);CHECK(!nav(true,0).back);
    aim.time=20;CHECK(!nav(true,0).back);nav(false,0);CHECK(nav(true,0).back);
    std::printf("PASS x86 menu pointer core: %u checks;20000 independent 6DoF plane cases,10000 click cycles, pixel edges, slider capture, context/space/frame refusal, cancellation, Back and refresh-independent scroll. Native cursor/render integration NOT RUN.\n",checks);
}

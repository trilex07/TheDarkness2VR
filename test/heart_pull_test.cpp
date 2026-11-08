#include "../src/heart_pull.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace d2vr_input;
static unsigned checks=0;
static void Check(bool v,const char* what) {++checks;if(!v){std::printf("FAIL %s\n",what);std::exit(1);}}
static HeartPullOutput Pull(HeartPull& p,bool enabled=true,bool tracked=true,float direction=1) {
    p.Update(enabled,tracked,0,{0,0,-.6f},{},0);
    p.Update(enabled,tracked,1,{0,0,-.6f},{},.01);
    HeartPullOutput o;
    for(int i=1;i<=12;++i)o=p.Update(enabled,tracked,1,{0,0,-.6f+direction*.02f*i},{},.01+i*.01);
    return o;
}
int main() {
    HeartPull p;
    auto o=Pull(p);Check(o.eat&&o.ownsGrip,"inward pull devours");
    Check(p.Update(true,true,1,{0,0,-.36f},{},.14).eat,"held X continues");
    Check(!p.Update(true,true,1,{0,0,-.36f},{},.15).fired,"fires once per grip");
    for(int i=0;i<150;++i)o=p.Update(true,true,1,{0,0,-.36f},{},.16+i*.01);
    Check(!o.eat&&o.ownsGrip,"bounded hold keeps throw ownership until release");
    o=p.Update(true,true,0,{0,0,-.36f},{},1.67);Check(!o.eat&&o.ownsGrip,"release cannot throw");
    Check(!p.Update(true,true,0,{0,0,-.36f},{},1.68).ownsGrip,"neutral releases ownership");
    for(bool gate:{false,true}) {
        HeartPull q;Check(!Pull(q,gate,false).eat,"tracking required");
    }
    HeartPull a;Check(!Pull(a,false).eat,"gameplay gate required");
    HeartPull b;Check(!Pull(b,true,true,-1).eat,"outward throw not eat");
    HeartPull c;for(int i=0;i<20;++i)Check(!c.Update(true,true,1,{0,0,-.6f+i*.01f},{},i*.01).eat,"held on reacquire requires release");
    HeartPull d;d.Update(true,true,0,{0,0,-.6f},{},0);
    for(int i=1;i<25;++i)Check(!d.Update(true,true,1,{0,0,-.6f},{0,0,-i*.01f},i*.01).eat,"head lean alone rejected");
    HeartPull e;e.Update(true,true,0,{0,0,-.6f},{},0);e.Update(true,true,1,{0,0,-.6f},{},.01);
    Check(!e.Update(true,true,1,{0,0,-.3f},{},.11).eat,"position jump rejected");
    Check(!e.Update(true,true,1,{0,0,-.28f},{},.12).eat,"jump needs release");
    HeartPull f;Pull(f);Check(!f.Update(false,true,1,{0,0,-.36f},{},.14).eat,"menu cancels active hold");
    Check(!f.Update(true,true,1,{0,0,-.34f},{},.15).eat,"resume held cannot retrigger");
    HeartPull g;Pull(g);Check(!g.Update(true,false,1,{},{},.14).eat,"tracking loss cancels hold");
    HeartPull h;Pull(h);Check(!h.Update(true,true,1,{0,0,-.36f},{},2).eat,"clock gap cancels");
    HeartPull n;Check(!n.Update(true,true,1,{0,0,std::numeric_limits<float>::quiet_NaN()},{},0).eat,"NaN rejected");
    // Body yaw does not matter: the pull follows the initial hand-to-head ray.
    HeartPull side;side.Update(true,true,0,{.6f,0,0},{},0);side.Update(true,true,1,{.6f,0,0},{},.01);
    for(int i=1;i<=12;++i)o=side.Update(true,true,1,{.6f-i*.02f,0,0},{},.01+i*.01);
    Check(o.eat,"side-oriented inward pull");
    std::printf("PASS %u heart-pull checks\n",checks);
}

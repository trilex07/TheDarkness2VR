#include "../src/gaze_lod_observation.h"
#include <limits>
#include <cstdio>
using namespace d2vr_gaze_lod;
int main() {
    int failures=0,checks=0;
    auto check=[&](bool ok){++checks;if(!ok)++failures;};
    Sample s; s.ownerVerified=s.gazeValid=s.decoration=true;
    s.now=s.gazeTime=1000;s.levels=3;s.direction={0,0,-1};s.center={0,0,-10};
    check(Observe(s).region==Region::Protected);
    s.center={10,0,0};check(Observe(s).candidate);
    s.decoration=false;check(!Observe(s).candidate);s.decoration=true;
    s.radius=9;check(!Observe(s).candidate); // large object overlaps protected cone
    s.radius=10;check(Observe(s).region==Region::Protected);s.radius=0;
    s.gazeTime=899;check(Observe(s).region==Region::Unknown);
    s.gazeTime=1001;check(Observe(s).region==Region::Unknown);s.gazeTime=1000;
    s.selected=2;check(!Observe(s).candidate);s.selected=3;check(Observe(s).region==Region::Unknown);
    s.selected=0;s.ownerVerified=false;check(!Observe(s).candidate);s.ownerVerified=true;
    s.direction={};check(Observe(s).region==Region::Unknown);
    s.direction={0,0,-1};s.radius=-1;check(Observe(s).region==Region::Unknown);
    s.radius=0;s.center.x=std::numeric_limits<float>::quiet_NaN();check(!Observe(s).candidate);
    std::printf("%d checks, %d failures\n",checks,failures);return failures?1:0;
}

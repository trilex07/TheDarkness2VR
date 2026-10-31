#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "../src/shared_color_reuse.h"
#define CHECK(x) do {if(!(x)){std::printf("FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace d2vr_color_reuse;
int main() {
    int rt0=0,other=0;unsigned checks=0;
    // Exhaustive public D3DFORMAT/multisample sweep: only X8/A8 single-sample.
    for(unsigned format=0;format<130;++format)for(unsigned ms=0;ms<17;++ms) {
        const bool expected=ms==0 && (format==21 || format==22);
        CHECK(LosslessSlotFormat(format,ms)==expected);++checks;
    }
    for(unsigned format:{21u,22u}) {
        Mirror slot{&rt0,5};
        CHECK(Mirrors(slot,&rt0,4,2676,2728,format,0,2676,2728));
        CHECK(!Mirrors(slot,&rt0,5,2676,2728,format,0,2676,2728));  // copy predates this pair
        CHECK(!Mirrors(slot,&rt0,6,2676,2728,format,0,2676,2728));
        CHECK(!Mirrors(slot,&other,4,2676,2728,format,0,2676,2728)); // different RT0
        CHECK(!Mirrors(slot,nullptr,4,2676,2728,format,0,2676,2728));
        CHECK(!Mirrors(slot,&rt0,4,2676,2728,format,1,2676,2728));   // MSAA
        CHECK(!Mirrors(slot,&rt0,4,2676,2727,format,0,2676,2728));   // size
        CHECK(!Mirrors(slot,&rt0,4,2675,2728,format,0,2676,2728));
        CHECK(!Mirrors(slot,&rt0,4,0,0,format,0,0,0));
        CHECK(!Mirrors(Mirror{},&rt0,0,2676,2728,format,0,2676,2728)); // failed/cleared capture
        CHECK(Unchanged(slot,&rt0,5));
        CHECK(!Unchanged(slot,&rt0,4) && !Unchanged(slot,&other,5) && !Unchanged(slot,&rt0,0));
        Mirror later{&rt0,6};CHECK(!Unchanged(later,&rt0,5)); // re-captured before restore
        checks+=13;
    }
    for(unsigned format:{20u,23u,31u,113u,114u,116u})              // R8G8B8,R5G6B5,A2B10G10R10,16F,32F
        {Mirror slot{&rt0,2};CHECK(!Mirrors(slot,&rt0,1,64,64,format,0,64,64));++checks;}
    // Pair sequence: left copy (serial n+1) mirrors; right copy must be newer.
    unsigned long long serial=0;
    for(unsigned pair=0;pair<100000;++pair) {
        const unsigned long long start=serial;
        Mirror left{&rt0,++serial};
        CHECK(Mirrors(left,&rt0,start,8,8,22,0,8,8));
        const auto validated=left.serial;
        Mirror right{&rt0,++serial};
        CHECK(Mirrors(right,&rt0,validated,8,8,22,0,8,8));
        CHECK(Unchanged(left,&rt0,validated));
        CHECK(!Mirrors(left,&rt0,validated,8,8,22,0,8,8)); // left copy is never the right history
        checks+=4;
    }
    std::printf("PASS shared colour reuse policy: %u checks (format sweep, identity, pair serial ordering, restore re-validation)\n",checks);
    return 0;
}

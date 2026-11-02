#include "../../src/cinematic_theater.h"
#include "../../src/native_player_camera_owner.h"
#include <cassert>
#include <cstdio>
#include <unordered_map>
#include <limits>
int main() {
    using namespace d2vr_cinema;
    assert(Select(false,true,true,true)==Mode::Gameplay);
    assert(Select(true,false,true,false)==Mode::Gameplay); // stale cinematic rejected
    assert(Select(true,true,true,false)==Mode::Scene3D);
    assert(Select(true,false,false,true)==Mode::Video2D);
    assert(Select(true,true,true,true)==Mode::Video2D);
    Playback p;p.Observe(0,true);assert(!p.Active());
    p.Observe(11,true);p.Observe(11,true);p.Observe(22,true);
    p.Observe(11,false);assert(p.Active());
    p.Observe(99,false);assert(p.Active());
    p.Observe(22,false);assert(!p.Active());
    for(unsigned i=1;i<=16;++i)p.Observe(i,true);
    p.Observe(17,true);p.Observe(17,false);assert(p.Active());
    for(unsigned i=1;i<=16;++i)p.Observe(i,false);assert(!p.Active());
    for(float a=.2f;a<1.5f;a+=.1f)for(float b=.1f;b<=a;b+=.1f) {
        const float crop=CommonCrop(a,b);
        assert(crop>0 && crop<=1);
        assert(std::abs(std::tan(a)*crop-std::tan(b))<.00001f);
    }
    assert(CommonCrop(.5f,.6f)==0);
    assert(CommonCrop(std::numeric_limits<float>::quiet_NaN(),.5f)==0);
    std::unordered_map<uint32_t,uint32_t> memory{{0x1000,0xfc7d00},{0x1268,0x2000},{0x2000,0x3000}};
    const auto read=[&](uint32_t a,uint32_t& v){auto i=memory.find(a);if(i==memory.end())return false;v=i->second;return true;};
    bool playing=false;
    assert(d2vr_player_camera::PlayingCinematic(0x1000,read,playing) && playing);
    memory[0x2000]=0;assert(d2vr_player_camera::PlayingCinematic(0x1000,read,playing) && !playing);
    memory.erase(0x2000);assert(!d2vr_player_camera::PlayingCinematic(0x1000,read,playing));
    memory[0x1000]=0;assert(!d2vr_player_camera::PlayingCinematic(0x1000,read,playing));
    puts("PASS cinematic modes, stale-state rejection, Bink lifetime, binocular angular crop, native owner reader");
}

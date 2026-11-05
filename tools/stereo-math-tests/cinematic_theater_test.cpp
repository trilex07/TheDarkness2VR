#include "../../src/cinematic_theater.h"
#include "../../src/native_player_camera_owner.h"
#include "../../src/vr_input.h"
#include <cassert>
#include <cstdio>
#include <unordered_map>
#include <limits>
int main() {
    // A supported/foreign/temporarily missing selected player all supersede a
    // stale Jackie renderer override; an unrelated scene/renderer does not.
    d2vr_player_camera::SelectedOwnerObservation selected;
    using OwnerStatus=d2vr_player_camera::Status;
    for(auto status:{OwnerStatus::Matched,OwnerStatus::AvatarType,
        OwnerStatus::CameraOwner,OwnerStatus::InventoryType,OwnerStatus::LocalPlayerReference}) {
        selected.Observe(8,0x1000,status);
        assert(selected.SupersedesFallback(8,0x1000));
        assert(!selected.SupersedesFallback(9,0x1000));
        assert(!selected.SupersedesFallback(8,0x2000));
    }
    for(auto status:{OwnerStatus::ViewContext,OwnerStatus::RegionManager,OwnerStatus::SceneReference}) {
        selected.Observe(8,0x1000,status);
        assert(!selected.SupersedesFallback(8,0x1000));
    }
    using namespace d2vr_cinema;
    assert(Select(false,true,true,true)==Mode::Gameplay);
    assert(Select(true,false,true,false)==Mode::Gameplay); // stale cinematic rejected
    assert(Select(true,true,true,false)==Mode::Scene3D);
    assert(Select(true,false,false,true)==Mode::Video2D);
    assert(Select(true,true,true,true)==Mode::Video2D);
    auto request=RequestOpposite(Mode::Video2D,1000);
    assert(TemporaryOverride(request,1000)==Override::Gameplay);
    assert(TemporaryOverride(request,60999)==Override::Gameplay);
    assert(TemporaryOverride(request,61000)==Override::Auto);
    assert(Select(true,true,true,true,TemporaryOverride(request,2000))==Mode::Gameplay);
    request=RequestOpposite(Mode::Gameplay,2000);
    assert(Select(true,true,false,true,TemporaryOverride(request,2000))==Mode::Scene3D);
    assert(Select(true,true,false,true,TemporaryOverride(request,62000))==Mode::Video2D);
    request=RequestOpposite(Mode::Scene3D,61000);
    assert(TemporaryOverride(request,62000)==Override::Gameplay); // renewed full minute
    assert(TemporaryOverride(request,121000)==Override::Auto);
    assert(Select(false,true,true,true,Override::Scene3D)==Mode::Gameplay);
    d2vr_input::RecenterModeChord chord;
    assert(chord.Update(true,true,0)); // existing click recenter
    int toggles=0;
    for(int i=1;i<=40;++i) {assert(!chord.Update(true,true,i*.1));toggles+=chord.modeToggle;}
    assert(toggles==1); // prolonged hold never repeats
    chord.Update(false,true,4.1);chord.Update(true,true,4.2);assert(!chord.modeToggle);
    chord.Update(false,false,4.3);
    assert(chord.Update(true,true,4.4));
    chord.Update(false,true,4.5);
    for(int i=46;i<=65;++i) {chord.Update(true,true,i*.1);assert(!chord.modeToggle);}
    chord.Update(false,false,6.6);chord.Update(true,true,6.7);
    chord.Update(true,true,8);assert(!chord.modeToggle); // polling gap cancels hold
    chord.Update(false,false,8.1);chord.Update(true,true,8.2);
    chord.Update(true,true,8.3,false);
    for(int i=84;i<=105;++i) {chord.Update(true,true,i*.1);assert(!chord.modeToggle);}
    chord.Update(false,false,10.6);chord.Update(true,true,10.7);
    toggles=0;
    for(int i=108;i<=126;++i) {chord.Update(true,true,i*.1);toggles+=chord.modeToggle;}
    assert(toggles==1);
    d2vr_input::ObjectiveButton objective;
    objective.Update(true,false,true,0);
    auto action=objective.Update(true,true,true,.1);
    assert(!action.nativeB && !action.showObjective);
    action=objective.Update(true,false,true,.2);assert(action.nativeB && !action.showObjective);
    objective.Update(true,false,true,.4);
    objective.Update(true,true,true,.5);
    int pulses=0;
    for(int i=6;i<=25;++i) {
        action=objective.Update(true,true,true,i*.1);
        assert(!action.nativeB);
        if(action.showObjective)++pulses;
    }
    assert(pulses>=10); // sustained BACK level after threshold, not a missed 120ms pulse
    action=objective.Update(true,false,true,2.6);assert(!action.nativeB && !action.showObjective);
    objective.Update(true,true,true,2.7);
    action=objective.Update(true,true,false,2.8);assert(!action.nativeB && !action.showObjective);
    action=objective.Update(true,false,false,2.9);assert(!action.nativeB);
    action=objective.Update(true,true,false,3);assert(action.nativeB && !action.showObjective);
    action=objective.Update(false,true,true,3.1);assert(!action.nativeB && !action.showObjective);
    action=objective.Update(true,true,true,3.2);assert(!action.nativeB && !action.showObjective);
    objective={};objective.Update(true,false,true,0);
    objective.Update(true,true,true,.01);
    assert(objective.Update(true,false,true,.05).nativeB);
    // A quick second press must cancel the previous short ability pulse.
    action=objective.Update(true,true,true,.08);assert(!action.nativeB && !action.showObjective);
    for(int i=1;i<=20;++i) {
        action=objective.Update(true,true,true,.08+i*.1);
        assert(!action.nativeB);
        if(i>=11)assert(action.showObjective);
    }
    action=objective.Update(true,false,true,2.2);assert(!action.nativeB && !action.showObjective);
    d2vr_input::Tentacles rightPause,leftPause;
    for(int cycle=0;cycle<10;++cycle) {
        rightPause.SyncNativePause(true);leftPause.SyncNativePause(true);
        assert(rightPause.paused && leftPause.paused);
        // Continue is a native UI action, with no second START edge.
        rightPause.SyncNativePause(false);leftPause.SyncNativePause(false);
        assert(!rightPause.paused && !leftPause.paused);
        assert(!rightPause.rightDown && !leftPause.leftDown);
    }
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
    memory[0x1000]=0xfc7d00;memory[0x2000]=0x3000;
    memory[0x3000]=0xfca7e0;memory[0x3708]=2;memory[0x37bc]=0;
    d2vr_player_camera::CinematicView scene;
    assert(d2vr_player_camera::InspectCinematicView(0x1000,read,scene) && scene.TakesCamera());
    memory[0x37bc]=1; // canDamagePlayer is NOT free camera.
    assert(d2vr_player_camera::InspectCinematicView(0x1000,read,scene) && scene.TakesCamera());
    memory[0x37bc]=0x100; // Native free-camera flag.
    assert(d2vr_player_camera::InspectCinematicView(0x1000,read,scene) && scene.running && scene.freeCamera);
    assert(Select(true,true,scene.TakesCamera(),false)==Mode::Gameplay);
    memory[0x37bc]=0x10000; // Camera on rails, native look/firing allowed.
    assert(d2vr_player_camera::InspectCinematicView(0x1000,read,scene) && scene.onRails && !scene.TakesCamera());
    memory[0x37bc]=0;memory[0x3708]=1; // retained object before playing
    assert(d2vr_player_camera::InspectCinematicView(0x1000,read,scene) && !scene.running && !scene.TakesCamera());
    memory[0x3708]=2;
    for(auto invalid:{0x200u,0x20000u}){memory[0x37bc]=invalid;assert(!d2vr_player_camera::InspectCinematicView(0x1000,read,scene));}
    for(auto foreign:{0xfdd0b0u,0xfdd318u,0xdeadbeefu}){memory[0x37bc]=0;memory[0x3000]=foreign;assert(!d2vr_player_camera::InspectCinematicView(0x1000,read,scene));}
    memory[0x2000]=0;assert(d2vr_player_camera::InspectCinematicView(0x1000,read,scene) && !scene.running);
    // Native ControlAvatar can replace the authoritative local player before
    // the renderer's optional FOV override. Reproduce that ownership split.
    memory={{0x43f8,0x5000},{0x5000,0x6000},{0x6000,0xfc6d40},
        {0x6064,0x7000},{0x7000,0x8000},{0x8000,0xfc7d00},
        {0x9000,0xa000},{0x81c8,0xa000},{0xb000,0xc000},
        {0xc000,0x1001630},{0xc62c,0xd000},{0xd000,0xe000},
        {0xe000,0xffc7c0},{0xe030,0xb000},{0xc630,0xf000},
        {0xf000,0x10000},{0x10000,0xffca10},{0x102a8,0xb000},
        {0x4460,0xb000}};
    unsigned releases=0,consumed=0;
    const auto get=[&](uint32_t manager,uint32_t* out){assert(manager==0x8000);*out=0xb000;return out;};
    const auto release=[&](uint32_t ref){assert(ref==0xb000);++releases;};
    const auto consume=[&](const d2vr_player_camera::Owner& owner){assert(owner.avatar==0xc000);++consumed;};
    auto ownerStatus=d2vr_player_camera::WithSelectedLocalPlayer(0x4000,0x8000,0x9000,read,get,release,consume);
    assert(ownerStatus==OwnerStatus::Matched && releases==1 && consumed==1);
    // Renderer still points to valid Jackie while selected local player is a
    // foreign avatar. No consume and no fallback into stale Jackie ownership.
    memory[0x11000]=0x12000;memory[0x12000]=0xdeadbeef;
    const auto foreignGet=[&](uint32_t,uint32_t* out){*out=0x11000;return out;};
    const auto foreignRelease=[&](uint32_t ref){assert(ref==0x11000);++releases;};
    ownerStatus=d2vr_player_camera::WithSelectedLocalPlayer(0x4000,0x8000,0x9000,read,foreignGet,foreignRelease,consume);
    d2vr_player_camera::Owner stale;
    assert(d2vr_player_camera::Inspect(0x4000,read,stale)==OwnerStatus::Matched);
    assert(ownerStatus==OwnerStatus::AvatarType && releases==2 && consumed==1);
    selected.Observe(10,0x4000,ownerStatus);assert(selected.SupersedesFallback(10,0x4000));
    bool unwound=false;
    try {d2vr_player_camera::WithSelectedLocalPlayer(0x4000,0x8000,0x9000,read,get,release,
        [](const d2vr_player_camera::Owner&){throw 7;});}
    catch(int value){unwound=value==7;}
    assert(unwound && releases==3);
    puts("PASS cinematic modes, stale-state rejection, Bink lifetime, binocular angular crop, native owner reader");
}

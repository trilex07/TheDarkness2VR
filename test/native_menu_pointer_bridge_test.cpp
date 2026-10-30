#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../src/native_menu_pointer_bridge.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace d2vr_menu_native;
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL %d %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Fixture {
    PointerBridge bridge;
    d2vr_menu::Surface surface{7,1,(XrSpace)1,{{0,0,0,1},{0,0,-2}},2,2,{10,20,800,600}};
    d2vr_menu::Aim aim{1,(XrSpace)1,{{0,0,0,1},{0,0,0}},true,true,0,false,0,1};
    uint8_t state[20]{};MouseEvent events[256]{};uint32_t count=0;unsigned moves=0;int x=-1,y=-1;
    PointerFrame Frame(float trigger,bool focused=true,bool submitted=true) {
        ++aim.frame;surface.frame=aim.frame;aim.time+=.01;aim.trigger=trigger;
        return bridge.Update(surface,aim,focused,submitted);
    }
    bool Poll(uint32_t status=0,uint32_t capacity=256,uint32_t device=2,double age=0) {
        count=0;state[12]=0;
        return bridge.Poll(1,device,3,1,status,0,20,state,events,count,capacity,
            aim.time+age,123,456,[&](int px,int py){++moves;x=px;y=py;return true;});
    }
};
int main(){
    // Actual ray -> client pixel -> queued trigger -> native event transaction.
    Fixture f;CHECK(f.Frame(0).pointer.visible);CHECK(f.Poll());
    CHECK(f.x==410&&f.y==320&&f.count==0);
    CHECK(f.Frame(1).pointer.press);CHECK(f.Poll());
    CHECK(f.bridge.Owned()&&f.count==1&&f.events[0].offset==12&&f.events[0].data==128);
    CHECK(f.events[0].time==123&&f.events[0].sequence==456);
    auto duplicate=f.bridge.Update(f.surface,f.aim,true,true);CHECK(!duplicate.pointer.press);
    CHECK(f.Poll()&&f.count==0&&f.state[12]==128);
    // Slider capture continues beyond the surface, then releases once.
    f.aim.pose.position.x=2;CHECK(f.Frame(1).pointer.captured);CHECK(f.Poll()&&f.x==1210);
    CHECK(f.Frame(0).pointer.release);CHECK(f.Poll()&&f.count==1&&!f.state[12]);
    CHECK(f.Poll()&&f.count==0&&!f.bridge.Owned());
    // Press outside cannot become a click by moving inside while held.
    CHECK(!f.Frame(1).pointer.press);f.aim.pose.position.x=0;
    CHECK(!f.Frame(1).pointer.press);CHECK(f.Poll()&&f.count==0);
    CHECK(f.Frame(0).pointer.visible);CHECK(f.Frame(1).pointer.press);CHECK(f.Poll());
    // Tracking/focus loss drains only the owned release; no press on reentry.
    CHECK(!f.Frame(1,false).pointer.visible);CHECK(f.Poll()&&f.count==1&&!f.bridge.Owned());
    CHECK(!f.Frame(1).pointer.press);CHECK(f.Poll()&&f.count==0);
    f.Frame(0);f.Frame(1);CHECK(f.Poll()&&f.bridge.Owned());
    f.aim.tracked=false;f.Frame(1);CHECK(f.Poll()&&f.count==1&&!f.bridge.Owned());
    f.aim.tracked=true;
    // Replaced surface cannot receive the previous surface's held click.
    f.Frame(0);f.Frame(1);CHECK(f.Poll()&&f.bridge.Owned());
    ++f.surface.identity;CHECK(!f.Frame(1).pointer.visible);
    CHECK(!f.Poll(0,256,4)&&f.bridge.Owned()); // never release another device
    CHECK(f.Poll()&&f.count==1&&!f.bridge.Owned());
    CHECK(!f.Frame(1).pointer.press);f.Frame(0);CHECK(f.Frame(1).pointer.press);
    // Overflow defers a quick complete click without losing either edge.
    Fixture q;q.Frame(0);q.Frame(1);q.Frame(0);CHECK(q.bridge.Pending()==2);
    CHECK(q.Poll(1)&&q.count==0&&q.bridge.Pending()==2);
    CHECK(q.Poll(0,1)&&q.count==1&&q.bridge.Owned()&&q.bridge.Pending()==1);
    CHECK(q.Poll(0,1)&&q.count==1&&!q.bridge.Owned()&&!q.bridge.Pending());
    // Stale/unsubmitted frames cannot inject a queued press.
    Fixture stale;stale.Frame(0);stale.Frame(1);CHECK(!stale.Poll(0,256,2,.251));
    CHECK(!stale.bridge.Pending()&&!stale.bridge.Owned());
    Fixture unseen;unseen.Frame(0);CHECK(!unseen.Frame(1,true,false).pointer.press);
    CHECK(!unseen.Poll()&&!unseen.bridge.Pending());
    // A native physical hold survives cancellation/release of our laser.
    Fixture physical;physical.Frame(0);physical.Frame(1);physical.state[12]=0x83;
    auto move=[](int,int){return true;};
    CHECK(physical.bridge.Poll(1,2,3,1,0,0,20,physical.state,physical.events,physical.count,
        256,physical.aim.time,1,2,move));CHECK(physical.count==0&&physical.bridge.Owned());
    physical.bridge.Cancel();physical.count=0;physical.state[12]=0x83;
    CHECK(physical.bridge.Poll(1,2,3,1,0,0,20,physical.state,physical.events,physical.count,
        256,physical.aim.time,1,2,move));CHECK(physical.state[12]==0x83&&!physical.count);
    // A cursor write failure must not commit a button to the wrong position.
    Fixture fail;fail.Frame(0);fail.Frame(1);
    CHECK(!fail.bridge.Poll(1,2,3,1,0,0,20,fail.state,fail.events,fail.count,256,fail.aim.time,
        1,2,[](int,int){return false;}));CHECK(!fail.bridge.Owned()&&!fail.count);
    // Reentrant input from a cursor callback is rejected before native writes.
    Fixture re;re.Frame(0);re.Frame(1);
    CHECK(!re.bridge.Poll(1,2,3,1,0,0,20,re.state,re.events,re.count,256,re.aim.time,1,2,
        [&](int,int){CHECK(!re.Poll());return true;}));CHECK(!re.bridge.Owned()&&!re.count);
    Fixture update;update.Frame(0);update.Frame(1);
    CHECK(!update.bridge.Poll(1,2,3,1,0,0,20,update.state,update.events,update.count,
        256,update.aim.time,1,2,[&](int,int){update.Frame(0);return true;}));
    CHECK(!update.count&&!update.bridge.Owned());
    // Exceptions do not strand the reentrancy guard or a synthetic press.
    Fixture thrown;thrown.Frame(0);thrown.Frame(1);
    CHECK(!thrown.bridge.Poll(1,2,3,1,0,0,20,thrown.state,thrown.events,thrown.count,
        256,thrown.aim.time,1,2,[](int,int)->bool{throw 1;}));
    thrown.Frame(0);thrown.Frame(1);CHECK(thrown.Poll()&&thrown.bridge.Owned());
    // Reused native addresses with a different lifetime cannot own our release.
    thrown.bridge.Cancel();thrown.state[12]=0;thrown.count=0;
    CHECK(!thrown.bridge.Poll(1,2,3,2,0,0,20,thrown.state,thrown.events,thrown.count,
        256,thrown.aim.time,1,2,move));CHECK(thrown.bridge.Owned());
    CHECK(thrown.Poll()&&!thrown.bridge.Owned());
    // Invalid geometry on a duplicate application frame cancels immediately.
    Fixture invalid;invalid.Frame(0);invalid.Frame(1);CHECK(invalid.Poll());
    invalid.surface.width=0;CHECK(!invalid.bridge.Update(invalid.surface,invalid.aim,true,true).pointer.visible);
    CHECK(invalid.Poll()&&invalid.count==1&&!invalid.bridge.Owned());
    // Multiple pre-poll locations must not turn a click on A into a click on B.
    Fixture delayed;delayed.Frame(0);delayed.Frame(1);delayed.aim.pose.position.x=.25f;
    CHECK(!delayed.Frame(0).pointer.visible);CHECK(!delayed.Poll()&&!delayed.count);
    // Back/scroll are emitted once by the shared submitted-surface navigation.
    Fixture nav;nav.Frame(0);nav.aim.back=true;CHECK(nav.Frame(0).navigation.back);
    CHECK(!nav.Frame(0).navigation.back);nav.aim.back=false;nav.Frame(0);
    nav.aim.scrollY=1;int scroll=0;for(unsigned i=0;i<20;++i)scroll+=nav.Frame(0).navigation.scroll;
    CHECK(scroll>=1);nav.aim.tracked=false;CHECK(!nav.Frame(0).navigation.scroll);
    std::printf("PASS native pointer-to-poll bridge: %u checks; ray/client pixels, trigger, drag, input ownership, focus/tracking/menu replacement, overflow, stale frames, cursor failure, reentrancy, navigation\n",checks);
}

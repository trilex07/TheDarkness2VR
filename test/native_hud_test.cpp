#include "../src/native_hud.h"
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main() {
    using namespace d2vr_hud;
    CHECK(!GuidanceActive(0,0));CHECK(GuidanceActive(100,8100));
    CHECK(GuidanceActive(8099,8100));CHECK(!GuidanceActive(8100,8100));
    CHECK(!GuidanceActive(99,8100)); // stale/future deadline outside bounded lease
    std::unordered_map<uint32_t,uint32_t> memory={{0x20000,0x1001630},{0x204e4,0x30000},
        {0x30000,0x40000},{0x40074,0x50000},{0x50000,0x20000},
        {0x40060,0x60000},{0x60000,0x70000},{0x70000,0xfe3b88}};
    auto read=[&](uint32_t a,uint32_t& v){auto it=memory.find(a);if(it==memory.end())return false;v=it->second;return true;};
    Owner owner;CHECK(Resolve(0x20000,read,owner));CHECK(owner.hud==0x70000);
    for(const auto& entry:memory) {
        auto saved=entry.second;memory[entry.first]=0;Owner out;out.hud=99;
        CHECK(!Resolve(0x20000,read,out));CHECK(out.hud==99);memory[entry.first]=saved;
    }
    CHECK(!Resolve(UINT32_MAX-2,read,owner));CHECK(!Resolve(0x20001,read,owner));
    CHECK(Resolve(0x20000,read,owner));
    for(uint8_t initial:{uint8_t(0),uint8_t(1)}) {
        Override state;uint8_t visible=initial;unsigned writes=0;
        auto rb=[&](uint32_t a,uint8_t& v){CHECK(a==owner.hud+0x13c);v=visible;return true;};
        auto wb=[&](uint32_t a,uint8_t v){CHECK(a==owner.hud+0x13c);visible=v;++writes;return true;};
        CHECK(state.Update(owner,true,rb,wb));CHECK(visible==0 && state.held);
        for(unsigned frame=0;frame<100;++frame)CHECK(state.Update(owner,true,rb,wb));
        CHECK(writes==initial); // no repeated writes once hidden
        CHECK(state.Update(owner,false,rb,wb));CHECK(visible==initial && !state.held);
        CHECK(state.Update(owner,true,rb,wb));
        visible=1; // native writer wins when leaving VR
        CHECK(state.Update(owner,false,rb,wb));CHECK(visible==1 && !state.held);
        CHECK(state.Update(owner,true,rb,wb));
        auto replacement=owner;replacement.hudRef+=4;visible=0;
        CHECK(state.Update(replacement,false,rb,wb));CHECK(!state.held && visible==0);
    }
    Override state;auto unread=[](uint32_t,uint8_t&){return false;};
    auto refuse=[](uint32_t,uint8_t){return false;};
    auto rb=[](uint32_t,uint8_t& v){v=1;return true;};
    CHECK(!state.Update(owner,true,unread,refuse));CHECK(!state.held);
    CHECK(!state.Update(owner,true,rb,refuse));CHECK(!state.held);
    auto invalid=[](uint32_t,uint8_t& v){v=3;return true;};
    CHECK(!state.Update(owner,true,invalid,refuse));CHECK(!state.held);
    std::printf("Native HUD owner/override: %u checks PASS\n",checks);
}

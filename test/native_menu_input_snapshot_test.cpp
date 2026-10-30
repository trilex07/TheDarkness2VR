#include "../src/native_menu_input_snapshot.h"
#include <map>
#include <string>
#include <cstdio>
#include <cstdlib>
using namespace d2vr_menu_native;
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL %d %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main() {
    std::map<uint32_t,uint32_t> memory{
        {0x100000,0xFC8930},{0x1000F8,0x1234},{0x1000A8,0x200000},
        {0x200000,0x210000},{0x210374,0x300000},{0x300000,0x310000},
        {0x310000,0xFBFCD0},{0x310A44,0x400000},{0x400000,0x410000},
        {0x410000,0xFC00C8},{0x4101A4,0x500000},{0x500000,0x600000},
        {0x600024,0x70001001},{0x600028,0x70002001}};
    unsigned reads=0;
    const auto read=[&](uint32_t address,uint32_t& value) {
        ++reads;const auto found=memory.find(address);if(found==memory.end())return false;
        value=found->second;return true;
    };
    Snapshot first,second;
    CHECK(Observe(0x1234,0x100000,read,first));CHECK(reads==14);
    CHECK(first.client==0x210000 && first.input==0x310000 && first.mouse==0x410000);
    CHECK(first.device==0x500000 && first.getState==0x70001001 && first.getData==0x70002001);
    CHECK(Observe(0x1234,0x100000,read,second));CHECK(first==second);
    // Refusals expose the exact contract failure, not a partial receiver.
    Refusal refusal;
    memory.erase(0x310A44);second=first;
    CHECK(!Observe(0x1234,0x100000,read,second,0,&refusal));
    CHECK(first==second && !refusal.readable && refusal.address==0x310A44);
    CHECK(std::string(refusal.stage)=="mouse-reference");
    memory[0x310A44]=0x400000;memory[0x310000]=0xFBFCD4;
    CHECK(!Observe(0x1234,0x100000,read,second,0,&refusal));
    CHECK(first==second && refusal.readable && refusal.observed==0xFBFCD4 && refusal.expected==0xFBFCD0);
    CHECK(std::string(refusal.stage)=="input-type");
    memory[0x310000]=0xFBFCD0;
    CHECK(Observe(0x1234,0x100000,read,second,0,&refusal));
    CHECK(first==second && std::string(refusal.stage)=="none" && !refusal.address);
    const auto originalMemory=memory;
    // Mode3 replaces FrameworkWin's instance table before input observation.
    // A valid receiver must survive exactly OUR admitted immutable clone.
    constexpr uint32_t clone=0x71000000;
    memory[0x100000]=clone;second=first;
    CHECK(!Observe(0x1234,0x100000,read,second));CHECK(first==second);
    CHECK(Observe(0x1234,0x100000,read,second,clone));CHECK(first==second);
    CHECK(!Observe(0x1234,0x100000,read,second,clone+4));
    CHECK(!Observe(0x4321,0x100000,read,second,clone));
    for(uint32_t field:{0x1000A8u,0x310000u,0x410000u,0x600024u}) {
        const auto saved=memory[field];memory[field]=0;
        CHECK(!Observe(0x1234,0x100000,read,second,clone));
        CHECK(first==second);memory[field]=saved;
    }
    memory[0x100000]=clone+4;
    CHECK(!Observe(0x1234,0x100000,read,second,clone));
    memory=originalMemory;
    for(auto entry:originalMemory) {
        memory.erase(entry.first);second=first;
        CHECK(!Observe(0x1234,0x100000,read,second));CHECK(first==second);
        memory.insert(entry);
    }
    for(uint32_t address:{0x100000u,0x1000F8u,0x310000u,0x410000u,0x600024u,0x600028u}) {
        const auto original=memory[address];memory[address]=0;second=first;
        CHECK(!Observe(0x1234,0x100000,read,second));CHECK(first==second);
        memory[address]=original;
    }
    for(uint32_t root:{0u,4u,0xFFFFu,0x100001u,0xFFFFFFFCu,0xFFFFFFF8u}) {
        second=first;CHECK(!Observe(0x1234,root,read,second));CHECK(first==second);
    }
    CHECK(!Observe(0,0x100000,read,second));CHECK(!Observe(0x4321,0x100000,read,second));
    memory[0x600028]=0x70002002;
    CHECK(Observe(0x1234,0x100000,read,second));CHECK(!(first==second));
    // Intrusive reference wrappers cannot be interpreted directly as objects.
    memory[0x1000A8]=0x210000;CHECK(!Observe(0x1234,0x100000,read,second));
    memory={{0x100000,0xFC7D00},{0xFC7E3C,0x490D30},{0x1000BC,0x200000},
        {0x200000,0x210000},{0x210000,0xFD8240},{0xFD82C0,0xA4D340},
        {0x210198,0},{0x2100EC,0x300000},{0x300000,0x310000}};
    PauseSnapshot pause,other;
    CHECK(ObservePause(0x100000,read,pause));CHECK(pause.showing);
    CHECK(pause.rules==0x210000 && pause.menu==0x310000);
    CHECK(ObservePause(0x100000,read,other) && pause==other);
    const auto pauseMemory=memory;
    for(auto entry:pauseMemory) {
        memory.erase(entry.first);other=pause;
        CHECK(!ObservePause(0x100000,read,other));CHECK(pause==other);
        memory.insert(entry);
    }
    memory[0x210198]=0xFFFFFF00;CHECK(ObservePause(0x100000,read,other) && other.showing);
    for(uint32_t flag:{1u,2u,255u,0x10001u}) {
        memory[0x210198]=flag;memory.erase(0x2100EC);reads=0;
        CHECK(ObservePause(0x100000,read,other));CHECK(!other.showing && other.suppressed==uint8_t(flag));
        CHECK(reads==7 && !other.menuRef && !other.menu);
    }
    memory=pauseMemory;memory[0x300000]=0;
    CHECK(ObservePause(0x100000,read,other) && !other.showing);
    memory=pauseMemory;memory[0xFD82C0]=0xA4D350;other=pause;
    CHECK(!ObservePause(0x100000,read,other) && other==pause);
    memory=pauseMemory;memory[0x210000]=0x1008248;memory[0x10082C8]=0xA4D340;
    CHECK(ObservePause(0x100000,read,other) && other.showing && !(other==pause));
    for(uint32_t root:{0u,4u,0xFFFFu,0x100001u,0xFFFFFFFCu,0xFFFFFFF8u}) {
        other=pause;CHECK(!ObservePause(root,read,other));CHECK(other==pause);
    }
    std::printf("PASS %u native menu snapshot checks: receiver graph, pause predicate/short circuit, inherited method, unknown override, reference indirections, bounded reads, unchanged failure output; no native calls or writes\n",checks);
}

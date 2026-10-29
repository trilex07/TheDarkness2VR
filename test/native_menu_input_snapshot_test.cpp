#include "../src/native_menu_input_snapshot.h"
#include <map>
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
    const auto originalMemory=memory;
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
    std::printf("PASS %u native menu snapshot checks: exact types, reference indirections, wrong HWND, bounded reads, null/alignment/overflow refusal, unchanged failure output, changed snapshot rejection; no native calls or writes\n",checks);
}

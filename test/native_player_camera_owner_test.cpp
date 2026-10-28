#include "../src/native_player_camera_owner.h"
#include <map>
#include <cstdio>
#include <cstdlib>
using namespace d2vr_player_camera;
static unsigned checks=0;
static void Check(bool v){++checks;if(!v){std::printf("FAIL %u\n",checks);std::exit(1);}}
int main(){
    // Synthetic addresses model the proven native smart-reference graph; no
    // game memory or native code is copied into this fixture.
    const std::map<uint32_t,uint32_t> valid={
        {0x1460,0x5000},{0x5000,0x2000},{0x2000,0x1001630},
        {0x262c,0x5010},{0x5010,0x3000},{0x3000,0xffc7c0},
        {0x3030,0x5020},{0x5020,0x2000},
        {0x2630,0x5030},{0x5030,0x4000},{0x4000,0xffca10},
        {0x42a8,0x5040},{0x5040,0x2000}};
    auto memory=valid;
    const auto read=[&](uint32_t address,uint32_t& word){
        auto i=memory.find(address);if(i==memory.end())return false;word=i->second;return true;
    };
    Owner result{1,2,3};
    Check(Inspect(0x1000,read,result)==Status::Matched);
    Check(result.avatar==0x2000 && result.camera==0x3000 && result.inventory==0x4000);
    for(auto& entry:valid) {
        for(uint32_t replacement:{0u,1u,0xfffffffcu,0x98765430u}) {
            memory=valid;memory[entry.first]=replacement;result={1,2,3};
            Check(Inspect(0x1000,read,result)!=Status::Matched);
            Check(!result.avatar && !result.camera && !result.inventory);
        }
        memory=valid;memory.erase(entry.first);result={1,2,3};
        Check(Inspect(0x1000,read,result)!=Status::Matched);
        Check(!result.avatar && !result.camera && !result.inventory);
    }
    memory=valid;
    for(uint32_t root:{0u,1u,0xfffffffcu,0x98765430u}){
        Check(Inspect(root,read,result)!=Status::Matched);
        Check(!result.avatar && !result.camera && !result.inventory);
    }
    // Every invocation rereads all owner links: replacement is not cached.
    for(unsigned i=0;i<1000;++i){
        memory=valid;Check(Inspect(0x1000,read,result)==Status::Matched);
        memory[0x5040]=0x9000;Check(Inspect(0x1000,read,result)==Status::InventoryOwner);
        Check(!result.camera);memory[0x5040]=0x2000;
        Check(Inspect(0x1000,read,result)==Status::Matched);
    }
    Check(memory==valid);
    std::printf("Native selected player owner read-only graph: %u checks PASS\n",checks);
}

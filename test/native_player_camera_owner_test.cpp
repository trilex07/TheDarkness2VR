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
    memory.erase(0x1460);memory.erase(0x5000);
    Check(Inspect(0x1000,read,result)==Status::RendererReference);
    Check(InspectAvatar(0x2000,read,result)==Status::Matched);
    Check(result.avatar==0x2000 && result.camera==0x3000 && result.inventory==0x4000);
    memory[0x2000]=0x100c918;
    Check(InspectAvatar(0x2000,read,result)==Status::AvatarType);
    Check(!result.avatar && !result.camera && !result.inventory);
    memory=valid;
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
    // Only the host's admitted private table may replace the native camera
    // type. It does not waive either reciprocal ownership chain.
    memory=valid;memory[0x3000]=0x77770000;
    Check(Inspect(0x1000,read,result)==Status::CameraType);
    Check(Inspect(0x1000,read,result,0x77770004)==Status::CameraType);
    Check(Inspect(0x1000,read,result,0x77770000)==Status::Matched);
    memory[0x5020]=0x9000;
    Check(Inspect(0x1000,read,result,0x77770000)==Status::CameraOwner);
    Check(!result.avatar&&!result.camera&&!result.inventory);
    // Rejection evidence reads only the known renderer reference fields and
    // one native vtable slot; it cannot invoke that getter or admit an owner.
    memory=valid;memory[0x1000]=0xfdd100;memory[0x13f4]=0x5100;memory[0x5100]=0x7000;
    memory[0x13f8]=0x5200;memory[0x5200]=0x8000;memory[0x8000]=0xfca000;
    memory[0xfca0cc]=0xabcdef;memory[0x14a8]=0x5300;
    const auto before=memory;
    auto trace=TraceRenderer(0x1000,read);
    Check(trace.rendererTable==0xfdd100 && trace.camera==0x7000);
    Check(trace.contextReference==0x5200 && trace.context==0x8000);
    Check(trace.contextTable==0xfca000 && trace.contextGetter==0xabcdef);
    Check(trace.overrideReference==0x5000 && trace.overrideAvatar==0x2000 && trace.sceneReference==0x5300);
    Check(memory==before);Check(Inspect(0x1000,read,result)==Status::Matched);
    for(uint32_t invalid:{0u,3u,0x1000u,0x11ebf34u,0xfffffffcu}) {
        memory[0x8000]=invalid;trace=TraceRenderer(0x1000,read);Check(!trace.contextGetter);
    }
    memory=before;memory[0x5000]=0;trace=TraceRenderer(0x1000,read);
    Check(trace.contextGetter==0xabcdef && !trace.overrideAvatar);
    Check(Inspect(0x1000,read,result)==Status::RendererReference);
    for(uint32_t invalid:{0u,1u,0xfffffffcu}) {
        trace=TraceRenderer(invalid,read);
        Check(!trace.camera&&!trace.context&&!trace.contextGetter&&!trace.overrideAvatar&&!trace.sceneReference);
    }
    std::printf("Native selected player owner read-only graph: %u checks PASS\n",checks);
}

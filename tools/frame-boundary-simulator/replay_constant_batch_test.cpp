#include "../../src/replay_constant_batch.h"
#include <array>
#include <vector>
#include <cassert>
#include <cstdio>
#include <type_traits>
#include <d3d9.h>
using namespace d2vr_replay_batch;
static_assert(sizeof(void*)==4,"production x86 ABI");
static_assert(std::is_trivially_destructible<Constants>::value,"ReplayStream has SEH");
struct Device {
    std::array<uint32_t,256*4> vs{}, ps{};
    unsigned calls=0, failures=0; long result=0;
    long Send(Stage stage,unsigned first,const float* values,unsigned count) {
        ++calls;
        if(result<0) {++failures;return result;}
        const unsigned limit=stage==Stage::Vertex?256:224;
        if(!values || !count || first>=limit || count>limit-first) return D3DERR_INVALIDCALL;
        auto& bank=stage==Stage::Vertex?vs:ps;
        memcpy(bank.data()+first*4,values,size_t(count)*16);
        return S_OK;
    }
};
struct Op {Stage stage;unsigned first,count;std::array<uint32_t,68*4> bits;};
static uint32_t seed=0x6ad18239;
static uint32_t Random(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static unsigned checks=0;
static void Same(const Device& a,const Device& b){assert(a.vs==b.vs && a.ps==b.ps);++checks;}
static void Scenario(bool enabled,unsigned vsLimit,unsigned psLimit) {
    Device native,grouped;
    auto send=[&](Stage stage,unsigned first,const float* data,unsigned count){return grouped.Send(stage,first,data,count);};
    Constants batch(enabled,vsLimit,psLimit);
    for(unsigned i=0;i<20000;++i) {
        const unsigned kind=Random()%9;
        if(kind<2){batch.Barrier(Stage::None,send);Same(native,grouped);continue;}
        const Stage stage=kind<6?Stage::Vertex:Stage::Pixel;
        const unsigned limit=stage==Stage::Vertex?vsLimit:psLimit;
        const unsigned first=Random()%(limit+5),count=Random()%69;
        std::array<uint32_t,68*4> bits;
        for(auto& value:bits)value=Random(); // includes NaN/Inf/signed zero bit patterns
        const auto* src=reinterpret_cast<const float*>(bits.data());
        batch.Barrier(stage,send);
        native.Send(stage,first,src,count);
        batch.Upload(stage,first,src,count,send);
        bits.fill(0xdeadbeef); // originals may disappear before Flush; batch must own bytes
    }
    batch.Flush(send);Same(native,grouped);
    if(!enabled)assert(native.calls==grouped.calls);
}
int main(){
    float payload[256*4];for(unsigned i=0;i<256*4;++i)payload[i]=float(i);
    Device d;auto send=[&](Stage s,unsigned f,const float* v,unsigned n){return d.Send(s,f,v,n);};
    Constants batch(true,256,224);
    batch.Upload(Stage::Vertex,0,payload,4,send);
    batch.Upload(Stage::Vertex,4,payload+16,8,send);
    assert(d.calls==0);batch.Barrier(Stage::None,send);
    assert(d.calls==1 && !memcmp(d.vs.data(),payload,12*16));++checks;
    // Gap, overlap, descending address, stage switch: never reorder/coalesce.
    const unsigned seq[]={20,24,24,10,200};
    const unsigned expected[]={1,1,2,3,4};
    for(unsigned i=0;i<5;++i){batch.Upload(Stage::Vertex,seq[i],payload,4,send);assert(d.calls==expected[i]);++checks;}
    batch.Barrier(Stage::Pixel,send);assert(d.calls==5);++checks;
    batch.Upload(Stage::Pixel,220,payload,4,send);
    batch.Upload(Stage::Pixel,224,payload,1,send); // invalid beyond PS caps: same original arguments
    assert(d.calls==7);++checks;
    batch.Flush(send);assert(d.calls==7);++checks;
    // Driver failure consumes exactly one submitted packet, disables further grouping.
    d.result=long(D3DERR_DEVICELOST);batch.Upload(Stage::Vertex,0,payload,4,send);
    batch.Flush(send);assert(d.calls==8 && d.failures==1);++checks;
    batch.Upload(Stage::Vertex,4,payload,4,send);batch.Upload(Stage::Vertex,8,payload,4,send);
    assert(d.calls==10 && d.failures==3);batch.Flush(send);assert(d.calls==10);++checks;
    // Unknown caps, older hardware caps, feature OFF and ON all preserve draw-time bits.
    for(bool enabled:{false,true})for(unsigned vs:{0u,96u,256u})for(unsigned ps:{0u,32u,224u})Scenario(enabled,vs,ps);
    printf("PASS replay constant packet equivalence: %u draw/barrier checks; 360000 input operations; exact-once failure, cap bounds, source lifetime\n",checks);
}

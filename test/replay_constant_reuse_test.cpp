#include "../src/replay_constant_reuse.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <type_traits>
using namespace d2vr_replay_batch;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
using Bank=std::array<uint32_t,256*4>;
int main(){
    static_assert(sizeof(void*)==4,"x86 required");
    static_assert(std::is_trivially_destructible<ConstantReuse>::value,"SEH host");
    static ConstantReuse reuse;
    unsigned calls=0;long result=0;Bank driver[2]{},expected[2]{};
    auto send=[&](Stage stage,unsigned first,const float* data,unsigned count)->long {
        ++calls;if(result)return result;
        if(stage==Stage::None || !data || !count || first>=256 || count>256-first)return -1;
        memcpy(driver[stage==Stage::Vertex?0:1].data()+first*4,data,count*16);return 0;
    };
    uint32_t bits[16]={0,0x80000000u,0x7fc12345u,0x7f800001u};
    const auto data=reinterpret_cast<const float*>(bits);
    reuse.Reset(true,256,224);
    CHECK(!reuse.Forward(Stage::Vertex,0,data,4,send));CHECK(calls==1);
    CHECK(!reuse.Forward(Stage::Vertex,0,data,4,send));CHECK(calls==1);
    bits[0]=0x80000000u; // negative zero differs; no float arithmetic/comparison
    CHECK(!reuse.Forward(Stage::Vertex,0,data,4,send));CHECK(calls==2);
    bits[2]^=1;CHECK(!reuse.Forward(Stage::Vertex,0,data,4,send));CHECK(calls==3);
    CHECK(!reuse.Forward(Stage::Pixel,0,data,4,send));CHECK(calls==4);
    reuse.Invalidate(Stage::Vertex,1,1);
    CHECK(!reuse.Forward(Stage::Vertex,0,data,1,send));CHECK(calls==4);
    CHECK(!reuse.Forward(Stage::Vertex,0,data,4,send));CHECK(calls==5);
    reuse.Clear();result=-1;
    CHECK(reuse.Forward(Stage::Vertex,0,data,4,send)==-1);
    CHECK(reuse.Forward(Stage::Vertex,0,data,4,send)==-1);CHECK(calls==7);
    result=1;CHECK(reuse.Forward(Stage::Vertex,0,data,4,send)==1);
    CHECK(reuse.Forward(Stage::Vertex,0,data,4,send)==1);CHECK(calls==9); // unknown success never learned
    result=0;CHECK(!reuse.Forward(Stage::Vertex,0,data,4,send));CHECK(calls==10);
    CHECK(reuse.Forward(Stage::None,0,data,4,send)==-1);
    CHECK(!reuse.Forward(Stage::Vertex,0,data,4,send));CHECK(calls==12);
    reuse.Reset(true,256,224);CHECK(!reuse.Forward(Stage::Vertex,0,data,4,send));CHECK(calls==13);
    reuse.Reset(false,256,224);
    CHECK(!reuse.Forward(Stage::Vertex,0,data,4,send));CHECK(!reuse.Forward(Stage::Vertex,0,data,4,send));CHECK(calls==15);
    // Exception leaves no certified rows; host aborts the failed replay, then
    // a new stream must materialize its first write instead of reusing it.
    reuse.Reset(true,256,224);
    static volatile bool throwNow=true;
    try {reuse.Forward(Stage::Vertex,0,data,4,[](Stage,unsigned,const float*,unsigned)->long{if(throwNow)throw 7;return 0;});CHECK(false);}
    catch(int){reuse.Abort();}
    reuse.Reset(true,256,224);CHECK(!reuse.Forward(Stage::Vertex,0,data,4,send));CHECK(calls==16);
    // Callback invalidation and reentrant calls cannot certify the outer write.
    reuse.Clear();bool nested=false;
    auto callback=[&](Stage stage,unsigned first,const float* p,unsigned count)->long {
        CHECK(!nested);nested=true;
        reuse.Forward(Stage::Pixel,0,p,1,send);return send(stage,first,p,count);
    };
    CHECK(!reuse.Forward(Stage::Vertex,0,data,4,callback));const auto afterCallback=calls;
    CHECK(!reuse.Forward(Stage::Vertex,0,data,4,send));CHECK(calls==afterCallback+1);
    // State-model differential: arbitrary overlaps, independent stages, zero/NaN
    // payloads, bounded caps, failed writes and opaque StateBlock/scope changes.
    reuse.Reset(true,32,16);driver[0]={};driver[1]={};
    uint32_t seed=0x84126u;const auto random=[&](){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;};
    uint32_t payload[16*4]{};
    for(unsigned i=0;i<60000;++i){
        const unsigned bank=random()%2,first=random()%(bank?16:32);
        const unsigned count=1+random()%((bank?16:32)-first>16?16:(bank?16:32)-first);
        if(i%17==0){reuse.Clear();expected[bank][first*4]=driver[bank][first*4]=random();}
        if(i%31==0){reuse.Invalidate(bank?Stage::Pixel:Stage::Vertex,first,count);expected[bank][first*4]=driver[bank][first*4]=random();}
        for(unsigned j=0;j<count*4;++j)payload[j]=i%3?expected[bank][first*4+j]:random();
        result=i%23==0?-1:0;
        CHECK(reuse.Forward(bank?Stage::Pixel:Stage::Vertex,first,reinterpret_cast<const float*>(payload),count,send)==result || result<0);
        // A failed synthetic driver request may be skipped only if its intended
        // entire state was already present. Otherwise it leaves prior state.
        if(result==0)memcpy(expected[bank].data()+first*4,payload,count*16);
        CHECK(driver[0]==expected[0] && driver[1]==expected[1]);
    }
    printf("PASS %u x86 float reuse checks: bit equality, overlaps, failures, callbacks, opaque writes, new streams, OFF, caps, NaN and signed zero; no runtime FPS claim\n",checks);
}

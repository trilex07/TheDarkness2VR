#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <vector>
#include "../src/replay_state_cache.h"
#include "../src/replay_constant_batch.h"
#define CHECK(x) do{if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static volatile unsigned sink=0;
int main(){
 using d2vr_replay_state::Cache;
 Cache cache;unsigned calls=0;auto send=[&]()->long{++calls;return 0;};
 CHECK(cache.Forward(1,42,send)==0);CHECK(cache.Forward(1,42,send)==0 && calls==1);
 CHECK(cache.Forward(1,43,send)==0 && calls==2);cache.Clear();cache.Forward(1,43,send);CHECK(calls==3);
 cache.Invalidate(1);cache.Forward(1,43,send);CHECK(calls==4);
 auto fail=[&]()->long{++calls;return -1;};cache.Invalidate(1);
 CHECK(cache.Forward(1,43,fail)<0);CHECK(cache.Forward(1,43,fail)<0 && calls==6);
 cache.Forward(-1,43,send);cache.Forward(-1,43,send);CHECK(calls==8);
 CHECK(Cache::Sampler(257,1)==-1 && Cache::Stage(8,1)==-1 && Cache::Render(256)==-1);
 // Compare full state at every simulated material draw, including scope and SB barriers.
 unsigned baseline[744]{},filtered[744]{};unsigned issued[2]{};
 for(unsigned i=0;i<4000;++i){
  if(i%37==0)cache.Clear();
  for(unsigned j=0;j<32;++j){int key=Cache::Sampler(j/8,1+j%8);unsigned type=1+j%8;unsigned value=type<=3?1+j%3:type==4?0x80808080u:type<=7?1u:0u;if(j==0)value=1+i%2;
   baseline[key]=value;++issued[0];cache.Forward(key,value,[&]()->long{filtered[key]=value;++issued[1];return 0;});
  }
  CHECK(!memcmp(baseline,filtered,sizeof baseline));
 }
 printf("PASS state snapshots at 4000 draws: %u -> %u scalar driver calls\n",issued[0],issued[1]);
 using namespace d2vr_replay_batch;
 float a[1024],b[1024];for(unsigned i=0;i<1024;++i){a[i]=float(i);b[i]=float(2048+i);}
 Constants batch(true,256,224);unsigned uploads=0;
 auto receive=[&](Stage,unsigned first,const float* data,unsigned count)->long{
  ++uploads;CHECK(first==0 && count==68 && data==a);return 0;};
 batch.Upload(Stage::Vertex,0,a,68,receive,true);batch.Flush(receive);CHECK(uploads==1);
 auto merged=[&](Stage,unsigned first,const float* data,unsigned count)->long{
  CHECK(first==0 && count==72 && !memcmp(data,a,68*16) && !memcmp(data+68*4,b,4*16));return 0;};
 batch.Upload(Stage::Vertex,0,a,68,merged,true);batch.Upload(Stage::Vertex,68,b,4,merged,true);batch.Flush(merged);
 // Default stack ownership survives producer mutation.
 batch.Upload(Stage::Vertex,0,b,68,[&](Stage,unsigned,const float* data,unsigned)->long{CHECK(data[0]==2048);return 0;});
 b[0]=999;batch.Flush([&](Stage,unsigned,const float* data,unsigned)->long{CHECK(data[0]==2048);return 0;});
 constexpr unsigned iterations=500000;
 for(bool durable:{false,true}){
  auto begin=std::chrono::steady_clock::now();
  auto consume=[](Stage,unsigned,const float* data,unsigned)->long{sink+=unsigned(data[0]);return 0;};
  for(unsigned i=0;i<iterations;++i){batch.Upload(Stage::Vertex,0,a,68,consume,durable);batch.Flush(consume);}
  printf("BENCH 68-vector singleton %s: %.3f ms / %u packets; copied payload %llu bytes\n",durable?"arena borrow":"copy",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count(),iterations,durable?0ull:iterations*68ull*16);
 }
 puts("PASS failure/unknown/invalidation receipts; borrowed arena, adjacent materialization and stack lifetime");
}

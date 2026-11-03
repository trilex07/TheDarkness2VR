#include "../src/native_submission_cache.h"
#include <cstdio>
#include <cstdlib>
#include <thread>
#define CHECK(x) do{if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace d2vr_submission;
int main(){
 // Scalar material cache: publication and epoch are as important as equality.
 {Epoch e;e.Configure(true);MaterialScalars m;unsigned n=0;bool receipt=true;
  auto run=[&](int key,unsigned value,long hr=0){return m.Forward(e,e.Begin(17),key,value,
      [&]()->long{++n;return hr;},[&](){return receipt;});};
  CHECK(run(MaterialScalars::Render(7),1)==0 && n==1);
  CHECK(run(MaterialScalars::Render(7),1)==0 && n==1);
  CHECK(run(MaterialScalars::Render(7),0)==0 && n==2);
  CHECK(run(MaterialScalars::Sampler(0,1),0)==0 && n==3);
  CHECK(run(MaterialScalars::Sampler(256,1),0)==0 && n==4);
  CHECK(run(MaterialScalars::Sampler(260,13),0)==0 && n==5);
  CHECK(run(MaterialScalars::Sampler(260,13),0)==0 && n==5);
  receipt=false;CHECK(run(9,2)==0 && n==6);receipt=true;
  CHECK(run(9,2)==0 && n==7);CHECK(run(9,2)==0 && n==7);
  CHECK(run(9,3,1)==1 && n==8);CHECK(run(9,3,-1)==-1 && n==9);
  CHECK(run(9,3)==0 && n==10);
  {Epoch::Bypass raw(e);CHECK(run(9,3)==0 && n==11);}
  CHECK(run(9,3)==0 && n==12);
  const auto before=n;auto ticket=e.Begin(17);
  CHECK(m.Forward(e,ticket,9,4,[&]()->long{++n;CHECK(run(9,5)==0);return 0;},[]{return true;})==0);
  CHECK(n==before+2);CHECK(run(9,4)==0 && n==before+3);
  CHECK(run(-1,4)==0 && n==before+4);
  CHECK(run(9,4)==0 && n==before+5);
  CHECK(MaterialScalars::Sampler(16,1)==-1 && MaterialScalars::Sampler(0,0)==-1);
  CHECK(MaterialScalars::Sampler(261,1)==-1 && MaterialScalars::Sampler(0,14)==-1);
  e.Deny(Epoch::OpaqueStateBlock);const auto denied=n;
  CHECK(run(9,4)==0 && run(9,4)==0 && n==denied+2);
 }
 requested=true;
 {InternalStateBlockCreation known;CHECK(ConsumeInternalStateBlock());CHECK(!ConsumeInternalStateBlock());}
 CHECK(!ConsumeInternalStateBlock());requested=false;
 // Bindings only certify exact successful command publication in this epoch.
 {Epoch e;e.Configure(true);FrameBindings b;unsigned calls=0;bool receipt=true;
  auto run=[&](int key,uintptr_t a,uintptr_t x=0,uintptr_t y=0,long status=0){
    return b.Forward(e,e.Begin(17),key,a,x,y,[&]()->long{++calls;return status;},[&](){return receipt;});
  };
  CHECK(run(FrameBindings::Stream(0),41,8,24)==0 && calls==1);
  CHECK(run(FrameBindings::Stream(0),41,8,24)==0 && calls==1);
  CHECK(run(FrameBindings::Stream(0),41,12,24)==0 && calls==2);
  CHECK(run(FrameBindings::Stream(0),41,12,32)==0 && calls==3);
  CHECK(run(FrameBindings::Stream(1),41,12,32)==0 && calls==4);
  CHECK(run(FrameBindings::Frequency(0),2)==0 && calls==5);
  CHECK(run(FrameBindings::Frequency(0),2)==0 && calls==5);
  CHECK(run(FrameBindings::Indices,0)==0 && calls==6);
  CHECK(run(FrameBindings::Indices,0)==0 && calls==6);
  e.Invalidate(); // reference release, UP/FVF, Reset, raw replay boundary
  CHECK(run(FrameBindings::Indices,0)==0 && calls==7);
  receipt=false;CHECK(run(FrameBindings::VertexShader,88)==0 && calls==8);
  receipt=true;CHECK(run(FrameBindings::VertexShader,88)==0 && calls==9);
  CHECK(run(FrameBindings::VertexShader,88)==0 && calls==9);
  CHECK(run(FrameBindings::PixelShader,88,0,0,-5)==-5 && calls==10);
  CHECK(run(FrameBindings::PixelShader,88,0,0,1)==1 && calls==11);
  CHECK(run(FrameBindings::PixelShader,88)==0 && calls==12);
  CHECK(run(FrameBindings::PixelShader,88)==0 && calls==12);
  auto ticket=e.Begin(17);
  CHECK(b.Forward(e,ticket,FrameBindings::Declaration,91,0,0,[&]()->long{
    ++calls;CHECK(run(FrameBindings::VertexShader,99)==0);return 0;
  },[]{return true;})==0);
  CHECK(calls==14);CHECK(run(FrameBindings::Declaration,91)==0 && calls==15);
  CHECK(run(FrameBindings::Declaration,91)==0 && calls==15);
  {Epoch::Bypass raw(e);CHECK(run(FrameBindings::Declaration,91)==0 && calls==16);}
  CHECK(run(FrameBindings::Declaration,91)==0 && calls==17);
  CHECK(run(FrameBindings::Stream(16),4)==0 && calls==18);
  CHECK(run(FrameBindings::Stream(16),4)==0 && calls==19);
  e.Deny(Epoch::OpaqueStateBlock);
  CHECK(run(FrameBindings::Declaration,91)==0 && calls==20);
  CHECK(run(FrameBindings::Declaration,91)==0 && calls==21);
  CHECK(b.skipped==6 && b.unowned==4);
 }
 Epoch e;e.Configure(true);Constants cache;
 float source[1024]{};source[0]=1;
 unsigned calls=0;
 auto send=[&]()->long{++calls;return 0;};
 auto upload=[&](Stage stage,unsigned first,unsigned count,const float* bits){
   auto p=cache.Begin(e,17,stage,first,count);
   CHECK(cache.Capture(e,p,bits,size_t(count)*16,first,count));
   return cache.Forward(e,p,send);
 };
 CHECK(upload(Stage::Vertex,0,4,source)==0 && calls==1);
 CHECK(upload(Stage::Vertex,0,4,source)==0 && calls==1);
 CHECK(upload(Stage::Pixel,0,4,source)==0 && calls==2); // independent banks
 source[0]=-0.0f;CHECK(upload(Stage::Vertex,0,4,source)==0 && calls==3);
 source[0]=0.0f;CHECK(upload(Stage::Vertex,0,4,source)==0 && calls==4); // bit-exact
 auto owned=cache.Begin(e,17,Stage::Vertex,4,4);
 CHECK(cache.Capture(e,owned,source,64,4,4));
 source[0]=9; // snapshot owns the bits; no borrow past Capture
 CHECK(cache.Forward(e,owned,send)==0 && calls==5);
 source[0]=0;CHECK(upload(Stage::Vertex,4,4,source)==0 && calls==5);
 auto failed=cache.Begin(e,17,Stage::Vertex,4,4);
 source[0]=3;CHECK(cache.Capture(e,failed,source,64,4,4));
 CHECK(cache.Forward(e,failed,[]()->long{return -7;})==-7);
 CHECK(upload(Stage::Vertex,4,4,source)==0 && calls==6);
 auto nonzero=cache.Begin(e,17,Stage::Vertex,4,4);source[0]=4;
 CHECK(cache.Capture(e,nonzero,source,64,4,4));
 CHECK(cache.Forward(e,nonzero,[]()->long{return 1;})==1);
 CHECK(upload(Stage::Vertex,4,4,source)==0 && calls==7);
 auto missing=cache.Begin(e,17,Stage::Vertex,4,4);
 CHECK(cache.Forward(e,missing,send)==0 && calls==8);
 CHECK(upload(Stage::Vertex,4,4,source)==0 && calls==9);
 // A raw render/restore boundary cannot retain final bits across frames/poses.
 {Epoch::Bypass pause(e);auto p=cache.Begin(e,17,Stage::Vertex,4,4);CHECK(!p.ticket);}
 CHECK(upload(Stage::Vertex,4,4,source)==0 && calls==10);
 // Reentry must not allow the outer successful native return to certify data.
 auto outer=cache.Begin(e,17,Stage::Vertex,4,4);source[0]=5;
 CHECK(cache.Capture(e,outer,source,64,4,4));
 CHECK(cache.Forward(e,outer,[&]()->long{
   auto inner=cache.Begin(e,17,Stage::Vertex,4,4);CHECK(!inner.ticket);
   return cache.Forward(e,inner,send);
 })==0);
 unsigned before=calls;CHECK(upload(Stage::Vertex,4,4,source)==0 && calls==before+1);
 // Uncaptured native sends need the same callback protection.
 auto unowned=cache.Begin(e,17,Stage::Vertex,4,4);
 cache.Forward(e,unowned,[&]()->long{CHECK(!cache.Begin(e,17,Stage::Vertex,4,4).ticket);return 0;});
 before=calls;CHECK(upload(Stage::Vertex,4,4,source)==0 && calls==before+1);
 auto invalid=cache.Begin(e,17,Stage::Pixel,224,1);
 CHECK(!cache.Capture(e,invalid,source,16,224,1));
 CHECK(cache.Forward(e,invalid,[]()->long{return -9;})==-9);
 // Same source proposal can be replaced only by owned final eye bytes.
 auto transformed=cache.Begin(e,17,Stage::Vertex,4,4);
 CHECK(cache.Capture(e,transformed,source,64,4,4));source[0]=7;
 CHECK(cache.Capture(e,transformed,source,64,0,4));cache.Forward(e,transformed,send);
 before=calls;CHECK(upload(Stage::Vertex,0,4,source)==0 && calls==before);
 std::thread foreign([&]{e.ObserveMutation(19);});foreign.join();
 CHECK(!cache.Begin(e,17,Stage::Vertex,0,4).ticket);CHECK(e.Reasons()&Epoch::ForeignThread);
 for(auto reason:{Epoch::OpaqueStateBlock,Epoch::InterfaceEscape,Epoch::DeviceReplaced}){
   Epoch denied;denied.Configure(true);Constants c;auto p=c.Begin(denied,17,Stage::Vertex,0,4);
   CHECK(c.Capture(denied,p,source,64,0,4));denied.Deny(reason);
   before=calls;CHECK(c.Forward(denied,p,send)==0 && calls==before+1);
   denied.Invalidate();CHECK(!c.Begin(denied,17,Stage::Vertex,0,4).ticket);
 }
 std::printf("PASS native float submission: exact owned bits, independent banks, failures, reset/bypass, reentry, transformed packets and foreign/opaque denial\n");
}

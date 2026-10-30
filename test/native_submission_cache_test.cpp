#include "../src/native_submission_cache.h"
#include <cstdio>
#include <cstdlib>
#include <thread>
#define CHECK(x) do{if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace d2vr_submission;
int main(){
 requested=true;
 {InternalStateBlockCreation known;CHECK(ConsumeInternalStateBlock());CHECK(!ConsumeInternalStateBlock());}
 CHECK(!ConsumeInternalStateBlock());requested=false;
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

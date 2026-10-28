#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "@HEADER@"
#define CHECK(x) do {if(!(x)){printf("FAIL line %d\n",__LINE__);exit(1);}} while(0)
static unsigned planeCalls=0,boundCalls=0;
static void* expectedFrustum=nullptr;
static const d2vr_culling_cover::Input* expectedInput=nullptr;
static void __fastcall BuildPlanes(void* self,void*,const float* vp) {
    CHECK(self==expectedFrustum && vp==expectedInput->viewProjection);
    CHECK(reinterpret_cast<uintptr_t>(vp)%16==0);
    auto p=*reinterpret_cast<float**>(static_cast<char*>(self)+0x30);
    for(int i=0;i<24;++i)p[i]=float(i+1);
    ++planeCalls;
}
static void __fastcall BuildBounds(void* self,void*,const float* inverse) {
    CHECK(self==expectedFrustum && inverse==expectedInput->inverseViewProjection);
    CHECK(reinterpret_cast<uintptr_t>(inverse)%16==0 && planeCalls==boundCalls+1);
    auto bounds=reinterpret_cast<float*>(static_cast<char*>(self)+0x10);
    for(int i=0;i<8;++i)bounds[i]=float(i);
    ++boundCalls;
}
@IMPLEMENTATION@
int main() {
    alignas(16) unsigned char camera[0x7e0]{};
    alignas(16) float planes[24]{};
    unsigned types[6]={1,0,0,0,0,2};
    auto f=camera+0x620;expectedFrustum=f;
    *reinterpret_cast<float**>(f+0x30)=planes;
    *reinterpret_cast<float**>(f+0x34)=planes+24;
    *reinterpret_cast<float**>(f+0x38)=planes+24;
    *reinterpret_cast<unsigned**>(f+0x150)=types;
    *reinterpret_cast<unsigned**>(f+0x154)=types+6;
    *reinterpret_cast<unsigned**>(f+0x158)=types+6;
    unsigned char original[sizeof(camera)];std::memcpy(original,camera,sizeof(camera));
    d2vr_culling_cover::Input input;expectedInput=&input;
    for(int i=0;i<1000;++i)CHECK(RebuildOwnedCullingFrustum(camera,input));
    CHECK(planeCalls==1000 && boundCalls==1000);
    CHECK(!std::memcmp(original,camera,0x620)); // FOV, P, V, VP untouched
    CHECK(!std::memcmp(original+0x7c0,camera+0x7c0,0x20));
    CHECK(*reinterpret_cast<float**>(f+0x30)==planes);
    types[5]=0;CHECK(!RebuildOwnedCullingFrustum(camera,input));types[5]=2;
    *reinterpret_cast<float**>(f+0x38)=planes+23;CHECK(!RebuildOwnedCullingFrustum(camera,input));
    *reinterpret_cast<float**>(f+0x38)=planes+24;
    planes[0]=NAN;CHECK(!RebuildOwnedCullingFrustum(camera,input));planes[0]=0;
    *reinterpret_cast<unsigned**>(f+0x154)=types+5;CHECK(!RebuildOwnedCullingFrustum(camera,input));
    CHECK(planeCalls==1000 && boundCalls==1000);
    puts("PASS actual-source culling rebuild: 1000 ABI/order/alignment calls, render camera preserved, malformed vectors rejected; native plane algorithms not emulated");
}

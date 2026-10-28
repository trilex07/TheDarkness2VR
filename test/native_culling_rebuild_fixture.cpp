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
    input.stereoUnion=input.haveBounds=input.haveCorners=true;
    for(unsigned axis=0;axis<3;++axis){input.lower[axis]=-10.f;input.upper[axis]=30.f;}
    for(unsigned i=0;i<1000;++i) {
        CHECK(RebuildOwnedCullingFrustum(camera,input));
        const auto bounds=reinterpret_cast<const float*>(f+0x10);
        for(unsigned axis=0;axis<3;++axis)CHECK(bounds[axis]==-10.f && bounds[4+axis]==30.f);
        CHECK(bounds[3]==3.f && bounds[7]==7.f);
        for(unsigned plane=0;plane<6;++plane) {
            for(unsigned axis=0;axis<3;++axis)CHECK(planes[plane*4+axis]==float(plane*4+axis+1));
            CHECK(planes[plane*4+3]<=0.f);
        }
    }
    CHECK(!std::memcmp(original,camera,0x620));
    CHECK(!std::memcmp(original+0x7c0,camera+0x7c0,0x20));
    input.lower[0]=NAN;CHECK(!RebuildOwnedCullingFrustum(camera,input));input.lower[0]=-10.f;
    input.haveBounds=false;CHECK(!RebuildOwnedCullingFrustum(camera,input));input.haveBounds=true;
    input.haveCorners=false;CHECK(!RebuildOwnedCullingFrustum(camera,input));input.haveCorners=true;
    input.corners[15][2]=NAN;CHECK(!RebuildOwnedCullingFrustum(camera,input));input.corners[15][2]=0;
    types[5]=0;CHECK(!RebuildOwnedCullingFrustum(camera,input));types[5]=2;
    *reinterpret_cast<float**>(f+0x38)=planes+23;CHECK(!RebuildOwnedCullingFrustum(camera,input));
    *reinterpret_cast<float**>(f+0x38)=planes+24;
    planes[0]=NAN;CHECK(!RebuildOwnedCullingFrustum(camera,input));planes[0]=0;
    *reinterpret_cast<unsigned**>(f+0x154)=types+5;CHECK(!RebuildOwnedCullingFrustum(camera,input));
    CHECK(planeCalls==2000 && boundCalls==2000);
    puts("PASS actual-source culling rebuild: 1000 native+1000 conservative frustum ABI/order/alignment calls, normals/W/render fields preserved, invalid bounds/corners rejected before calls, malformed vectors rejected; native algorithms not emulated");
}

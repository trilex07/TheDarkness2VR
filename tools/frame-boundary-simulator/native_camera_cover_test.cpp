#include "../../src/native_camera_cover.h"
#include "../../src/stereo_geometry.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

static unsigned checks=0;
static void check(bool v) {if(++checks && !v) {std::printf("FAIL %u\n",checks);std::exit(1);}}
static float nearValue=.125f;
static unsigned helperCalls=0,getterCalls=0,seenCamera=0,seenOwner=0;
static void __fastcall Disrupt(unsigned camera,unsigned owner) {
    ++helperCalls;seenCamera=camera;seenOwner=owner;
    __asm fninit
    __asm xorps xmm0,xmm0
    __asm xor ecx,ecx
    __asm xor edx,edx
}
static void __declspec(naked) Getter() {
    // Preserve flags as the actual FLD/RET leaf getter does.
    __asm pushfd
    __asm inc getterCalls
    __asm popfd
    __asm fld nearValue
    __asm ret
}
static void* originalGetter=reinterpret_cast<void*>(&Getter);
D2VR_SELECTED_CAMERA_SHIM(TestShim,Disrupt,originalGetter)
static float latent=2.5f,returned=0,remaining=0;
static unsigned flagsAfter=0,ecxAfter=0,edxAfter=0;
static unsigned xmmIn[4]={0x12345678,0x9abcdef0,0x24681357,0x76543210},xmmOut[4]{};
static void __declspec(naked) RunAbiTest() {
    __asm {
        push ebx
        movups xmm0,xmmIn
        fld latent
        mov ecx,0x12345678
        mov edx,0x87654321
        mov ebx,0x34567890
        stc
        call TestShim
        pushfd
        pop flagsAfter
        mov ecxAfter,ecx
        mov edxAfter,edx
        pop ebx
        movups xmmOut,xmm0
        fstp returned
        fstp remaining
        ret
    }
}

int main() {
    using namespace d2vr_camera_cover;
    const float angles[2][4]={{-1.04f,.89f,.86f,-.91f},{-.89f,1.04f,.86f,-.91f}};
    Plan plan;
    check(Cover(60,3076.f/3140,angles,1.03f,1.02f,plan));
    check(plan.fov>120 && plan.fov<140);
    // Rays the old 60 degree frustum rejected now lie inside the SAME native
    // frustum whose matrix will be uploaded. Contracting the wider native image
    // back to each lens reproduces the existing desired clip coordinates.
    for(int eye=0;eye<2;++eye) {
        const float tx=std::tan(1.03f*std::max(std::fabs(angles[eye][0]),std::fabs(angles[eye][1])))*1.02f;
        const float ty=std::tan(1.03f*std::max(std::fabs(angles[eye][2]),std::fabs(angles[eye][3])))*1.02f;
        float sx=0,sy=0;check(EyeScale(plan.px,plan.py,angles[eye],1.03f,1.02f,sx,sy));
        check(sy>1); // must support contraction, not legacy widen-only clamping
        for(float z:{.05f,1.f,20.f,1000.f}) for(int xi=-10;xi<=10;++xi) for(int yi=-10;yi<=10;++yi) {
            const float x=tx*z*xi/10,y=ty*z*yi/10;
            check(std::fabs(plan.px*x)<=z*1.00001f && std::fabs(plan.py*y)<=z*1.00001f);
            check(std::fabs(plan.px*x*sx/z-xi/10.f)<1e-5f && std::fabs(plan.py*y*sy/z-yi/10.f)<1e-5f);
        }
        check(std::tan(1.04f)*std::sqrt(3.f)/(3076.f/3140)>1); // reproduces narrow rejection
    }
    for(float bad:{0.f,-1.f,161.f,std::numeric_limits<float>::quiet_NaN()}) check(!Cover(bad,1,angles,1,1,plan));
    check(!Cover(60,0,angles,1,1,plan));check(!Cover(60,1,angles,0,1,plan));
    check(Cover(60,1,angles,1,1,plan));
    const float fov=plan.fov,py=plan.py,q=1000.f/(1000.f-.1f);
    float p[16]={py,0,0,0,0,py,0,0,0,0,q,1,0,0,-q*.1f,0};
    check(Projection(p,fov,1,.1f,1000));
    for(int i=0;i<16;++i) {p[i]+=.1f;check(!Projection(p,fov,1,.1f,1000));p[i]-=.1f;}
    check(!Projection(p,fov,1,-1,1000));check(!Projection(p,fov,1,1000,1000));
    struct Memory {
        unsigned char data[5];bool write=true;int flushFail=0,restoreFail=0,nf=0,nr=0;
        Memory(){std::memcpy(data,original,5);} unsigned char* bytes(){return data;}
        bool writable(){return write;}bool flush(){return ++nf!=flushFail;}bool restore(){return ++nr!=restoreFail;}
    };
    {Memory m;Patch patch;check(patch.Install(m,0x12345678));check(patch.installed && !patch.rollbackUncertain);
        uint32_t rel;std::memcpy(&rel,m.data+1,4);check(m.data[0]==0xe8 && site+5+rel==0x12345678);check(!patch.Install(m,1));}
    for(int fault=0;fault<4;++fault) {
        Memory m;Patch patch;if(fault==0)m.write=false;if(fault==1)m.flushFail=1;
        if(fault==2)m.restoreFail=1;if(fault==3){m.flushFail=1;m.restoreFail=1;}
        check(!patch.Install(m,0x12345678));check(!patch.installed);check(!std::memcmp(m.data,original,5));
        check(patch.rollbackUncertain==(fault==3));
    }
    for(int i=0;i<5;++i){Memory m;Patch patch;m.data[i]^=1;check(!patch.Install(m,1));check(m.nf==0);}
    // Exercise the exact production shim with an intentionally destructive
    // helper: x87 stack, SSE, ECX/EDX, carry, stack cleanup, one original call.
    RunAbiTest();
    check(helperCalls==1 && getterCalls==1);check(seenCamera==0x12345678 && seenOwner==0x34567890);
    check(ecxAfter==0x12345678 && edxAfter==0x87654321);check(flagsAfter&1);
    check(returned==nearValue && remaining==latent);check(!std::memcmp(xmmIn,xmmOut,sizeof(xmmIn)));
    std::printf("native camera cover: %u checks PASS (including real x86 shim)\n",checks);
    return 0;
}

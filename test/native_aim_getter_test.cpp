#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include <xmmintrin.h>
#include <cstdio>
#include <cstdlib>
#include "../src/native_aim_getter.h"
#include "../src/native_view_instance.h"
static unsigned checks=0,calls=0;
static bool callbackAccepted=true,nest=false;
static void Check(bool v){++checks;if(!v){std::printf("FAIL %u\n",checks);std::exit(1);}}
struct __declspec(align(16)) State {DWORD regs[9];DWORD padding[3];BYTE floating[512];};
struct __declspec(align(16)) Camera {void** table;BYTE padding[124];float matrix[16];};
static_assert(offsetof(Camera,matrix)==0x80,"native leaf offset");
using Run=float*(__cdecl*)(void*,void*,State*,State*,uintptr_t*);
using Other=float*(__cdecl*)(void*,void*);
static Run run=nullptr;
static Camera* expected=nullptr;
static bool Resolve(void* self,const float* base,float* output){
    ++calls;Check(self==expected && base==expected->matrix);
    Check(!(reinterpret_cast<uintptr_t>(output)&15));
    if(nest){State a{},b{};uintptr_t caller=0;
        Check(run(self,d2vr_aim_getter::Bridge::Entry(),&a,&b,&caller)==base);}
    for(unsigned i=0;i<16;++i)output[i]=base[i]+100;
    SetLastError(99);_mm_setcsr(_mm_getcsr()^0x6000);
    __asm fld1
    __asm pxor xmm0,xmm0
    __asm pxor xmm1,xmm1
    __asm pxor xmm2,xmm2
    __asm pxor xmm3,xmm3
    __asm pxor xmm4,xmm4
    __asm pxor xmm5,xmm5
    __asm pxor xmm6,xmm6
    __asm pxor xmm7,xmm7
    return callbackAccepted;
}
static uint64_t TextHash(HMODULE module){
    auto image=reinterpret_cast<const BYTE*>(module);
    auto pe=reinterpret_cast<const IMAGE_NT_HEADERS32*>(image+reinterpret_cast<const IMAGE_DOS_HEADER*>(image)->e_lfanew);
    auto section=IMAGE_FIRST_SECTION(pe);uint64_t hash=14695981039346656037ull;
    for(unsigned n=0;n<pe->FileHeader.NumberOfSections;++n)if(section[n].Characteristics&IMAGE_SCN_MEM_EXECUTE)
        for(unsigned i=0;i<section[n].Misc.VirtualSize;++i)hash=(hash^image[section[n].VirtualAddress+i])*1099511628211ull;
    return hash;
}
int main(int argc,char** argv){
    Check(argc==2);auto module=LoadLibraryA(argv[1]);Check(module!=nullptr);
    const auto hash=TextHash(module);
    auto original=reinterpret_cast<d2vr_aim_getter::Getter>(GetProcAddress(module,"@Original@8"));
    run=reinterpret_cast<Run>(GetProcAddress(module,"InvokeKnown"));
    auto other=reinterpret_cast<Other>(GetProcAddress(module,"InvokeOther"));Check(original && run && other);
    void* table[72]{};table[40]=reinterpret_cast<void*>(original); // leading header then slot9C
    void* savedTable[72];memcpy(savedTable,table,sizeof(table));
    Camera camera{};camera.table=table+1;expected=&camera;
    for(unsigned i=0;i<16;++i)camera.matrix[i]=float(i)+.125f;
    State before{},after{};uintptr_t caller=0;
    Check(run(&camera,reinterpret_cast<void*>(original),&before,&after,&caller)==camera.matrix);
    auto image=reinterpret_cast<BYTE*>(module);
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS32*>(image+reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew);
    d2vr_native_image::Guard guard{true,reinterpret_cast<uintptr_t>(image),pe->OptionalHeader.SizeOfImage};
    Check(!d2vr_aim_getter::Bridge::Prepare(nullptr,caller,Resolve,guard));
    Check(!d2vr_aim_getter::Bridge::Prepare(original,0,Resolve,guard));
    Check(!d2vr_aim_getter::Bridge::Prepare(original,caller,nullptr,guard));
    auto disabled=guard;disabled.enabled=false;
    Check(!d2vr_aim_getter::Bridge::Prepare(original,caller,Resolve,disabled));
    Check(d2vr_aim_getter::Bridge::Prepare(original,caller,Resolve,guard));
    d2vr_native_view::InstanceTable copy;
    Check(copy.Prepare(table+1,71,0x9c/4,d2vr_aim_getter::Bridge::Entry(),guard));
    Check(copy.Publish(&camera,guard) && copy.Owns(&camera));
    Check(!memcmp(table,savedTable,sizeof(table)));
    Check(camera.table[0x9c/4]==d2vr_aim_getter::Bridge::Entry());
    for(unsigned mode=0;mode<3;++mode){callbackAccepted=mode!=0;nest=mode==2;
        for(unsigned i=0;i<1000;++i){
            before={};after={};SetLastError(0x13572468);
            float* result=run(&camera,camera.table[0x9c/4],&before,&after,&caller);
            Check(GetLastError()==0x13572468);
            for(unsigned r=0;r<9;++r)if(r!=7)Check(before.regs[r]==after.regs[r]);
            Check(!memcmp(before.floating,after.floating,288));
            Check(result==(callbackAccepted?d2vr_aim_getter::detail::result:camera.matrix));
            for(unsigned n=0;n<16;++n){Check(camera.matrix[n]==float(n)+.125f);
                Check(result[n]==float(n)+.125f+(callbackAccepted?100:0));}
        }
    }
    Check(calls==3000);
    const auto seen=calls;
    for(unsigned i=0;i<1000;++i){SetLastError(0x1234);
        Check(other(&camera,camera.table[0x9c/4])==camera.matrix);Check(GetLastError()==0x1234);}
    Check(calls==seen && TextHash(module)==hash && !memcmp(table,savedTable,sizeof(table)));
    std::printf("Private aim getter exact caller/ABI/x87/SSE/reentry/LastError/image preservation: %u checks PASS\n",checks);
}

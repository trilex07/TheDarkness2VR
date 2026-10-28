#include <windows.h>
#include <intrin.h>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <thread>
#include <atomic>
#include "../src/native_constant_source.h"
#include "../src/native_constant_gate.h"
#include "../src/small_record_copy.h"
using IDirect3DDevice9=void;
using namespace d2vr_native_constants;
static uintptr_t apiReturn,callbackReturn,parentReturn;
static const float* overrideSource;
static uintptr_t fixtureOwner=0;
static bool fixtureMaterial=false;
static bool fixtureNested=false;
static unsigned fixtureField=7;
static unsigned firstRegister=0,vectorCount=4;
static float recorded[80],forwarded[80];
static bool copied=false;
static unsigned checks=0;
static void Check(bool condition,const char* name) {
    ++checks;if(!condition){std::printf("FAIL %s\n",name);std::exit(1);}
}
static void ClassifierFailures() {
    auto* page=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Check(page!=nullptr,"fixture allocation");
    const uintptr_t low=reinterpret_cast<uintptr_t>(page),high=low+4096,frame=low+0x900;
    auto* slot=reinterpret_cast<uintptr_t*>(frame-0x794-48);
    auto* source=reinterpret_cast<float*>(frame-0x794+64);
    Sites sites{{11,12},{21,22},{31,32},{{41,0,0},{42,0,0}}};
    slot[0]=12;slot[1]=0;slot[2]=0;slot[3]=reinterpret_cast<uintptr_t>(source);slot[4]=4;
    slot[5]=22;slot[6]=0;slot[7]=slot[3];slot[8]=4;
    reinterpret_cast<uintptr_t*>(frame)[1]=42;
    auto inspect=[&] {return Proof::Inspect(slot,frame,source,64,Stage::Pixel,low,high,sites);};
    Check(inspect().Matches(source,64),"classifier exact frame");
    for(unsigned field: {0u,2u,3u,4u,5u,6u,7u,8u}) {
        const auto saved=slot[field];slot[field]=~uintptr_t(0);
        Check(!inspect().Matches(source,64),"each dispatch/argument mismatch rejects");slot[field]=saved;
    }
    reinterpret_cast<uintptr_t*>(frame)[1]=43;
    Check(!inspect().Matches(source,64),"unknown native parent rejects");
    reinterpret_cast<uintptr_t*>(frame)[1]=42;
    Check(!Proof::Inspect(slot,frame+16,source,64,Stage::Pixel,low,high,sites).Matches(source,64),"shifted frame rejects");
    Check(!Proof::Inspect(slot,frame,source,63,Stage::Pixel,low,high,sites).Matches(source,63),"non-vector extent rejects");
    auto proof=inspect();Check(!proof.Matches(source+1,64)&&!proof.Matches(source,48),"proof binds exact payload");
    DWORD old=0;Check(VirtualProtect(page,4096,PAGE_NOACCESS,&old)!=FALSE,"fault fixture protection");
    Check(!Copy(proof,recorded,source,64)&&!admitted.load()&&faulted.load()&&counts.faults==1,"violated contract disables admission and returns fallback");
    admitted=true;Check(!proof.Matches(source,64),"fault latch survives attempted readmission");
    VirtualProtect(page,4096,old,&old);VirtualFree(page,0,MEM_RELEASE);faulted=false;admitted=true;
    auto* guard=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    Check(guard!=nullptr,"guard fixture allocation");
    VirtualProtect(guard,4096,PAGE_READWRITE|PAGE_GUARD,&old);
    Proof none;Check(!Copy(none,recorded,guard,64),"unproven guard never direct copied");
    d2vr_record_copy::Copy(recorded,guard,64,true,&ReadProcessMemory);
    MEMORY_BASIC_INFORMATION mbi{};VirtualQuery(guard,&mbi,sizeof(mbi));
    Check((mbi.Protect&PAGE_GUARD)!=0,"fallback preserves foreign source guard");
    VirtualFree(guard,0,MEM_RELEASE);
}
static void ActiveStackFailures() {
    auto* page=static_cast<unsigned char*>(VirtualAlloc(nullptr,8192,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Check(page!=nullptr,"active stack classifier allocation");
    const uintptr_t low=reinterpret_cast<uintptr_t>(page),high=low+8192;
    auto* slot=reinterpret_cast<uintptr_t*>(page+64);
    auto* source=reinterpret_cast<float*>(page+4096);
    Sites sites{{11,12},{},{},{}};
    slot[0]=12;slot[2]=0;slot[3]=reinterpret_cast<uintptr_t>(source);slot[4]=4;
    slot[6]=0;slot[7]=slot[3];slot[8]=4;
    auto inspect=[&](const void* p,size_t n,Stage stage=Stage::Pixel) {
        return Proof::InspectActiveStack(slot,p,n,stage,low,high,sites);
    };
    Check(inspect(source,64).Matches(source,64),"fresh current-stack range accepts without EBP owner");
    Check(inspect(source,64).ActiveStack()&&!inspect(source,64).Material(),"stack proof cannot grant material ownership");
    for(unsigned field:{0u,2u,3u,4u,6u,7u,8u}) {
        const auto saved=slot[field];slot[field]=~uintptr_t(0);
        Check(!inspect(source,64).Matches(source,64),"active stack exact wrapper argument receipt rejects mismatches");slot[field]=saved;
    }
    Check(!inspect(source,64,Stage::Vertex).Matches(source,64),"active stack exact stage forwarder required");
    slot[0]=11;slot[4]=slot[8]=256;
    Check(inspect(source,4096,Stage::Vertex).Matches(source,4096),"active stack complete VS bank extent admits");
    slot[0]=12;slot[4]=slot[8]=224;
    Check(inspect(source,3584).Matches(source,3584),"active stack complete PS bank extent admits");
    slot[4]=slot[8]=225;
    Check(!inspect(source,3600).Matches(source,3600),"active stack PS hardware limit rejects");
    slot[4]=slot[8]=4;
    Check(!inspect(source,63).Matches(source,63),"active stack vector extent required");
    for(uintptr_t p:{low-16,reinterpret_cast<uintptr_t>(slot)+44,high-48,high,UINTPTR_MAX-15}) {
        auto* invalid=reinterpret_cast<void*>(p);
        slot[3]=slot[7]=p;
        Check(!inspect(invalid,64).Matches(invalid,64),"active stack lower/top/overflow range rejects");
    }
    slot[3]=slot[7]=high-64;
    Check(inspect(reinterpret_cast<void*>(high-64),64).Matches(reinterpret_cast<void*>(high-64),64),"active stack exact upper extent admits");
    Check(!Proof::InspectActiveStack(slot,reinterpret_cast<void*>(high-64),64,Stage::Pixel,low,high-16,sites).Matches(reinterpret_cast<void*>(high-64),64),"fresh reduced stack bounds cannot reuse old proof");
    slot[3]=slot[7]=reinterpret_cast<uintptr_t>(source);slot[2]=slot[6]=223;slot[4]=slot[8]=2;
    Check(!inspect(source,32).Matches(source,32),"active stack caps overflow rejects");
    slot[2]=slot[6]=0;slot[4]=slot[8]=4;
    auto proof=inspect(source,64);
    Check(!proof.Matches(source+4,64)&&!proof.Matches(source,48),"active stack proof is one exact payload");
    enabled=false;Check(!proof.Matches(source,64)&&!inspect(source,64).Matches(source,64),"active stack package off rejects");enabled=true;
    admitted=false;Check(!inspect(source,64).Matches(source,64),"active stack image admission required");admitted=true;
    faulted=true;Check(!inspect(source,64).Matches(source,64),"active stack sticky fault refuses");faulted=false;
    VirtualFree(page,0,MEM_RELEASE);
}
static Proof InspectFixture(const uintptr_t* slot,uintptr_t frame,const void* p,size_t n,Stage stage,uintptr_t owner) {
    Sites sites{};
    for(unsigned b=0;b<2;++b) {
        sites.apiReturn[b]=apiReturn;sites.builderFirst[b]=callbackReturn;
        sites.builderEnd[b]=callbackReturn+1;sites.parentReturn[b][0]=parentReturn;
        if(fixtureMaterial)sites.fieldReturn[b][fixtureField]=callbackReturn;
        if(fixtureNested)sites.nestedReturn[b][fixtureField]=callbackReturn;
    }
    return InspectCurrent(slot,frame,p,n,stage,owner,sites);
}
static void NestedFields() {
    auto* page=static_cast<unsigned char*>(VirtualAlloc(nullptr,8192,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Check(page!=nullptr,"nested classifier frame allocation");
    const uintptr_t low=reinterpret_cast<uintptr_t>(page),high=low+8192,frame=low+0x900;
    auto* slot=reinterpret_cast<uintptr_t*>(frame-0x794-48);
    alignas(16) unsigned char owner[0x20]{},context[0x600]{},model[0x160]{},reference[4]{},payload[0x200]{};
    for(unsigned i=0;i<sizeof(payload);++i)payload[i]=static_cast<unsigned char>(i*31+7);
    *reinterpret_cast<uintptr_t*>(owner+0x1c)=reinterpret_cast<uintptr_t>(context);
    *reinterpret_cast<uintptr_t*>(context+0x5d4)=reinterpret_cast<uintptr_t>(model);
    *reinterpret_cast<uintptr_t*>(context+0x5d8)=reinterpret_cast<uintptr_t>(payload);
    *reinterpret_cast<uintptr_t*>(model+0x154)=reinterpret_cast<uintptr_t>(reference);
    *reinterpret_cast<uintptr_t*>(reference)=reinterpret_cast<uintptr_t>(payload);
    Sites sites{{11,12},{21,22},{31,32},{{41,0,0},{42,0,0}}};
    materialEnabled=true;
    for(unsigned bank=0;bank<2;++bank)for(unsigned field=0;field<4;++field) {
        const auto stage=bank?Stage::Pixel:Stage::Vertex;
        sites.nestedReturn[bank][field]=23+field;
        auto* source=payload+nestedOffset[field];const size_t bytes=nestedVectors[field]*16;
        slot[0]=sites.apiReturn[bank];slot[2]=slot[6]=0;
        slot[3]=slot[7]=reinterpret_cast<uintptr_t>(source);slot[4]=slot[8]=bytes/16;
        slot[5]=23+field;reinterpret_cast<uintptr_t*>(frame)[1]=sites.parentReturn[bank][0];
        auto inspect=[&]{return Proof::Inspect(slot,frame,source,bytes,stage,low,high,sites,reinterpret_cast<uintptr_t>(owner));};
        auto proof=inspect();Check(proof.Matches(source,bytes)&&proof.Nested()&&proof.Material(),"both stages exact nested field receipt");
        float result[40]{};Check(Copy(proof,result,source,bytes)&&!memcmp(result,source,bytes),"nested exact copy bits");
        for(auto* link: {owner+0x1c,context+(field==3?0x5d8:0x5d4),model+0x154,reference}) {
            if(field==3 && (link==model+0x154 || link==reference))continue;
            auto& value=*reinterpret_cast<uintptr_t*>(link);const auto saved=value;
            value=0;Check(!inspect().Matches(source,bytes),"null nested link rejects");
            value=saved+4;Check(!inspect().Matches(source,bytes),"replacement nested link cannot reuse ownership");
            value=UINTPTR_MAX-1;Check(!inspect().Matches(source,bytes),"overflow or unaligned nested link rejects");
            value=saved;
        }
        slot[3]=slot[7]=reinterpret_cast<uintptr_t>(source)+4;
        Check(!inspect().Matches(source,bytes),"nested shifted submitted pointer rejects");slot[3]=slot[7]=reinterpret_cast<uintptr_t>(source);
        slot[4]=slot[8]=bytes/16+1;Check(!inspect().Matches(source,bytes),"nested argument extent mismatch rejects");slot[4]=slot[8]=bytes/16;
        ++slot[5];Check(!inspect().Matches(source,bytes),"nested foreign call site rejects");--slot[5];
        materialEnabled=false;Check(!proof.Matches(source,bytes)&&!inspect().Matches(source,bytes),"nested material option off rejects");materialEnabled=true;
    }
    VirtualFree(page,0,MEM_RELEASE);
}
static bool g_recActive=true;
static int g_replayEye=-1,g_activePsInverse=-1,g_activePsViewExtents=-1,g_submitMode=0;
static int OP_SetPSConstF=1;
static void ObserveInverseProjection(const float*,UINT,UINT,int){}
static int MakeEyeParams(int){return 0;}
static bool CompensatePsConstants(float*,const float*,UINT,UINT,int,int,int){return false;}
static void RecEmitData(int,const void* data,size_t bytes,UINT,UINT,int,int,const Proof& proof) {
    copied=Copy(proof,recorded,data,bytes);
    if(!copied)d2vr_record_copy::Copy(recorded,data,bytes,true,&ReadProcessMemory);
}
static HRESULT STDMETHODCALLTYPE Forward(void*,UINT,const float* data,UINT count) {
    // The original API is a separate consumer, after recorder capture.
    if(data && count<=17)std::memcpy(forwarded,data,count*16);
    return S_OK;
}
static struct {decltype(&Forward) SetPixelShaderConstantF=Forward;} g_orig;
#define D3D_OK S_OK
// Generated directly from the production PS hook. Only the policy-site lookup
// is redirected to fixture addresses; EBP/return-slot capture is unchanged.
#include "actual_native_constant_hook.inl"

__declspec(naked) static void Wrapper() {
    __asm {
        mov edx,[esp+12]
        push edx
        mov edx,[esp+12]
        push edx
        mov edx,[esp+12]
        push edx
        push ecx
        lea eax,afterApi
        mov apiReturn,eax
        call Hook_SetPSConstF
    afterApi:
        ret 12
    }
}
__declspec(naked) static void Builder() {
    __asm {
        push ebp
        mov ebp,esp
        and esp,0xfffffff0
        sub esp,0x794
        push ebx
        push esi
        push edi
        lea edi,[esp+12+0x100]
        mov ecx,16
        mov eax,0x3f800000
    fill:
        stosd
        add eax,1
        loop fill
        lea eax,[esp+12+0x100]
        cmp overrideSource,0
        je useLocal
        mov eax,overrideSource
    useLocal:
        mov ebx,fixtureOwner
        push vectorCount
        push eax
        push firstRegister
        xor ecx,ecx
        lea eax,afterCallback
        mov callbackReturn,eax
        call Wrapper
    afterCallback:
        pop edi
        pop esi
        pop ebx
        mov esp,ebp
        pop ebp
        ret
    }
}
__declspec(naked) static void Parent() {
    __asm {
        lea eax,afterBuilder
        mov parentReturn,eax
        call Builder
    afterBuilder:
        ret
    }
}
__declspec(naked) static void FramePointerOmittedProducer() {
    __asm {
        push ebp
        push ebx
        xor ebp,ebp
        xor ebx,ebx
        push vectorCount
        push overrideSource
        push firstRegister
        xor ecx,ecx
        lea eax,afterCallback
        mov callbackReturn,eax
        call Wrapper
    afterCallback:
        pop ebx
        pop ebp
        ret
    }
}
int main() {
    enabled=true;admitted=true;
    for(unsigned i=0;i<1000;++i) {
        copied=false;Parent();Check(copied,"actual hook accepts active native-shaped local frame");
        Check(std::memcmp(recorded,forwarded,sizeof(recorded))==0,"recorder and forwarded payload bits match");
    }
    float outerLocal[16];for(unsigned i=0;i<16;++i)outerLocal[i]=float(i+1)/16;
    overrideSource=outerLocal;
    const auto beforeStack=counts.activeStackCopies;
    for(unsigned i=0;i<1000;++i) {
        Parent();Check(copied,"native caller local above incoming ESP admits active stack");
        Check(!memcmp(recorded,outerLocal,64)&&!memcmp(recorded,forwarded,64),"outer native caller local bits match recorder and API");
    }
    Check(counts.activeStackCopies==beforeStack+1000,"separate active stack copy receipt");
    for(unsigned i=0;i<1000;++i) {
        FramePointerOmittedProducer();
        Check(copied&&!memcmp(recorded,outerLocal,64)&&!memcmp(recorded,forwarded,64),"actual frame-pointer-omitted caller preserves current-stack payload bits");
    }
    auto* heapLike=static_cast<float*>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,64));Check(heapLike!=nullptr,"heap source allocation");
    overrideSource=heapLike;Parent();Check(!copied,"unowned heap source rejects active stack copy");
    HeapFree(GetProcessHeap(),0,heapLike);
    std::atomic<const float*> peerSource{nullptr};std::atomic<bool> peerDone{false};
    std::thread peer([&]{float local[16]{};peerSource.store(local);while(!peerDone.load())SwitchToThread();});
    while(!peerSource.load())SwitchToThread();
    overrideSource=peerSource.load();Parent();Check(!copied,"another live thread's stack rejects direct copy");
    peerDone=true;peer.join();overrideSource=nullptr;
    alignas(16) unsigned char ownerObject[0x20]{};
    auto* materialBlock=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Check(materialBlock!=nullptr,"material heap allocation independent of current stack");
    for(unsigned i=0;i<0x600;++i)materialBlock[i]=static_cast<unsigned char>(i*73+19);
    *reinterpret_cast<uintptr_t*>(ownerObject+0x1c)=reinterpret_cast<uintptr_t>(materialBlock);
    fixtureOwner=reinterpret_cast<uintptr_t>(ownerObject);
    fixtureMaterial=true;materialEnabled=true;
    overrideSource=reinterpret_cast<float*>(materialBlock+0x260);
    for(unsigned i=0;i<1000;++i) {
        copied=false;Parent();Check(copied,"actual gate preserves EBX owner and admits exact material field");
        Check(!std::memcmp(recorded,forwarded,sizeof(recorded)),"owned field bits match forwarding");
    }
    for(unsigned field=0;field<fieldCount;++field) {
        fixtureField=field;vectorCount=static_cast<unsigned>(fieldVectors[field]);
        overrideSource=reinterpret_cast<float*>(materialBlock+fieldOffset[field]);
        std::memset(recorded,0,sizeof(recorded));std::memset(forwarded,0,sizeof(forwarded));
        Parent();Check(copied,"every admitted fixed-field extent works through actual gate");
        Check(!std::memcmp(recorded,overrideSource,vectorCount*16)&&!std::memcmp(recorded,forwarded,sizeof(recorded)),"fixed-field nonzero payload bit equality");
        overrideSource+=4;Parent();Check(!copied,"each field rejects shifted source");overrideSource-=4;
        ++vectorCount;Parent();Check(!copied,"each field rejects oversized extent");--vectorCount;
    }
    fixtureField=7;vectorCount=4;overrideSource=reinterpret_cast<float*>(materialBlock+0x260);
    LARGE_INTEGER frequency{},begin{},end{};QueryPerformanceFrequency(&frequency);
    double elapsed[2]{};
    constexpr unsigned benchCalls=20000;
    for(unsigned mode=0;mode<2;++mode) {
        materialEnabled=mode!=0;
        for(unsigned i=0;i<200;++i)Parent();
        QueryPerformanceCounter(&begin);
        for(unsigned i=0;i<benchCalls;++i)Parent();
        QueryPerformanceCounter(&end);
        elapsed[mode]=(end.QuadPart-begin.QuadPart)*1e9/frequency.QuadPart/benchCalls;
    }
    std::printf("OFFLINE gate + 64B owned capture + stub forward: RPM=%.1f ns/call owner=%.1f ns/call; not game/FPS evidence\n",elapsed[0],elapsed[1]);
    overrideSource=outerLocal;
    for(unsigned mode=0;mode<2;++mode) {
        enabled=mode!=0;
        for(unsigned i=0;i<200;++i)Parent();
        QueryPerformanceCounter(&begin);
        for(unsigned i=0;i<benchCalls;++i)Parent();
        QueryPerformanceCounter(&end);
        elapsed[mode]=(end.QuadPart-begin.QuadPart)*1e9/frequency.QuadPart/benchCalls;
    }
    std::printf("OFFLINE gate + 64B caller-stack capture + stub forward: RPM=%.1f ns/call activeStack=%.1f ns/call; not game/FPS evidence\n",elapsed[0],elapsed[1]);
    enabled=true;overrideSource=reinterpret_cast<float*>(materialBlock+0x260);
    materialEnabled=false;Parent();Check(!copied,"material option off rejects field");materialEnabled=true;
    vectorCount=1;Parent();Check(!copied,"wrong fixed-field extent rejects");vectorCount=4;
    *reinterpret_cast<uintptr_t*>(ownerObject+0x1c)=reinterpret_cast<uintptr_t>(materialBlock)+16;
    Parent();Check(!copied,"owner block replacement never reuses prior ownership");
    *reinterpret_cast<uintptr_t*>(ownerObject+0x1c)=reinterpret_cast<uintptr_t>(materialBlock);
    overrideSource+=4;Parent();Check(!copied,"interior field pointer rejects");overrideSource-=4;
    fixtureMaterial=false;Parent();Check(!copied,"unknown field call site rejects");fixtureMaterial=true;
    fixtureOwner=0;Parent();Check(!copied,"absent EBX owner rejects");
    fixtureOwner=reinterpret_cast<uintptr_t>(ownerObject);fixtureMaterial=false;fixtureNested=true;
    alignas(16) unsigned char nestedModel[0x160]{};
    auto* nestedPayload=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    Check(nestedPayload!=nullptr,"nested payload allocation outside current stack");
    for(unsigned i=0;i<0x200;++i)nestedPayload[i]=static_cast<unsigned char>(i*13+29);
    uintptr_t nestedReference=reinterpret_cast<uintptr_t>(nestedPayload);
    *reinterpret_cast<uintptr_t*>(nestedModel+0x154)=reinterpret_cast<uintptr_t>(&nestedReference);
    *reinterpret_cast<uintptr_t*>(materialBlock+0x5d4)=reinterpret_cast<uintptr_t>(nestedModel);
    *reinterpret_cast<uintptr_t*>(materialBlock+0x5d8)=reinterpret_cast<uintptr_t>(nestedPayload);
    const auto nestedBefore=counts.nestedCopies;
    for(unsigned field=0;field<4;++field) {
        fixtureField=field;vectorCount=static_cast<unsigned>(nestedVectors[field]);
        overrideSource=reinterpret_cast<float*>(nestedPayload+nestedOffset[field]);
        for(unsigned i=0;i<250;++i) {
            Parent();Check(copied,"actual native-shaped producer admits current nested owner field");
            Check(!memcmp(recorded,forwarded,vectorCount*16),"actual hook nested bytes match forwarded native payload");
        }
    }
    Check(counts.nestedCopies==nestedBefore+1000,"separate nested copy coverage receipt");
    VirtualFree(nestedPayload,0,MEM_RELEASE);fixtureNested=false;vectorCount=4;
    overrideSource=nullptr;fixtureMaterial=false;materialEnabled=false;
    firstRegister=223;Parent();Check(!copied,"invalid register extent rejects");firstRegister=0;
    enabled=false;Parent();Check(!copied,"disabled rejects");enabled=true;
    admitted=false;Parent();Check(!copied,"unadmitted image rejects");admitted=true;
    Check(!Proof::Inspect(nullptr,0,nullptr,64,Stage::Pixel,1,2).Matches(nullptr,64),"invalid bounds reject");
    Proof invalid;Check(!Copy(invalid,recorded,forwarded,sizeof(recorded)),"default proof rejects");
    ClassifierFailures();
    ActiveStackFailures();
    NestedFields();
    Check(!AdmitImage("wrong",nullptr),"wrong image SHA rejects");
    VirtualFree(materialBlock,0,MEM_RELEASE);
    std::printf("PASS actual x86 gate/hook/native-shaped producer ABI: %u checks; copies=%llu materialCopies=%llu activeStackCopies=%llu injectedFaults=%llu\n",checks,counts.copies,counts.materialCopies,counts.activeStackCopies,counts.faults);
    return 0;
}

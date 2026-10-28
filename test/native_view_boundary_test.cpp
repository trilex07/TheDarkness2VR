#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include <xmmintrin.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <thread>
#include <vector>
#include "../src/native_view_boundary.h"
#include "../src/native_view_witness.h"
#include "../src/native_projection_upload.h"
#include "../src/native_culling_cover.h"
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
// Exercise the actual selected-camera admission callback. Native object writes
// and geometry rebuilding are stubbed here; the builder ABI and binocular
// geometry have separate fixtures. No game is loaded.
struct NativeCameraFields {float aspect,fov,nearZ,farZ,projection[16];};
static d2vr_native_view::ProjectionWitness g_nativeViewWitness;
static uint64_t g_nativeCullingAppliedFrame=0;
static struct Frame {uint64_t id=0;} g_trackedFrame;
static DWORD g_renderThreadId=0;
static int g_nativeViewBoundaryMode=3;
static bool g_nativeTranslatedViewPrepared=true;
static struct BoundaryStub {bool Installed()const{return false;}} g_nativeFrustumBoundary;
namespace d2vr_replay {static bool g_cameraProjectionKnown=true;static float g_cameraInverseProjection[16]{};}
static bool prepareOK=true,rebuildOK=true;
static unsigned prepareCalls=0,rebuildCalls=0;
static void ObserveSelectedPlayerCamera(void*){}
static bool PrepareOwnedCullingInput(void*,const NativeCameraFields&,d2vr_culling_cover::Input& input) {
    ++prepareCalls;input.stereoUnion=true;return prepareOK;
}
static bool RebuildOwnedCullingFrustum(void*,const d2vr_culling_cover::Input&){++rebuildCalls;return rebuildOK;}
template<class... Args> static void Log(const char*,Args...){}
static void LogNativeProjectionPair(const char*,const float*,const float*){}
#include "selected_camera_under_test.inl"
static volatile LONG callbacks=0;
static void* expectedOwner=reinterpret_cast<void*>(0x12345678);
static void Observe(void* owner) {
    CHECK(owner==expectedOwner);InterlockedIncrement(&callbacks);
    SetLastError(999);
    _mm_setcsr(_mm_getcsr()^0x6000);
    __asm fld1 // deliberately leave a dirty x87 stack; interception must restore it
}
static uint64_t TextHash(HMODULE module) {
    auto image=reinterpret_cast<unsigned char*>(module);
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS32*>(image+reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew);
    auto section=IMAGE_FIRST_SECTION(pe);uint64_t hash=14695981039346656037ull;
    for(unsigned n=0;n<pe->FileHeader.NumberOfSections;++n)if(section[n].Characteristics&IMAGE_SCN_MEM_EXECUTE)
        for(unsigned i=0;i<section[n].Misc.VirtualSize;++i)hash=(hash^image[section[n].VirtualAddress+i])*1099511628211ull;
    return hash;
}
int main(int argc,char** argv) {
    d2vr_native_view::ProjectionWitness witness;
    const float inverse[16]={1,0,0,0,0,2,0,0,0,0,0,-10,0,0,1,10};
    CHECK(witness.ProjectionMismatch(inverse,inverse)==0);
    CHECK(witness.ProjectionMismatch(nullptr,inverse)==0xffff);
    for(unsigned i=0;i<16;++i) {
        float changed[16];memcpy(changed,inverse,sizeof(changed));changed[i]+=1;
        CHECK(witness.ProjectionMismatch(changed,inverse)==(1u<<i));
        changed[i]=INFINITY;CHECK(witness.ProjectionMismatch(changed,inverse)==(1u<<i));
        changed[i]=NAN;CHECK(witness.ProjectionMismatch(changed,inverse)==(1u<<i));
    }
    witness.Begin(10,1,2,3,inverse);
    CHECK(!witness.CanCover(11,1,2,3));
    CHECK(!witness.Observe(9,true,inverse) && !witness.Observe(10,false,inverse));
    float wrong[16];memcpy(wrong,inverse,sizeof(wrong));wrong[5]=3;
    CHECK(!witness.Observe(10,true,wrong));
    CHECK(witness.Observe(10,true,inverse));
    CHECK(witness.CanCover(11,1,2,3));
    CHECK(witness.CanCoverProjection(11,1,2,3,inverse));
    CHECK(!witness.CanCoverProjection(11,1,2,3,wrong));
    CHECK(!witness.CanCoverProjection(11,1,2,3,nullptr));
    CHECK(!witness.CanCover(10,1,2,3) && !witness.CanCover(12,1,2,3));
    CHECK(!witness.CanCover(11,4,2,3) && !witness.CanCover(11,1,4,3) && !witness.CanCover(11,1,2,4));
    witness.Begin(11,1,2,3,inverse);CHECK(witness.Observe(11,true,inverse));
    CHECK(witness.CanCover(12,1,2,3));
    d2vr_native_view::ProjectionWitness early;
    CHECK(!early.Observe(20,true,inverse));
    early.Begin(20,1,2,3,inverse);
    CHECK(early.CanCover(21,1,2,3));
    early.Begin(21,1,2,3,inverse);
    CHECK(!early.CanCover(22,1,2,3)); // previous-frame upload cannot corroborate
    CHECK(!early.Observe(21,false,inverse));
    CHECK(!early.Observe(21,true,wrong));
    CHECK(!early.CanCover(22,1,2,3));
    CHECK(early.Observe(21,true,inverse));
    CHECK(early.CanCover(22,1,2,3));
    d2vr_native_view::ProjectionWitness mismatch;
    CHECK(!mismatch.Observe(30,true,wrong));
    mismatch.Begin(30,1,2,3,inverse);
    CHECK(!mismatch.CanCover(31,1,2,3));
    // Actual runtime rejection: depth components11/15, not a matrix transpose
    // or frame-phase mismatch. Native projection remains its own witness.
    const float py=1.f/std::tan(60.f*.00872664625997f),q=1000.f/(1000.f-.1f);
    float projection[16]={py/.980938435f,0,0,0,0,py,0,0,0,0,q,1,0,0,-q*.1f,0};
    float nativeInverse[16],shaderInverse[16];
    CHECK(d2vr_math::Invert(projection,nativeInverse));
    CHECK(d2vr_native_projection::ExpectedInverseUpload(projection,shaderInverse));
    const float capturedUpload[16]={.566345036f,0,0,0,0,.577350199f,0,0,0,0,0,-10.5252628f,0,0,1,10.5263157f};
    CHECK(witness.ProjectionMismatch(capturedUpload,nativeInverse)==0x8800);
    CHECK(witness.SameProjection(capturedUpload,shaderInverse));
    d2vr_native_view::ProjectionWitness converted;
    converted.Begin(100,1,2,3,nativeInverse,shaderInverse);
    CHECK(!converted.Observe(100,true,nativeInverse));
    CHECK(converted.Observe(100,true,capturedUpload));
    CHECK(converted.CanCoverProjection(101,1,2,3,nativeInverse));
    // The selected renderer can rebuild/copy its frustum again within one XR
    // frame. Begin can corroborate an already-observed current upload; that
    // must not erase the independent preceding-frame proof needed by a later
    // copy. No same-frame-only proof may authorize the first copy.
    CHECK(!converted.Observe(101,true,capturedUpload));
    converted.Begin(101,1,2,3,nativeInverse,shaderInverse);
    CHECK(converted.CanCoverProjection(101,1,2,3,nativeInverse));
    CHECK(converted.CanCoverProjection(102,1,2,3,nativeInverse));
    CHECK(!converted.CanCoverProjection(101,1,2,4,nativeInverse));
    CHECK(!converted.CanCoverProjection(101,1,2,3,shaderInverse));
    CHECK(!converted.CanCoverProjection(103,1,2,3,nativeInverse));
    // A camera change cannot inherit the prior camera's admission; once it
    // has its own previous-frame upload it can be admitted normally.
    converted.Begin(101,1,2,4,nativeInverse,shaderInverse);
    CHECK(!converted.CanCoverProjection(101,1,2,4,nativeInverse));
    CHECK(converted.CanCoverProjection(102,1,2,4,nativeInverse));
    converted.Begin(102,1,2,4,nativeInverse,shaderInverse);
    CHECK(converted.Observe(102,true,capturedUpload));
    CHECK(converted.CanCoverProjection(102,1,2,4,nativeInverse));
    CHECK(converted.CanCoverProjection(103,1,2,4,nativeInverse));
    CHECK(!converted.CanCoverProjection(104,1,2,4,nativeInverse));
    converted={};
    converted.Begin(100,1,2,3,nativeInverse,shaderInverse);
    CHECK(converted.Observe(100,true,capturedUpload));
    CHECK(!converted.CanCoverProjection(101,1,2,3,shaderInverse));
    CHECK(!converted.CanCoverProjection(101,1,2,4,nativeInverse));
    CHECK(!converted.CanCoverProjection(102,1,2,3,nativeInverse));
    for(unsigned i=0;i<16;++i) {
        float changed[16];memcpy(changed,capturedUpload,sizeof(changed));changed[i]+=.01f;
        d2vr_native_view::ProjectionWitness reject;
        reject.Begin(100,1,2,3,nativeInverse,shaderInverse);
        CHECK(!reject.Observe(100,true,changed));
        CHECK(!reject.CanCoverProjection(101,1,2,3,nativeInverse));
    }
    CHECK(!d2vr_native_projection::ExpectedInverseUpload(nullptr,shaderInverse));
    CHECK(!d2vr_native_projection::ExpectedInverseUpload(projection,nullptr));
    // Reproduce the production boundary -> shader upload -> next boundary
    // order, including a second native frustum copy in the same XR frame.
    NativeCameraFields fields{};memcpy(fields.projection,projection,sizeof(projection));
    fields.fov=60;fields.aspect=.980938435f;fields.nearZ=.1f;fields.farZ=1000;
    g_renderThreadId=GetCurrentThreadId();g_trackedFrame.id=100;
    memcpy(d2vr_replay::g_cameraInverseProjection,capturedUpload,sizeof(capturedUpload));
    const auto call=[&](void* camera=reinterpret_cast<void*>(3)) {
        ObserveNativeSelectedCamera(reinterpret_cast<void*>(0x98618E),reinterpret_cast<void*>(2),camera,fields,true);
    };
    call();CHECK(rebuildCalls==0 && !g_nativeCullingAppliedFrame);
    CHECK(g_nativeViewWitness.Observe(100,true,capturedUpload));
    g_trackedFrame.id=101;call();CHECK(rebuildCalls==1 && g_nativeCullingAppliedFrame==101);
    CHECK(g_nativeViewWitness.Observe(101,true,capturedUpload));
    call();CHECK(rebuildCalls==2 && g_nativeCullingAppliedFrame==101);
    // Current upload may also precede the first selected-camera callback.
    g_trackedFrame.id=102;CHECK(!g_nativeViewWitness.Observe(102,true,capturedUpload));
    call();call();CHECK(rebuildCalls==4 && g_nativeCullingAppliedFrame==102);
    d2vr_replay::g_cameraProjectionKnown=false;call();CHECK(rebuildCalls==4 && !g_nativeCullingAppliedFrame);
    d2vr_replay::g_cameraProjectionKnown=true;
    d2vr_replay::g_cameraInverseProjection[11]+=1;call();CHECK(rebuildCalls==4 && !g_nativeCullingAppliedFrame);
    memcpy(d2vr_replay::g_cameraInverseProjection,capturedUpload,sizeof(capturedUpload));
    call(reinterpret_cast<void*>(4));CHECK(rebuildCalls==4 && !g_nativeCullingAppliedFrame);
    call();CHECK(rebuildCalls==5 && g_nativeCullingAppliedFrame==102);
    prepareOK=false;call();CHECK(rebuildCalls==5 && !g_nativeCullingAppliedFrame);prepareOK=true;
    rebuildOK=false;call();CHECK(rebuildCalls==6 && !g_nativeCullingAppliedFrame);rebuildOK=true;
    g_trackedFrame.id=104;call();CHECK(rebuildCalls==6 && !g_nativeCullingAppliedFrame);
    puts("PASS actual selected-camera admission: repeated/early uploads, camera identity, full projection, freshness, prepare/rebuild failure");
    CHECK(argc==2);auto fixture=LoadLibraryA(argv[1]);CHECK(fixture);
    using Invoke=BOOL(__cdecl*)(LARGE_INTEGER*,void*);
    auto known=reinterpret_cast<Invoke>(GetProcAddress(fixture,"InvokeKnown"));CHECK(known);
    auto other=reinterpret_cast<Invoke>(GetProcAddress(fixture,"InvokeOther"));CHECK(other);
    auto caller=reinterpret_cast<uintptr_t(__cdecl*)()>(GetProcAddress(fixture,"FixtureCaller"));CHECK(caller);
    auto calls=reinterpret_cast<LONG(__cdecl*)()>(GetProcAddress(fixture,"FixtureCalls"));CHECK(calls);
    auto target=GetProcAddress(fixture,"_FixtureQuery@4");CHECK(target);
    LARGE_INTEGER output{};CHECK(known(&output,expectedOwner));const auto selected=caller();
    auto image=GetModuleHandleW(nullptr);
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS32*>(reinterpret_cast<char*>(image)+reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew);
    d2vr_native_image::Guard guard{true,reinterpret_cast<uintptr_t>(image),pe->OptionalHeader.SizeOfImage};
    const auto originalText=TextHash(image);
    d2vr_native_view::Boundary gate;
    CHECK(!gate.Start(target,selected,Observe,{}));
    CHECK(!gate.Start(reinterpret_cast<void*>(&Observe),selected,Observe,guard));
    CHECK(gate.Start(target,selected,Observe,guard));CHECK(gate.Installed());
    unsigned short controlBefore=0,statusBefore=0,controlAfter=0,statusAfter=0;
    __asm fnstcw controlBefore
    __asm fnstsw statusBefore
    const auto mxcsr=_mm_getcsr();
    CHECK(known(&output,expectedOwner));CHECK(GetLastError()==0x13572468);
    __asm fnstcw controlAfter
    __asm fnstsw statusAfter
    CHECK(controlBefore==controlAfter && statusBefore==statusAfter && _mm_getcsr()==mxcsr);
    CHECK(output.QuadPart==0x123456789abcdefLL && callbacks==1 && calls()==2);
    CHECK(!known(nullptr,expectedOwner));CHECK(GetLastError()==0x13572468 && callbacks==1 && calls()==3);
    CHECK(other(&output,reinterpret_cast<void*>(0xdeadbeef)));CHECK(callbacks==1 && calls()==4);
    std::vector<std::thread> threads;
    for(int t=0;t<4;++t)threads.emplace_back([=]{for(int i=0;i<1000;++i){LARGE_INTEGER value{};
        CHECK(known(&value,expectedOwner));CHECK(value.QuadPart==0x123456789abcdefLL && GetLastError()==0x13572468);
        CHECK(other(&value,nullptr));CHECK(GetLastError()==0x13572468);}});
    for(auto& thread:threads)thread.join();
    CHECK(callbacks==4001 && calls()==8004 && TextHash(image)==originalText);
    puts("PASS x86 external boundary: exact ESI/caller, API once, BOOL/output/LastError, x87/SSE, concurrent bypass, EXE text unchanged");
    return 0;
}

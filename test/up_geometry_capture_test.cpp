#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <atomic>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "@REPO@/src/up_geometry_capture.h"
#include "@REPO@/src/native_constant_source.h"
#include "@REPO@/src/small_record_copy.h"
#include "@REPO@/src/record_payload_storage.h"
#include "@REPO@/src/frame_resource_refs.h"
static unsigned checks=0,driverCalls=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %u line %d: %s\n",checks,__LINE__,#x);exit(1);}}while(0)
#define HOOK_MARK(x) ((void)0)
@TYPES@
static bool g_recActive=true,g_recOverflow=false;
static std::vector<Rec> g_rec;
static d2vr_record_payload::Storage g_recData;
static std::atomic<LONG> g_recLock{0};
static std::atomic<bool> g_wideRecordCopy{true};
static std::atomic<bool> g_directRecordCopy{false};
static uint64_t g_recCounts[40]{};
static d2vr_replay::FrameResourceRefs<IUnknown> g_recordRefs;
static bool SmallRecordCopyIsEnabled(){return true;}
static void NoteRecordedWorldDraw(){} // classification covered by projection fixture
@CORE@
struct UiClipState {};
static UiClipState BeginScaledUiClip(IDirect3DDevice9*){return {};}
static void EndScaledUiClip(IDirect3DDevice9*,const UiClipState&){}
static std::vector<uint8_t> rendered;
static HRESULT driverResult=D3D_OK;
static HRESULT DrawPrimitiveUPWithDiffuse(IDirect3DDevice9*,D3DPRIMITIVETYPE t,UINT pc,const void* vb,UINT stride) {
    ++driverCalls;if(FAILED(driverResult))return driverResult;
    uint64_t count=0;CHECK(d2vr_up_geometry::Elements(t,pc,false,count));
    rendered.clear();
    if(count)rendered.assign(static_cast<const uint8_t*>(vb),static_cast<const uint8_t*>(vb)+count*stride);
    return driverResult;
}
static HRESULT DrawIndexedPrimitiveUPWithDiffuse(IDirect3DDevice9*,D3DPRIMITIVETYPE t,UINT minimum,UINT vertices,UINT pc,
    const void* ib,D3DFORMAT fmt,const void* vb,UINT stride) {
    ++driverCalls;if(FAILED(driverResult))return driverResult;
    uint64_t count=0;CHECK(d2vr_up_geometry::Elements(t,pc,true,count));
    rendered.clear();
    for(unsigned n=0;n<count;++n) {
        const unsigned index=fmt==D3DFMT_INDEX16?static_cast<const uint16_t*>(ib)[n]:static_cast<const uint32_t*>(ib)[n];
        CHECK(index>=minimum && uint64_t(index)<uint64_t(minimum)+vertices);
        const auto* data=static_cast<const uint8_t*>(vb)+size_t(index)*stride;
        rendered.insert(rendered.end(),data,data+stride);
    }
    return driverResult;
}
@HOOKS@
static bool Replay() {
    const uint8_t* dataBase=g_recData.data();IDirect3DDevice9* dev=nullptr;
    bool replayComplete=!g_recOverflow;
    for(const auto& r:g_rec)switch((RecOp)r.op) {
@CASES@
    default:CHECK(false);
    }
    return replayComplete;
}
static void Reset(){g_rec.clear();g_recData.clear();g_recOverflow=false;g_recActive=true;driverCalls=0;driverResult=D3D_OK;}
int main() {
    // Indexed legacy capture stores only NumVertices from the base even though
    // the original fetch uses indices >= MinVertexIndex. Reproduce first.
    for(unsigned format=0;format<2;++format)for(unsigned minimum:{4u,17u,128u}) {
        Reset();const unsigned vertices=3,stride=16;
        std::vector<uint8_t> data((minimum+vertices)*stride+1024);
        for(unsigned i=0;i<data.size();++i)data[i]=uint8_t(i*17+31);
        uint16_t shortIndices[3]={uint16_t(minimum),uint16_t(minimum+2),uint16_t(minimum+1)};
        uint32_t longIndices[3]={minimum,minimum+2,minimum+1};
        const void* indices=format?static_cast<void*>(longIndices):static_cast<void*>(shortIndices);
        CHECK(Hook_DrawIndexedPrimitiveUP(nullptr,D3DPT_TRIANGLELIST,minimum,vertices,1,indices,
            format?D3DFMT_INDEX32:D3DFMT_INDEX16,data.data(),stride)==D3D_OK);
        const auto expected=rendered;
        // Force spare arena storage so the legacy short snapshot produces a
        // deterministic wrong fetch rather than an out-of-allocation read.
        g_recData.resize((minimum+vertices)*stride+1024,0xcd);
        std::fill(data.begin(),data.end(),uint8_t(0xee));memset(shortIndices,0,sizeof(shortIndices));memset(longIndices,0,sizeof(longIndices));
        g_recActive=false;CHECK(Replay());CHECK(rendered==expected && driverCalls==2);
    }
    for(unsigned type=1;type<=6;++type)for(unsigned primitives:{0u,1u,7u,99u}) {
        Reset();d2vr_up_geometry::Plan plan;
        CHECK(d2vr_up_geometry::NonIndexed(D3DPRIMITIVETYPE(type),primitives,16,plan));
        std::vector<uint8_t> data(plan.vertices+16,0xa7);
        CHECK(Hook_DrawPrimitiveUP(nullptr,D3DPRIMITIVETYPE(type),primitives,data.data(),16)==D3D_OK);
        CHECK(g_recData.size()==plan.vertices && g_rec.size()==1 && !g_recOverflow);
        const auto expected=rendered;std::fill(data.begin(),data.end(),uint8_t(0xb6));g_recActive=false;
        CHECK(Replay() && rendered==expected && driverCalls==2 && g_recLock==0);
    }
    // Unused prefix is not part of the API's readable input range.
    Reset();SYSTEM_INFO info{};GetSystemInfo(&info);const size_t page=info.dwPageSize;
    auto* data=static_cast<uint8_t*>(VirtualAlloc(nullptr,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));CHECK(data);
    memset(data+page,0x69,48);DWORD old=0;CHECK(VirtualProtect(data,page,PAGE_NOACCESS,&old));
    const unsigned minimum=unsigned(page/16);uint32_t indices[3]={minimum,minimum+1,minimum+2};
    CHECK(Hook_DrawIndexedPrimitiveUP(nullptr,D3DPT_TRIANGLELIST,minimum,3,1,indices,D3DFMT_INDEX32,data,16)==D3D_OK);
    const auto expected=rendered;memset(data+page,0xff,48);g_recActive=false;
    CHECK(Replay() && rendered==expected && driverCalls==2);
    for(size_t n=0;n<page;++n)CHECK(g_recData[n]==0); // arena prefix initialized despite nonzeroing allocator
    VirtualFree(data,0,MEM_RELEASE);
    // A sane pointer/range can still cross an unreadable page. Never publish
    // the readable-prefix/zero-tail storage as valid deferred geometry.
    data=static_cast<uint8_t*>(VirtualAlloc(nullptr,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));CHECK(data);
    std::vector<uint8_t> validVertices(48,0x39);
    uint16_t validIndices[3]={0,1,2};
    for(DWORD protection:{PAGE_NOACCESS,PAGE_READWRITE|PAGE_GUARD}) {
        CHECK(VirtualProtect(data+page,page,protection,&old));
        Reset();driverResult=D3DERR_INVALIDCALL;
        CHECK(Hook_DrawPrimitiveUP(nullptr,D3DPT_TRIANGLELIST,1,data+page-16,16)==D3DERR_INVALIDCALL);
        CHECK(g_recOverflow && g_rec.empty() && driverCalls==1 && !Replay());
        Reset();driverResult=D3DERR_INVALIDCALL;
        CHECK(Hook_DrawIndexedPrimitiveUP(nullptr,D3DPT_TRIANGLELIST,0,3,1,validIndices,D3DFMT_INDEX16,
            data+page-16,16)==D3DERR_INVALIDCALL);
        CHECK(g_recOverflow && g_rec.empty() && driverCalls==1 && !Replay());
        for(D3DFORMAT format:{D3DFMT_INDEX16,D3DFMT_INDEX32}) {
            Reset();driverResult=D3DERR_INVALIDCALL;
            CHECK(Hook_DrawIndexedPrimitiveUP(nullptr,D3DPT_TRIANGLELIST,0,3,1,data+page-2,format,
                validVertices.data(),16)==D3DERR_INVALIDCALL);
            CHECK(g_recOverflow && g_rec.empty() && driverCalls==1 && !Replay());
        }
        MEMORY_BASIC_INFORMATION protectedPage{};
        CHECK(VirtualQuery(data+page,&protectedPage,sizeof(protectedPage)));
        CHECK(protectedPage.Protect==protection);
        CHECK(VirtualProtect(data+page,page,PAGE_READWRITE,&old));
        Reset();CHECK(Hook_DrawPrimitiveUP(nullptr,D3DPT_TRIANGLELIST,1,validVertices.data(),16)==D3D_OK);
        CHECK(!g_recOverflow && g_rec.size()==1 && Replay() && driverCalls==2);
    }
    VirtualFree(data,0,MEM_RELEASE);
    d2vr_up_geometry::Plan untouched;untouched.vertices=77;
    for(unsigned type=1;type<=6;++type) {
        CHECK(!d2vr_up_geometry::NonIndexed(D3DPRIMITIVETYPE(type),UINT32_MAX,UINT32_MAX,untouched));
        CHECK(untouched.vertices==77);
    }
    CHECK(!d2vr_up_geometry::Indexed(D3DPT_TRIANGLELIST,UINT32_MAX,2,1,D3DFMT_INDEX16,16,untouched));
    CHECK(!d2vr_up_geometry::Indexed(D3DPT_POINTLIST,0,3,1,D3DFMT_INDEX16,16,untouched));
    CHECK(!d2vr_up_geometry::Indexed(D3DPT_TRIANGLELIST,0,3,1,D3DFMT_UNKNOWN,16,untouched));
    CHECK(!d2vr_up_geometry::SourceRange(reinterpret_cast<void*>(UINTPTR_MAX-15),16,16));
    Reset();driverResult=D3DERR_INVALIDCALL;
    CHECK(Hook_DrawPrimitiveUP(nullptr,D3DPT_TRIANGLELIST,UINT32_MAX,reinterpret_cast<void*>(1),16)==D3DERR_INVALIDCALL);
    CHECK(g_recOverflow && g_rec.empty() && g_recData.empty() && driverCalls==1 && !Replay());
    Reset();driverResult=D3DERR_INVALIDCALL;
    CHECK(Hook_DrawIndexedPrimitiveUP(nullptr,D3DPT_TRIANGLELIST,UINT32_MAX,5,1,reinterpret_cast<void*>(1),D3DFMT_INDEX16,
        reinterpret_cast<void*>(1),16)==D3DERR_INVALIDCALL);
    CHECK(g_recOverflow && g_rec.empty() && g_recData.empty() && driverCalls==1 && !Replay());
    Reset();g_recActive=false;std::vector<uint8_t> direct(48,0x37);
    CHECK(Hook_DrawPrimitiveUP(nullptr,D3DPT_TRIANGLELIST,1,direct.data(),16)==D3D_OK && g_rec.empty() && driverCalls==1);
    Reset();driverResult=S_FALSE;
    CHECK(Hook_DrawPrimitiveUP(nullptr,D3DPT_TRIANGLELIST,1,direct.data(),16)==S_FALSE);
    g_recActive=false;CHECK(Replay() && driverCalls==2);
    // Readable, valid geometry must still invalidate the entire recording if
    // the native draw fails. A later successful draw cannot repair that frame.
    for(bool indexed:{false,true})for(bool active:{false,true}) {
        Reset();g_recActive=active;driverResult=D3DERR_INVALIDCALL;
        auto draw=[&] {
            return indexed ? Hook_DrawIndexedPrimitiveUP(nullptr,D3DPT_TRIANGLELIST,0,3,1,
                validIndices,D3DFMT_INDEX16,direct.data(),16) :
                Hook_DrawPrimitiveUP(nullptr,D3DPT_TRIANGLELIST,1,direct.data(),16);
        };
        CHECK(draw()==D3DERR_INVALIDCALL && driverCalls==1 && g_recOverflow==active);
        CHECK(g_rec.size()==(active?1u:0u) && g_recLock==0);
        driverResult=D3D_OK;
        CHECK(draw()==D3D_OK && driverCalls==2 && g_recOverflow==active);
        CHECK(g_rec.size()==(active?1u:0u) && g_recLock==0);
    }
    printf("PASS actual UP hooks+replay: %u checks; both index formats/nonzero minima, all primitive extents, overwritten inputs, inaccessible prefix, initialized arena padding, overflow rejection/original once; no GPU/HMD claim\n",checks);
}

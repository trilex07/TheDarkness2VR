#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <chrono>
#include "mapped_vertex_copy.h"
#include "selected_shadow.h"
#define CHECK(x) do{if(!(x)){std::printf("FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)

static void CheckCopy(const unsigned char* src,size_t count) {
    for(size_t offset=0;offset<16;++offset) {
        std::vector<unsigned char> dst(count+32,0xac);
        d2vr_ui_vertices::CopyMappedVertexBytes(dst.data()+offset,src,count);
        CHECK(!std::memcmp(dst.data()+offset,src,count));
        for(size_t i=0;i<offset;++i)CHECK(dst[i]==0xac);
        for(size_t i=offset+count;i<dst.size();++i)CHECK(dst[i]==0xac);
        std::memset(dst.data(),0xac,dst.size());
        d2vr_ui_vertices::CopyMappedVertexBytes(dst.data()+offset,src,count,false);
        CHECK(!std::memcmp(dst.data()+offset,src,count));
    }
}
int main() {
    constexpr size_t page=4096,capacity=256*1024;
    auto guarded=static_cast<unsigned char*>(VirtualAlloc(nullptr,3*page,MEM_RESERVE,PAGE_NOACCESS));
    CHECK(guarded);
    auto middle=static_cast<unsigned char*>(VirtualAlloc(guarded+page,page,MEM_COMMIT,PAGE_READWRITE));
    CHECK(middle);
    for(size_t i=0;i<page;++i)middle[i]=static_cast<unsigned char>(i*37);
    // End each range at an inaccessible page; also exercise every start and
    // destination alignment, cache-line tail and the non-SSE4.1 fallback.
    for(size_t count=1;count<=256;++count)CheckCopy(middle+page-count,count);
    for(size_t offset=0;offset<64;++offset)CheckCopy(middle+offset,257);
    CHECK(VirtualFree(guarded,0,MEM_RELEASE));

    auto wc=static_cast<unsigned char*>(VirtualAlloc(nullptr,capacity,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE|PAGE_WRITECOMBINE));
    CHECK(wc);
    MEMORY_BASIC_INFORMATION region{};
    CHECK(VirtualQuery(wc,&region,sizeof(region))==sizeof(region));
    CHECK(region.Protect&PAGE_WRITECOMBINE);
    std::vector<unsigned char> expected(capacity);
    for(size_t i=0;i<capacity;++i)expected[i]=static_cast<unsigned char>(i*13+7);
    std::memcpy(wc,expected.data(),capacity);
    d2vr_ui_vertices::Shadow shadow;shadow.Resize(capacity);
    // Actual production Shadow::Stage over real WC pages, no game or GPU.
    const auto start=std::chrono::steady_clock::now();
    for(unsigned pass=0;pass<4;++pass){
        CHECK(shadow.Stage(0,capacity,wc,true));CHECK(shadow.Commit(true));
    }
    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    CHECK(shadow.Read(0,capacity));CHECK(!std::memcmp(shadow.Read(0,capacity),expected.data(),capacity));
    // Reuse the same WC address with different contents; streaming buffers
    // must not supply values from the previous capture.
    for(unsigned pass=0;pass<16;++pass){
        expected[pass]=static_cast<unsigned char>(pass+101);
        std::memcpy(wc,expected.data(),capacity);
        CHECK(shadow.Stage(0,capacity,wc,true));CHECK(shadow.Commit(true));
        CHECK(!std::memcmp(shadow.Read(0,capacity),expected.data(),capacity));
    }
    CHECK(VirtualFree(wc,0,MEM_RELEASE));
    std::printf("PASS mapped copy: guard pages, alignments, fallback, WC reuse; SSE4.1=%d; actual Shadow::Stage 4x256KiB WC=%.3f ms (CPU fixture, not game frame time)\n",d2vr_ui_vertices::StreamingLoadsSupported(),ms);
}

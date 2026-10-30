#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "mapped_vertex_copy.h"
#define CHECK(x) do{if(!(x)){std::printf("FAIL line %d\n",__LINE__);std::exit(1);}}while(0)
int main(){
 const bool supported=d2vr_mapped_copy::Supported();
 unsigned checks=0;
 for(bool wc:{false,true}){
  auto src=static_cast<unsigned char*>(VirtualAlloc(nullptr,8192,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE|(wc?PAGE_WRITECOMBINE:0)));
  CHECK(src);for(unsigned i=0;i<8192;++i)src[i]=static_cast<unsigned char>(i*37);
  std::vector<unsigned char> dest(8256);
  for(bool fast:{false,true}){if(fast&&!supported)continue;
   for(unsigned offset=0;offset<32;++offset)for(unsigned size=0;size<=2048;++size){
    const unsigned target=(offset*7)&31;std::memset(dest.data(),0xcd,dest.size());
    d2vr_mapped_copy::Copy(dest.data()+target,src+offset,size,fast);
    for(unsigned i=0;i<size;++i)CHECK(dest[target+i]==static_cast<unsigned char>((offset+i)*37));
    for(unsigned i=0;i<target;++i)CHECK(dest[i]==0xcd);
    CHECK(dest[target+size]==0xcd);++checks;
   }
  }
  DWORD old=0;CHECK(VirtualProtect(src+4096,4096,PAGE_NOACCESS,&old));
  for(unsigned size=1;size<=4096;++size){d2vr_mapped_copy::Copy(dest.data(),src+4096-size,size,supported);CHECK(dest[size-1]==static_cast<unsigned char>(4095*37));++checks;}
  VirtualFree(src,0,MEM_RELEASE);
 }
 for(bool wc:{false,true})for(size_t bytes:{size_t(256),size_t(65536),size_t(524288),size_t(1048576)}){
 auto src=static_cast<unsigned char*>(VirtualAlloc(nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE|(wc?PAGE_WRITECOMBINE:0)));CHECK(src);
 std::memset(src,91,bytes);std::vector<unsigned char> dst(bytes);LARGE_INTEGER freq,a,b;QueryPerformanceFrequency(&freq);
 for(bool fast:{false,true}){if(fast&&!supported)continue;QueryPerformanceCounter(&a);for(unsigned i=0;i<128;++i)d2vr_mapped_copy::Copy(dst.data(),src,bytes,fast);QueryPerformanceCounter(&b);CHECK(dst[bytes-1]==91);std::printf("%s bytes=%zu streaming=%d: %.6f ms/copy\n",wc?"WC":"WB",bytes,fast,1000.*(b.QuadPart-a.QuadPart)/freq.QuadPart/128);}
 VirtualFree(src,0,MEM_RELEASE);
 }
 std::printf("PASS %u cached/WC copy cases, offsets, sentinels, inaccessible page boundary; SSE4.1=%d\n",checks,supported);
}

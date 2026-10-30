#include "../../src/command_history.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace d2vr_command_history;
int main(){
    Ring<4> ring;
    Event event{};event.begin=100;event.pipeline.vs=123;event.pipeline.known=1;
    auto first=ring.Begin(event);event.pipeline.vs=999;
    assert(ring.At(0).pipeline.vs==123); // immutable value snapshot, no COM pointer dereference
    auto nested=ring.Begin(event);
    ring.End(nested,101,0x80004005u,12);ring.End(first,110,0,12);
    assert(ring.At(1).result==0x80004005u && !ring.frozen);
    for(unsigned i=0;i<5;++i){event.begin=120+i;ring.End(ring.Begin(event),121+i,0,12);}
    assert(ring.Count()==4 && ring.overwritten==3 && ring.At(0).sequence==4);
    ring.End(first,200,0,12);assert(ring.missingEnds==1);
    event.begin=200;auto trigger=ring.Begin(event);ring.End(trigger,220,1,12,true);
    assert(ring.frozen && ring.trigger==trigger && ring.At(3).uncertain==1);
    const auto count=ring.written;auto saved=ring.At(3);
    assert(!ring.Begin(event));ring.End(trigger,230,9,12);
    assert(ring.written==count && !std::memcmp(&saved,&ring.At(3),sizeof(Event)));
    // Emit an actual-header fixture for the Python decoder, including open nested records.
    Ring<4> fixture;event={};event.begin=100;event.pipeline.vs=123;event.pipeline.known=1;
    auto outer=fixture.Begin(event);event.begin=101;auto inner=fixture.Begin(event);
    fixture.End(inner,130,0,12);fixture.End(outer,131,0,12);
    assert(fixture.At(0).end==0 && fixture.At(1).end==130);
    FILE* file=nullptr;fopen_s(&file,"history-fixture.bin","wb");assert(file);
    uint64_t header[12]={0x3154534948323244ull,1,sizeof(Event),fixture.Count(),10000,1,inner,2,0,0,10,0};
    assert(fwrite(header,sizeof(header),1,file)==1);
    for(size_t i=0;i<fixture.Count();++i)assert(fwrite(&fixture.At(i),sizeof(Event),1,file)==1);
    fclose(file);printf("PASS bounded wrap, nested calls, failed HRESULT, immutable freeze; eventBytes=%zu\n",sizeof(Event));
}

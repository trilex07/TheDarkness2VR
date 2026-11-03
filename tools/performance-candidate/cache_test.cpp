#include "../../src/shader_preload_pool.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
using namespace d2vr_shader_preload;
unsigned checks=0,releases=0;
void check(bool ok){++checks;if(!ok){printf("FAIL %u\n",checks);exit(1);}}
struct Object{void Release(){++releases;}};
std::vector<uint32_t> code(unsigned i){return {0xfffe0300u,0x0001fffeu,i,0xffffu};}
int main(int argc,char** argv){
    int device=0,other=0;uint64_t version=0;
    Pool<Object> legacy(&device,2),pool(&device,2,true);
    for(unsigned i=0;i<MaxShaders;++i){check(pool.Remember(&device,code(i)));check(legacy.Remember(&device,code(i))==(i<LegacyMaxShaders));}
    check(!pool.Remember(&device,code(MaxShaders)));check(pool.Size()==MaxShaders);
    Object objects[MaxCopies];
    check(pool.Adopt(&device,code(47),&objects[0],0));check(pool.Adopt(&device,code(47),&objects[1],1));
    check(!pool.Adopt(&device,code(47),&objects[2],0));
    int32_t result=0;TakeStatus status{};
    check(!pool.Take(&other,code(47),0xfffe0300u,result,&status));
    check(pool.Take(&device,code(47),0xfffe0300u,result,&status)==&objects[1]&&result==1);
    check(!pool.Take(&device,code(48),0xfffe0300u,result,&status)&&status==TakeStatus::Depleted);
    for(unsigned i=0;i<9;++i)pool.ObserveCreation(&device,code(4095));
    auto archive=pool.Archive(version);
    check(ValidateArchive((const unsigned char*)archive.data(),archive.size()*4));
    check(archive[2]==MaxCopies&&archive[6]==4095); // most recent demand first
    pool.ClearObjects();check(pool.Pending()==0&&releases==1); // transferred reference retained
    {
        Pool<Object> bulk(&device,2,true);Object items[130];
        for(unsigned i=0;i<65;++i){check(bulk.Remember(&device,code(i)));check(bulk.Adopt(&device,code(i),&items[2*i],0));check(bulk.Adopt(&device,code(i),&items[2*i+1],0));}
        bulk.ClearObjects();check(bulk.Pending()==0&&releases==131); // multiple bounded release chunks
    }
    check(Inventory({2,8,3},true,8)==std::vector<size_t>({0,1,2,1,1,1,1,1}));
    check(Inventory({2,8,3},false,8)==std::vector<size_t>({0,1,2,0,1,2,1,2}));
    std::vector<unsigned> quotas(4096,8);auto selected=Inventory(quotas,true);
    check(selected.size()==MaxObjects&&selected.front()==0&&selected.back()==MaxObjects-1);
    // Archive byte cap remains independent of enlarged entry count.
    Pool<Object> bytes(&device,1,true);unsigned admitted=0;
    for(unsigned i=0;i<MaxShaders;++i){auto c=code(i);c.insert(c.end()-1,1024,0u);if(!bytes.Remember(&device,c))break;++admitted;}
    check(admitted<MaxShaders&&admitted>0);auto bounded=bytes.Archive(version);
    check(bounded.size()*4<=d2vr_startup_shader::MaxBytes);
    check(ValidateArchive((const unsigned char*)bounded.data(),bounded.size()*4));
    if(argc>1&&argv[1][0]){
        std::ifstream file(argv[1],std::ios::binary);std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)),{});
        check(ValidateArchive(data.data(),data.size()));
        Pool<Object> restored(&device,2,true);size_t at=8;std::vector<unsigned> q;
        const bool demand=d2vr_startup_shader::Word(data.data())==DemandMagic;
        for(unsigned i=0;i<d2vr_startup_shader::Word(data.data()+4);++i){
            unsigned quota=demand?d2vr_startup_shader::Word(data.data()+at):1;if(demand)at+=4;
            unsigned words=d2vr_startup_shader::Word(data.data()+at);at+=4;std::vector<uint32_t> c(words);
            memcpy(c.data(),data.data()+at,words*4);at+=words*4;
            check(restored.Remember(&device,c,quota));q.push_back(std::max(2u,quota));
        }
        auto inventory=Inventory(q,true);std::vector<unsigned> counts(q.size());for(auto i:inventory){check(i<q.size());++counts[i];}
        check(inventory.size()<=MaxObjects);for(auto n:counts)check(n>=1);
        printf("Retained archive: %zu exact codes; %zu startup objects; full single-copy coverage within unchanged budget\n",q.size(),inventory.size());
    }
    printf("PASS %u actual cache checks; 4096 cap, legacy cap, byte cap, exact identity, recency, inventory and one-use ownership\n",checks);
}

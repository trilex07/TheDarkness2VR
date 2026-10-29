#include "../../src/shader_preload_pool.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
using namespace d2vr_shader_preload;
unsigned checks=0,releases=0;
void check(bool ok) {++checks;if(!ok){std::printf("FAIL %u\n",checks);std::exit(1);}}
struct Object { void Release(){++releases;} };
const std::vector<uint32_t> vs{0xfffe0300u,65535u},ps{0xffff0300u,65535u};
bool valid(const std::vector<uint32_t>& data,size_t bytes=0) {
    return ValidateArchive(reinterpret_cast<const unsigned char*>(data.data()),bytes?bytes:data.size()*4);
}
int main(int argc,char** argv) {
    int device=0,other=0;Object objects[MaxCopies+1];
    Pool<Object> pool(&device,2);
    check(pool.Remember(&device,vs));check(pool.Remember(&device,ps));
    check(!pool.Remember(&other,vs));
    check(pool.Adopt(&device,vs,&objects[0],0));
    check(pool.Adopt(&device,vs,&objects[1],1));
    check(!pool.Adopt(&device,vs,&objects[2],0));
    check(!pool.Adopt(&other,vs,&objects[2],0));
    int32_t result=-1;TakeStatus status{};
    auto version=pool.Version();
    pool.Enable(false);
    check(!pool.Take(&device,vs,vs[0],result,&status)&&status==TakeStatus::Disabled);
    pool.Enable(true);
    check(!pool.Take(&device,vs,ps[0],result,&status)&&status==TakeStatus::Ineligible);
    check(pool.Take(&device,vs,vs[0],result,&status)==&objects[1]&&result==1);
    check(pool.Take(&device,vs,vs[0],result,&status)==&objects[0]&&result==0);
    check(pool.Version()==version);
    check(!pool.Take(&device,vs,vs[0],result,&status)&&status==TakeStatus::Depleted);
    check(pool.Version()==version); // failure/fallback alone cannot learn demand
    pool.ObserveCreation(&other,vs);check(pool.Version()==version);
    pool.ObserveCreation(&device,vs);check(pool.Version()==version+1);
    uint64_t saved=0;auto archive=pool.Archive(saved);
    check(valid(archive)&&archive[0]==DemandMagic&&archive[2]==3&&saved==version+1);
    for(unsigned i=0;i<10000;++i)pool.ObserveCreation(&device,vs);
    archive=pool.Archive(saved);check(archive[2]==MaxCopies);
    const auto saturated=pool.Version();pool.ObserveCreation(&device,vs);check(pool.Version()==saturated);
    for(size_t n=1;n<archive.size()*4;++n)check(!valid(archive,n));
    auto malformed=archive;malformed[2]=0;check(!valid(malformed));
    malformed[2]=MaxCopies+1;check(!valid(malformed));
    malformed=archive;malformed[3]=0xffffffffu;check(!valid(malformed));
    malformed=archive;malformed.push_back(0);check(!valid(malformed));
    check(valid({d2vr_startup_shader::Magic,1,2,vs[0],vs[1]}));
    {
        Pool<Object> restored(&device,1);
        check(restored.Remember(&device,vs,archive[2]));
        for(unsigned i=0;i<MaxCopies;++i)check(restored.Adopt(&device,vs,&objects[i],0));
        check(!restored.Adopt(&device,vs,&objects[MaxCopies],0));
        check(restored.Take(&device,vs,vs[0],result)==&objects[MaxCopies-1]);
        restored.ClearObjects();check(releases==MaxCopies-1); // never release transferred objects
        check(restored.Pending()==0);
    }
    check(releases==MaxCopies-1);
    if(argc==2) {
        std::ifstream file(argv[1],std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
        check(ValidateArchive(bytes.data(),bytes.size()));
        Pool<Object> migrated(&device,2);
        size_t cursor=8;
        const bool demand=d2vr_startup_shader::Word(bytes.data())==DemandMagic;
        for(uint32_t i=0;i<d2vr_startup_shader::Word(bytes.data()+4);++i) {
            const auto quota=demand?d2vr_startup_shader::Word(bytes.data()+cursor):1u;
            if(demand)cursor+=4;
            const auto words=d2vr_startup_shader::Word(bytes.data()+cursor);cursor+=4;
            std::vector<uint32_t> code(words);
            std::memcpy(code.data(),bytes.data()+cursor,words*4);cursor+=words*4;
            check(migrated.Remember(&device,code,quota));
            for(unsigned n=0;n<3;++n)migrated.ObserveCreation(&device,code);
        }
        auto migratedData=migrated.Archive(saved);
        check(valid(migratedData));
        check(migrated.Size()==d2vr_startup_shader::Word(bytes.data()+4));
        check(migrated.Pending()==0);
        std::printf("Retained private archive admitted (%zu bytes), no driver execution\n",bytes.size());
    }
    std::printf("PASS %u actual-pool checks: exact identity, bounded demand, migration, ownership, malformed data\n",checks);
}

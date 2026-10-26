#include "../src/frame_resource_refs.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>

#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n",__LINE__,#c); std::exit(1); } } while (0)
struct Resource {
    unsigned refs=1, adds=0, releases=0, generation=0;
    bool dead=false;
    void AddRef() { CHECK(!dead); ++adds; ++refs; }
    void Release() { CHECK(refs>0); ++releases; if(!--refs) dead=true; }
    void Draw(unsigned expected) const { CHECK(!dead && refs>0 && generation==expected); }
};

static void TestBaseline() {
    Resource resource;
    d2vr_replay::FrameResourceRefs<Resource> refs;
    refs.Begin(false);
    refs.Retain(nullptr);
    for(unsigned i=0;i<100;++i) refs.Retain(&resource);
    CHECK(!refs.unique() && refs.requests()==100 && refs.size()==100);
    CHECK(resource.refs==101 && resource.adds==100);
    refs.Clear();
    CHECK(resource.refs==1 && resource.releases==100);
    refs.Clear();
    CHECK(resource.releases==100 && refs.size()==0 && refs.requests()==0);
}
static void TestOwnerReleaseAndBothEyes() {
    for(bool unique : {false,true}) {
        Resource shader, mesh, target;
        d2vr_replay::FrameResourceRefs<Resource> refs;
        refs.Begin(unique);
        // Each record remains present. Only retention ownership is deduplicated.
        for(unsigned draw=0;draw<200;++draw) {
            refs.Retain(&shader); refs.Retain(&mesh); refs.Retain(&target);
            shader.Draw(0); mesh.Draw(0); target.Draw(0); // live eye
        }
        shader.Release(); mesh.Release(); target.Release(); // native owners drop
        CHECK(!shader.dead && !mesh.dead && !target.dead);
        for(unsigned draw=0;draw<200;++draw) {
            shader.Draw(0); mesh.Draw(0); target.Draw(0); // same-frame replay eye
        }
        CHECK(refs.requests()==600 && refs.size()==(unique?3u:600u));
        refs.Clear();
        CHECK(shader.dead && mesh.dead && target.dead);
    }
}
static void TestExactIdentityAndFrameBoundary() {
    Resource aliases[2]; // distinct interface addresses are not merged
    d2vr_replay::FrameResourceRefs<Resource> refs;
    refs.Begin(true);
    for(unsigned i=0;i<100;++i) for(auto& resource:aliases) refs.Retain(&resource);
    CHECK(refs.size()==2 && refs.requests()==200);
    refs.Begin(true);
    CHECK(aliases[0].refs==1 && aliases[1].refs==1);
    refs.Retain(&aliases[0]);
    CHECK(aliases[0].adds==2); // no cross-frame or shader cache
    refs.Clear();
    alignas(Resource) unsigned char storage[sizeof(Resource)];
    auto* resource=new(storage) Resource;
    resource->generation=1;
    refs.Begin(true); refs.Retain(resource); resource->Release(); refs.Clear();
    CHECK(resource->dead); resource->~Resource();
    resource=new(storage) Resource; resource->generation=2; // same VA, new lifetime
    refs.Begin(true); refs.Retain(resource); resource->Draw(2);
    CHECK(resource->adds==1 && refs.size()==1);
    refs.Clear(); resource->Release(); resource->~Resource();
}
static void TestResetFailureAndModeChange() {
    Resource resource;
    d2vr_replay::FrameResourceRefs<Resource> refs;
    refs.Begin(true);
    refs.Retain(&resource); refs.Retain(&resource);
    refs.Clear(); // capture/overflow abort, lost/reset or ordinary frame teardown
    CHECK(resource.refs==1);
    refs.Begin(false); refs.Retain(&resource); refs.Retain(&resource);
    CHECK(refs.size()==2 && resource.refs==3);
    refs.Begin(true); // configuration changes only affect new frame
    CHECK(resource.refs==1); refs.Retain(&resource); refs.Retain(&resource);
    CHECK(refs.size()==1 && resource.refs==2);
    refs.Clear(); CHECK(resource.refs==1);
}
static void TestTenThousandFrames() {
    constexpr unsigned frames=10000, resources=64, binds=2048;
    std::array<Resource,resources> objects{};
    unsigned long long adds[2]{};
    for(unsigned mode=0;mode<2;++mode) {
        d2vr_replay::FrameResourceRefs<Resource> refs;
        for(unsigned frame=0;frame<frames;++frame) {
            refs.Begin(mode!=0);
            for(unsigned bind=0;bind<binds;++bind) {
                auto& resource=objects[(bind*17+frame)%resources];
                refs.Retain(&resource);
                resource.Draw(0);
            }
            CHECK(refs.requests()==binds);
            adds[mode]+=refs.size();
            refs.Clear();
            for(const auto& resource:objects) CHECK(resource.refs==1 && !resource.dead);
        }
    }
    CHECK(adds[0]==20480000ULL && adds[1]==640000ULL);
    std::printf("PASS 10000 frames/mode: COM AddRef+Release pairs %llu -> %llu; all resources alive through replay\n",adds[0],adds[1]);
}
int main() {
    TestBaseline(); TestOwnerReleaseAndBothEyes(); TestExactIdentityAndFrameBoundary();
    TestResetFailureAndModeChange(); TestTenThousandFrames();
    std::puts("PASS baseline, exact identity, owner drop, both eyes, frame reuse, reset/abort, mode change");
}

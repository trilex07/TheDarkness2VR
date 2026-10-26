#include "../src/shared_reader_fence.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#define CHECK(c) do {if(!(c)){std::printf("FAIL line %d: %s\n",__LINE__,#c);std::exit(1);}}while(0)
using d2vr_shared_reader::Fence;
using d2vr_shared_reader::Poll;
struct Ops {
    int contextToken=0,queryToken=0;
    const void* context=&contextToken;
    const void* query=&queryToken;
    uint32_t thread=7;
    bool valid=true,failed=false;
    unsigned submitted=0,completed=0,cutoff=0,ends=0,flushes=0,polls=0;
    uint64_t now=0;
    std::vector<std::string> events;
    bool Valid() const {return valid;}
    const void* Context() const {return context;}
    const void* Query() const {return query;}
    uint32_t Thread() const {return thread;}
    void Submit(const char* name){events.push_back(name);++submitted;}
    void End(){CHECK(valid);events.push_back("End");++ends;cutoff=submitted;}
    void Flush(){CHECK(valid);events.push_back("Flush");++flushes;}
    Poll GetData(){
        CHECK(valid);events.push_back("GetData");++polls;
        return failed?Poll::Failed:completed>=cutoff?Poll::Ready:Poll::Pending;
    }
    uint64_t Now() const {return now;}
    void PauseThread(){events.push_back("Yield");now+=1001;}
    void Release(){valid=false;events.push_back("Release-old-query");}
};
static bool Draw(Fence& fence,Ops& ops,const char* eye) {
    if(!fence.CanRead(ops)) return false;
    ops.Submit(eye);ops.Submit("unbind-shared-SRV");return fence.SealRead(ops);
}
static void NoReaderOrFailedDraw() {
    Fence fence;Ops ops;
    CHECK(fence.Wait(true,ops));CHECK(ops.events.empty());
    CHECK(fence.CanRead(ops)); // e.g. CB Map then fails before the Draw
    CHECK(fence.Wait(true,ops));CHECK(ops.events.empty());
    ops.valid=false;CHECK(!fence.CanRead(ops) && !fence.Wait(true,ops));
}
static void LastConsumerExcludesLateUnrelatedWork() {
    Fence fence;Ops ops;
    CHECK(Draw(fence,ops,"left-shared-read"));
    CHECK(fence.pending && ops.ends==1 && ops.cutoff==2);
    ops.Submit("late-NIS-eye-cache");ops.Submit("late-XR-eye-copy");
    ops.completed=2; // shared read complete; unrelated later commands still queued
    CHECK(fence.Wait(true,ops));
    CHECK(!fence.pending && ops.completed<ops.submitted && ops.ends==1);
    CHECK(ops.events[2]=="End" && ops.events[3]=="late-NIS-eye-cache");
    CHECK(ops.events[5]=="Flush" && ops.events[6]=="GetData");
    CHECK(Draw(fence,ops,"right-shared-read"));
    ops.completed=ops.cutoff;CHECK(fence.Wait(true,ops));
    CHECK(ops.ends==2 && ops.flushes==2);
}
static void LegacyRetainsOverwriteTimeEnd() {
    Fence fence;Ops ops;
    ops.Submit("left-shared-read");ops.Submit("late-XR-copy");ops.completed=1;
    CHECK(!fence.Wait(false,ops)); // original event includes that late copy
    CHECK(ops.ends==1 && ops.cutoff==2 && ops.flushes==1 && ops.polls==3);
    CHECK(ops.events[2]=="End" && ops.events[3]=="Flush");
    ops.completed=2;CHECK(fence.Wait(false,ops));CHECK(ops.ends==2);
    CHECK(!fence.pending && !fence.context); // no new bookkeeping in mode 0
}
static void PollFailureAndTimeoutRetainDependency() {
    for(bool driverFailure:{false,true}) {
        Fence fence;Ops ops;CHECK(Draw(fence,ops,"shared-read"));
        ops.failed=driverFailure;CHECK(!fence.Wait(true,ops));
        CHECK(fence.pending && !fence.CanRead(ops) && ops.ends==1);
        CHECK(!fence.Wait(true,ops));CHECK(ops.ends==1 && fence.pending);
        ops.failed=false;ops.completed=ops.cutoff;
        CHECK(fence.Wait(true,ops));CHECK(!fence.pending && ops.ends==1);
    }
}
static void PendingQueryCannotBeReended() {
    Fence fence;Ops ops;CHECK(Draw(fence,ops,"left"));
    CHECK(!Draw(fence,ops,"unexpected-right"));CHECK(ops.ends==1);
    CHECK(!fence.SealRead(ops) && fence.fault); // fail closed if caller bypasses admission
    ops.completed=ops.cutoff;CHECK(!fence.Wait(true,ops));CHECK(fence.pending);
}
static void ContextQueryAndThreadMustMatch() {
    for(unsigned change=0;change<3;++change) for(bool pending:{false,true}) {
        Fence fence;Ops ops;int other=0;CHECK(Draw(fence,ops,"shared-read"));
        if(!pending){ops.completed=ops.cutoff;CHECK(fence.Wait(true,ops));}
        if(change==0)ops.context=&other;
        if(change==1)ops.query=&other;
        if(change==2)++ops.thread;
        const auto events=ops.events.size();
        CHECK(!fence.CanRead(ops) && !fence.Wait(true,ops));
        CHECK(events==ops.events.size()); // no wrong-thread/context query API call
    }
}
static void ResetAndResizeOwnSeparateLifetimes() {
    Fence fence;Ops old;CHECK(Draw(fence,old,"old-read"));
    CHECK(!fence.Wait(true,old));CHECK(old.valid); // failed resize cannot release/reuse
    old.completed=old.cutoff;CHECK(fence.Wait(true,old));
    old.Release();fence.Reset();Ops resized;
    CHECK(fence.Wait(true,resized) && resized.events.empty());
    CHECK(Draw(fence,resized,"resized-read"));
    // Reset already releases the old query/texture before creating new resources.
    // It may abandon old pending commands, never reuse that old query/texture.
    resized.Release();fence.Reset();Ops reset;
    CHECK(fence.Wait(true,reset) && reset.events.empty());CHECK(Draw(fence,reset,"reset-read"));
}
static void RepeatedEyesHaveOneDependencyEach() {
    Fence fence;Ops ops;
    for(unsigned frame=0;frame<1000;++frame) for(unsigned eye=0;eye<2;++eye) {
        CHECK(fence.Wait(true,ops)); // must precede D3D9 overwrite
        CHECK(!fence.pending);CHECK(Draw(fence,ops,eye?"right":"left"));
        const auto cutoff=ops.cutoff;
        ops.Submit("later-eye-cache-capture-or-XR-copy");
        ops.completed=cutoff; // only the required shared consumer must complete
    }
    CHECK(fence.Wait(true,ops));CHECK(!fence.pending && ops.ends==2000);
}
int main(){
    NoReaderOrFailedDraw();LastConsumerExcludesLateUnrelatedWork();
    LegacyRetainsOverwriteTimeEnd();PollFailureAndTimeoutRetainDependency();
    PendingQueryCannotBeReended();ContextQueryAndThreadMustMatch();
    ResetAndResizeOwnSeparateLifetimes();RepeatedEyesHaveOneDependencyEach();
    std::puts("PASS actual shared-reader Fence helper: last-consumer/legacy order, later unrelated commands, repeated eyes, timeout/error, wrong owner, pending reuse, Reset/resize");
}

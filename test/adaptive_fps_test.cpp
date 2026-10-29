#include "../src/adaptive_fps.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace d2vr_adaptive;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL line%d %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Pacer {
    std::vector<bool> calls;
    Result result=Result::Applied;
    static Result Set(void* object,bool half) {
        auto& p=*static_cast<Pacer*>(object);p.calls.push_back(half);return p.result;
    }
    Runtime Adapter(){return {this,Set};}
};
struct Run {
    Controller control;Pacer pacer;
    double now=10,hz;
    uint64_t id=0;
    explicit Run(double rate,Mode mode=Mode::Auto):hz(rate) {
        CHECK(control.Bind(pacer.Adapter(),now));CHECK(control.Configure(mode,hz,now));
    }
    void Frame(double fps,double cpu=0,double gpu=0) {
        now+=1/fps;control.Frame(++id,now);
        if(cpu || gpu)control.Workload({id,cpu,gpu},now);
    }
    void Seconds(double seconds,double fps,double cpu=0,double gpu=0) {
        const double end=now+seconds;
        while(now<end)Frame(fps,cpu,gpu);
    }
};
int main() {
    {
        Presentation receipt;
        receipt.StereoSubmitted(1);CHECK(!receipt.End(1,true));
        auto owner=receipt.Begin();CHECK(owner!=0);CHECK(!receipt.Begin());
        receipt.StereoSubmitted(10);CHECK(!receipt.End(0,true));
        CHECK(!receipt.End(owner+1,true));CHECK(receipt.End(owner,true)==10);
        CHECK(!receipt.End(owner,true));
        owner=receipt.Begin();receipt.StereoSubmitted(11);CHECK(!receipt.End(owner,false));
        owner=receipt.Begin();CHECK(!receipt.End(owner,true)); // no stale prior stereo
        owner=receipt.Begin();receipt.StereoSubmitted(11);receipt.StereoSubmitted(12);
        CHECK(!receipt.End(owner,true)); // ambiguous source is not one proven app frame
        owner=receipt.Begin();receipt.StereoSubmitted(12);receipt.StereoSubmitted(12);
        CHECK(receipt.End(owner,true)==12);
        for(uint64_t frame=12;frame<1012;++frame) {
            owner=receipt.Begin();CHECK(owner!=0);receipt.StereoSubmitted(frame);
            CHECK(receipt.End(owner,true)==frame);CHECK(!receipt.End(owner,true));
        }
    }
    for(const double hz:{72.,90.,120.,89.9,119.88}) {
        Run r(hz);CHECK(r.control.State().status==Status::Full);
        r.Seconds(6,hz);CHECK(r.pacer.calls.empty());
        r.Frame(2);r.Seconds(3,hz);CHECK(r.pacer.calls.empty()); // isolated hitch
        r.Seconds(5,hz*.7);CHECK(r.control.State().half);
        CHECK(r.pacer.calls.size()==1 && r.pacer.calls[0]);
        CHECK(std::fabs(r.control.State().target-hz/2)<1e-6);
        // Capped FPS alone never recovers. Neither does a fast frame cadence
        // accompanied by heavy CPU or incomplete GPU work.
        r.Seconds(10,hz/2);CHECK(r.control.State().half);
        r.Seconds(5,hz,.02,.001);CHECK(r.control.State().half);
        r.Seconds(5,hz/2,.001,NAN);CHECK(r.control.State().half);
        r.Seconds(5,hz/2,.001,0);CHECK(r.control.State().half);
        // Omitted workload frames break sustained headroom coverage.
        for(unsigned i=0;i<250;++i){r.Frame(hz/2,.001,.001);r.Frame(hz/2);}
        CHECK(r.control.State().half);
        // A future, unsubmitted work identity must not block subsequent valid
        // samples forever, nor authorize recovery by itself.
        r.control.Workload({UINT64_MAX,.001,.001},r.now);
        CHECK(r.control.State().half);
        r.Seconds(4,hz/2,.001,.001);CHECK(!r.control.State().half);
        CHECK(r.pacer.calls.size()==2 && !r.pacer.calls[1]);
        CHECK(r.control.State().status==Status::Full);
        r.Seconds(1.5,hz*.5);CHECK(!r.control.State().half); // residence/hysteresis
    }
    for(const double threshold:{60.,80.,100.}) {
        Run r(threshold==60?72:threshold==80?90:120);
        r.Seconds(12,threshold);CHECK(r.pacer.calls.empty()); // exact boundary
        r.Seconds(5,threshold-2);CHECK(r.control.State().half);
    }
    { // Duplicate frame/eye/compositor notifications cannot inflate app FPS.
        Run r(90);
        for(unsigned i=0;i<300;++i) {
            r.Frame(60);
            for(unsigned j=0;j<20;++j)r.control.Frame(r.id,r.now);
        }
        CHECK(r.control.State().half);CHECK(std::fabs(r.control.State().fps-60)<1e-6);
    }
    { // Backwards/restarted identities and invalid clocks discard streaks.
        Run r(90);r.Seconds(2,45);r.control.Frame(0,r.now);
        r.Seconds(1,45);CHECK(!r.control.State().half);
        r.control.Frame(1,r.now);r.control.Frame(++r.id,NAN);
        r.Seconds(1,45);CHECK(!r.control.State().half);
        r.control.Frame(++r.id,r.now-1);r.Seconds(1,45);CHECK(!r.control.State().half);
    }
    {
        Run r(90,Mode::Half);CHECK(r.control.State().half);CHECK(r.pacer.calls.size()==1);
        r.Seconds(7,45,.001,.001);CHECK(r.control.State().half); // manual mode stays half
        CHECK(r.control.Configure(Mode::Auto,90,r.now));CHECK(r.control.State().half);
        CHECK(r.pacer.calls.size()==1); // preserve existing pacing on HALF -> AUTO
        r.Seconds(4,45,.001,.001);CHECK(!r.control.State().half);
        CHECK(r.control.Configure(Mode::Half,90,r.now));CHECK(r.control.State().half);
        CHECK(r.control.Configure(Mode::Off,90,r.now));CHECK(!r.control.State().half);
        CHECK(r.control.State().status==Status::Off);
    }
    {
        Run r(90,Mode::Half);r.pacer.result=Result::Failed;
        CHECK(!r.control.Configure(Mode::Off,90,r.now));CHECK(r.control.State().half);
        CHECK(r.control.State().status==Status::RuntimeError);
        Pacer next;CHECK(!r.control.Bind(next.Adapter(),r.now));CHECK(next.calls.empty());
        CHECK(r.control.State().half);r.pacer.result=Result::Applied;
        CHECK(r.control.Configure(Mode::Off,90,r.now));CHECK(!r.control.State().half);
        CHECK(r.control.Bind(next.Adapter(),r.now));
        CHECK(r.control.Configure(Mode::Half,120,r.now));CHECK(next.calls.size()==1);
        CHECK(std::fabs(r.control.State().target-60)<1e-6);
        CHECK(r.control.Configure(Mode::Auto,72,r.now));CHECK(!r.control.State().half);
        CHECK(next.calls.size()==2 && !next.calls.back());
    }
    {
        Run r(90);r.pacer.result=Result::Failed;r.Seconds(5,45);
        CHECK(r.control.State().status==Status::RuntimeError);CHECK(r.pacer.calls.size()==1);
        for(unsigned i=0;i<20;++i) {
            CHECK(!r.control.Configure(Mode::Auto,90,r.now));r.Seconds(.2,45);
            CHECK(r.control.State().status==Status::RuntimeError);CHECK(r.pacer.calls.size()==1);
        }
    }
    {
        Run r(90);r.pacer.result=Result::Unsupported;r.Seconds(10,45);
        CHECK(r.pacer.calls.size()==1);CHECK(!r.control.State().half);
        CHECK(r.control.State().status==Status::Unsupported);
        r.Seconds(10,45);CHECK(r.pacer.calls.size()==1); // no repeated rejected calls
        r.control.SessionEnded();CHECK(!r.control.State().half);
        CHECK(r.control.State().status==Status::UnknownRefresh);
        CHECK(r.control.Configure(Mode::Half,90,r.now));
        CHECK(r.control.State().status==Status::Unsupported);
    }
    {
        Controller c;
        CHECK(c.Configure(Mode::Half,90,1));CHECK(c.State().status==Status::Unsupported);
        c.Frame(1,1);c.Frame(2,2);CHECK(!c.State().half);
        Pacer p;CHECK(c.Bind(p.Adapter(),3));
        CHECK(c.Configure(Mode::Auto,0,3));CHECK(c.State().status==Status::UnknownRefresh);
        CHECK(c.Configure(Mode::Half,NAN,4));CHECK(c.State().status==Status::UnknownRefresh);
        CHECK(c.Configure(Mode::Auto,144,5));CHECK(c.State().status==Status::UnsupportedRefresh);
        CHECK(!c.Configure(Mode(7),90,6));CHECK(!c.Configure(Mode::Off,90,NAN));
    }
    {
        Run r(90,Mode::Half);r.control.Configure(Mode::Auto,90,r.now);
        for(unsigned i=0;i<250;++i) {
            r.Frame(45);r.control.Workload({r.id,.001,.001},r.now+1);
        }
        CHECK(r.control.State().half); // stale work is not recovery evidence
        r.Seconds(4,45,.001,.001);CHECK(!r.control.State().half);
    }
    std::printf("PASS x86 adaptive FPS policy: %u checks; supported-rate thresholds, unique frames, sustained entry, workload-only recovery, missing/stale samples, mode/rate/session changes, and rejected runtime transitions. Real runtime pacing and HMD validation NOT RUN.\n",checks);
}

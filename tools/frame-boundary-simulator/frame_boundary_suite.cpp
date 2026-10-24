// Offline frame-boundary validation: synthetic traces, rc20 regression, pacing, mutations.
#include "../../src/frame_boundary.h"
#include "../../src/capture_present_witness.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

static uint64_t g_assertions = 0;
static uint64_t g_failures = 0;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        ++g_assertions;                                                                            \
        if (!(cond)) {                                                                             \
            ++g_failures;                                                                          \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                   \
        }                                                                                          \
    } while (0)

static d2vr_frame::Event Ev(d2vr_frame::EventType t, uint64_t ms, int depth = 0, uint32_t rec = 0) {
    d2vr_frame::Event e{};
    e.type = t;
    e.timestampMs = ms;
    e.sceneDepthAfter = depth;
    e.recordCount = rec;
    return e;
}

static uint64_t ReplayVrFrames(const std::vector<d2vr_frame::Event>& trace, d2vr_frame::Policy p) {
    d2vr_frame::BoundaryMachine m(p);
    uint64_t vr = 0;
    for (const auto& e : trace) {
        const auto d = m.OnEvent(e);
        if (d.requestVrFrame) ++vr;
    }
    return vr;
}

static void test_rc20_infinite_loading_regression() {
    // Simulates Darkness II loading: ~90 EndScene/s, tiny record stream, no Present for 5s.
    std::vector<d2vr_frame::Event> trace;
    for (uint64_t t = 0; t < 5000; t += 11) {
        trace.push_back(Ev(d2vr_frame::EventType::BeginScene, t));
        trace.push_back(Ev(d2vr_frame::EventType::EndScene, t, 0, 2));
    }
    const uint64_t legacy = ReplayVrFrames(trace, d2vr_frame::Policy::LegacyRc20b);
    const uint64_t present = ReplayVrFrames(trace, d2vr_frame::Policy::PresentPrimary);
    const uint64_t traceOnly = ReplayVrFrames(trace, d2vr_frame::Policy::TraceOnly);
    CHECK(legacy > 400); // ~111 Hz cap from 9 ms debounce over 5s
    CHECK(present == 0);
    CHECK(traceOnly == 0);
    // Debounce math: 9 ms => max ~111.11 Hz, NOT 1 Hz.
    CHECK(std::fabs(d2vr_frame::MaxVrFrameHzFromLegacyDebounce() - (1000.0 / 9.0)) < 0.01);
}

static void test_present_primary_one_per_present() {
    d2vr_frame::BoundaryMachine m(d2vr_frame::Policy::PresentPrimary);
    m.OnEvent(Ev(d2vr_frame::EventType::BeginScene, 0));
    auto d = m.OnEvent(Ev(d2vr_frame::EventType::EndScene, 1, 0));
    CHECK(!d.requestVrFrame);
    d = m.OnEvent(Ev(d2vr_frame::EventType::SwapChainPresent, 16));
    CHECK(d.requestVrFrame);
    d = m.OnEvent(Ev(d2vr_frame::EventType::EndScene, 17, 0));
    CHECK(!d.requestVrFrame);
    d = m.OnEvent(Ev(d2vr_frame::EventType::DevicePresent, 33));
    CHECK(d.requestVrFrame);
    CHECK(m.Stats().vrFrameRequests == 2);
}

static void test_nested_endscene() {
    d2vr_frame::BoundaryMachine m(d2vr_frame::Policy::LegacyRc20b);
    m.OnEvent(Ev(d2vr_frame::EventType::BeginScene, 0));
    m.OnEvent(Ev(d2vr_frame::EventType::BeginScene, 0));
    auto d = m.OnEvent(Ev(d2vr_frame::EventType::EndScene, 0, 1));
    CHECK(!d.requestVrFrame);
    d = m.OnEvent(Ev(d2vr_frame::EventType::EndScene, 10, 0));
    CHECK(d.requestVrFrame);
}

static void test_mutation_detection() {
    // Duplicate EndScene boundaries must not 10x XR under PresentPrimary.
    std::vector<d2vr_frame::Event> burst;
    for (int i = 0; i < 100; ++i) {
        burst.push_back(Ev(d2vr_frame::EventType::BeginScene, i));
        burst.push_back(Ev(d2vr_frame::EventType::EndScene, i, 0, 800));
    }
    burst.push_back(Ev(d2vr_frame::EventType::SwapChainPresent, 100));
    const uint64_t vr = ReplayVrFrames(burst, d2vr_frame::Policy::PresentPrimary);
    CHECK(vr == 1);
    const uint64_t legacy = ReplayVrFrames(burst, d2vr_frame::Policy::LegacyRc20b);
    CHECK(legacy > vr * 5); // mutation: legacy EndScene path floods vs PresentPrimary
}

static std::vector<d2vr_frame::Event> RandomTrace(std::mt19937& rng, int maxMs) {
    std::vector<d2vr_frame::Event> t;
    int depth = 0;
    int ms = 0;
    std::uniform_int_distribution<int> step(1, 20);
    std::uniform_int_distribution<int> kind(0, 8);
    std::uniform_int_distribution<int> rec(0, 2000);
    while (ms < maxMs) {
        ms += step(rng);
        switch (kind(rng)) {
            case 0:
            case 1:
                t.push_back(Ev(d2vr_frame::EventType::BeginScene, ms));
                ++depth;
                break;
            case 2:
            case 3:
                if (depth > 0) {
                    --depth;
                    t.push_back(Ev(d2vr_frame::EventType::EndScene, ms, depth, (uint32_t)rec(rng)));
                }
                break;
            case 4:
                t.push_back(Ev(d2vr_frame::EventType::DevicePresent, ms));
                break;
            case 5:
                t.push_back(Ev(d2vr_frame::EventType::PresentEx, ms));
                break;
            case 6:
                t.push_back(Ev(d2vr_frame::EventType::SwapChainPresent, ms));
                break;
            case 7:
                t.push_back(Ev(d2vr_frame::EventType::Reset, ms));
                depth = 0;
                break;
            default:
                break;
        }
    }
    return t;
}

static void test_synthetic_traces(uint64_t count, uint32_t seed) {
    std::mt19937 rng(seed);
    for (uint64_t i = 0; i < count; ++i) {
        const auto trace = RandomTrace(rng, 500 + (int)(i % 200));
        d2vr_frame::BoundaryMachine a(d2vr_frame::Policy::PresentPrimary);
        d2vr_frame::BoundaryMachine b(d2vr_frame::Policy::PresentPrimary);
        for (const auto& e : trace) {
            const auto da = a.OnEvent(e);
            const auto db = b.OnEvent(e);
            CHECK(da.requestVrFrame == db.requestVrFrame);
        }
        // PresentPrimary: never more than one VrFrame per consecutive Present without Reset.
        uint64_t vr = a.Stats().vrFrameRequests;
        uint64_t presents = a.Stats().devicePresent + a.Stats().presentEx + a.Stats().swapChainPresent;
        CHECK(vr <= presents);
    }
}

static void test_timing_simulations(uint64_t count, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> gameHz(30.0, 120.0);
    std::uniform_real_distribution<double> esPer(1.0, 100.0);
    for (uint64_t i = 0; i < count; ++i) {
        d2vr_frame::PaceScenario s{};
        s.gameHz = gameHz(rng);
        s.endScenesPerGameFrame = esPer(rng);
        s.durationMs = 1000;
        s.policy = d2vr_frame::Policy::LegacyRc20b;
        const auto r = d2vr_frame::SimulatePace(s);
        CHECK(r.vrFrameHz <= d2vr_frame::MaxVrFrameHzFromLegacyDebounce() + 1.0);
        s.policy = d2vr_frame::Policy::PresentPrimary;
        const auto r2 = d2vr_frame::SimulatePace(s);
        CHECK(r2.vrFrameRequests == 0);
    }
}

static void test_loading_then_present() {
    std::vector<d2vr_frame::Event> t;
    for (uint64_t ms = 0; ms < 3000; ms += 11) {
        t.push_back(Ev(d2vr_frame::EventType::BeginScene, ms));
        t.push_back(Ev(d2vr_frame::EventType::EndScene, ms, 0, 4));
    }
    t.push_back(Ev(d2vr_frame::EventType::SwapChainPresent, 3000));
    for (uint64_t ms = 3011; ms < 3100; ms += 11) {
        t.push_back(Ev(d2vr_frame::EventType::BeginScene, ms));
        t.push_back(Ev(d2vr_frame::EventType::EndScene, ms, 0, 900));
    }
    t.push_back(Ev(d2vr_frame::EventType::SwapChainPresent, 3100));
    const uint64_t vr = ReplayVrFrames(t, d2vr_frame::Policy::PresentPrimary);
    CHECK(vr == 2);
}

static void test_capture_present_witness() {
    using d2vr_capture::PresentWitness;
    PresentWitness w;
    CHECK(!w.Matches(0,0,0,1));
    const char* invalid[]={"EndScene@42(fallback)","trace-only","reset-active","notPresent",nullptr};
    for (const char* bad : invalid)
        CHECK(!w.Enter(bad,0x100,0x200,1,3));
    CHECK(!w.Enter("DevicePresent",0,0x200,1,3));
    CHECK(!w.Enter("DevicePresent",0x100,0x200,0,3));
    for (const char* kind : {"DevicePresent","PresentEx","SwapChainPresent"}) {
        CHECK(w.Enter(kind,0x100,0x200,1,3));
        const auto serial=w.serial, epoch=w.epoch;
        CHECK(w.Matches(serial,epoch,3,1));
        CHECK(!w.Matches(serial,epoch,3,2)); // other thread
        CHECK(!w.Matches(serial,epoch,4,1)); // other frame
        CHECK(!w.Enter(kind,0x100,0x200,1,3)); // nested call
        w.Leave();
        CHECK(!w.Matches(serial,epoch,3,1)); // historical Present is insufficient
        CHECK(w.Enter(kind,0x100,0x200,1,4));
        CHECK(!w.Matches(serial,epoch,4,1)); // prior call, even on same thread
        w.Reset();
        CHECK(!w.Matches(w.serial,w.epoch,4,1));
        w.ResetResult(false);
        CHECK(!w.Enter(kind,0x100,0x200,1,5));
        w.ResetResult(true);
        CHECK(w.Enter(kind,0x100,0x200,1,5));
        CHECK(!w.Matches(w.serial,epoch,5,1)); // prior reset generation
        w.Leave();
    }
    // Exercise the actual two-phase schedule, not just an all-in-one callback:
    // previous Present tail arms -> live draw recording -> next Present closes.
    CHECK(w.Enter("DevicePresent",0x100,0x200,1,20));
    auto seq=w.serial+1, epoch=w.epoch;
    CHECK(w.InCaptureWindow(seq,epoch,21,1,20));
    w.Leave();
    CHECK(w.InCaptureWindow(seq,epoch,21,1,20)); // live eye between Present calls
    CHECK(!w.InCaptureWindow(seq,epoch,21,2,20));
    CHECK(!w.InCaptureWindow(seq,epoch,21,1,21)); // EndScene fallback isn't closure
    CHECK(w.Enter("DevicePresent",0x100,0x200,1,21));
    CHECK(w.InCaptureWindow(seq,epoch,21,1,21)); // source, replay, submission
    CHECK(w.Matches(seq,epoch,21,1)); // original HRESULT may now finalize receipt
    w.Leave();
    CHECK(!w.InCaptureWindow(seq,epoch,21,1,21)); // cannot reuse a closed call
    CHECK(w.Enter("PresentEx",0x100,0x200,1,22));
    seq=w.serial+1;
    w.Leave();
    w.Reset(); w.ResetResult(true);
    CHECK(!w.InCaptureWindow(seq,epoch,23,1,22)); // reset during live recording
    // The real boundary policy and capture gate must agree. EndScene never
    // produces a witness, including the legacy policy's fallback requests.
    for (auto policy : {d2vr_frame::Policy::PresentPrimary,d2vr_frame::Policy::LegacyRc20b,d2vr_frame::Policy::TraceOnly}) {
        d2vr_frame::BoundaryMachine machine(policy);
        for (auto event : {d2vr_frame::EventType::EndScene,d2vr_frame::EventType::DevicePresent,
                          d2vr_frame::EventType::Reset,d2vr_frame::EventType::PresentEx,
                          d2vr_frame::EventType::ResetSucceeded,d2vr_frame::EventType::SwapChainPresent}) {
            auto d=machine.OnEvent(Ev(event,100));
            bool entered=d.requestVrFrame && w.Enter(d.reason,0x100,0x200,1,6);
            CHECK(entered==(d.requestVrFrame && d2vr_capture::IsPresent(d.reason)));
            w.Leave();
        }
    }
}

int main() {
    test_capture_present_witness();
    // A failed Reset must suppress submissions until a successful retry;
    // the old machine latched inReset forever, even after successful Reset.
    for (const auto reset : {d2vr_frame::EventType::Reset, d2vr_frame::EventType::ResetEx}) {
        d2vr_frame::BoundaryMachine m(d2vr_frame::Policy::PresentPrimary);
        CHECK(m.OnEvent(Ev(d2vr_frame::EventType::SwapChainPresent, 1)).requestVrFrame);
        m.OnEvent(Ev(reset, 2));
        CHECK(!m.OnEvent(Ev(d2vr_frame::EventType::SwapChainPresent, 3)).requestVrFrame);
        m.OnEvent(Ev(reset, 4));
        m.OnEvent(Ev(d2vr_frame::EventType::ResetSucceeded, 5));
        CHECK(!m.OnEvent(Ev(d2vr_frame::EventType::EndScene, 6)).requestVrFrame);
        CHECK(m.OnEvent(Ev(d2vr_frame::EventType::SwapChainPresent, 7)).requestVrFrame);
    }
    test_rc20_infinite_loading_regression();
    test_present_primary_one_per_present();
    test_nested_endscene();
    test_mutation_detection();
    test_loading_then_present();
    test_synthetic_traces(50000, 0xD2E00001u);
    test_timing_simulations(25000, 0xD2E00002u);

    std::printf("frame_boundary_suite: assertions=%llu failures=%llu\n", g_assertions, g_failures);
    if (g_failures) return 1;
    std::printf("PASS synthetic=50000 timing=25000\n");
    return 0;
}

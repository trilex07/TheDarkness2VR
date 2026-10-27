#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../src/render_command_worker.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <memory>
#include <stdexcept>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::printf("FAIL %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
struct Event {
    HANDLE handle = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Event() { CHECK(handle); }
    ~Event() { CloseHandle(handle); }
    void Set() { CHECK(SetEvent(handle)); }
    void Wait() { CHECK(WaitForSingleObject(handle, 5000) == WAIT_OBJECT_0); }
};
struct Probe {
    DWORD created = 0, destroyed = 0;
    std::vector<unsigned> draws;
    unsigned calls = 0;
};
static Probe* probe;
struct Command {
    enum Op { Upload, Draw, Gate, Fail, Throw } op;
    std::vector<unsigned> bytes; // owned snapshot, never an engine pointer
    std::shared_ptr<Event> entered, release;
};
struct Backend {
    DWORD owner = GetCurrentThreadId();
    std::vector<unsigned> resource;
    Backend() { probe->created = owner; }
    ~Backend() { probe->destroyed = GetCurrentThreadId(); }
    bool Execute(const Command& c) {
        CHECK(GetCurrentThreadId() == owner);
        ++probe->calls;
        switch (c.op) {
        case Command::Upload: resource = c.bytes; break;
        case Command::Draw: CHECK(!resource.empty()); probe->draws.push_back(resource[0]); break;
        case Command::Gate: c.entered->Set(); c.release->Wait(); break;
        case Command::Fail: return false;
        case Command::Throw: throw std::runtime_error("simulated backend failure");
        }
        return true;
    }
};
using Queue = d2vr_render_worker::Queue<Backend, Command, 2>;
static void OwnershipAndParallelProgress() {
    Probe p; probe = &p;
    auto entered = std::make_shared<Event>(), release = std::make_shared<Event>();
    {
        Queue q;
        CHECK(q.Submit({Command::Gate, {}, entered, release}).accepted);
        entered->Wait();
        Command upload{Command::Upload, {7}};
        CHECK(q.Submit(upload).accepted);
        upload.bytes[0] = 99; // producer can immediately reuse its input
        auto draw = q.Submit({Command::Draw});
        CHECK(draw.accepted); // producer progresses while worker is occupied
        CHECK(p.calls == 1);
        release->Set();
        auto done = q.Wait(draw.ticket);
        CHECK(done.complete && !done.firstFailure);
        CHECK(p.draws == std::vector<unsigned>{7});
        CHECK(!q.Wait(draw.ticket + 1).complete);
        CHECK(!q.Wait(0).complete);
    }
    CHECK(p.created != GetCurrentThreadId() && p.created == p.destroyed);
    std::puts("PASS owned payload, parallel producer progress, worker lifetime, invalid tickets");
}
static void BackpressureAndResourceVersions() {
    Probe p; probe = &p;
    auto entered = std::make_shared<Event>(), release = std::make_shared<Event>();
    Queue q;
    CHECK(q.Submit({Command::Gate, {}, entered, release}).accepted);
    entered->Wait();
    CHECK(q.Submit({Command::Upload, {1}}).accepted);
    CHECK(q.Submit({Command::Draw}).accepted);
    Event submitting;
    auto future = std::async(std::launch::async, [&] {
        submitting.Set(); return q.Submit({Command::Upload, {2}});
    });
    submitting.Wait();
    CHECK(future.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
    release->Set(); CHECK(future.get().accepted);
    auto last = q.Submit({Command::Draw});
    CHECK(q.Wait(last.ticket).complete);
    CHECK((p.draws == std::vector<unsigned>{1, 2}));
    std::puts("PASS bounded backpressure, ordered draw/update/draw resource versions");
}
static void FailureIsNotSuccess(bool throwing) {
    Probe p; probe = &p;
    Queue q;
    auto r = q.Submit({throwing ? Command::Throw : Command::Fail});
    CHECK(r.accepted);
    auto done = q.Wait(r.ticket);
    CHECK(done.complete && done.firstFailure == r.ticket);
    CHECK(!q.Submit({Command::Upload, {9}}).accepted);
    CHECK(q.Drain().firstFailure == r.ticket);
    std::puts("PASS deferred failure observed at barrier; further submission refused");
}
struct BrokenBackend {
    BrokenBackend() { throw std::runtime_error("initialization failed"); }
    bool Execute(const Command&) { return true; }
};
static void InitializationFailure() {
    d2vr_render_worker::Queue<BrokenBackend, Command, 2> q;
    const auto submitted = q.Submit({Command::Draw});
    if (submitted.accepted) CHECK(!q.Wait(submitted.ticket).complete);
    CHECK(q.Drain().firstFailure != 0);
    CHECK(!q.Submit({Command::Draw}).accepted);
    std::puts("PASS failed initialization wakes producer and completion waiters");
}
static void DrainAndStop() {
    Probe p; probe = &p;
    Queue q;
    CHECK(q.Drain().complete);
    for (unsigned i = 0; i < 10000; ++i) {
        CHECK(q.Submit({Command::Upload, {i}}).accepted);
        CHECK(q.Submit({Command::Draw}).accepted);
    }
    q.Stop(); // accepted work finishes before destruction, no abandoned pointers
    CHECK(p.draws.size() == 10000);
    for (unsigned i = 0; i < 10000; ++i) CHECK(p.draws[i] == i);
    CHECK(p.created == p.destroyed);
    CHECK(!q.Submit({Command::Draw}).accepted);
    q.Stop();
    std::puts("PASS 20000 ordered commands, drain on stop, repeated stop, closed submission");
}
int main() {
    static_assert(sizeof(void*) == 4, "run the target x86 regression");
    OwnershipAndParallelProgress(); BackpressureAndResourceVersions();
    FailureIsNotSuccess(false); FailureIsNotSuccess(true);
    InitializationFailure(); DrainAndStop();
    std::puts("PASS render worker contract; mock backend only, no FPS/runtime claim");
}

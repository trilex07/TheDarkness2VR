#pragma once
#include <cstdint>
namespace d2vr_input {
// Cache one observed boolean only within one input poll, on its calling thread.
// No native pointer, reference or lifetime admission is retained. A changed
// owner/context key forces a fresh observation even inside the same poll.
class PollBoolSnapshot {
    inline static thread_local PollBoolSnapshot* current_=nullptr;
    PollBoolSnapshot* previous_;
    uint64_t key_=0;
    bool valid_=false,value_=false;
public:
    PollBoolSnapshot():previous_(current_){current_=this;}
    ~PollBoolSnapshot(){current_=previous_;}
    PollBoolSnapshot(const PollBoolSnapshot&)=delete;
    PollBoolSnapshot& operator=(const PollBoolSnapshot&)=delete;
    static bool Active(){return current_!=nullptr;}
    template<class Observe> static bool Read(uint64_t key,Observe observe) {
        auto* scope=current_;
        if(!scope)return observe();
        if(!scope->valid_ || scope->key_!=key) {
            scope->value_=observe();scope->key_=key;scope->valid_=true;
        }
        return scope->value_;
    }
};
// Render-path variant: outside an input poll, reuse one observation per
// presented frame on the calling thread. Per-draw callers (HideMenuFirstPerson,
// NativeCameraUiScale) otherwise re-walked the native pause graph with up to
// 18 ReadProcessMemory syscalls per draw (WPR 2026-11-05: ~12% of render
// thread). A changed owner/context key, a new frame id, or an observation
// older than maxAgeMs forces a fresh read, so a stalled frame counter cannot
// pin a stale value. Inside a PollBoolSnapshot scope, poll semantics win.
class FrameBoolSnapshot {
    struct Entry { uint64_t key=0,frame=0,time=0; bool valid=false,value=false; };
    inline static thread_local Entry entry_{};
public:
    template<class Observe> static bool Read(uint64_t key,uint64_t frame,uint64_t nowMs,
                                             uint64_t maxAgeMs,Observe observe) {
        if(PollBoolSnapshot::Active())return PollBoolSnapshot::Read(key,observe);
        auto& e=entry_;
        if(!e.valid || e.key!=key || e.frame!=frame || nowMs<e.time || nowMs-e.time>maxAgeMs) {
            e.value=observe();
            e.key=key;e.frame=frame;e.time=nowMs;e.valid=true;
        }
        return e.value;
    }
    static void Reset(){entry_=Entry{};}
};
}

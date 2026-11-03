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
    template<class Observe> static bool Read(uint64_t key,Observe observe) {
        auto* scope=current_;
        if(!scope)return observe();
        if(!scope->valid_ || scope->key_!=key) {
            scope->value_=observe();scope->key_=key;scope->valid_=true;
        }
        return scope->value_;
    }
};
}

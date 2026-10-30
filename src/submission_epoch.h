#pragma once
#include <atomic>
#include <cstdint>

namespace d2vr_submission {
// Ownership gate for native submission elimination, not a device-state cache.
// Every intercepted mutation must enter this protocol before touching native
// state. A ticket can certify a successful write only if no external/recursive
// mutation intervened. State values remain thread-local in the eventual cache.
class Epoch {
    std::atomic<uint64_t> epoch_{1};
    std::atomic<uint32_t> owner_{0};
    std::atomic<uint32_t> denied_{0};
    std::atomic<bool> requested_{false};
public:
    enum Denial : uint32_t { ForeignThread=1, OpaqueStateBlock=2, InterfaceEscape=4,
                             DeviceReplaced=8, UncontrolledMutation=16 };
    struct Ticket {
        uint64_t epoch=0;
        explicit operator bool()const{return epoch!=0;}
    };
    // Configuration is fixed before publishing device hooks. Resetting a D3D9
    // device does NOT revoke outstanding StateBlocks or escaped interfaces.
    void Configure(bool requested){requested_.store(requested,std::memory_order_release);Invalidate();}
    void Invalidate(){epoch_.fetch_add(1,std::memory_order_acq_rel);}
    void Deny(Denial reason){denied_.fetch_or(reason,std::memory_order_acq_rel);Invalidate();}
    uint32_t Reasons()const{return denied_.load(std::memory_order_acquire);}
    uint64_t Generation()const{return epoch_.load(std::memory_order_acquire);}
    bool Enabled()const{return requested_.load(std::memory_order_acquire) && !Reasons();}
    // Explicit thread identities avoid a Windows query in every setter. Zero
    // is invalid, and callers must use actual runtime thread IDs, never a hash.
    void ObserveMutation(uint32_t thread){
        if(!requested_.load(std::memory_order_acquire))return;
        if(!thread){Deny(UncontrolledMutation);return;}
        uint32_t expected=0;
        owner_.compare_exchange_strong(expected,thread,std::memory_order_acq_rel);
        if(owner_.load(std::memory_order_acquire)!=thread)Deny(ForeignThread);
    }
    Ticket Begin(uint32_t thread) {
        ObserveMutation(thread);
        if(!Enabled() || pauses_.load(std::memory_order_acquire))return {};
        const auto value=Generation();
        return Enabled() && !pauses_.load(std::memory_order_acquire)?Ticket{value}:Ticket{};
    }
    bool Current(Ticket ticket)const {
        return ticket && Enabled() && ticket.epoch==Generation();
    }
    // Pause surrounds all raw renderer/replay mutations that bypass live hooks.
    // Both edges invalidate. A nested operation can never certify a cache entry
    // while native code is restoring state outside the intercepted setters.
    class Bypass {
        Epoch& gate_;
    public:
        explicit Bypass(Epoch& gate):gate_(gate){gate_.Pause();}
        Bypass(const Bypass&)=delete;
        ~Bypass(){gate_.Resume();}
    };
private:
    std::atomic<unsigned> pauses_{0};
    void Pause(){pauses_.fetch_add(1,std::memory_order_acq_rel);Invalidate();}
    void Resume(){Invalidate();pauses_.fetch_sub(1,std::memory_order_acq_rel);}
public:
    bool CanReuse(Ticket ticket)const {
        return !pauses_.load(std::memory_order_acquire) && Current(ticket);
    }
};
}

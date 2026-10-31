#pragma once
#include <windows.h>
#include <atomic>
#include "native_submission_cache.h"
#include "recorder_lock.h"
namespace d2vr_record_sync {
// Sleep only for short CPU bookkeeping. Native calls release every nesting
// level; callbacks can enter the recorder without depending on their caller.
inline Lock mutex; // was SRWLOCK: see recorder_lock.h (AutoBoost release cost)
inline thread_local unsigned depth=0,nativeDepth=0,nativeLeases=0;
inline std::atomic<LONG> held{0},nativeCalls{0};
inline std::atomic<bool> conflict{false},replaying{false};
inline void Acquire(){if(depth++==0){mutex.lock();held.store(1,std::memory_order_relaxed);}}
inline void Release(){if(--depth==0){held.store(0,std::memory_order_relaxed);mutex.unlock();}}
inline bool Callback(){return nativeDepth!=0 || replaying.load(std::memory_order_acquire);}
inline bool Busy(){return nativeDepth!=0 || nativeCalls.load(std::memory_order_acquire)!=0 || replaying.load(std::memory_order_acquire);}
// Hooks may forward while recording is off (notably during replay). They must
// still invalidate a pair whose native state changed outside its command stream.
inline bool NativeMutation(){
    if(d2vr_submission::requested){
        d2vr_submission::gate.ObserveMutation(GetCurrentThreadId());
        if(Callback() || nativeCalls.load(std::memory_order_acquire)!=0)
            d2vr_submission::gate.Deny(d2vr_submission::Epoch::UncontrolledMutation);
    }
    if(Callback() || nativeCalls.load(std::memory_order_acquire)!=0){conflict.store(true,std::memory_order_release);return false;}
    return true;
}
// Replay catches SEH per command. /EHsc does not unwind native-faulted CPU
// guards; release only this thread's abandoned bookkeeping/leases at that gate.
inline void RecoverReplayException(){
    if(depth){depth=0;held.store(0,std::memory_order_relaxed);mutex.unlock();}
    if(nativeLeases)nativeCalls.fetch_sub(nativeLeases,std::memory_order_release);
    nativeLeases=0;nativeDepth=0;conflict.store(true,std::memory_order_release);
}
class DriverScope {
    unsigned saved_=depth;bool resumed_=false,finished_=false;
public:
    DriverScope(){
        if(saved_){
            ++nativeLeases;
            if(nativeCalls.fetch_add(1,std::memory_order_acq_rel)!=0 || nativeDepth)conflict.store(true,std::memory_order_release);
            depth=0;held.store(0,std::memory_order_relaxed);mutex.unlock();
        }
        ++nativeDepth;
    }
    DriverScope(const DriverScope&)=delete;
    void Resume(){if(saved_ && !resumed_){mutex.lock();depth=saved_;held.store(1,std::memory_order_relaxed);resumed_=true;}}
    void Finish(){if(!finished_){Resume();--nativeDepth;if(saved_){--nativeLeases;nativeCalls.fetch_sub(1,std::memory_order_release);}finished_=true;}}
    ~DriverScope(){Finish();}
};
}

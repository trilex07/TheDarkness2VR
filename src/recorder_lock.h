#pragma once
#include <atomic>
#include <cstdint>
#if defined(_WIN32)
#include <windows.h>
#include <intrin.h>
#else
#include <condition_variable>
#include <mutex>
#include <thread>
#endif

// Recorder mutex without SRW AutoBoost bookkeeping.
//
// Evidence (combat ETW 20261030T205050Z-eab0975b, render thread, 1586 frames):
// RecLockGuard acquire 0.121 ms + release 0.243 ms per frame, of which the
// release-side AutoBoost path RtlAbPostRelease/RtlpAbFindLockEntry alone was
// 0.191 ms/frame. The lock is taken once per recorded D3D9 call, so this cost
// grows with draw/constant volume (outdoor scenes).
//
// Classic three-state mutex (0 free, 1 locked, 2 locked with waiters):
// uncontended lock/unlock is one atomic RMW each, no system call and no
// AutoBoost. Contended waiters BLOCK on a kernel semaphore exactly like an SRW
// waiter would (no unbounded spinning, no priority-inversion livelock), after a
// short bounded pause loop for sub-microsecond holds. Exclusive, non-recursive;
// recursion depth stays in recorder_sync.h as before.
namespace d2vr_record_sync {
class Lock {
    std::atomic<long> state_{0};
#if defined(_WIN32)
    // Decide the wait backend before publishing the lock. Lazy semaphore
    // creation could lose a wake if creation failed during unlock but a waiter
    // subsequently created a new, empty semaphore. Exhaustion selects one SRW
    // backend for this entire lock lifetime; it never mixes the two protocols.
    HANDLE semaphore_=nullptr;
    SRWLOCK fallback_=SRWLOCK_INIT;
    void Wait() {WaitForSingleObject(semaphore_,INFINITE);}
    void Wake() {ReleaseSemaphore(semaphore_,1,nullptr);}
    static void Pause() {_mm_pause();}
#else
    std::mutex m_;std::condition_variable cv_;long tickets_=0;
    void Wait() {std::unique_lock<std::mutex> l(m_);cv_.wait(l,[&]{return tickets_>0;});--tickets_;}
    void Wake() {{std::lock_guard<std::mutex> l(m_);++tickets_;}cv_.notify_one();}
    static void Pause() {std::this_thread::yield();}
#endif
public:
#if defined(_WIN32)
    explicit Lock(bool fallbackOnly=false):semaphore_(fallbackOnly?nullptr:CreateSemaphoreW(nullptr,0,0x7fffffff,nullptr)) {}
    ~Lock(){if(semaphore_)CloseHandle(semaphore_);}
#else
    Lock()=default;
#endif
    Lock(const Lock&)=delete;
    Lock& operator=(const Lock&)=delete;
    bool try_lock() {
#if defined(_WIN32)
        if(!semaphore_)return TryAcquireSRWLockExclusive(&fallback_)!=FALSE;
#endif
        long expected=0;return state_.compare_exchange_strong(expected,1,std::memory_order_acquire);
    }
    void lock() {
#if defined(_WIN32)
        if(!semaphore_){AcquireSRWLockExclusive(&fallback_);return;}
#endif
        long c=0;
        if(state_.compare_exchange_strong(c,1,std::memory_order_acquire)) return;
        for(unsigned spin=0;spin<128;++spin) {           // brief holds: stay on CPU
            Pause();
            c=0;
            if(state_.load(std::memory_order_relaxed)==0 &&
               state_.compare_exchange_strong(c,1,std::memory_order_acquire)) return;
        }
        // Announce a waiter (2) and block until the owner releases.
        while(state_.exchange(2,std::memory_order_acquire)!=0) Wait();
    }
    void unlock() {
#if defined(_WIN32)
        if(!semaphore_){ReleaseSRWLockExclusive(&fallback_);return;}
#endif
        // Holder of 2 must wake one waiter; extra wakes are harmless (the woken
        // thread re-checks with exchange(2) and sleeps again if not free).
        if(state_.exchange(0,std::memory_order_release)==2) Wake();
    }
    bool Held() const {return state_.load(std::memory_order_relaxed)!=0;}
};
}

// Portable stress test for d2vr_record_sync::Lock (recorder_lock.h).
// The Win32 build differs only in Wait()/Wake() (kernel semaphore vs a
// counting condition variable here); the 0/1/2 state machine is shared.
#include "../src/recorder_lock.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
static unsigned long long checks=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(){
    using d2vr_record_sync::Lock;
    {   // uncontended: lock/unlock/try_lock state transitions
        Lock l;CHECK(!l.Held());l.lock();CHECK(l.Held());CHECK(!l.try_lock());
        l.unlock();CHECK(!l.Held());CHECK(l.try_lock());l.unlock();CHECK(!l.Held());
    }
    for(int threads:{2,4,8,16}){  // exclusivity + no lost wakeups under contention
        Lock l;long long counter=0;std::atomic<int> inside{0};std::atomic<bool> violation{false};
        const int iterations=threads>=8?40000:120000;
        std::vector<std::thread> pool;
        for(int t=0;t<threads;++t)pool.emplace_back([&,t]{
            for(int i=0;i<iterations;++i){
                l.lock();
                if(inside.fetch_add(1,std::memory_order_relaxed)!=0)violation=true;
                ++counter;
                if((i+t)%97==0)std::this_thread::yield(); // long hold: forces the blocking path
                inside.fetch_sub(1,std::memory_order_relaxed);
                l.unlock();
            }
        });
        for(auto& th:pool)th.join();
        CHECK(!violation);CHECK(counter==(long long)threads*iterations);CHECK(!l.Held());
    }
    {   // recorder pattern: owner releases around a "driver call" and resumes
        Lock l;std::atomic<int> phase{0};long long shared=0;
        std::thread other([&]{while(phase.load()!=1)std::this_thread::yield();l.lock();shared+=10;l.unlock();phase=2;});
        l.lock();shared=1;l.unlock();       // DriverScope ctor
        phase=1;while(phase.load()!=2)std::this_thread::yield();
        l.lock();CHECK(shared==11);l.unlock(); // Resume()
        other.join();
    }
    {   // a waiter blocked while the owner holds for a long time is woken exactly once
        Lock l;l.lock();std::atomic<bool> got{false};
        std::thread w([&]{l.lock();got=true;l.unlock();});
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        CHECK(!got);l.unlock();w.join();CHECK(got);CHECK(!l.Held());
    }
#if defined(_WIN32)
    { // Semaphore creation failure uses one consistent blocking backend.
        Lock fallback(true);long long value=0;std::vector<std::thread> workers;
        for(unsigned t=0;t<8;++t)workers.emplace_back([&]{
            for(unsigned i=0;i<20000;++i){fallback.lock();++value;fallback.unlock();}
        });
        for(auto& worker:workers)worker.join();
        CHECK(value==160000);CHECK(fallback.try_lock());fallback.unlock();
    }
#endif
    printf("recorder_lock_test: %llu checks passed\n",checks);
}

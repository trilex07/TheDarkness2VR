#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstdarg>
#include <thread>
static void Log(const char*,...) {}
#include "../src/cpu_sync_timing.inl"
int main() {
    using namespace d2vr_cpu_sync;
    timing.Enable(true);
    {Scope s(SharedReader);Sleep(180);}
    const auto idle=timing.entries[SharedReader];
    {Scope s(BoundaryWork);LARGE_INTEGER a{},b{};QueryPerformanceCounter(&a);
        do {for(volatile unsigned i=0;i<10000;++i){} QueryPerformanceCounter(&b);}
        while((b.QuadPart-a.QuadPart)*1000.0/timing.frequency.QuadPart<180);}
    const auto busy=timing.entries[BoundaryWork];
    if(idle.count!=1 || idle.cpuCount!=1 || busy.cpuCount!=1 ||
       idle.sum<150 || busy.sum<150 || busy.cpuTicks<800000 ||
       idle.cpuTicks>=busy.cpuTicks/2) return 1;
    ThreadStamp foreign{};foreign.Read();
    std::thread t([&]{LARGE_INTEGER start{};QueryPerformanceCounter(&start);
        timing.Add(PresentForward,start,&foreign);});t.join();
    if(timing.entries[PresentForward].cpuCount) return 2;
    timing.Enable(false);const auto count=timing.entries[BoundaryWork].count;
    {Scope s(BoundaryWork);}
    if(timing.entries[BoundaryWork].count!=count) return 3;
    printf("PASS: sleep wall %.2fms CPU %.2fms; busy wall %.2fms CPU %.2fms; foreign-thread and disabled scopes excluded\n",
        idle.sum,idle.cpuTicks/10000.0,busy.sum,busy.cpuTicks/10000.0);
}

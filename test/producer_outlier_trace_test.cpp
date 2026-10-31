// Offline check of the protected-only producer outlier trace helper
// (src/cpu_sync_timing.inl): threshold, cap, exact per-call field values and
// per-call overhead of the non-outlier path. CPU model only; no device/GPU claim.
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
static std::vector<std::string> g_lines;
static void Log(const char* fmt,...) {
    char b[1024]; va_list a; va_start(a,fmt); vsnprintf(b,sizeof b,fmt,a); va_end(a); g_lines.emplace_back(b);
}
#include "../src/cpu_sync_timing.inl"
static int failures=0;
#define CHECK(x) do{ if(!(x)){ ++failures; std::printf("FAIL %d %s\n",__LINE__,#x);} }while(0)
using d2vr_cpu_sync::Timing;
static Timing::ProducerCall Call(long long i0,long long i1,long long f0,long long f1) {
    Timing::ProducerCall c; c.timed=true; c.issueHr=S_OK; c.flushHr=S_FALSE;
    c.issueBegin.QuadPart=i0; c.issueEnd.QuadPart=i1; c.flushBegin.QuadPart=f0; c.flushEnd.QuadPart=f1; return c;
}
int main() {
    Timing t; t.enabled=true; t.frequency.QuadPart=10000000; // 10 MHz, as PresentMon logs
    // Below threshold: 1 ms issue + 10 ms flush, gap (history scope) excluded.
    CHECK(!t.ProducerOutlier(Call(0,10000,500000,600000),4,0,1));
    // Gap of 40 ms between calls must NOT count: only issue+flush are measured.
    CHECK(!t.ProducerOutlier(Call(0,1000,400000,401000),4,0,2));
    // Outlier: issue 0.1 ms, flush 42.656 ms.
    g_lines.clear();
    CHECK(t.ProducerOutlier(Call(1000,2000,3000,3000+426560),4,1,55440));
    CHECK(g_lines.size()==1);
    const std::string& l=g_lines.empty()?std::string():g_lines[0];
    CHECK(l.find("mode=4 capture=1 frame=55440")!=std::string::npos);
    CHECK(l.find("issueQpc=1000..2000 issueMs=0.100 issueHr=0x00000000")!=std::string::npos);
    CHECK(l.find("flushQpc=3000..429560 flushMs=42.656 flushHr=0x00000001")!=std::string::npos);
    CHECK(l.find("totalMs=42.756")!=std::string::npos);
    // Issue-only stall (flush never executed) is attributed to Issue.
    Timing::ProducerCall issueOnly=Call(0,200000,0,0); issueOnly.flushHr=E_PENDING;
    CHECK(t.ProducerOutlier(issueOnly,4,0,3));
    // Disabled timing / untimed calls never log.
    Timing off; off.frequency.QuadPart=10000000;
    CHECK(!off.ProducerOutlier(Call(0,1000000,0,0),4,0,4));
    Timing::ProducerCall untimed=Call(0,1000000,0,0); untimed.timed=false;
    CHECK(!t.ProducerOutlier(untimed,4,0,5));
    // Cap: at most 256 events in total.
    unsigned logged=0;
    for(int i=0;i<1000;++i) logged+=t.ProducerOutlier(Call(0,0,0,200000),4,0,i);
    CHECK(logged==256-2);
    CHECK(t.producerOutliers>=256);
    // Overhead of the always-taken, non-outlier path with real QPC reads.
    Timing live; live.enabled=QueryPerformanceFrequency(&live.frequency)!=0;
    const int n=2000000; LARGE_INTEGER a{},b{}; QueryPerformanceCounter(&a);
    size_t before=g_lines.size();
    for(int i=0;i<n;++i) {
        Timing::ProducerCall c; c.timed=true;
        QueryPerformanceCounter(&c.issueBegin); QueryPerformanceCounter(&c.issueEnd);
        QueryPerformanceCounter(&c.flushBegin); QueryPerformanceCounter(&c.flushEnd);
        live.ProducerOutlier(c,4,0,i);
    }
    QueryPerformanceCounter(&b);
    const double ns=(b.QuadPart-a.QuadPart)*1e9/live.frequency.QuadPart/n;
    CHECK(g_lines.size()==before);
    CHECK(ns<2000.0);
    if(failures) { std::printf("FAIL producer outlier trace: %d\n",failures); return 1; }
    std::printf("PASS producer outlier trace: threshold, excluded inter-call gap, exact fields, issue-only, disabled, cap=256; non-outlier overhead %.1f ns/call (4 QPC + check, ~2 calls/frame)\n",ns);
    return 0;
}

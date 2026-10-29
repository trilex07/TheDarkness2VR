#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include "../src/shader_register_history.h"
#include "baseline_register_bank.h"
static unsigned checks=0;
#define CHECK(x) do {++checks;if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t randomState=0x45A6BA12;
static uint32_t Next(){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState;}
template<class A,class B> static void Equal(const A& a,const B& b){
    CHECK(!memcmp(a.valid,b.valid,sizeof(a.valid)));
    CHECK(!memcmp(a.values,b.values,sizeof(a.values)));
}
struct Packet {unsigned first,count;};
static Packet packets[4096];
static volatile uint64_t sink=0;
struct Measurement {double ms;uint64_t checksum;};
template<class Bank,bool invalidate> __declspec(noinline) static Measurement Bench(const float* payload){
    Bank bank;LARGE_INTEGER begin,end,freq;QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&begin);
    uint64_t checksum=0;
    for(unsigned repeat=0;repeat<256;++repeat)for(const auto& packet:packets){
        bank.Write(packet.first,packet.count,payload,packet.count*16);
        checksum+=bank.valid[packet.first/64];
        if(invalidate)bank.Invalidate(packet.first,packet.count);
    }
    QueryPerformanceCounter(&end);sink^=checksum;
    return {double(end.QuadPart-begin.QuadPart)*1000.0/double(freq.QuadPart),checksum};
}
template<bool invalidate> static void Measure(const float* payload){
    double oldTimes[5],newTimes[5];
    for(unsigned trial=0;trial<5;++trial){
        Measurement oldResult,newResult;
        if(trial%2){newResult=Bench<d2vr_shader_registers::Bank,invalidate>(payload);oldResult=Bench<baseline_registers::Bank,invalidate>(payload);}
        else {oldResult=Bench<baseline_registers::Bank,invalidate>(payload);newResult=Bench<d2vr_shader_registers::Bank,invalidate>(payload);}
        CHECK(oldResult.checksum==newResult.checksum);oldTimes[trial]=oldResult.ms;newTimes[trial]=newResult.ms;
    }
    std::sort(oldTimes,oldTimes+5);std::sort(newTimes,newTimes+5);
    printf("Synthetic x86 1048576 %s packets, five-run median: baseline %.3f ms, current %.3f ms\n",invalidate?"write+invalidate":"write-only",oldTimes[2],newTimes[2]);
}
int main(){
    float payload[256*4];for(auto& value:payload){const uint32_t bits=Next();memcpy(&value,&bits,4);}
    for(unsigned pattern=0;pattern<32;++pattern) {
        d2vr_shader_registers::Bank bank;CHECK(bank.Write(0,256,payload,sizeof(payload)));
        for(unsigned word=0;word<4;++word)bank.valid[word]=pattern==0?0:pattern==1?~uint64_t(0):
            pattern==2?0x5555555555555555ull:(uint64_t(Next())<<32)|Next();
        for(unsigned requested:{0u,1u,63u,64u,65u,255u,256u,257u,UINT32_MAX}) {
            bool seen[256]{};unsigned previousEnd=0,runs=0;
            const unsigned limit=std::min(requested,256u);
            bank.ForEachValidRun(requested,[&](unsigned first,const float* values,unsigned count) {
                CHECK(count && first<limit && count<=limit-first);
                CHECK(!runs || first>previousEnd); // maximal disjoint runs
                CHECK(!memcmp(values,payload+first*4,count*16));
                for(unsigned row=first;row<first+count;++row){CHECK(!seen[row]);seen[row]=true;}
                previousEnd=first+count;++runs;
            });
            CHECK(runs<=128);
            for(unsigned row=0;row<256;++row)
                CHECK(seen[row]==(row<limit && (bank.valid[row/64]&(uint64_t(1)<<(row%64)))));
        }
    }
    for(unsigned first=0;first<=256;++first)for(unsigned count=0;count<=256-first;++count){
        d2vr_shader_registers::Bank current;baseline_registers::Bank baseline;
        for(unsigned word=0;word<4;++word)current.valid[word]=baseline.valid[word]=(uint64_t(Next())<<32)|Next();
        CHECK(current.Write(first,count,payload,count*16)==baseline.Write(first,count,payload,count*16));Equal(current,baseline);
        current.Invalidate(first,count);baseline.Invalidate(first,count);Equal(current,baseline);
    }
    d2vr_shader_registers::History current;baseline_registers::History baseline;
    for(unsigned iteration=0;iteration<100000;++iteration){
        unsigned first=Next()%258,count=Next()%258;
        if(iteration%97==0)first=(std::numeric_limits<unsigned>::max)();
        if(iteration%101==0)count=(std::numeric_limits<unsigned>::max)();
        const size_t bytes=iteration%7 ? size_t(count)*16 : 3;
        const void* data=iteration%11 ? payload : nullptr;
        if(iteration%3){CHECK(current.live.Write(first,count,data,bytes)==baseline.live.Write(first,count,data,bytes));}
        else {current.live.Invalidate(first,count);baseline.live.Invalidate(first,count);}
        Equal(current.live,baseline.live);
        if(iteration%13==0){current.BeginRecord();baseline.BeginRecord();Equal(current.begin,baseline.begin);}
        if(iteration%17==0){current.BeginReplay();baseline.BeginReplay();Equal(current.replay,baseline.replay);}
        float a[16]{},b[16]{};const int matrix=int(Next()%260)-2;
        CHECK(current.live.Matrix(matrix,a)==baseline.live.Matrix(matrix,b));CHECK(!memcmp(a,b,sizeof(a)));
        if(iteration%109==0){current.Clear();baseline.Clear();Equal(current.live,baseline.live);}
    }
    // Synthetic API-size mix, not a game frame or an FPS measurement.
    const unsigned sizes[]={1,4,6,17,68,64,256};
    for(auto& packet:packets){packet.count=sizes[Next()%7];packet.first=Next()%(257-packet.count);}
    printf("PASS %u checks: all valid ranges, random prior masks, invalid bounds/bytes/null, matrix/history equivalence\n",checks);
    Measure<false>(payload);Measure<true>(payload);
    return 0;
}

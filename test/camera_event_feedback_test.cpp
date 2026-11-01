#include "../src/camera_event_feedback.h"
#include <map>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
using namespace d2vr_camera_event;
static unsigned checks=0;
static void Check(bool value){++checks;if(!value){std::printf("FAIL %u\n",checks);std::exit(1);}}
int main(int argc,char** argv) {
    const std::map<uint32_t,uint32_t> original={
        {0x10000,0xfdf170},{0x10828,0x20000},{0x20000,0x30000},
        {0x30000,0xfe3ee8},{0x30074,0x40000},{0x40000,0x50000},{0x50000,0x1001630}};
    auto memory=original;
    const auto read=[&](uint32_t address,uint32_t& value){
        const auto it=memory.find(address);if(it==memory.end())return false;value=it->second;return true;};
    Check(Matches(0x10000,0x50000,read));
    for(const auto& entry:original){memory=original;memory.erase(entry.first);Check(!Matches(0x10000,0x50000,read));}
    for(const auto& entry:original){memory=original;memory[entry.first]=3;Check(!Matches(0x10000,0x50000,read));}
    memory=original;memory[0x40000]=0x60000;memory[0x60000]=0x1001630;
    Check(!Matches(0x10000,0x50000,read)); // valid type, different avatar
    memory=original;for(uint32_t bad:{0u,3u,0xfffffffcu})Check(!Matches(bad,0x50000,read));
    memory=original;memory[0x10000]=0x60000;
    Check(!Matches(0x10000,0x50000,read));
    Check(Matches(0x10000,0x50000,read,0x60000));
    memory[0x40000]=0x60000;Check(!Matches(0x10000,0x50000,read,0x60000));
    Sample s{};Check(Amount(s)==0);
    s.impactPosition[0]=.00012f;Check(Amount(s)==0); // captured paused residue
    s.impactPosition[0]=.8f;Check(Amount(s)>.99f);
    s={};s.environmentalPosition[1]=.4f;Check(Amount(s)>.5f);
    s={};s.environmentalRotation[0]=1.196f;Check(Amount(s)==1);
    s.impactPosition[2]=std::numeric_limits<float>::quiet_NaN();Check(Amount(s)==0);
    s={};s.impactRotation[0]=std::numeric_limits<float>::infinity();Check(Amount(s)==0);
    if(argc==2) {
        std::ifstream input(argv[1]);Check(bool(input));std::string line;unsigned rows=0,quiet=0,train=0,impact=0;
        while(std::getline(input,line)) {
            double seconds=0;Sample sample{};std::istringstream row(line);row>>seconds;
            for(auto* v:{sample.impactPosition,sample.impactRotation,sample.environmentalPosition,sample.environmentalRotation})
                for(unsigned i=0;i<3;++i)row>>v[i];
            Check(bool(row));++rows;const auto amount=Amount(sample);
            if(seconds<33 || seconds>61){Check(amount==0);++quiet;}
            if(seconds>=34 && seconds<39 && amount>.05f)++train;
            if(seconds>=41 && seconds<56 && amount>.05f)++impact;
        }
        Check(rows>1000 && quiet>100 && train>100 && impact>100);
        std::printf("Captured event replay PASS rows=%u quiet=%u trainActive=%u impactActive=%u\n",rows,quiet,train,impact);
    }
    std::printf("Camera event feedback PASS checks=%u\n",checks);
}

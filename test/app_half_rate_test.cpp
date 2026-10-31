#include "../src/app_half_rate.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main(){
 d2vr_adaptive::AppHalfRate p;
 assert(p.Delay(10,90,true)==0);p.Started(10);
 assert(std::fabs(p.Delay(10.01,90,true)-(2./90-.01))<1e-9);
 assert(p.Delay(10.03,90,true)==0); // runtime already waited: no double throttling
 assert(p.Delay(10,90,false)==0);assert(p.Delay(10,0,true)==0);
 assert(p.Delay(9,90,true)==0);assert(p.Delay(10,NAN,true)==0);
 for(double hz:{72.,90.,120.,144.}) {
   p.Started(20);assert(std::fabs(p.Delay(20,hz,true)-2/hz)<1e-9);
 }
 p.Reset();assert(p.Delay(20,90,true)==0);
 puts("PASS application half-rate deadlines: physical rates, missed deadlines, runtime pacing, reset, invalid inputs");
}

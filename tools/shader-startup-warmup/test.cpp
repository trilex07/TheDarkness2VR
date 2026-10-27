#include "../../src/startup_shader_warmup.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
using namespace d2vr_startup_shader;
int checks=0;
void check(bool b){++checks;if(!b){std::printf("FAIL %d\n",checks);std::exit(1);}}
struct Ops {unsigned calls=0;unsigned failAt=0;uint64_t clock=0,step=0;uint64_t Now(){return clock;} bool CreateAndRelease(const unsigned char*,uint32_t){++calls;clock+=step;return calls!=failAt;}};
std::vector<unsigned char> bytes(std::vector<uint32_t> w){const auto*p=reinterpret_cast<const unsigned char*>(w.data());return {p,p+w.size()*4};}
int main(int argc,char** argv){
 auto good=bytes({Magic,2,2,0xfffe0300,65535,2,0xffff0300,65535});
 Ops o;auto r=Run(good.data(),good.size(),100,o);check(r.valid&&r.succeeded==2&&o.calls==2);
 for(size_t n=0;n<good.size();++n){Ops x;auto z=Run(good.data(),n,100,x);check(!z.valid&&x.calls==0);}
 auto bad=good;bad.push_back(0);check(!Validate(bad.data(),bad.size()));
 auto overflow=bytes({Magic,1,0xffffffff});check(!Validate(overflow.data(),overflow.size()));
 auto comment=bytes({Magic,1,4,0xfffe0300,0x0001fffe,65535,65535});check(Validate(comment.data(),comment.size()));
 comment.resize(comment.size()-4);check(!Validate(comment.data(),comment.size()));
 auto early=bytes({Magic,1,3,0xfffe0300,65535,65535});check(!Validate(early.data(),early.size()));
 Ops fail;fail.failAt=1;r=Run(good.data(),good.size(),100,fail);check(r.failed==1&&fail.calls==1);
 Ops timed;timed.step=101;r=Run(good.data(),good.size(),100,timed);check(r.timedOut&&timed.calls==1);
 Ops zero;r=Run(good.data(),good.size(),0,zero);check(r.timedOut&&zero.calls==0);
 check(!Validate(nullptr,8));
 if(argc==2){std::ifstream f(argv[1],std::ios::binary);std::vector<unsigned char> data((std::istreambuf_iterator<char>(f)),{});Ops real;r=Run(data.data(),data.size(),100,real);check(r.valid&&r.failed==0&&r.attempted==Word(data.data()+4));std::printf("Private package validated: %u shaders; fake ops, NO GPU execution\n",r.attempted);}
 std::printf("PASS %d checks (bounds, full admission, failure stop, timing, no GPU)\n",checks);
}

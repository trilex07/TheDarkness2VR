#include "../../src/lit_mask_projection.h"
#include "../../src/replay_packet_core.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
static void Check(bool ok,const char* why){if(!ok){std::printf("FAIL %s\n",why);std::exit(1);}}
int main(){
    using namespace d2vr_packet_prepare;
    for(const auto& e:d2vr_lit_mask::kWorldInput){
        Check(d2vr_lit_mask::Admit(e.hash,e.bytes,[&](const char* s){return !std::strcmp(s,e.sha256);}),"pinned identity");
        Check(!d2vr_lit_mask::Admit(e.hash,e.bytes,[](const char*){return false;}),"digest gate");
        Check(!d2vr_lit_mask::Admit(e.hash,e.bytes+4,[](const char*){return true;}),"length gate");
    }
    for(unsigned foreign:{0x9E39DAF1u,0xA7CEBFAFu,0xC97C93B7u,0x91A7C47Cu})
        Check(!d2vr_lit_mask::Admit(foreign,1116,[](const char*){return true;}),"particle/world projector untouched");
    const float bias[16]={.5f,0,0,0,0,-.5f,0,0,0,0,1,0,.5f,.5f,0,1};
    for(float yaw:{-.3f,0.f,.3f})for(float eye:{-.0325f,.0325f}){
        float raw[60]{};for(int i=0;i<60;++i)raw[i]=float(i)*.123f;
        const float wvp[16]={0,0,1,1,0,.713f,0,0,.727f,0,0,0,2,1,30,30};
        std::memcpy(raw,wvp,sizeof(wvp));d2vr_math::Multiply(wvp,bias,raw+32);
        Shader s;s.reflected=true;s.reflectedWvp=s.activeWvp=0;s.reflectedStp=s.activeStp=8;
        Effects effects;Params p;p.effects=&effects;p.projectionFix=1;p.projectionPx=.727f;p.projectionPy=.713f;
        p.fovScaleX=.95f;p.fovScaleY=1.01f;p.transX=eye;p.transY=.015f;p.headGeometry=true;
        const float h[16]={std::cos(yaw),0,std::sin(yaw),0,0,1,0,0,-std::sin(yaw),0,std::cos(yaw),0,.2f,.1f,0,1};
        std::memcpy(p.headClip,h,sizeof(h));float fixed[60],expected[16];
        Check(ApplyShiftToUpload(fixed,raw,0,15,&s,p),"actual packet transform");
        d2vr_math::Multiply(fixed,bias,expected);
        for(int i=0;i<16;++i)Check(std::fabs(fixed[32+i]-expected[i])<.00001f,"mask equals raster times UV bias");
        Check(!std::memcmp(raw+48,fixed+48,12*sizeof(float)),"eye/fog/light constants preserved");
        Check(!std::memcmp(raw+16,fixed+16,16*sizeof(float)),"world/constants preserved");
        Shader old=s;old.reflectedWvp=old.reflectedStp=-1;float broken[60];
        Check(ApplyShiftToUpload(broken,raw,0,15,&old,p),"old fallback raster");
        Check(!std::memcmp(broken+32,raw+32,16*sizeof(float)),"old mask stayed native reproducing mismatch");
    }
    std::puts("PASS LitInstance mask/raster alignment both eyes and head poses; identity gates; light/eye isolation");
}

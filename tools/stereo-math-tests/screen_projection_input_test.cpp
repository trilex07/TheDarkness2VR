#include "../../src/screen_projection_input.h"
#include "../../src/stereo_math.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
static void Check(bool ok,const char* why){if(!ok){std::printf("FAIL %s\n",why);std::exit(1);}}
int main(){
    for(const auto& entry:d2vr_screen_projection::kClipInput) {
        Check(d2vr_screen_projection::WorldInputRegister(entry.hash,entry.bytes,4,
            [&](const char* s){return !std::strcmp(s,entry.sha256);})==-1,"verified particle variant");
        Check(d2vr_screen_projection::WorldInputRegister(entry.hash,entry.bytes,4,
            [](const char*){return false;})==4,"variant digest mismatch");
    }
    const auto digest=[](const char* s){return !std::strcmp(s,"bf3a521ace33c7b509785dede723728b10a23b25d65bf85cab78e4f872beaa48");};
    const auto select=[&](unsigned hash,size_t bytes,int base){return d2vr_screen_projection::WorldInputRegister(hash,bytes,base,digest);};
    Check(select(0x9E39DAF1,1948,4)==-1,"post-clip matrix excluded");
    Check(select(0x9E39DAF1,1944,4)==4,"byte count gate");
    Check(select(0x9E39DAF0,1948,4)==4,"foreign shader");
    Check(select(0x9E39DAF1,1948,20)==20,"different register");
    Check(d2vr_screen_projection::WorldInputRegister(0x9E39DAF1,1948,4,[](const char*){return false;})==4,"digest collision rejected");
    // Shader data flow: output = (world * trackedWvp) * clipToTexture.
    // Check texture fetch coordinates against the raster position, in both eyes
    // and translated/rotated head views. Old named-STP treatment applies H twice.
    const float bias[16]={.5f,0,0,0,0,-.5f,0,0,0,0,1,0,.5f,.5f,0,1};
    for(float yaw:{-.3f,0.f,.3f})for(float eye:{-.032f,.032f}) {
        float h[16]={std::cos(yaw),0,std::sin(yaw),0,0,1,0,0,-std::sin(yaw),0,std::cos(yaw),0,eye,.1f,0,1};
        float old[16];std::memcpy(old,bias,sizeof(old));
        d2vr_math::UnbiasScreenTextureProjection(old);
        d2vr_math::Multiply(old,h,old);
        d2vr_math::RebiasScreenTextureProjection(old);
        const float clip[4]={.4f,-.2f,3.f,4.f};float expected[4]{},actual[4]{},broken[4]{};
        for(int c=0;c<4;++c)for(int r=0;r<4;++r){
            expected[c]+=clip[r]*bias[r*4+c];
            actual[c]+=clip[r]*(select(0x9E39DAF1,1948,4)<0?bias:old)[r*4+c];
            broken[c]+=clip[r]*old[r*4+c];
        }
        Check(std::fabs(actual[0]/actual[3]-(.5f+.5f*clip[0]/clip[3]))<1e-6f,"depth U matches raster");
        Check(std::fabs(actual[1]/actual[3]-(.5f-.5f*clip[1]/clip[3]))<1e-6f,"depth V matches raster");
        Check(!std::memcmp(expected,actual,sizeof(actual)),"native clip conversion preserved");
        Check(std::fabs(broken[0]/broken[3]-expected[0]/expected[3])+std::fabs(broken[1]/broken[3]-expected[1]/expected[3])>1e-4f,"old double transform reproduces drift");
    }
    std::puts("PASS clip-input depth projection; identity gates, both eyes, three head poses");
}

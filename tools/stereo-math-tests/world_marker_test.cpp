#include "../../src/world_marker_projection.h"
#include "../../src/stereo_geometry.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <limits>
static void Vector(const float p[3],const float m[16],float out[4]) {
    for(int c=0;c<4;++c)out[c]=p[0]*m[c]+p[1]*m[4+c]+p[2]*m[8+c]+m[12+c];
}
int main() {
    unsigned checked=0;
    const float native[16]={1.4f,0,0,0,0,2,0,0,0,0,1.001f,1,0,0,-.1f,0};
    for(int yaw=-60;yaw<=60;yaw+=5)for(int lean=-3;lean<=3;++lean)for(int depth=2;depth<80;depth+=7) {
        const float a=yaw*.01745329252f;
        float view[16]={std::cos(a),0,-std::sin(a),0,0,1,0,0,std::sin(a),0,std::cos(a),0,lean*.1f,.05f,-.03f,1};
        d2vr_stereo::EyeShiftParams sp;sp.fovScaleX=.7f;sp.fovScaleY=.6f;sp.headGeometry=true;
        float wide[16];memcpy(wide,native,sizeof(wide));
        for(int r=0;r<4;++r){wide[r*4]*=sp.fovScaleX;wide[r*4+1]*=sp.fovScaleY;}
        float inverse[16],inverseClip[16];assert(d2vr_math::Invert(wide,inverse));
        assert(d2vr_math::HeadClipTransform(inverse,view,sp.headClip,inverseClip));
        float rendered[16];memcpy(rendered,native,sizeof(rendered));
        d2vr_stereo::ApplyEyeShiftToMatrix(rendered,4,sp,true,1.4f,2,sp.headClip);
        const float world[3]={.25f,.1f,float(depth)};
        float before[2];assert(d2vr_marker::Project(native,world,before));
        // Nonzero viewport origins/native movie scale and an untouched Z sentinel.
        const float width=2676,height=2728,sx=1280.f/width,sy=720.f/height;
        const float originX=17,originY=-13;
        float output[3]={originX+(before[0]+1)*640,originY+(1-before[1])*360,.987f};
        assert(d2vr_marker::Correct(native,rendered,world,.81f,.73f,width,height,sx,sy,output));
        // Independent per-vector reference, matching the actual GPU sequence.
        float clip[4];Vector(world,native,clip);clip[0]*=sp.fovScaleX;clip[1]*=sp.fovScaleY;
        float expected[4]{};for(int c=0;c<4;++c)for(int r=0;r<4;++r)expected[c]+=clip[r]*sp.headClip[r*4+c];
        const float x=((output[0]-originX)/640-1)*.81f;
        const float y=(1-(output[1]-originY)/360)*.73f;
        assert(std::abs(x-expected[0]/expected[3])<.00001f);
        assert(std::abs(y-expected[1]/expected[3])<.00001f);
        assert(output[2]==.987f);++checked;
    }
    float out[3]={100,200,.9f},saved[3];memcpy(saved,out,sizeof(out));
    float behind[3]={0,0,-1};
    assert(!d2vr_marker::Correct(native,native,behind,1,1,1280,720,1,1,out));
    assert(!memcmp(saved,out,sizeof(out)));
    float invalid[3]={std::numeric_limits<float>::quiet_NaN(),0,1};
    assert(!d2vr_marker::Correct(native,native,invalid,1,1,1280,720,1,1,out));
    assert(!d2vr_marker::Correct(native,native,behind,0,1,1280,720,1,1,out));
    assert(!memcmp(saved,out,sizeof(out)));
    printf("PASS world marker projected point/GPU agreement: %u cases; native depth, offsets, rejection unchanged\n",checked);
}

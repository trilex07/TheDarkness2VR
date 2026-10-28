#include "../src/native_culling_cover.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){printf("FAIL line %d\n",__LINE__);return 1;}} while(0)
int main() {
    const float eyes[2][4]={{-.9f,.8f,.8f,-.85f},{-.8f,.9f,.8f,-.85f}};
    unsigned checks=0;
    for(int step=0;step<100;++step) {
        const float angle=step*.037f,c=std::cos(angle),s=std::sin(angle);
        float view[16]={c,0,s,0, 0,1,0,0, -s,0,c,0, float(step),-3,2,1};
        const float nearZ=.1f,farZ=1000,aspect=.98f,py=1/std::tan(30.f*.01745329252f),q=farZ/(farZ-nearZ);
        float p[16]={py/aspect,0,0,0, 0,py,0,0, 0,0,q,1, 0,0,-q*nearZ,0};
        float vp[16];d2vr_math::Multiply(view,p,vp);
        float savedView[16],savedP[16],savedVp[16];
        std::memcpy(savedView,view,sizeof(view));std::memcpy(savedP,p,sizeof(p));std::memcpy(savedVp,vp,sizeof(vp));
        d2vr_culling_cover::Input out;
        CHECK(d2vr_culling_cover::Prepare(view,p,vp,60,aspect,nearZ,farZ,eyes,1.1f,1.02f,out));
        CHECK(!std::memcmp(view,savedView,sizeof(view)) && !std::memcmp(p,savedP,sizeof(p)) && !std::memcmp(vp,savedVp,sizeof(vp)));
        CHECK(out.fov>60);
        float identity[16];d2vr_math::Multiply(out.viewProjection,out.inverseViewProjection,identity);
        for(int i=0;i<16;++i)CHECK(std::fabs(identity[i]-(i%5==0?1.f:0.f))<.004f);
        // Wider perspective contains the original cone and both expanded eyes;
        // depth mapping stays exactly unchanged because only Px/Py differ.
        const float tangent=std::tan(out.fov*.00872664626f);
        for(int e=0;e<2;++e)for(int side=0;side<4;++side)
            CHECK(std::tan(std::fabs(eyes[e][side])*1.1f)*1.02f <= tangent*(side<2?aspect:1.f)+1e-5f);
        vp[0]+=.1f;auto sentinel=out;
        CHECK(!d2vr_culling_cover::Prepare(view,p,vp,60,aspect,nearZ,farZ,eyes,1.1f,1.02f,out));
        CHECK(!std::memcmp(&out,&sentinel,sizeof(out)));
        ++checks;
    }
    puts("PASS separate culling input: 100 translated/rotated cameras, binocular coverage, inverse, unchanged render matrices, mismatched camera rejection");
    return checks==100?0:1;
}

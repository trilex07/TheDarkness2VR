#include "../src/native_culling_cover.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){printf("FAIL line %d\n",__LINE__);return 1;}} while(0)
int main() {
    const float eyes[2][4]={{-.9f,.8f,.8f,-.85f},{-.8f,.9f,.8f,-.85f}};
    unsigned checks=0,points=0,newlyVisible=0;
    const auto transform=[](const float* v,const float* m,float* out) {
        for(int c=0;c<4;++c) {
            out[c]=0;for(int r=0;r<4;++r)out[c]+=v[r]*m[r*4+c];
        }
    };
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
        float inverseView[16];CHECK(d2vr_math::Invert(view,inverseView));
        for(int e=0;e<2;++e)for(int row=0;row<9;++row)for(int col=0;col<9;++col)
            for(const float depth : {nearZ,1.f,100.f,farZ}) {
                const float left=-std::tan(std::fabs(eyes[e][0])*1.1f)*1.02f;
                const float right=std::tan(std::fabs(eyes[e][1])*1.1f)*1.02f;
                const float down=-std::tan(std::fabs(eyes[e][3])*1.1f)*1.02f;
                const float up=std::tan(std::fabs(eyes[e][2])*1.1f)*1.02f;
                const float local[4]={(left+(right-left)*col/8)*depth,(down+(up-down)*row/8)*depth,depth,1};
                float world[4],wideClip[4],oldClip[4];transform(local,inverseView,world);
                transform(world,out.viewProjection,wideClip);transform(world,vp,oldClip);
                const float epsilon=.003f*(1+depth);
                CHECK(wideClip[3]>0 && std::fabs(wideClip[0])<=wideClip[3]+epsilon && std::fabs(wideClip[1])<=wideClip[3]+epsilon);
                CHECK(wideClip[2]>=-epsilon && wideClip[2]<=wideClip[3]+epsilon);
                CHECK(std::fabs(wideClip[2]-oldClip[2])<=epsilon && std::fabs(wideClip[3]-oldClip[3])<=epsilon);
                if(std::fabs(oldClip[0])>oldClip[3]+epsilon || std::fabs(oldClip[1])>oldClip[3]+epsilon)++newlyVisible;
                ++points;
            }
        auto sentinel=out;
        float badView[16]{};
        CHECK(!d2vr_culling_cover::Prepare(badView,p,vp,60,aspect,nearZ,farZ,eyes,1.1f,1.02f,out));
        CHECK(!std::memcmp(&out,&sentinel,sizeof(out)));
        badView[0]=NAN;
        CHECK(!d2vr_culling_cover::Prepare(badView,p,vp,60,aspect,nearZ,farZ,eyes,1.1f,1.02f,out));
        CHECK(!std::memcmp(&out,&sentinel,sizeof(out)));
        vp[0]+=.1f;
        CHECK(!d2vr_culling_cover::Prepare(view,p,vp,60,aspect,nearZ,farZ,eyes,1.1f,1.02f,out));
        CHECK(!std::memcmp(&out,&sentinel,sizeof(out)));
        ++checks;
    }
    CHECK(newlyVisible>0 && points==64800);
    printf("PASS separate culling input: 100 cameras, %u world-space edge/depth points, %u newly included, unchanged render/depth matrices, invalid camera rejection\n",points,newlyVisible);
    return checks==100?0:1;
}

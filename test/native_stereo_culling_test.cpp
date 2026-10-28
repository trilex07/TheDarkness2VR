#include "../src/native_culling_cover.h"
#include "../src/stereo_geometry.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::printf("FAIL step=%u line=%d\n",step,__LINE__);return 1;}}while(0)
static void Transform(const float* p,const float* m,float* out) {
    for(unsigned c=0;c<4;++c) {out[c]=0;for(unsigned r=0;r<4;++r)out[c]+=p[r]*m[r*4+c];}
}
int main() {
    d2vr_stereo::Rng rng(0x581031u);
    unsigned points=0,outsideOld=0;
    for(unsigned step=0;step<240;++step) {
        const float nearZ=.05f+.05f*(step%4),farZ=500.f+500.f*(step%2),aspect=.98f;
        const float fov=45.f+15.f*(step%4),py=1/std::tan(fov*.00872664626f),q=farZ/(farZ-nearZ);
        const float projection[16]={py/aspect,0,0,0,0,py,0,0,0,0,q,1,0,0,-q*nearZ,0};
        const float yaw=rng.Range(-1.4f,1.4f),pitch=rng.Range(-.7f,.7f),roll=rng.Range(-.5f,.5f);
        const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch),cr=std::cos(roll),sr=std::sin(roll);
        const float y[16]={cy,0,sy,0,0,1,0,0,-sy,0,cy,0,0,0,0,1};
        const float p[16]={1,0,0,0,0,cp,sp,0,0,-sp,cp,0,0,0,0,1};
        const float r[16]={cr,sr,0,0,-sr,cr,0,0,0,0,1,0,0,0,0,1};
        float temp[16],delta[16];d2vr_math::Multiply(y,p,temp);d2vr_math::Multiply(temp,r,delta);
        delta[12]=rng.Range(-.2f,.2f);delta[13]=rng.Range(-.2f,.2f);delta[14]=rng.Range(-.2f,.2f);
        float view[16];std::memcpy(view,y,sizeof(view));
        view[12]=rng.Range(-1000,1000);view[13]=rng.Range(-20,20);view[14]=rng.Range(-1000,1000);
        float vp[16],inverseView[16];d2vr_math::Multiply(view,projection,vp);CHECK(d2vr_math::Invert(view,inverseView));
        float eyeP[2][16];
        for(unsigned eye=0;eye<2;++eye) {
            d2vr_stereo::EyeShiftParams eyeParams;
            eyeParams.fovScaleX=.25f+.02f*(step%8)+eye*.013f;
            eyeParams.fovScaleY=.3f+.03f*(step%6)+eye*.009f;
            eyeParams.mode=step%2;
            eyeParams.transX=(eye?1.f:-1.f)*(.015f+.005f*(step%6));
            eyeParams.transY=(eye?1.f:-1.f)*.002f;
            eyeParams.separation=(eye?1.f:-1.f)*.02f;
            eyeParams.convergence=2;
            float wideInverse[16],inverseHead[16];CHECK(d2vr_math::Invert(projection,wideInverse));
            for(unsigned c=0;c<4;++c){wideInverse[c]/=eyeParams.fovScaleX;wideInverse[4+c]/=eyeParams.fovScaleY;}
            CHECK(d2vr_math::HeadClipTransform(wideInverse,delta,eyeParams.headClip,inverseHead));
            eyeParams.headGeometry=true;
            std::memcpy(eyeP[eye],projection,sizeof(projection));
            d2vr_stereo::ApplyEyeShiftToMatrix(eyeP[eye],4,eyeParams,true,projection[0],projection[5],eyeParams.headClip);
        }
        float savedEye[2][16];std::memcpy(savedEye,eyeP,sizeof(eyeP));
        d2vr_culling_cover::Input out;
        CHECK(d2vr_culling_cover::PrepareStereo(view,projection,vp,eyeP,fov,aspect,nearZ,farZ,out));
        CHECK(out.stereoUnion && !std::memcmp(savedEye,eyeP,sizeof(eyeP)));
        float referenceVP[16];d2vr_math::Multiply(view,eyeP[0],referenceVP);
        // Only clip X/Y change during the fit. The rotated/transformed depth
        // interval is shared by both eyes and remains unchanged by union fitting.
        for(unsigned row=0;row<4;++row)for(unsigned col=2;col<4;++col)
            CHECK(out.viewProjection[row*4+col]==referenceVP[row*4+col]);
        for(unsigned eye=0;eye<2;++eye) {
            float inverseEye[16];CHECK(d2vr_math::Invert(eyeP[eye],inverseEye));
            for(unsigned sample=0;sample<520;++sample) {
                const float clip[4]={sample<8?(sample&1?1.f:-1.f):rng.Range(-1,1),
                    sample<8?(sample&2?1.f:-1.f):rng.Range(-1,1),sample<8?(sample&4?1.f:0.f):rng.Range(0,1),1};
                float local[4],world[4],covered[4],old[4];
                Transform(clip,inverseEye,local);Transform(local,inverseView,world);
                Transform(world,out.viewProjection,covered);Transform(world,vp,old);
                CHECK(covered[3]>0);
                const float tolerance=.003f*(1+std::fabs(covered[3]));
                CHECK(std::fabs(covered[0])<=covered[3]+tolerance && std::fabs(covered[1])<=covered[3]+tolerance);
                CHECK(covered[2]>=-tolerance && covered[2]<=covered[3]+tolerance);
                if(old[3]<=0 || std::fabs(old[0])>old[3]+tolerance || std::fabs(old[1])>old[3]+tolerance)++outsideOld;
                ++points;
            }
        }
        const auto sentinel=out;
        eyeP[1][10]+=.1f;
        CHECK(!d2vr_culling_cover::PrepareStereo(view,projection,vp,eyeP,fov,aspect,nearZ,farZ,out));
        CHECK(!std::memcmp(&out,&sentinel,sizeof(out)));
        std::memset(eyeP[1],0,sizeof(eyeP[1]));
        CHECK(!d2vr_culling_cover::PrepareStereo(view,projection,vp,eyeP,fov,aspect,nearZ,farZ,out));
        eyeP[1][0]=NAN;
        CHECK(!d2vr_culling_cover::PrepareStereo(view,projection,vp,eyeP,fov,aspect,nearZ,farZ,out));
    }
    {
        const unsigned step=240;
        const float view[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        const float p[16]={1,0,0,0,0,1,0,0,0,0,1,1,0,0,-.1f,0};
        float eyes[2][16];std::memcpy(eyes[0],p,sizeof(p));std::memcpy(eyes[1],p,sizeof(p));
        d2vr_culling_cover::Input out;out.fov=123;
        const auto sentinel=out;
        CHECK(!d2vr_culling_cover::PrepareStereo(view,p,p,eyes,90,1,.1f,1e7f,out));
        CHECK(!std::memcmp(&out,&sentinel,sizeof(out))); // finite native AABB required
        float nonAffine[16];std::memcpy(nonAffine,view,sizeof(view));nonAffine[3]=.1f;
        float vp[16];d2vr_math::Multiply(nonAffine,p,vp);
        CHECK(!d2vr_culling_cover::PrepareStereo(nonAffine,p,vp,eyes,90,1,.1f,1e7f,out));
    }
    std::printf("PASS stereo culling union: %u tracked poses, %u binocular points, %u outside native cone, unchanged depth, mismatch/singular/NaN rejection\n",240u,points,outsideOld);
}

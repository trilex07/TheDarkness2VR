#include "../src/native_culling_cover.h"
#include "../src/stereo_geometry.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::printf("FAIL step=%u line=%d\n",step,__LINE__);return 1;}}while(0)
static void Transform(const float* p,const float* m,float* out) {
    for(unsigned c=0;c<4;++c) {out[c]=0;for(unsigned r=0;r<4;++r)out[c]+=p[r]*m[r*4+c];}
}
int main(int argc,char** argv) {
    const bool fullFrustum=argc==4 && !std::strcmp(argv[1],"--expand-native-frustum");
    if(argc==4 && (!std::strcmp(argv[1],"--expand-native-bounds") || fullFrustum)) {
        FILE* input=nullptr;FILE* output=nullptr;
        if(fopen_s(&input,argv[2],"rb") || !input || fopen_s(&output,argv[3],"wb") || !output)return 2;
        char magic[8];unsigned count=0;
        if(fread(magic,1,8,input)!=8 || std::memcmp(magic,fullFrustum?"D2BND002":"D2BND001",8) || fread(&count,4,1,input)!=1 || count>10000)return 2;
        if(fwrite(magic,1,8,output)!=8 || fwrite(&count,4,1,output)!=1)return 2;
        for(unsigned i=0;i<count;++i) {
            d2vr_culling_cover::Input data;data.haveBounds=true;
            float bounds[8],planes[24];
            if(fread(data.lower,4,3,input)!=3 || fread(data.upper,4,3,input)!=3)return 2;
            if(fullFrustum) {
                data.haveCorners=true;
                if(fread(data.corners,8,48,input)!=48 || fread(planes,4,24,input)!=24)return 2;
            }
            if(fread(bounds,4,8,input)!=8)return 2;
            if(fullFrustum) {
                if(!d2vr_culling_cover::ExpandOwnedFrustum(data,planes,bounds) || fwrite(planes,4,24,output)!=24)return 2;
            } else if(!d2vr_culling_cover::ExpandOwnedBounds(data,bounds))return 2;
            if(fwrite(bounds,4,8,output)!=8)return 2;
        }
        if(fgetc(input)!=EOF || fclose(input) || fclose(output))return 2;
        return 0;
    }
    // Optional generated inputs for the separate, private native-code emulator.
    // Only our matrices/points are exported; no executable bytes or game assets.
    FILE* exported=nullptr;
    const bool fullRotation=argc==4 && !std::strcmp(argv[3],"--full-rotation");
    const bool referenceEyes=(argc==3 || fullRotation) && !std::strcmp(argv[2],"--reference-eyes");
    if(argc>1 && argc!=2 && !referenceEyes)return 2;
    if(argc==2 || referenceEyes) {
        if(fopen_s(&exported,argv[1],"wb") || !exported)return 2;
        const unsigned cases=240;
        if(fwrite(referenceEyes?"D2FRC004":"D2FRC001",1,8,exported)!=8 || fwrite(&cases,4,1,exported)!=1)return 2;
    }
    d2vr_stereo::Rng rng(0x581031u);
    unsigned points=0,outsideOld=0;
    for(unsigned step=0;step<240;++step) {
        const float nearZ=.05f+.05f*(step%4),farZ=500.f+500.f*(step%2),aspect=.98f;
        const float fov=45.f+15.f*(step%4),py=1/std::tan(fov*.00872664626f),q=farZ/(farZ-nearZ);
        const float projection[16]={py/aspect,0,0,0,0,py,0,0,0,0,q,1,0,0,-q*nearZ,0};
        const float yawLimit=fullRotation?3.14159265f:1.4f;
        const float pitchLimit=fullRotation?1.5f:.7f,rollLimit=fullRotation?3.14159265f:.5f;
        const float yaw=rng.Range(-yawLimit,yawLimit),pitch=rng.Range(-pitchLimit,pitchLimit),roll=rng.Range(-rollLimit,rollLimit);
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
        if(exported) {
            const unsigned samples=1040;
            if(referenceEyes) {
                CHECK(fwrite(view,sizeof(float),16,exported)==16);
                CHECK(fwrite(eyeP,sizeof(float),32,exported)==32);
                CHECK(out.haveBounds);
                CHECK(fwrite(out.lower,sizeof(float),3,exported)==3);
                CHECK(fwrite(out.upper,sizeof(float),3,exported)==3);
                CHECK(out.haveCorners);
                CHECK(fwrite(out.corners,sizeof(double),48,exported)==48);
            }
            CHECK(fwrite(out.viewProjection,sizeof(float),16,exported)==16);
            CHECK(fwrite(out.inverseViewProjection,sizeof(float),16,exported)==16);
            CHECK(fwrite(&samples,4,1,exported)==1);
        }
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
                if(exported)CHECK(fwrite(world,sizeof(float),4,exported)==4);
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
        d2vr_culling_cover::Input cube;cube.haveBounds=cube.haveCorners=true;
        for(unsigned axis=0;axis<3;++axis){cube.lower[axis]=-1;cube.upper[axis]=1;}
        for(unsigned corner=0;corner<16;++corner)for(unsigned axis=0;axis<3;++axis)
            cube.corners[corner][axis]=corner&(1<<axis)?1.:-1.;
        const float rawPlanes[24]={1,0,0,0,-1,0,0,0,0,1,0,0,0,-1,0,0,0,0,1,0,0,0,-1,0};
        const float rawBounds[8]={-.5f,-.5f,-.5f,7,.5f,.5f,.5f,8};
        float planes[24],bounds[8];std::memcpy(planes,rawPlanes,sizeof(planes));std::memcpy(bounds,rawBounds,sizeof(bounds));
        CHECK(d2vr_culling_cover::ExpandOwnedFrustum(cube,planes,bounds));
        for(unsigned plane=0;plane<6;++plane) {
            CHECK(planes[plane*4+3]<-1.f);
            for(unsigned axis=0;axis<3;++axis)CHECK(planes[plane*4+axis]==rawPlanes[plane*4+axis]);
        }
        CHECK(bounds[3]==7 && bounds[7]==8);
        // Failure must not partially update earlier planes or bounds.
        for(unsigned failure=0;failure<5;++failure) {
            auto invalid=cube;std::memcpy(planes,rawPlanes,sizeof(planes));std::memcpy(bounds,rawBounds,sizeof(bounds));
            if(failure==0)planes[23]=NAN;
            if(failure==1)planes[20]=planes[21]=planes[22]=0;
            if(failure==2)bounds[6]=-2;
            if(failure==3)invalid.corners[15][2]=NAN;
            if(failure==4)invalid.corners[15][2]=2;
            float savedPlanes[24],savedBounds[8];std::memcpy(savedPlanes,planes,sizeof(planes));std::memcpy(savedBounds,bounds,sizeof(bounds));
            CHECK(!d2vr_culling_cover::ExpandOwnedFrustum(invalid,planes,bounds));
            CHECK(!std::memcmp(planes,savedPlanes,sizeof(planes)) && !std::memcmp(bounds,savedBounds,sizeof(bounds)));
        }
        // Already wider native half-spaces must never become tighter.
        std::memcpy(planes,rawPlanes,sizeof(planes));std::memcpy(bounds,rawBounds,sizeof(bounds));
        for(unsigned plane=0;plane<6;++plane)planes[plane*4+3]=-2;
        CHECK(d2vr_culling_cover::ExpandOwnedFrustum(cube,planes,bounds));
        for(unsigned plane=0;plane<6;++plane)CHECK(planes[plane*4+3]==-2);
    }
    {
        const unsigned step=241;
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
    if(exported && fclose(exported))return 2;
    std::printf("PASS stereo culling union: %u tracked poses, %u binocular points, %u outside native cone, unchanged depth, mismatch/singular/NaN rejection\n",240u,points,outsideOld);
}

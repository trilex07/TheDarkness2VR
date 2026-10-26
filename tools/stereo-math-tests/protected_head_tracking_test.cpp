#include "../../src/stereo_geometry.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <initializer_list>
using UINT=unsigned;
struct StereoParams {
    int mode=0; float fovScaleX=1,fovScaleY=1,transX=0,transY=0;
    bool headGeometry=false; float inverseHeadClip[16]{};
};
bool g_noInvProj=false,g_headGeometry=true,g_skipLightVsShift=false,g_lightPosShift=false;
int g_lightingPreset=0,g_nativeStereoHeadGeometry=1,g_stereoShiftMode=0,g_projectionFix=3,g_stereoLayerPose=0,g_headLookMode=0;
#include "d2vr_tracking_gate.inl"
#include "d2vr_tracking_inverse.inl"
static unsigned checks=0;
static void Check(bool ok,const char* why) {
    ++checks;if(!ok){std::printf("FAIL %s check=%u\n",why,checks);std::exit(1);}
}
static void Same(const float* a,const float* b,const char* why) {
    for(int i=0;i<16;++i) Check(std::fabs(a[i]-b[i])<.003f,why);
}
int main() {
    const float rq[4]={0,0,0,1},rp[3]={12,2,-5},identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    float projection[16]={1.4f,0,0,0,0,1.8f,0,0,0,0,1.001f,1,0,0,-.1001f,0},inverseP[16];
    Check(d2vr_math::Invert(projection,inverseP),"invert projection");
    for(int i=0;i<513;++i) {
        float angle=(i-256)*.009f;
        float q[4]={.15f*std::sin(angle*.7f),.18f*std::sin(angle*.4f),std::sin(angle*.5f),std::cos(angle*.5f)};
        float p[3]={rp[0]+.2f*std::sin(angle),rp[1]+.1f*std::cos(angle),rp[2]+.13f*std::sin(angle*.5f)};
        for(float scale:{.1f,1.f,10.f}) {
            float delta[16],clip[16],inverseClip[16];
            Check(d2vr_math::RelativeTrackingView(q,p,rq,rp,scale,delta),"relative pose");
            Check(d2vr_math::HeadClipTransform(inverseP,delta,clip,inverseClip),"clip conjugation");
            float forward[2][16]{};
            for(int eye=0;eye<2;++eye) {
                const float eyeOffset=(eye ? -1:1)*.032f*scale;
                d2vr_stereo::EyeShiftParams sp;sp.transX=eyeOffset;sp.headGeometry=true;
                std::memcpy(forward[eye],projection,sizeof(projection));
                d2vr_stereo::ApplyEyeShiftToMatrix(forward[eye],4,sp,true,1.4f,1.8f,clip);
                float eyeView[16],expected[16];
                std::memcpy(eyeView,delta,sizeof(delta));eyeView[12]+=eyeOffset;
                d2vr_math::Multiply(eyeView,projection,expected);
                Same(forward[eye],expected,"head before current-eye baseline");
                StereoParams ps;ps.transX=eyeOffset;ps.headGeometry=true;
                std::memcpy(ps.inverseHeadClip,inverseClip,sizeof(inverseClip));
                float reconstructed[16],product[16];
                Check(ApplyInverseProjection(reconstructed,inverseP,0,4,0,ps),"actual PS compensation");
                d2vr_math::Multiply(expected,reconstructed,product);
                Same(product,identity,"forward/reconstruction inverse");
            }
            for(int row=0;row<4;++row) {
                Check(std::fabs(forward[0][row*4+1]-forward[1][row*4+1])<1e-6f,"no vertical stereo disparity");
                Check(std::fabs(forward[0][row*4+3]-forward[1][row*4+3])<1e-6f,"equal eye clip depth");
            }
        }
    }
    float delta[16],p[3]={12.25f,2,-5};
    Check(d2vr_math::RelativeTrackingView(rq,p,rq,rp,10,delta),"scale check");
    Check(std::fabs(delta[12]+2.5f)<1e-6f,"metres use IPD world scale");
    for(float scale:{0.f,-1.f,std::numeric_limits<float>::infinity()})
        Check(!d2vr_math::RelativeTrackingView(rq,p,rq,rp,scale,delta),"invalid scale refused");
    float bad[4]={0,0,0,0};
    Check(!d2vr_math::RelativeTrackingView(bad,p,rq,rp,1,delta),"invalid orientation refused");
    Check(d2vr_math::RelativeTrackingView(rq,rp,rq,rp,1,delta),"recenter identity");
    Same(delta,identity,"recenter no double pose");
    Check(ProtectedHeadConfigurationAllowed(),"valid protected configuration");
    for(int* setting:{&g_stereoShiftMode,&g_projectionFix,&g_stereoLayerPose,&g_headLookMode}) {
        const int before=*setting;*setting=before==0 ? 1:0;
        Check(!ProtectedHeadConfigurationAllowed(),"incompatible runtime setting refuses tracking");*setting=before;
    }
    for(bool* setting:{&g_noInvProj,&g_skipLightVsShift,&g_lightPosShift}) {
        *setting=true;Check(!ProtectedHeadConfigurationAllowed(),"diagnostic override refuses tracking");*setting=false;
    }
    g_lightingPreset=-1;Check(ProtectedHeadConfigurationAllowed(),"normal configured lighting accepted");
    g_lightingPreset=9;Check(!ProtectedHeadConfigurationAllowed(),"experimental lighting refused");
    std::printf("PASS protected head tracking %u checks; no runtime/culling claim\n",checks);
}

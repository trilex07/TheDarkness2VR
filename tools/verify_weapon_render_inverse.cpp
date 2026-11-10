// CPU-only continuation of the actual protected-weapon fixture. No game access.
// The build script changes only the original fixture's main signature to void.
// A renamed int main loses C++'s implicit return 0 rule; never inspect that value.
#include "actual_weapon_indicator_fixture.inl"
#include "../src/vr_gameplay_options.h"
#include "../src/stereo_geometry.h"
int main(int argc,char** argv) {
    OriginalWeaponFixtureMain(argc,argv);
    const unsigned before=checks;
    for(unsigned i=0;i<160;++i)for(unsigned slot=0;slot<2;++slot)for(float units:{1.f,100.f}) {
        Reset();g_gameUnitsPerMeter=units;
        const float a=float(i)*.031f;
        const float cq[4]={std::sin(a*.5f),0,0,std::cos(a*.5f)};
        const float cp[3]={100.f,-5.f,-80.f};
        float base[16],inverseBase[16];CHECK(d2vr_math::TrackingPose(cq,cp,base));
        memcpy(camera+0x80,base,sizeof(base));CHECK(d2vr_math::Invert(base,inverseBase));
        g_headGeometryReference={{0,std::sin(a*.13f),0,std::cos(a*.13f)},{-1.f,.4f,-.8f}};
        const float x=std::sin(a*.7f),y=std::cos(a*.3f),z=std::sin(a*.4f);
        const float n=std::sqrt(x*x+y*y+z*z),sn=std::sin(a*.5f)/n;
        const unsigned hand=1-slot;
        g_hands[hand].aim={{x*sn,y*sn,z*sn,std::cos(a*.5f)},{slot?-1.3f:-.7f,.2f,-1.2f}};
        alignas(16) float shot[16];
        CHECK(ResolveProtectedWeaponFire(fires[slot],inventory+0xB0+12*slot,shot));
        XrPosef indicatorPose{};CHECK(CopyProtectedWeaponHandPose(Ptr(camera),hand,indicatorPose));
        const XrPosef head{{0,0,0,1},{0,0,0}};XrPosef dot{};XrExtent2Df size{};
        CHECK(d2vr_gameplay::WeaponDot(indicatorPose,head,(XrSpace)1,dot,size));
        const auto& ref=g_headGeometryReference;
        const float rq[4]={ref.orientation.x,ref.orientation.y,ref.orientation.z,ref.orientation.w};
        const float rp[3]={ref.position.x*units,ref.position.y*units,ref.position.z*units};
        float scaledReference[16],worldToTracking[16];
        CHECK(d2vr_math::TrackingPose(rq,rp,scaledReference));
        d2vr_math::Multiply(inverseBase,scaledReference,worldToTracking);
        const float endpoint[4]={shot[12]+shot[8]*8*units,shot[13]+shot[9]*8*units,shot[14]+shot[10]*8*units,1};
        float projected[3]{};
        for(unsigned c=0;c<3;++c)for(unsigned k=0;k<4;++k)projected[c]+=endpoint[k]*worldToTracking[k*4+c];
        CHECK(std::fabs(projected[0]/units-dot.position.x)<.00015f);
        CHECK(std::fabs(projected[1]/units-dot.position.y)<.00015f);
        CHECK(std::fabs(-projected[2]/units-dot.position.z)<.00015f);
    }
    printf("PASS actual final resolver to XR indicator: %u additional checks; 640 hand/scale/camera cases\n",checks-before);
    const unsigned inverseBefore=checks;
    unsigned wrongOwnerRejected=0;
    for(unsigned i=0;i<80;++i)for(unsigned slot=0;slot<2;++slot)for(float units:{1.f,100.f}) {
        Reset();g_gameUnitsPerMeter=units;
        const float angle=float(i)*.047f;
        const float cq[4]={std::sin(angle*.5f),0,0,std::cos(angle*.5f)};
        const float cp[3]={100.f,-5.f,-80.f};
        float base[16],inverseBase[16];CHECK(d2vr_math::TrackingPose(cq,cp,base));
        memcpy(camera+0x80,base,sizeof(base));CHECK(d2vr_math::Invert(base,inverseBase));
        g_headGeometryReference={{0,std::sin(angle*.13f),0,std::cos(angle*.13f)},{-1.f,.4f,-.8f}};
        const unsigned hand=1-slot;
        g_hands[hand].aim={{0,std::sin(angle*.21f),0,std::cos(angle*.21f)},{slot?-1.3f:-.7f,.2f,-1.2f}};
        alignas(16) float shot[16];
        CHECK(ResolveProtectedWeaponFire(fires[slot],inventory+0xB0+12*slot,shot));
        XrPosef aim{};CHECK(CopyProtectedWeaponHandPose(Ptr(camera),hand,aim));
        const XrPosef head{{std::sin(angle*.17f),0,0,std::cos(angle*.17f)},{-.9f,.42f,-.85f}};
        XrPosef dot{};XrExtent2Df size{};CHECK(d2vr_gameplay::WeaponDot(aim,head,(XrSpace)1,dot,size));
        const auto& ref=g_headGeometryReference;
        const float rq[4]={ref.orientation.x,ref.orientation.y,ref.orientation.z,ref.orientation.w};
        const float rp[3]={ref.position.x,ref.position.y,ref.position.z};
        const float hq[4]={head.orientation.x,head.orientation.y,head.orientation.z,head.orientation.w};
        const float hp[3]={head.position.x,head.position.y,head.position.z};
        float delta[16];CHECK(d2vr_math::RelativeTrackingView(hq,hp,rq,rp,units,delta));
        const float projection[16]={1.1f,0,0,0,0,1.4f,0,0,0,0,1.01f,1,0,0,-.101f,0};
        float inverseProjection[16];CHECK(d2vr_math::Invert(projection,inverseProjection));
        const float endpoint[4]={shot[12]+shot[8]*8*units,shot[13]+shot[9]*8*units,shot[14]+shot[10]*8*units,1};
        const float dotRelative[4]={(dot.position.x-rp[0])*units,(dot.position.y-rp[1])*units,-(dot.position.z-rp[2])*units,1};
        const float hpRelative[3]={(hp[0]-rp[0])*units,(hp[1]-rp[1])*units,(hp[2]-rp[2])*units};
        float current[16],inverseCurrent[16];CHECK(d2vr_math::TrackingPose(hq,hpRelative,current));CHECK(d2vr_math::Invert(current,inverseCurrent));
        for(unsigned eye=0;eye<2;++eye) {
            d2vr_stereo::EyeShiftParams ep;ep.mode=0;ep.headGeometry=true;
            ep.fovScaleX=.71f;ep.fovScaleY=.83f;
            ep.transX=(eye?-.032f:.032f)*units;
            float wideInverse[16];memcpy(wideInverse,inverseProjection,sizeof(wideInverse));
            for(unsigned c=0;c<4;++c){wideInverse[c]/=ep.fovScaleX;wideInverse[4+c]/=ep.fovScaleY;}
            float inverseClip[16];CHECK(d2vr_math::HeadClipTransform(wideInverse,delta,ep.headClip,inverseClip));
            float nativeWvp[16];d2vr_math::Multiply(inverseBase,projection,nativeWvp);
            d2vr_stereo::ApplyEyeShiftToMatrix(nativeWvp,4,ep,true,projection[0],projection[5],ep.headClip);
            float actual[4]{},expectedView[4]{},expected[4]{};
            for(unsigned c=0;c<4;++c)for(unsigned k=0;k<4;++k){actual[c]+=endpoint[k]*nativeWvp[k*4+c];expectedView[c]+=dotRelative[k]*inverseCurrent[k*4+c];}
            expectedView[0]+=ep.transX;
            float wideProjection[16];CHECK(d2vr_math::Invert(wideInverse,wideProjection));
            for(unsigned c=0;c<4;++c)for(unsigned k=0;k<4;++k)expected[c]+=expectedView[k]*wideProjection[k*4+c];
            for(unsigned c=0;c<4;++c)CHECK(std::fabs(actual[c]-expected[c])<.002f*units);
            // Wrong camera owner is a meaningful negative: same algebra cannot
            // establish that the native uploaded view belongs to shot's camera.
            if(i==20 && slot==0 && units==1.f && eye==0) {
                float foreign[16];memcpy(foreign,inverseBase,sizeof(foreign));foreign[13]+=1.f;
                float wrong[16];d2vr_math::Multiply(foreign,projection,wrong);
                d2vr_stereo::ApplyEyeShiftToMatrix(wrong,4,ep,true,projection[0],projection[5],ep.headClip);
                float mismatch=0;for(unsigned k=0;k<4;++k)mismatch+=endpoint[k]*(wrong[k*4+1]-nativeWvp[k*4+1]);
                CHECK(std::fabs(mismatch)>.1f);++wrongOwnerRejected;
            }
        }
    }
    CHECK(wrongOwnerRejected==1);
    printf("PASS actual HeadClip/RelativeTrackingView/ApplyEyeShift inverse: %u additional checks, 640 eye/hand/scale cases. Assumes same camera view owner; actual upload owner/timing/spread/collision NOT RUN\n",checks-inverseBefore);
    return 0;
}

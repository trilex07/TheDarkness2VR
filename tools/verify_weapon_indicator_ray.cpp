// CPU-only continuation of the actual protected-weapon fixture. No game access.
// The build script changes only the original fixture's main signature to void.
// A renamed int main loses C++'s implicit return 0 rule; never inspect that value.
#include "actual_weapon_indicator_fixture.inl"
#include "../src/vr_gameplay_options.h"
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
    printf("PASS actual final resolver to XR indicator: %u additional checks; 640 hand/scale/camera cases, no spread/collision or image acceptance\n",checks-before);
    return 0;
}

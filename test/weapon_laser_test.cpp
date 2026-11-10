#define D2VR_MATCHED_CAPTURE
#include "../src/vr_gameplay_options.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
using DWORD=unsigned long;
static unsigned checks,renders;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static d2vr_settings::Panel g_vrSettings;
static struct {bool open=false;} g_wheelGate;
static bool paused=false,available=true,owned=true,hand[2]{true,true},rendered=true;
static bool MenuConfirmActive(){return paused;}
static bool ProtectedWeaponAvailable(){return available;}
struct NativeGameplaySample{DWORD camera=1;};
static bool CopyNativeGameplay(NativeGameplaySample&){return owned;}
static bool CopyProtectedWeaponHandPose(DWORD,unsigned h,XrPosef& p){
    p={{0,0,0,1},{h?.3f:-.3f,-.2f,-.4f}};return hand[h];
}
static XrSessionState g_xrSessionState=XR_SESSION_STATE_FOCUSED;
static XrSpace g_xrSpace=(XrSpace)1;
static XrPosef g_eyePose[2]{};
static XrPosef HeadCenterPose(const XrPosef&,const XrPosef&){return {{0,0,0,1},{0,0,0}};}
static struct {
    unsigned maxLayers=3;
    bool Render(XrCompositionLayerQuad& q,bool solid){++renders;CHECK(solid);
        q={XR_TYPE_COMPOSITION_LAYER_QUAD};q.subImage.swapchain=(XrSwapchain)2;return rendered;}
} g_weaponLaserRenderer;
#include "../src/weapon_laser.inl"
int main(){
    for(unsigned mode:{1u,2u})for(unsigned bits=0;bits<1024;++bits)for(unsigned limit=1;limit<=3;++limit){
        g_vrSettings.Options().weaponLaser=(bits&1)?mode:0;paused=bool(bits&2);
        available=bool(bits&4);owned=bool(bits&8);hand[0]=bool(bits&16);hand[1]=bool(bits&32);
        g_wheelGate.open=bool(bits&64);rendered=bool(bits&128);
        g_xrSessionState=(bits&256)?XR_SESSION_STATE_FOCUSED:XR_SESSION_STATE_VISIBLE;
        const bool shouldRender=bool(bits&512);
        g_weaponLaserRenderer.maxLayers=limit;
        const XrCompositionLayerBaseHeader* layers[3]{};XrCompositionLayerQuad beams[2]{};
        uint32_t count=1;renders=0;
        AppendWeaponLaserLayers(layers,count,3,beams,shouldRender);
        const bool attempt=(bits&1)&&!paused&&available&&owned&&!g_wheelGate.open&&
            g_xrSessionState==XR_SESSION_STATE_FOCUSED&&shouldRender&&(hand[0]||hand[1]);
        CHECK(renders==unsigned(attempt));
        const unsigned expected=attempt&&rendered?std::min(limit-1,unsigned(hand[0])+unsigned(hand[1])):0;
        CHECK(count==1+expected);CHECK(count<=limit);
        for(unsigned i=1;i<count;++i){auto q=reinterpret_cast<const XrCompositionLayerQuad*>(layers[i]);
            CHECK(q->space==g_xrSpace&&q->eyeVisibility==XR_EYE_VISIBILITY_BOTH);
            if(mode==1) {CHECK(q->subImage.imageRect.extent.width==1);CHECK(q->size.height>7.9f);}
            else {CHECK(q->subImage.imageRect.extent.width==16);CHECK(q->subImage.imageRect.offset.x==32);
                CHECK(q->layerFlags&XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT);
                CHECK(q->size.width==.018f&&q->size.height==.018f);}
}
    }
    // Long-axis endpoint and dot share one finite aim ray across rotations.
    for(unsigned i=0;i<240;++i) {
        const float a=float(i)*.025f;
        const float x=std::sin(a*.7f),y=std::cos(a*.3f),z=std::sin(a*.4f);
        const float n=std::sqrt(x*x+y*y+z*z),sn=std::sin(a*.5f)/n;
        XrPosef aim{{x*sn,y*sn,z*sn,std::cos(a*.5f)},{.3f,-.2f,-.4f}};
        XrPosef head{{0,0,0,1},{0,0,0}},dot{},beam{};XrExtent2Df ds{},bs{};
        CHECK(d2vr_gameplay::WeaponDot(aim,head,g_xrSpace,dot,ds));
        double q[4];CHECK(d2vr_menu::detail::Pose(aim,q));
        const auto ray=d2vr_menu::detail::Rotate(q,{0,0,-1});
        CHECK(std::fabs(dot.position.x-aim.position.x-ray.x*8)<.00001);
        CHECK(std::fabs(dot.position.y-aim.position.y-ray.y*8)<.00001);
        CHECK(std::fabs(dot.position.z-aim.position.z-ray.z*8)<.00001);
        if(d2vr_gameplay::WeaponBeam(aim,head,g_xrSpace,beam,bs)) {
            CHECK(d2vr_menu::detail::Pose(beam,q));
            const auto axis=d2vr_menu::detail::Rotate(q,{0,1,0});
            CHECK(axis.x*ray.x+axis.y*ray.y+axis.z*ray.z>.99999);
        }
    }
    XrPosef invalid{{0,0,0,0},{0,0,0}},good{{0,0,0,1},{0,0,0}},out{};
    XrExtent2Df size{};CHECK(!d2vr_gameplay::WeaponDot(invalid,good,g_xrSpace,out,size));
    CHECK(!d2vr_gameplay::WeaponDot(good,invalid,g_xrSpace,out,size));
    CHECK(!d2vr_gameplay::WeaponDot(good,good,XR_NULL_HANDLE,out,size));
    printf("PASS actual weapon laser/dot gates %u checks\n",checks);
}

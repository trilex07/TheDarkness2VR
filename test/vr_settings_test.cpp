#include "../src/vr_settings.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
using namespace d2vr_settings;
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL %d %s\n",__LINE__,#x);std::exit(1);}}while(0)
static XrPosef head{{0,0,0,1},{0,1.6f,0}};
static XrSpace space=(XrSpace)2;
static d2vr_menu::Aim aim;
static void AimAt(Panel& panel,int x,int y,float trigger=0,bool back=false) {
    const auto& s=panel.Surface();
    aim.frame++;aim.space=space;aim.menu=aim.tracked=true;
    aim.pose={{0,0,0,1},{float((x+.25)/Width-.5)*s.width,
        head.position.y+float(.5-(y+.25)/Height)*s.height,0}};
    aim.trigger=trigger;aim.back=back;aim.time=aim.frame/90.;
}
int main(){
    // Entry and full panel track the actual rotated/translated native menu.
    {
        Panel p;p.ShowEntry(true,head,space);
        XrPosef anchor{{0,.38268343f,0,.92387953f},{2,3,-4}};
        p.AnchorMenu(anchor,2,1,space);
        const auto s=p.Surface();
        CHECK(std::fabs(s.width-.8f)<1e-6f&&std::fabs(s.height-.1f)<1e-6f);
        CHECK(std::fabs(s.pose.position.y-2.57f)<1e-5f);
        CHECK(std::fabs(s.pose.position.x-(2+.015f*.70710678f))<1e-5f);
        CHECK(s.pose.orientation.y==anchor.orientation.y);
        anchor.position.x+=1;p.AnchorMenu(anchor,2,1,space);
        CHECK(std::fabs(p.Surface().pose.position.x-s.pose.position.x-1)<1e-6f);
        const auto saved=p.Surface();p.AnchorMenu(anchor,2,1,(XrSpace)3);
        CHECK(p.Surface().pose.position.x==saved.pose.position.x);
        p.AnchorMenu(anchor,NAN,1,space);CHECK(p.Surface().width==saved.width);
        p.Toggle(head,space);p.AnchorMenu(anchor,2,1,space);
        CHECK(p.Open()&&p.Surface().pose.position.x==anchor.position.x);
        CHECK(std::fabs(p.Surface().width-1.6f)<1e-6f&&std::fabs(p.Surface().height-.9f)<1e-6f);
    }
    ToggleAction toggle;
    CHECK(!toggle.Update(true,true));CHECK(!toggle.Update(true,false));
    CHECK(toggle.Update(true,true));CHECK(!toggle.Update(true,true));
    CHECK(!toggle.Update(false,false));CHECK(!toggle.Update(true,true));
    Panel panel;auto mode=d2vr_adaptive::Mode::Off;
    CHECK(!panel.Toggle(head,XR_NULL_HANDLE));
    auto bad=head;bad.orientation.w=0;CHECK(!panel.Toggle(bad,space));
    CHECK(panel.Toggle(head,space));CHECK(panel.Open()&&!panel.Presented());
    CHECK(panel.OwnInput(true));CHECK(panel.Surface().width==1.28f);
    CHECK(std::fabs(panel.Surface().pose.position.z+1.5f)<1e-6f);
    // A held trigger on open, or a pending/unsubmitted surface, cannot click.
    AimAt(panel,250,170,1);panel.Input(aim,mode);CHECK(mode==d2vr_adaptive::Mode::Off);
    panel.Submitted(true);AimAt(panel,250,170,1);panel.Input(aim,mode);
    CHECK(mode==d2vr_adaptive::Mode::Off);
    AimAt(panel,250,170);panel.Input(aim,mode);CHECK(panel.Pointer().visible);
    AimAt(panel,250,170,1);panel.Input(aim,mode);CHECK(mode==d2vr_adaptive::Mode::Auto);
    // No second edge from another call in the same XR application frame.
    panel.Input(aim,mode);CHECK(!panel.Pointer().press);
    CHECK(panel.Pointer().visible&&panel.Pointer().captured);
    AimAt(panel,440,170);panel.Input(aim,mode);
    AimAt(panel,440,170,1);panel.Input(aim,mode);CHECK(mode==d2vr_adaptive::Mode::Half);
    panel.SuspendSurface();CHECK(!panel.Presented());
    AimAt(panel,60,170);panel.Input(aim,mode);CHECK(!panel.Pointer().visible);
    panel.Submitted(true);AimAt(panel,60,170,1);panel.Input(aim,mode);
    CHECK(mode==d2vr_adaptive::Mode::Half);
    AimAt(panel,60,170);panel.Input(aim,mode);
    AimAt(panel,60,170,1);panel.Input(aim,mode);CHECK(mode==d2vr_adaptive::Mode::Off);
    AimAt(panel,550,300);panel.Input(aim,mode);
    AimAt(panel,550,300,1);panel.Input(aim,mode);CHECK(!panel.Open());
    CHECK(panel.OwnInput(false));CHECK(!panel.OwnInput(true));
    // Back works without a ray hit, but needs a release in this panel.
    CHECK(panel.Toggle(head,space));AimAt(panel,1000,170,0,true);panel.Input(aim,mode);
    CHECK(panel.Open());AimAt(panel,1000,170);panel.Input(aim,mode);
    AimAt(panel,1000,170,0,true);panel.Input(aim,mode);CHECK(!panel.Open());
    CHECK(panel.Toggle(head,space));panel.Submitted(false);CHECK(!panel.Open());
    CHECK(panel.Toggle(head,space));AimAt(panel,100,170);aim.space=(XrSpace)3;
    panel.Input(aim,mode);CHECK(!panel.Open());
    // New settings share their drawing rectangles with the exact ray surface.
    Panel settings;CHECK(settings.Toggle(head,space));settings.Submitted(true);
    auto click=[&](int x,int y) {
        AimAt(settings,x,y);CHECK(settings.InputSettings(aim)==Action::None);
        AimAt(settings,x,y,1);return settings.InputSettings(aim);
    };
    CHECK(click(80,135)==Action::NisToggle);
    CHECK(click(80,181)==Action::NisLess);
    CHECK(click(550,181)==Action::NisMore);
    CHECK(click(80,227)==Action::Supersample);
    CHECK(click(300,85)==Action::None);CHECK(settings.Options().page==1);
    CHECK(click(80,135)==Action::Voice);CHECK(click(80,181)==Action::Text);
    CHECK(click(500,85)==Action::None);CHECK(settings.Options().page==2);
    CHECK(click(80,135)==Action::None); // controls are information, not settings
    settings.Close();CHECK(!settings.OwnInput(true));
    settings.ShowEntry(true,head,space);CHECK(settings.Entry()&&!settings.Open());
    CHECK(!settings.OwnInput(true));CHECK(settings.Surface().cursor.height==80);
    settings.Submitted(true);
    aim={};aim.frame=100;aim.menu=aim.tracked=true;aim.space=space;
    aim.pose={{0,0,0,1},{0,head.position.y-.48f,0}};
    CHECK(settings.InputSettings(aim)==Action::None);aim.frame++;aim.trigger=1;
    CHECK(settings.InputSettings(aim)==Action::None);CHECK(settings.Open()&&!settings.Entry());
    CHECK(settings.OwnInput(false));settings.Close();settings.ShowEntry(false,head,space);
    CHECK(!settings.Visible());
    CHECK(Neutral(true,0,0,0,0,0,0,0,0,0));
    for(unsigned i=0;i<8;++i){float values[8]{};values[i]=NAN;
        CHECK(!Neutral(true,0,values[0],values[1],values[2],values[3],values[4],values[5],values[6],values[7]));}
    CHECK(!Neutral(false,0,0,0,0,0,0,0,0,0));
    CHECK(!Neutral(true,1,0,0,0,0,0,0,0,0));
    CHECK(!Neutral(true,0,1,0,0,0,0,0,0,0));
    CHECK(!Neutral(true,0,0,0,.12f,.12f,0,0,0,0));
    CHECK(!Neutral(true,0,0,0,0,0,0,0,1,0));
    for(float yaw:{-2.f,-1.f,0.f,1.f,2.f}){
        Panel rotated;auto pose=head;
        pose.orientation={0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
        CHECK(rotated.Toggle(pose,space));rotated.Submitted(true);
        d2vr_menu::Aim centered;centered.frame=1;centered.time=1;
        centered.space=space;centered.tracked=centered.menu=true;centered.pose=pose;
        rotated.Input(centered,mode);
        CHECK(rotated.Pointer().visible);
        CHECK(std::abs(rotated.Pointer().hit.x-Width/2)<=1);
        CHECK(std::abs(rotated.Pointer().hit.y-Height/2)<=1);
    }
    // The submitted panel remains anchored when the controller translates.
    for(unsigned i=0;i<500;++i){
        Panel p;CHECK(p.Toggle(head,space));p.Submitted(true);
        int x=33+int(i%175),y=145+int(i%55);
        AimAt(p,x,y);p.Input(aim,mode);
        CHECK(p.Pointer().visible);CHECK(p.Pointer().hit.x==x&&p.Pointer().hit.y==y);
        CHECK(std::fabs(p.Surface().pose.position.z+1.5f)<1e-6f);
    }
    for(float yaw:{-3.f,-2.f,-1.f,0.f,1.f,2.f,3.f})for(int j=0;j<25;++j){
        Panel p;auto h=head;h.orientation={0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
        CHECK(p.Toggle(h,space));auto s=p.Surface();s.frame=1;
        d2vr_menu::Aim a{};a.frame=1;a.space=space;a.menu=a.tracked=true;a.pose=h;
        double q[4];CHECK(d2vr_menu::detail::Pose(h,q));
        const auto offset=d2vr_menu::detail::Rotate(q,{.05+j*.018,-.22+j*.014,0});
        a.pose.position.x+=float(offset.x);a.pose.position.y+=float(offset.y);a.pose.position.z+=float(offset.z);
        XrPosef beam{};XrExtent2Df size{};CHECK(Laser(s,a,h,beam,size));
        d2vr_menu::Hit hit;CHECK(d2vr_menu::Project(s,a,hit));
        CHECK(d2vr_menu::detail::Pose(beam,q));
        const auto y=d2vr_menu::detail::Rotate(q,{0,1,0});
        const auto z=d2vr_menu::detail::Rotate(q,{0,0,1});
        const double dx=hit.point.x-a.pose.position.x,dy=hit.point.y-a.pose.position.y,dz=hit.point.z-a.pose.position.z;
        const double len=std::sqrt(dx*dx+dy*dy+dz*dz);
        CHECK(std::fabs(size.height-(len-.022))<1e-5&&size.width==.002f);
        CHECK(std::fabs(y.x-dx/len)<1e-5&&std::fabs(y.y-dy/len)<1e-5&&std::fabs(y.z-dz/len)<1e-5);
        CHECK(z.x*(h.position.x-beam.position.x)+z.y*(h.position.y-beam.position.y)+
              z.z*(h.position.z-beam.position.z)>0);
        CHECK(std::fabs(beam.position.x+y.x*size.height*.5-(hit.point.x-dx/len*.002))<1e-5);
        CHECK(std::fabs(beam.position.y+y.y*size.height*.5-(hit.point.y-dy/len*.002))<1e-5);
        CHECK(std::fabs(beam.position.z+y.z*size.height*.5-(hit.point.z-dz/len*.002))<1e-5);
        const auto saved=beam;const auto savedSize=size;
        a.frame=2;CHECK(!Laser(s,a,h,beam,size));CHECK(beam.position.x==saved.position.x&&size.height==savedSize.height);
        a.frame=1;a.tracked=false;CHECK(!Laser(s,a,h,beam,size));
    }
    CHECK(NextSupersample(2676,2728,100)==125);
    CHECK(NextSupersample(2676,2728,125)==100);
    CHECK(NextSupersample(2000,2000,125)==150);
    CHECK(NextSupersample(2000,2000,150)==100);
    CHECK(NextSupersample(4096,4096,100)==100);
    CHECK(!SupersampleFits(0,2728,125));
    CHECK(!SupersampleFits(UINT32_MAX,UINT32_MAX,150));
    std::printf("PASS VR settings input and laser %u checks\n",checks);
}

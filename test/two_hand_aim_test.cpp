#include "../src/two_hand_aim.h"
#include "../src/stereo_math.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
static int n;
#define CHECK(x) do{++n;if(!(x)){std::printf("FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
    using namespace d2vr_twohand;
    for(float x:{-.25f,0.f,.25f})for(float y:{-.1f,0.f,.2f}) {
        XrPosef r{{0,0,0,1},{.2f,1.3f,-.3f}},s{};
        XrVector3f l{r.position.x+x,r.position.y+y,r.position.z-.3f};
        CHECK(Pose(r,l,s));CHECK(s.position.x==r.position.x&&s.position.z==r.position.z);
        float q[4]={s.orientation.x,s.orientation.y,s.orientation.z,s.orientation.w};
        float p[3]={s.position.x,s.position.y,s.position.z},m[16];
        CHECK(d2vr_math::TrackingPose(q,p,m));
        float len=std::sqrt(x*x+y*y+.09f);
        CHECK(std::fabs(m[8]-x/len)<1e-5f&&std::fabs(m[9]-y/len)<1e-5f&&std::fabs(m[10]-.3f/len)<1e-5f);
    }
    XrPosef r{{0,0,0,1},{0,0,0}},s{};
    CHECK(!Pose(r,{0,0,0},s));CHECK(!Pose(r,{0,0,.3f},s));
    CHECK(!Pose(r,{0,0,-2},s));CHECK(!Pose(r,{NAN,0,-.3f},s));
    r.orientation.w=NAN;CHECK(!Pose(r,{0,0,-.3f},s));r.orientation={0,.70710678f,0,.70710678f};
    CHECK(Pose(r,{-.3f,.1f,0},s));
    CHECK(SupportedCone(4,true)==2&&SupportedCone(4,false)==4);
    CHECK(SupportedCone(0,true)==0&&SupportedCone(-1,true)==-1);
    CHECK(std::isnan(SupportedCone(NAN,true)));
    std::printf("PASS %d two-hand direction, right origin, degenerate and cone checks\n",n);
}

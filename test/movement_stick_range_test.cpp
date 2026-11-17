#include "movement_stick_range.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <limits>
#include <cstdio>
using namespace trilex::input;
static bool near(float a,float b) { return std::fabs(a-b)<.00001f; }
int main() {
    for(int d:{0,10,20,40}) for(int angle=0;angle<360;angle++) {
        const float a=angle*.0174532925199433f;
        float previous=0;
        for(int step=0;step<=100;step++) {
            const float radius=step*.01f;
            const float rawX=std::cos(a)*radius,rawY=std::sin(a)*radius;
            float x=rawX,y=rawY; CalibrateMovementStick(x,y,d);
            const float mapped=std::hypot(x,y);
            assert(mapped+.00001f>=previous && mapped<=1.00001f);
            assert(near(x*rawY-y*rawX,0));
            if(!d) assert(x==rawX && y==rawY);
            if(radius>0 && radius<1.f-.01f*d-.00001f) assert(mapped>0 && mapped<1);
            if(radius>=1.f-.01f*d+.00001f) assert(near(mapped,1));
            previous=mapped;
        }
    }
    float x=0,y=.8f; CalibrateMovementStick(x,y,20); assert(near(y,1) && x==0);
    x=.2f;y=0; CalibrateMovementStick(x,y,20); assert(near(x,.25f) && y==0);
    x=1;y=1; CalibrateMovementStick(x,y,0); assert(x==1 && y==1);
    x=0;y=0; CalibrateMovementStick(x,y,20); assert(x==0 && y==0);
    x=std::numeric_limits<float>::quiet_NaN();y=.8f;CalibrateMovementStick(x,y,10);assert(x==0 && y==0);
    x=.8f;y=std::numeric_limits<float>::infinity();CalibrateMovementStick(x,y,10);assert(x==0 && y==0);
    assert(ClampOuterPercent(-10)==0 && ClampOuterPercent(100)==40);
    MovementStickProbe probe;
    assert(probe.Observe(0,0,0));assert(!probe.Observe(.9f,.8f,1));
    assert(!probe.Observe(-.7f,-.6f,4999));assert(probe.Observe(0,0,5000));
    assert(near(probe.peakForward,.8f) && near(probe.peakRight,.9f));
    assert(near(probe.peakBack,.6f) && near(probe.peakLeft,.7f));
    for(unsigned i=2;i<60;i++) assert(probe.Observe(0,0,i*5000ull));
    assert(!probe.Observe(0,0,9999999));
    std::puts("movement stick range: PASS (radial direction, monotonic analog speed, default passthrough, bounds, diagnostic budget)");
}

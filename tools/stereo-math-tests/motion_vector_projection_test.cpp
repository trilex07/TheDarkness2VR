#include "../../src/stereo_geometry.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Numerical regression for MotionBlur VS 73EBA724: current WVP c0 and
// PrevWorldProj c4 must describe the same eye. This tests projection math,
// not shader classification, native animation, or headset acceptance.
static unsigned checks = 0;
static void Check(bool value, const char* message) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static bool Near(float a, float b) { return std::fabs(a-b) < 2e-6f; }
struct Point { float x,y,z; };
struct Clip { float x,y,z,w; };
static Clip Project(const float m[16], Point p) {
    return {p.x*m[0]+p.y*m[4]+p.z*m[8]+m[12],
            p.x*m[1]+p.y*m[5]+p.z*m[9]+m[13],
            p.x*m[2]+p.y*m[6]+p.z*m[10]+m[14],
            p.x*m[3]+p.y*m[7]+p.z*m[11]+m[15]};
}
struct Velocity { float x,y; };
static Velocity Motion(Clip current, Clip previous) {
    Check(current.w > 0 && previous.w > 0, "test samples remain in front of camera");
    // Test the pre-clamp quantities: saturation would hide incorrect motion.
    return {(current.x/current.w-previous.x/previous.w)*8.f,
            (current.y/current.w-previous.y/previous.w)*-8.f};
}
static void Transform(const float in[16], float out[16],
                      const d2vr_stereo::EyeShiftParams& eye, float px,float py) {
    std::memcpy(out,in,16*sizeof(float));
    d2vr_stereo::ApplyEyeShiftToMatrix(out,4,eye,true,px,py);
    for(int i=0;i<16;i+=4)
        Check(out[i+2]==in[i+2] && out[i+3]==in[i+3], "projection retains z/w exactly");
}
int main() {
    using d2vr_stereo::PreviousProjectionBase;
    Check(PreviousProjectionBase(0x73EBA724u,0,4,true,false)==4,"exact shader/register pair enabled");
    Check(PreviousProjectionBase(0x73EBA724u,0,4,false,false)==-1,"default OFF preserves old path");
    Check(PreviousProjectionBase(0x73EBA724u,0,4,true,true)==-1,"unknown head history excluded");
    Check(PreviousProjectionBase(0x34D4EEE8u,0,4,true,false)==-1,"other tentacle shader excluded");
    Check(PreviousProjectionBase(0x73EBA724u,-1,4,true,false)==-1,"missing current reflection excluded");
    Check(PreviousProjectionBase(0x73EBA724u,0,-1,true,false)==-1,"missing previous reflection excluded");
    // Coefficients observed in capture; no native FOV or depth change.
    const float px=1.156956f, py=1.732051f;
    const float native[16]={px,0,0,0, 0,py,0,0,
        0,0,1.0001f,1, -0.0015f,-0.031177f,-0.10001f,0};
    const Point samples[]={{0,0,2},{0.32f,-0.11f,1},{-0.7f,0.23f,5}};
    const float scales[][2]={{1,1},{0.4692968f,0.4344307f}};
    for(int mode=0;mode<2;++mode) for(const auto& scale:scales) {
        float eyeMatrices[2][16]{};
        for(int eye=0;eye<2;++eye) {
            d2vr_stereo::EyeShiftParams sp;
            sp.mode=mode; sp.fovScaleX=scale[0]; sp.fovScaleY=scale[1];
            sp.transX=eye==0 ? 0.0325f:-0.0325f;
            sp.transY=0.004f;
            sp.separation=eye==0 ? 0.02f:-0.02f; sp.convergence=2.f;
            float current[16],previous[16];
            Transform(native,current,sp,px,py);
            Transform(native,previous,sp,px,py);
            std::memcpy(eyeMatrices[eye],current,sizeof current);
            for(Point point:samples) {
                const auto fixed=Motion(Project(current,point),Project(previous,point));
                Check(Near(fixed.x,0) && Near(fixed.y,0), "stationary point has zero motion in each eye");
                const auto old=Motion(Project(current,point),Project(native,point));
                // Shear's convergence plane is intentionally zero-disparity.
                if(mode==0 || point.z!=sp.convergence)
                    Check(std::fabs(old.x)+std::fabs(old.y)>1e-4f, "old current-only correction creates false motion");
                const Point moved{point.x+0.018f,point.y-0.012f,point.z};
                const auto nativeMotion=Motion(Project(native,moved),Project(native,point));
                const auto correctedMotion=Motion(Project(current,moved),Project(previous,point));
                Check(Near(correctedMotion.x,nativeMotion.x*scale[0]) &&
                      Near(correctedMotion.y,nativeMotion.y*scale[1]), "real lateral object motion preserved at render scale");
                Check(correctedMotion.x>0 && correctedMotion.y>0, "real motion is not cancelled or reversed");
            }
            // Different native matrices also represent object/camera motion;
            // aligning their eye must not collapse their native difference.
            float movedNative[16]; std::memcpy(movedNative,native,sizeof movedNative);
            movedNative[12]+=px*0.021f; movedNative[13]-=py*0.009f;
            float movedCurrent[16]; Transform(movedNative,movedCurrent,sp,px,py);
            const Point p{0.12f,0.08f,3};
            const auto raw=Motion(Project(movedNative,p),Project(native,p));
            const auto aligned=Motion(Project(movedCurrent,p),Project(previous,p));
            Check(Near(aligned.x,raw.x*scale[0]) && Near(aligned.y,raw.y*scale[1]),
                  "native matrix motion remains intact");
        }
        for(Point p:samples) {
            const Clip l=Project(eyeMatrices[0],p),r=Project(eyeMatrices[1],p);
            const float disparity=l.x/l.w-r.x/r.w;
            const float expected=mode==0 ? px*0.065f*scale[0]/p.z : 0.04f*(1.f-2.f/p.z);
            Check(Near(disparity,expected), "current stereo disparity unchanged by previous-matrix correction");
            if(mode==0 || p.z!=2.f) Check(std::fabs(disparity)>1e-4f,"stereo not collapsed to mono");
        }
    }
    std::printf("PASS: %u motion projection checks (both eyes, stationary/real motion, stereo, z/w)\n",checks);
}

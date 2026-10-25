// Compile the production refresh body; the builder double exposes its inputs
// so these tests verify ordering, both-eye freezing, and capture invalidation.
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
namespace d2vr_replay {
struct StereoParams { bool headGeometry=false; float fovScaleX=1,fovScaleY=1,headClip[16]{},inverseHeadClip[16]{}; };
float g_cameraPx=0,g_cameraPy=0,g_cameraInverseProjection[16]{};
bool g_cameraProjectionKnown=false;
}
using d2vr_replay::StereoParams;
StereoParams g_frameEyeParams[2]; bool g_frameEyeParamsValid=false;
float g_estPx=0,g_estPy=0,g_nativeCoveredPx=.5f,g_nativeCoveredPy=.6f;
LONG g_estValid=0; float g_perEyeCropX[2]{},g_perEyeCropY[2]{},g_edgeCrop=0;
int rejects=0,builds=0,checks=0;
void check(bool b) { ++checks; if(!b) { std::printf("FAIL %d\n",checks); std::exit(1); } }
void NativeCameraCaptureChanged() { ++rejects; }
StereoParams BuildEyeParams(int eye) {
    check(!g_frameEyeParamsValid); check(g_estValid==1);
    check(d2vr_replay::g_cameraProjectionKnown);
    check(g_estPx==g_nativeCoveredPx && g_estPy==g_nativeCoveredPy);
    ++builds; StereoParams p; p.headGeometry=true;
    std::memcpy(p.headClip,d2vr_replay::g_cameraInverseProjection,64);
    std::memcpy(p.inverseHeadClip,p.headClip,64);
    p.fovScaleX=1+eye*.1f;return p;
}
#include "d2vr_camera_refresh.inl"
int main() {
    float native[16]{};native[0]=2;native[5]=3;native[10]=4;native[15]=1;
    RefreshSelectedEyeParams(native,true);
    check(builds==2 && rejects==1 && g_frameEyeParamsValid);
    // A shader overwrites the shared estimate without any change of Px/Py.
    d2vr_replay::g_cameraInverseProjection[10]=999;
    RefreshSelectedEyeParams(native,false);
    check(builds==4 && rejects==1); // stable frozen frame remains capturable
    check(d2vr_replay::g_cameraInverseProjection[10]==4);
    check(g_frameEyeParams[0].headClip[10]==4 && g_frameEyeParams[1].headClip[10]==4);
    // Model the next frame having frozen from the polluted probe before callback.
    g_frameEyeParams[0].headClip[10]=999;g_frameEyeParams[1].headClip[10]=999;
    RefreshSelectedEyeParams(native,false);
    check(rejects==2 && g_frameEyeParams[0].headClip[10]==4 && g_frameEyeParams[1].headClip[10]==4);
    // Clip-plane change at identical horizontal/vertical FOV must reach BOTH eyes.
    native[10]=8; RefreshSelectedEyeParams(native,true);
    check(rejects==3 && g_frameEyeParams[0].headClip[10]==8 && g_frameEyeParams[1].headClip[10]==8);
    RefreshSelectedEyeParams(native,false);check(rejects==3 && builds==10);
    check(g_perEyeCropX[0]==1 && g_perEyeCropY[1]==1);
    std::printf("PASS %d camera refresh checks\n",checks);
}

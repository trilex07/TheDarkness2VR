#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <openxr/openxr.h>

#define CHECK(x) do { if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);return 1;} } while(0)
static void Log(const char*, ...) {}
static bool g_headGeometry=true,g_haveViews=true,g_in3DGameplayMode=true;
static bool g_headGeometryReferenceValid=true;
static XrPosef g_headGeometryReference{{0,0,0,1},{0,0,0}};
static bool g_clickHeadGeometryReferenceValid=false;
static XrPosef g_clickHeadGeometryReference{};
static struct {
    bool open=true;
    unsigned id=9;
    XrViewStateFlags flags=XR_VIEW_STATE_POSITION_TRACKED_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
} g_trackedFrame;
static XrPosef g_eyePose[2]={{{0,0,0,1},{2,1,3}},{{0,0,0,1},{4,1,3}}};
static XrPosef HeadCenterPose(const XrPosef& left,const XrPosef& right) {
    auto head=left;head.position.x=(left.position.x+right.position.x)*.5f;return head;
}
static float ComputeHorizontalYaw(const XrQuaternionf&) {return 1.f;}
#include "actual_head_recenter.inl"

int main() {
    CaptureClickRecenterReference();
    CHECK(g_headGeometryReferenceValid && g_headGeometryReference.position.x==0);
    g_eyePose[0].position.x=10;
    ApplyClickRecenterReference();
    CHECK(g_headGeometryReferenceValid && g_headGeometryReference.position.x==3);
    CHECK(g_headGeometryReference.orientation.x==0 && g_headGeometryReference.orientation.z==0);
    CHECK(std::abs(g_headGeometryReference.orientation.y-std::sin(.5f))<1e-6f);
    CHECK(!g_clickHeadGeometryReferenceValid);

    CaptureClickRecenterReference();
    g_trackedFrame.flags=XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
    CaptureClickRecenterReference();
    CHECK(g_headGeometryReferenceValid && g_headGeometryReference.position.x==3);
    ApplyClickRecenterReference();
    CHECK(!g_headGeometryReferenceValid && !g_clickHeadGeometryReferenceValid);

    g_trackedFrame.flags|=XR_VIEW_STATE_POSITION_TRACKED_BIT;
    CaptureClickRecenterReference();
    CHECK(g_clickHeadGeometryReferenceValid);
    InvalidateHeadGeometryReference();
    ApplyClickRecenterReference();
    CHECK(!g_headGeometryReferenceValid && !g_clickHeadGeometryReferenceValid);
    std::puts("PASS current-frame reference, click-pose handoff and tracking/reference loss");
    return 0;
}

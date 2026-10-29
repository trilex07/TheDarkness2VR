#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <openxr/openxr.h>
#include <intrin.h>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../src/adaptive_fps.h"
static unsigned checks=0,logs=0;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL line%d %s\n",__LINE__,#x);std::exit(1);}}while(0)
static d2vr_adaptive::Mode g_adaptiveFPSMode=d2vr_adaptive::Mode::Auto;
static d2vr_adaptive::Controller g_adaptiveFPS;
static thread_local d2vr_adaptive::Presentation g_adaptivePresentation;
static int g_submitMode=4,g_frameBoundaryPolicy=0;
static bool g_pairSubmitMetaValid=true;
static uint64_t g_eyePairFrameId=1;
static struct {uint64_t id=1;} g_trackedFrame;
static DWORD g_renderThreadId=0;
static XrSessionState g_xrSessionState=XR_SESSION_STATE_FOCUSED;
static struct {float Hz()const{return 90;}} g_displayRefresh;
static ULONGLONG tick=10000;
static ULONGLONG FixtureTick(){return tick;}
static void Log(const char*,...){++logs;SetLastError(0xBAD);}
#define GetTickCount64 FixtureTick
#include "adaptive_actual.inl"
#undef GetTickCount64
static void Reset(uint64_t frame=1) {
    g_adaptivePresentation={};g_adaptiveFPS={};g_adaptiveFPSMode=d2vr_adaptive::Mode::Auto;
    g_submitMode=4;g_frameBoundaryPolicy=0;g_pairSubmitMetaValid=true;
    g_eyePairFrameId=g_trackedFrame.id=frame;g_xrSessionState=XR_SESSION_STATE_FOCUSED;
    g_renderThreadId=GetCurrentThreadId();tick+=1000;
}
int main() {
    // Exercise the actual extracted host predicates, acknowledgement and RAII,
    // not a second implementation of the integration in this test.
    for(unsigned variant=0;variant<11;++variant) {
        Reset();const auto owner=BeginAdaptivePresentation();CHECK(owner!=0);
        XrResult xr=XR_SUCCESS;bool pixels=true;uint32_t layers=1;
        HRESULT native=D3D_OK;bool forwarded=true;
        switch(variant) {
            case 0:xr=XR_SESSION_LOSS_PENDING;break;
            case 1:xr=XR_ERROR_RUNTIME_FAILURE;break;
            case 2:pixels=false;break;
            case 3:layers=0;break;
            case 4:g_pairSubmitMetaValid=false;break;
            case 5:++g_eyePairFrameId;break;
            case 6:native=S_PRESENT_OCCLUDED;break;
            case 7:native=D3DERR_DEVICELOST;break;
            case 8:forwarded=false;break;
            case 9:g_xrSessionState=XR_SESSION_STATE_VISIBLE;break;
            default:break;
        }
        AdaptiveStereoSubmitted(xr,pixels,layers);EndAdaptivePresentation(owner,native,forwarded);
        CHECK(g_adaptiveFPS.State().frame==(variant==10 ? 1u:0u));
        CHECK(!g_adaptiveFPS.State().half);
    }
    Reset();g_adaptiveFPSMode=d2vr_adaptive::Mode::Off;CHECK(!BeginAdaptivePresentation());
    Reset();g_submitMode=1;CHECK(!BeginAdaptivePresentation());
    Reset();g_frameBoundaryPolicy=1;CHECK(!BeginAdaptivePresentation());
    Reset();
    {
        AdaptivePresentationScope scope;AdaptiveStereoSubmitted(XR_SUCCESS,true,1);
    } // an unreturned/aborted native call cannot leave a reusable receipt
    auto token=BeginAdaptivePresentation();CHECK(token!=0);EndAdaptivePresentation(token,D3D_OK);
    CHECK(!g_adaptiveFPS.State().frame);
    {
        AdaptivePresentationScope outer;
        {
            AdaptivePresentationScope inner;AdaptiveStereoSubmitted(XR_SUCCESS,true,1);
            inner.Returned(D3D_OK);
        }
        CHECK(!g_adaptiveFPS.State().frame);outer.Returned(D3D_OK);
    }
    CHECK(g_adaptiveFPS.State().frame==1);
    Reset();
    for(uint64_t frame=1;frame<=1000;++frame) {
        g_eyePairFrameId=g_trackedFrame.id=frame;tick+=17;
        AdaptivePresentationScope scope;AdaptiveStereoSubmitted(XR_SUCCESS,true,1);
        scope.Returned(D3D_OK);CHECK(g_adaptiveFPS.State().frame==frame);
    }
    CHECK(g_adaptiveFPS.State().status==d2vr_adaptive::Status::Unsupported);
    CHECK(!g_adaptiveFPS.State().half);CHECK(g_adaptiveFPS.State().fps>58 && g_adaptiveFPS.State().fps<60);
    Reset(300);
    __declspec(align(16)) unsigned char original[512]{},before[512]{},after[512]{};
    _fxsave(original);
    __asm fld1
    __asm fldpi
    _fxsave(before);
    SetLastError(0x12345);
    {
        AdaptivePresentationScope scope;AdaptiveStereoSubmitted(XR_SUCCESS,true,1);
        scope.Returned(D3D_OK);
    }
    const DWORD error=GetLastError();
    _fxsave(after);_fxrstor(original);
    CHECK(error==0x12345);CHECK(logs>0);
    CHECK(!memcmp(before,after,8)); // x87 control/status/tag, including live stack
    CHECK(!memcmp(before+24,after+24,8)); // MXCSR and mask
    for(unsigned i=0;i<8;++i)CHECK(!memcmp(before+32+i*16,after+32+i*16,10));
    Reset();
    std::thread other([]{
        AdaptivePresentationScope scope;AdaptiveStereoSubmitted(XR_SUCCESS,true,1);scope.Returned(D3D_OK);
    });other.join();CHECK(!g_adaptiveFPS.State().frame);
    {
        AdaptivePresentationScope scope;AdaptiveStereoSubmitted(XR_SUCCESS,true,1);scope.Returned(D3D_OK);
    }
    CHECK(g_adaptiveFPS.State().frame==1);
    std::printf("PASS actual x86 adaptive host integration: %u checks; exact XR/D3D success, fresh stereo identity, nested forwarding, aborted scopes, render-thread/focus ownership, unique app cadence, LastError and live x87/MXCSR preservation. Game/HMD pacing NOT RUN.\n",checks);
    return 0;
}

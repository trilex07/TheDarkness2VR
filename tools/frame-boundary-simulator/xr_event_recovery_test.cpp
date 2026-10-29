// Actual callback + PumpXR + admission bodies, native external COM fixture,
// and deterministic XR event queue. No graphics device or Steam is launched.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define XR_NO_PROTOTYPES
#include <windows.h>
#include <openxr/openxr.h>
#include "../../src/image_present_observer.h"
#include "../../src/capture_present_witness.h"
#include "../../src/frame_boundary.h"
#include "../../src/vr_settings.h"
#include "image_present_fixture.h"
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <thread>
static unsigned checks=0;
static void Check(bool value){++checks;if(!value){std::printf("FAIL check %u\n",checks);std::exit(1);}}
static void Log(const char*,...){}
static d2vr_image_present::Observer g_imagePresent;
static d2vr_native_image::Guard g_nativeImageGuard;
static d2vr_frame::BoundaryMachine g_frameBoundary;
static d2vr_capture::PresentWitness g_capturePresent;
static bool g_imagePreservingPresent=true,g_disableOpenXR=false,g_xrReady=true,g_xrRunning=true;
static bool g_insidePresentation=false,g_presentReadiness=true,g_anchorValid=true,g_vrRefValid=true;
static bool g_lockedHeadValid=true,g_headGeometryReferenceValid=true,g_lastWorldHeadContextValid=true;
static bool g_hideGameWindow=false,g_renderHaveViews=true;
static HWND g_gameHwnd=nullptr;
static LONG g_captureResetting=0,g_frameId=0,g_stereoPresentBoundarySeen=0;
static LONG g_swapChainPresentCount=0,g_presentSeen=0,g_needRecBegin=0;
static int g_testMode=0,g_submitMode=4,g_stereoMode=1,g_trackingSubMode=1;
static unsigned g_srcW=171,g_srcH=113;
static uintptr_t g_presentCaller=0;
static XrInstance g_xrInstance=static_cast<XrInstance>(0x1110);
static XrSession g_xrSession=static_cast<XrSession>(0x2220);
static XrSessionState g_xrSessionState=XR_SESSION_STATE_SYNCHRONIZED;
struct Frame {bool open=false;uint64_t id=0;} g_trackedFrame;
struct Hand {bool valid=true,gripValid=true,aimValid=true;struct {bool valid=true;} motion;} g_hands[2];
static d2vr_settings::Panel g_vrSettings;
static d2vr_settings::ToggleAction g_vrSettingsToggle;
static unsigned settingsReleases=0;
static void ReleaseVrSettingsRenderer(){++settingsReleases;}
static struct {void DiscardFrames(){}} g_adaptiveFPS;
static XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance,const char*,PFN_xrVoidFunction*){return XR_ERROR_FUNCTION_UNSUPPORTED;}
static struct {
    void Start(XrInstance,XrSession,PFN_xrGetInstanceProcAddr){}void Stop(){}
    float Hz()const{return 0;}bool Enabled()const{return false;}
    bool Event(const XrEventDataBuffer&){return false;}
} g_displayRefresh;
static struct Chord {unsigned held=0;} g_recenterChord;
static struct Chord g_triangleRecenter;
static uint64_t BeginAdaptivePresentation(){return 1;}
static void EndAdaptivePresentation(uint64_t,HRESULT,bool){}
static unsigned inputResets=0,inputClears=0,eventPolls=0,sessionBegins=0,sessionEnds=0;
static unsigned frameLeaves=0,frameWaits=0,frameBegins=0,frameEnds=0,recordReleases=0;
static unsigned captureReturns=0,captureRejects=0,gpuReleases=0;
static uint64_t timestamp=1;
static std::deque<XrSessionState> events;
static bool referencePending=false,instancePending=false,qualified=true;
static void ResetPoseInput(){++inputResets;}
static void PublishXrPad(void*){++inputClears;}
static XrResult xrPollEvent(XrInstance instance,XrEventDataBuffer* buffer){
    Check(instance==g_xrInstance && g_imagePresent.OwnsCurrentNativePresent());
    ++eventPolls;
    if(instancePending){
        instancePending=false;
        *reinterpret_cast<XrEventDataInstanceLossPending*>(buffer)={XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING};
        return XR_SUCCESS;
    }
    if(referencePending){
        referencePending=false;
        *reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(buffer)={XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING};
        return XR_SUCCESS;
    }
    if(events.empty())return XR_EVENT_UNAVAILABLE;
    auto* event=reinterpret_cast<XrEventDataSessionStateChanged*>(buffer);
    *event={XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED};event->session=g_xrSession;
    event->state=events.front();events.pop_front();return XR_SUCCESS;
}
static XrResult xrBeginSession(XrSession session,const XrSessionBeginInfo*){
    Check(session==g_xrSession);++sessionBegins;return XR_SUCCESS;
}
static XrResult xrEndSession(XrSession session){Check(session==g_xrSession);Check(settingsReleases==sessionEnds+1);++sessionEnds;return XR_SUCCESS;}
namespace d2vr_cpu_sync {
enum Kind {BoundaryWork};
struct Scope {explicit Scope(Kind){}};
static struct Timing {void Report(unsigned,unsigned,uint64_t){}} timing;
}
namespace d2vr_replay {static void ReleaseRecording(){++recordReleases;}}
static struct GpuTiming {void Release(){++gpuReleases;}} g_gpuTiming;
namespace matched_capture {
static void Reject(const char*){++captureRejects;}
static void PresentReturned(HRESULT,bool forwarded){Check(forwarded);++captureReturns;}
}
static d2vr_frame::Event MakePipelineEvent(d2vr_frame::EventType type){
    d2vr_frame::Event e{};e.type=type;e.timestampMs=timestamp++;e.threadId=GetCurrentThreadId();return e;
}
void PumpXR();
static void VrFrame(IDirect3DDevice9*,const char*){
    // Stand-in only for the GPU leaf. Its admission and Present callbacks are
    // actual source. Preserve the ready-path's single BeginTrackedFrame pump.
    Check(g_imagePresent.Ready() && g_capturePresent.active);
    ++frameLeaves;++frameWaits;++frameBegins;++frameEnds;++g_trackedFrame.id;
    PumpXR();
}
#include "xr_event_recovery_under_test.inl"
static bool Qualify(d2vr_image_present::Method method,uintptr_t caller){
    return caller && (method!=d2vr_image_present::Method::Present || qualified);
}
int main(int argc,char** argv){
    Check(argc==2);
    auto dll=LoadLibraryA(argv[1]);Check(dll!=nullptr);
    auto deviceFn=reinterpret_cast<IDirect3DDevice9*(__cdecl*)()>(GetProcAddress(dll,"FixtureDevice"));
    auto counters=reinterpret_cast<const FixtureStats*(__cdecl*)()>(GetProcAddress(dll,"FixtureCounters"));
    auto expect=reinterpret_cast<void(__cdecl*)(const RECT*,const RECT*,HWND,const RGNDATA*,DWORD,HRESULT)>(GetProcAddress(dll,"FixtureExpect"));
    Check(deviceFn && counters && expect);auto device=deviceFn();
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto pe=reinterpret_cast<IMAGE_NT_HEADERS*>(image+reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_lfanew);
    g_nativeImageGuard={true,image,pe->OptionalHeader.SizeOfImage};
    d2vr_image_present::Callbacks cb{Qualify,ImagePresentBefore,ImagePresentAfter,Log};
    Check(g_imagePresent.Start(device,g_nativeImageGuard,cb));
    D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=171;pp.BackBufferHeight=113;
    IDirect3DSwapChain9* chain=nullptr;Check(device->CreateAdditionalSwapChain(&pp,&chain)==S_OK);
    auto Present=[&](HRESULT result){
        const auto native=counters()->native;
        expect(nullptr,nullptr,nullptr,nullptr,0,result);
        Check(chain->Present(nullptr,nullptr,nullptr,nullptr,0)==result);
        Check(counters()->native==native+1 && !g_insidePresentation);
    };
    Check(!g_imagePresent.Ready() && !g_imagePresent.OwnsCurrentNativePresent());
    events={XR_SESSION_STATE_READY,XR_SESSION_STATE_SYNCHRONIZED};
    Present(D3D_OK);Check(g_imagePresent.Ready() && frameLeaves==0 && sessionBegins==1);
    const auto polls=eventPolls;
    Present(D3D_OK);Check(frameLeaves==1 && eventPolls==polls+1 && captureReturns==1);
    Present(S_PRESENT_OCCLUDED);Check(!g_imagePresent.Ready() && frameLeaves==2);
    const auto frames=frameLeaves,waits=frameWaits,begins=frameBegins,ends=frameEnds;
    const auto refs=counters()->adds,releases=counters()->releases;
    const auto serial=g_capturePresent.serial;
    events={XR_SESSION_STATE_VISIBLE,XR_SESSION_STATE_FOCUSED};
    Present(S_PRESENT_OCCLUDED);
    Check(g_xrSessionState==XR_SESSION_STATE_FOCUSED && events.empty());
    Check(!g_imagePresent.Ready() && frameLeaves==frames && frameWaits==waits && frameBegins==begins && frameEnds==ends);
    Check(g_capturePresent.serial==serial && !g_capturePresent.active);
    Check(counters()->adds==refs && counters()->releases==releases); // no GetDevice/AddRef at all
    const auto wokePolls=eventPolls;
    Present(S_PRESENT_OCCLUDED);Check(eventPolls==wokePolls+1 && frameLeaves==frames);
    Present(D3D_OK);Check(g_imagePresent.Ready() && frameLeaves==frames); // warm only
    const auto readyPolls=eventPolls;
    Present(D3D_OK);Check(frameLeaves==frames+1 && eventPolls==readyPolls+1); // single ordinary pump

    Present(S_PRESENT_OCCLUDED);
    auto NoPump=[&]{const auto before=eventPolls,leaves=frameLeaves;Present(S_PRESENT_OCCLUDED);Check(eventPolls==before && frameLeaves==leaves);};
    g_xrReady=false;NoPump();g_xrReady=true;
    g_disableOpenXR=true;NoPump();g_disableOpenXR=false;
    g_imagePreservingPresent=false;NoPump();g_imagePreservingPresent=true;
    g_nativeImageGuard.enabled=false;NoPump();g_nativeImageGuard.enabled=true;
    const auto instance=g_xrInstance;g_xrInstance=XR_NULL_HANDLE;NoPump();g_xrInstance=instance;
    const auto session=g_xrSession;g_xrSession=XR_NULL_HANDLE;NoPump();g_xrSession=session;
    g_captureResetting=1;NoPump();g_captureResetting=0;
    qualified=false;NoPump();qualified=true; // unrelated overlay cannot pump
    g_imagePresent.ResetBegin();NoPump();
    g_imagePresent.ResetEnd(false);NoPump();
    g_imagePresent.ResetEnd(true);
    const auto resetPolls=eventPolls;Present(S_PRESENT_OCCLUDED);Check(eventPolls==resetPolls+1);

    // Existing PumpXR lifecycle remains intact even with frame admission shut.
    Check(g_vrSettings.Toggle({{0,0,0,1},{0,1.6f,0}},static_cast<XrSpace>(0x3330)));
    g_vrSettingsToggle.Update(true,false);
    g_trackedFrame.open=true;events={XR_SESSION_STATE_STOPPING};
    Present(S_PRESENT_OCCLUDED);
    Check(sessionEnds==1 && !g_xrRunning && !g_trackedFrame.open);
    Check(!g_hands[0].valid && !g_hands[1].motion.valid);
    Check(!g_vrSettings.Open()&&!g_vrSettingsToggle.Update(true,true));
    events={XR_SESSION_STATE_READY,XR_SESSION_STATE_SYNCHRONIZED,XR_SESSION_STATE_FOCUSED};
    Present(S_PRESENT_OCCLUDED);Check(sessionBegins==2 && g_xrRunning);
    Check(g_vrSettings.Toggle({{0,0,0,1},{0,1.6f,0}},static_cast<XrSpace>(0x3330)));
    referencePending=true;Present(S_PRESENT_OCCLUDED);Check(!g_vrSettings.Open());
    Check(!g_vrRefValid && !g_lockedHeadValid && !g_headGeometryReferenceValid && !g_lastWorldHeadContextValid);
    Check(g_vrSettings.Toggle({{0,0,0,1},{0,1.6f,0}},static_cast<XrSpace>(0x3330)));
    instancePending=true;Present(S_PRESENT_OCCLUDED);Check(!g_vrSettings.Open());
    Check(frameWaits==frameBegins && frameBegins==frameEnds);

    // Foreign thread/fault must forward without touching render-owned XR.
    const auto priorPolls=eventPolls,priorFrames=frameLeaves;
    std::thread foreign([&]{Present(S_PRESENT_OCCLUDED);});foreign.join();
    Check(g_imagePresent.Failed() && !g_imagePresent.Ready() && eventPolls==priorPolls && frameLeaves==priorFrames);
    NoPump();Check(g_imagePresent.recursive==0 && captureRejects==0);
    Check(g_imagePresent.forwarded==g_imagePresent.returned && g_imagePresent.errors==0);
    Check(chain->Release()==0);
    std::printf("XR recovery actual-source callbacks/lifecycle/admission: %u checks PASS; frames=%u polls=%u; no occluded frame or device calls\n",checks,frameLeaves,eventPolls);
}

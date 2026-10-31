// Exercises the actual capture-consumption gate and native camera notification.
#include <cstdint>
#include <cstdio>
#include <string>
#include <cstdlib>
static unsigned GetCurrentThreadId() { return 3; }
static struct Present {
    uintptr_t device=8;
    bool valid=true;
    bool InCaptureWindow(uint64_t,uint64_t,uint64_t,unsigned,uint64_t) {return valid;}
} g_capturePresent;
static bool g_captureResetting=false;
static int g_captureResetGeneration=2;
static uint64_t g_frameId=7;
static struct Frame {uint64_t id=7;bool open=true;} g_trackedFrame;
static bool g_nativeCameraCoverInstalled=true,g_nativeCameraCoverConfirmed=true;
static uint64_t g_nativeCoveredFrame=7;
static std::string settings="armed";
namespace matched_capture {
static bool active=true,failed=false,headerPending=true;
static unsigned mask;
#include "matched_skin_window.inl"
static uint64_t frame=7,presentSerial=9,presentEpoch=1,presentFrame=7;
static int resetGeneration=2,headers=0;
static uintptr_t presentDevice=8;
static std::string configToken="armed";
static std::string ConfigToken(){return settings;}
static void Reject(const char*){failed=true;}
static void WriteHeader(){++headers;headerPending=false;}
#include "matched_current.inl"
}
#include "matched_camera_changed.inl"
static int checks;
static void Check(bool condition) {++checks;if(!condition){std::printf("FAIL %d\n",checks);std::exit(1);}}
static void Reset() {
    using namespace matched_capture;
    active=true;failed=false;headerPending=true;headers=0;
    g_capturePresent.valid=true;g_capturePresent.device=8;
    g_captureResetting=false;g_captureResetGeneration=2;
    g_trackedFrame.id=7;g_trackedFrame.open=true;
    g_nativeCameraCoverInstalled=true;g_nativeCameraCoverConfirmed=true;
    g_nativeCoveredFrame=7;settings="armed";
}
int main() {
    using namespace matched_capture;
    Reset();NativeCameraCaptureChanged();Check(!failed);Check(Current());Check(headers==1);
    Check(Current());Check(headers==1);NativeCameraCaptureChanged();Check(failed);Check(!Current());
    Reset();g_nativeCoveredFrame=6;Check(!Current());Check(failed&&headers==0);
    Reset();g_nativeCameraCoverConfirmed=false;Check(!Current());Check(headers==0);
    Reset();g_nativeCameraCoverInstalled=false;Check(Current());Check(headers==1);
    Reset();settings="changed";Check(!Current());Check(headers==0);
    Reset();Check(Current());settings="changed";Check(!Current());
    Reset();g_capturePresent.valid=false;Check(!Current());Check(headers==0);
    Reset();g_captureResetting=true;Check(!Current());Check(headers==0);
    Reset();g_captureResetGeneration=3;Check(!Current());Check(headers==0);
    Reset();g_capturePresent.device=9;Check(!Current());Check(headers==0);
    Reset();g_trackedFrame.id=8;Check(!Current());Check(headers==0);
    Reset();g_trackedFrame.open=false;Check(!Current());Check(headers==0);
    Reset();active=false;NativeCameraCaptureChanged();Check(!failed);Check(!Current());Check(headers==0);
    Reset();failed=true;Check(!Current());Check(headers==0);
    Reset();mask=0;Check(LiveSkinCaptureWindow());
    mask=1;Check(!LiveSkinCaptureWindow());mask=3;Check(!LiveSkinCaptureWindow());
    mask=15;Check(!LiveSkinCaptureWindow());mask=0;active=false;Check(!LiveSkinCaptureWindow());
    std::printf("PASS %d capture header checks (actual source)\n",checks);
}

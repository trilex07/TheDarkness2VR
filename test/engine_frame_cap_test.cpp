#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <initializer_list>
#include <cstdio>
static void Log(const char*, ...) {}
struct Refresh { float Hz() const { return 90.f; } } g_displayRefresh;
#include "../src/engine_frame_cap.inl"
int main() {
    for(const char* s : {"", "0", "-1", "29", "1001", "90junk", "nan", "inf", "1e300", "AUTOjunk"}) {
        ParseEngineMaxFrameRate(s);
        assert(g_engineMaxFrameRate==0 && !g_engineMaxFrameRateAuto && !g_engineMaxFrameRateObserve);
    }
    for(const char* s : {"30", "90", "90.5", "1000", "120 "}) {
        ParseEngineMaxFrameRate(s); assert(g_engineMaxFrameRate>=30 && g_engineMaxFrameRate<=1000);
    }
    ParseEngineMaxFrameRate("auto"); assert(g_engineMaxFrameRateAuto);
    ParseEngineMaxFrameRate("OBSERVE"); assert(g_engineMaxFrameRateObserve && !g_engineMaxFrameRateAuto);
    ParseEngineMaxFrameRate(nullptr); assert(!g_engineMaxFrameRateObserve && g_engineMaxFrameRate==0);
    // This host test is not DarknessII.exe: layout guard must refuse access.
    assert(!VerifyEngineFrameCap());
    ParseEngineMaxFrameRate("90"); ApplyEngineFrameCap();
    puts("engine_frame_cap: parser and foreign-image refusal PASS");
}

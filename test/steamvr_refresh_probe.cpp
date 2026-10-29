#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/steamvr_refresh_native.h"
#include <cstdio>
int main() {
    d2vr_refresh::SteamReader<d2vr_refresh::SteamNative> reader;
    if(!reader.Start("SteamVR")) {
        std::puts("UNSUPPORTED: background OpenVR client/interface unavailable; no game or scene submission");
        return 3;
    }
    const float hz=reader.Read();reader.Stop();
    if(!hz){std::puts("UNKNOWN: no connected HMD with a valid DisplayFrequency property");return 3;}
    std::printf("READ_ONLY physical HMD DisplayFrequency=%.3f Hz; background client stopped; no scene, settings or pacing changes\n",double(hz));
}

#include "../src/xr_display_refresh.h"
#include "../src/steamvr_refresh.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using d2vr_refresh::Display;
static unsigned checks=0,enumerations=0,resolutions=0,queries=0;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static int enumerationMode=0;
static XrResult resolutionResult=XR_SUCCESS,queryResult=XR_SUCCESS;
static float queryHz=90;
static bool nullFunction=false;
static XrInstance instance=(XrInstance)1;
static XrSession session=(XrSession)2;
struct FakeSteam {
    bool load=true,init=true,system=true;
    unsigned loads=0,inits=0,systems=0,reads=0,stops=0,unloads=0;
    float hz=120;
    bool Load(){++loads;return load;}
    bool Initialize(){++inits;return init;}
    bool System(){++systems;return system;}
    float Frequency(){++reads;return hz;}
    void Shutdown(){++stops;}
    void Unload(){++unloads;}
};
static XrResult XRAPI_CALL Enumerate(const char* layer,uint32_t capacity,uint32_t* count,XrExtensionProperties* properties) {
    ++enumerations;CHECK(!layer);
    if(enumerationMode==1)return XR_ERROR_RUNTIME_FAILURE;
    if(!capacity){CHECK(!properties);*count=enumerationMode==2?1025:enumerationMode==3?0:2;return XR_SUCCESS;}
    CHECK(properties && capacity==2);
    for(uint32_t i=0;i<capacity;++i)CHECK(properties[i].type==XR_TYPE_EXTENSION_PROPERTIES && !properties[i].next);
    if(enumerationMode==4 || (enumerationMode==5 && enumerations==2))return XR_ERROR_SIZE_INSUFFICIENT;
    if(enumerationMode==6)return XR_ERROR_RUNTIME_FAILURE;
    *count=enumerationMode==7?3:2;
    strcpy_s(properties[0].extensionName,"XR_KHR_D3D11_enable");
    strcpy_s(properties[1].extensionName,enumerationMode==8?"XR_FB_display_refresh_rate_WRONG":XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
    return XR_SUCCESS;
}
static XrResult XRAPI_CALL Query(XrSession received,float* hz) {
    ++queries;CHECK(received==session && hz);*hz=queryHz;return queryResult;
}
static XrResult XRAPI_CALL Resolve(XrInstance received,const char* name,PFN_xrVoidFunction* output) {
    ++resolutions;CHECK(received==instance && !strcmp(name,"xrGetDisplayRefreshRateFB") && output);
    *output=nullFunction?nullptr:reinterpret_cast<PFN_xrVoidFunction>(&Query);return resolutionResult;
}
static void Reset() {enumerations=resolutions=queries=0;enumerationMode=0;resolutionResult=queryResult=XR_SUCCESS;queryHz=90;nullFunction=false;}
static XrEventDataBuffer Event(float hz) {
    XrEventDataBuffer buffer{XR_TYPE_EVENT_DATA_BUFFER};
    XrEventDataDisplayRefreshRateChangedFB change{XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB};
    change.fromDisplayRefreshRate=90;change.toDisplayRefreshRate=hz;
    static_assert(sizeof(change)<=sizeof(buffer));memcpy(&buffer,&change,sizeof(change));return buffer;
}
int main() {
    {
        d2vr_refresh::SteamReader<FakeSteam> steam;
        auto& backend=steam.Backend();
        for(const char* name:{"", "Ste", "SteamVRfake", "Windows Mixed Reality", "Oculus"})
            CHECK(!steam.Start(name));
        CHECK(!steam.Start(nullptr));CHECK(!backend.loads && !steam.Read());
        backend.load=false;CHECK(!steam.Start("SteamVR"));CHECK(!backend.inits && !backend.unloads);
        backend.load=true;backend.init=false;CHECK(!steam.Start("SteamVR"));CHECK(backend.unloads==1 && !backend.stops);
        backend.init=true;backend.system=false;CHECK(!steam.Start("SteamVR/OpenXR"));
        CHECK(backend.stops==1 && backend.unloads==2);
        backend.system=true;CHECK(steam.Start("SteamVR/OpenXR"));CHECK(steam.Read()==120);
        backend.hz=119.88f;CHECK(steam.Read()==119.88f);
        for(float hz:{0.f,-90.f,1001.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
            backend.hz=hz;CHECK(steam.Read()==0);
        }
        steam.Stop();CHECK(!steam.Read());CHECK(backend.stops==2 && backend.unloads==3);
        steam.Stop();CHECK(backend.stops==2 && backend.unloads==3);
        CHECK(steam.Start("steamvr"));CHECK(!steam.Start("Other runtime"));
        CHECK(backend.stops==3 && backend.unloads==4);
    }
    {
        Display fallback;
        CHECK(fallback.Fallback(90));CHECK(fallback.Hz()==90 && fallback.FallbackActive());
        CHECK(!fallback.Fallback(0));CHECK(!fallback.Hz() && fallback.FallbackActive());
        CHECK(fallback.Fallback(120));CHECK(!fallback.Event(Event(72)));CHECK(fallback.Hz()==120);
        fallback.Stop();CHECK(!fallback.Hz() && !fallback.FallbackActive());
        CHECK(fallback.Fallback(72));fallback.Enable(false);CHECK(!fallback.FallbackActive());
        Reset();CHECK(fallback.Discover(&Enumerate));fallback.Enable(true);
        CHECK(fallback.Start(instance,session,&Resolve));CHECK(!fallback.Fallback(120));
        CHECK(fallback.Hz()==90 && !fallback.FallbackActive());
    }
    for(int mode=0;mode<=8;++mode) {
        Reset();enumerationMode=mode;Display display;
        const bool supported=display.Discover(&Enumerate);
        CHECK(supported==(mode==0 || mode==5));
        display.Enable(true);CHECK(display.Enabled()==supported);
        CHECK(display.Start(instance,session,&Resolve)==supported);
        CHECK(queries==(supported?1u:0u));CHECK(resolutions==(supported?1u:0u));
        CHECK(enumerations<=4);CHECK(display.Hz()==(supported?90.f:0.f));
    }
    Reset();Display display;CHECK(!display.Discover(nullptr));display.Enable(true);
    CHECK(!display.Enabled());CHECK(!display.Start(instance,session,&Resolve));CHECK(!resolutions);
    CHECK(display.Discover(&Enumerate));display.Enable(false);
    CHECK(!display.Start(instance,session,&Resolve));CHECK(!resolutions);
    display.Enable(true);
    CHECK(!display.Start(XR_NULL_HANDLE,session,&Resolve));
    CHECK(!display.Start(instance,XR_NULL_HANDLE,&Resolve));
    CHECK(!display.Start(instance,session,nullptr));CHECK(!resolutions);
    for(auto error:{XR_ERROR_FUNCTION_UNSUPPORTED,XR_ERROR_RUNTIME_FAILURE,XR_SESSION_LOSS_PENDING}) {
        resolutionResult=error;CHECK(!display.Start(instance,session,&Resolve));CHECK(display.Hz()==0 && queries==0);
    }
    resolutionResult=XR_SUCCESS;nullFunction=true;CHECK(!display.Start(instance,session,&Resolve));CHECK(!queries);
    nullFunction=false;
    for(auto error:{XR_ERROR_SESSION_LOST,XR_ERROR_RUNTIME_FAILURE,XR_SESSION_LOSS_PENDING}) {
        queryResult=error;CHECK(!display.Start(instance,session,&Resolve));CHECK(display.Hz()==0);
        CHECK(!display.Event(Event(120)));CHECK(display.Hz()==0);
    }
    queryResult=XR_SUCCESS;
    const float invalid[]={0,-90,.5f,1001,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()};
    for(float rate:invalid){queryHz=rate;CHECK(!display.Start(instance,session,&Resolve));CHECK(display.Hz()==0);CHECK(!display.Event(Event(120)));}
    for(float rate:{72.f,80.f,90.f,120.f,144.f,119.88f}) {
        queryHz=rate;CHECK(display.Start(instance,session,&Resolve));CHECK(display.Hz()==rate);
        CHECK(display.Event(Event(72)));CHECK(display.Hz()==72);
        for(float value:invalid){CHECK(display.Event(Event(value)));CHECK(display.Hz()==0);}
        CHECK(display.Event(Event(rate)));CHECK(display.Hz()==rate);
        XrEventDataBuffer unrelated{XR_TYPE_EVENT_DATA_BUFFER};CHECK(!display.Event(unrelated));CHECK(display.Hz()==rate);
        display.Stop();CHECK(display.Hz()==0 && display.Enabled());CHECK(!display.Event(Event(120)));CHECK(display.Hz()==0);
    }
    queryHz=120;CHECK(display.Start(instance,session,&Resolve));
    enumerationMode=8;CHECK(!display.Discover(&Enumerate));CHECK(!display.Enabled() && display.Hz()==0);
    CHECK(!display.Event(Event(90)));CHECK(!display.Start(instance,session,&Resolve));
    std::printf("PASS x86 refresh adapter: %u checks; optional discovery, bounded enumeration retry, unsupported/error/loss refusal, fractional Hz, event changes and stopped-session invalidation; no runtime/game execution\n",checks);
}

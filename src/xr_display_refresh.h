#pragma once
#include <openxr/openxr.h>
#include <cmath>
#include <cstring>
#include <vector>

namespace d2vr_refresh {
// The predicted display period is application pacing, not proof of physical
// refresh. This adapter only accepts the enabled runtime extension's result.
class Display {
    bool advertised_=false,enabled_=false;
    float hz_=0;
    PFN_xrGetDisplayRefreshRateFB get_=nullptr;
    bool fallback_=false;
    static bool Valid(float hz) {return std::isfinite(hz) && hz>=1 && hz<=1000;}
public:
    bool Discover(PFN_xrEnumerateInstanceExtensionProperties enumerate) {
        advertised_=enabled_=false;Stop();
        if(!enumerate)return false;
        for(unsigned attempt=0;attempt<2;++attempt) {
            uint32_t count=0;
            if(enumerate(nullptr,0,&count,nullptr)!=XR_SUCCESS || !count || count>1024)return false;
            std::vector<XrExtensionProperties> extensions(count);
            for(auto& extension:extensions)extension={XR_TYPE_EXTENSION_PROPERTIES};
            uint32_t written=0;
            const auto result=enumerate(nullptr,count,&written,extensions.data());
            if(result==XR_ERROR_SIZE_INSUFFICIENT)continue;
            if(result!=XR_SUCCESS || written>count)return false;
            for(uint32_t i=0;i<written;++i)
                if(std::strncmp(extensions[i].extensionName,XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME,
                                XR_MAX_EXTENSION_NAME_SIZE)==0) {
                    advertised_=true;return true;
                }
            return false;
        }
        return false;
    }
    void Enable(bool requested) {enabled_=requested && advertised_;Stop();}
    bool Enabled() const {return enabled_;}
    float Hz() const {return hz_;} // zero means unknown; never substitute 90
    void Stop() {hz_=0;get_=nullptr;fallback_=false;}
    bool FallbackActive()const{return fallback_;}
    bool Fallback(float hz) {
        if(get_)return false; // enabled successful OpenXR query always wins
        fallback_=true;hz_=Valid(hz)?hz:0;return hz_>0;
    }
    bool Start(XrInstance instance,XrSession session,PFN_xrGetInstanceProcAddr resolve) {
        Stop();
        if(!enabled_ || !instance || !session || !resolve)return false;
        PFN_xrVoidFunction function=nullptr;
        if(resolve(instance,"xrGetDisplayRefreshRateFB",&function)!=XR_SUCCESS || !function)return false;
        get_=reinterpret_cast<PFN_xrGetDisplayRefreshRateFB>(function);
        float rate=0;
        if(get_(session,&rate)!=XR_SUCCESS || !Valid(rate)){get_=nullptr;return false;}
        hz_=rate;return true;
    }
    bool Event(const XrEventDataBuffer& event) {
        if(!enabled_ || !get_ || event.type!=XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB)return false;
        const auto& change=reinterpret_cast<const XrEventDataDisplayRefreshRateChangedFB&>(event);
        hz_=Valid(change.toDisplayRefreshRate)?change.toDisplayRefreshRate:0;
        return true;
    }
};
}

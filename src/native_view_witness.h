#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>
namespace d2vr_native_view {
// Numeric identities are never dereferenced after the owning native callback.
// A fresh primary-view inverse upload must corroborate each preceding frame.
struct ProjectionWitness {
    uint64_t frame=0,verifiedFrame=0;
    uintptr_t view=0,renderer=0,camera=0,verifiedView=0,verifiedRenderer=0,verifiedCamera=0;
    float inverse[16]{};
    float uploadedInverse[16]{};
    uint64_t uploadedFrame=0;
    bool uploaded=false;
    bool active=false,verified=false;
    void Begin(uint64_t f,uintptr_t v,uintptr_t r,uintptr_t c,const float* p) {
        frame=f;view=v;renderer=r;camera=c;active=p && v && r && c;
        if(active)std::memcpy(inverse,p,sizeof(inverse));
        // The live upload may precede this boundary. Keep its frame identity,
        // rather than silently requiring one particular callback order.
        if(uploaded && uploadedFrame==f)Verify(f,uploadedInverse);
    }
    bool Observe(uint64_t f,bool world,const float* p) {
        if(!world || !p)return false;
        uploaded=true;uploadedFrame=f;
        std::memcpy(uploadedInverse,p,sizeof(uploadedInverse));
        return Verify(f,p);
    }
    bool Verify(uint64_t f,const float* p) {
        if(!active || f!=frame || !p)return false;
        for(unsigned i=0;i<16;++i)if(!std::isfinite(p[i]) || !std::isfinite(inverse[i]) ||
            std::fabs(p[i]-inverse[i])>2e-4f*(1+std::fabs(inverse[i])))return false;
        verified=true;verifiedFrame=f;verifiedView=view;verifiedRenderer=renderer;verifiedCamera=camera;
        return true;
    }
    bool CanCover(uint64_t f,uintptr_t v,uintptr_t r,uintptr_t c)const {
        return verified && f>verifiedFrame && f-verifiedFrame==1 &&
            v==verifiedView && r==verifiedRenderer && c==verifiedCamera;
    }
};
}

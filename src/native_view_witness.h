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
    float verifiedInverse[16]{};
    float uploadedInverse[16]{};
    uint64_t uploadedFrame=0;
    bool uploaded=false;
    bool active=false,verified=false;
    // Bits identify rejected components, using exactly the acceptance tolerance.
    // Diagnostics must not establish a second, weaker matching contract.
    static unsigned ProjectionMismatch(const float* a,const float* b) {
        if(!a || !b)return 0xffff;
        unsigned mask=0;
        for(unsigned i=0;i<16;++i)if(!std::isfinite(a[i]) || !std::isfinite(b[i]) ||
            std::fabs(a[i]-b[i])>2e-4f*(1+std::fabs(b[i])))mask|=1u<<i;
        return mask;
    }
    static bool SameProjection(const float* a,const float* b) {
        return ProjectionMismatch(a,b)==0;
    }
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
        if(!SameProjection(p,inverse))return false;
        verified=true;verifiedFrame=f;verifiedView=view;verifiedRenderer=renderer;verifiedCamera=camera;
        std::memcpy(verifiedInverse,inverse,sizeof(verifiedInverse));
        return true;
    }
    bool CanCover(uint64_t f,uintptr_t v,uintptr_t r,uintptr_t c)const {
        return verified && f>verifiedFrame && f-verifiedFrame==1 &&
            v==verifiedView && r==verifiedRenderer && c==verifiedCamera;
    }
    bool CanCoverProjection(uint64_t f,uintptr_t v,uintptr_t r,uintptr_t c,const float* p)const {
        return CanCover(f,v,r,c) && SameProjection(p,verifiedInverse);
    }
};
}

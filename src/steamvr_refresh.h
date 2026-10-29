#pragma once
#include <cmath>
#include <cstring>

namespace d2vr_refresh {
inline bool SteamVR(const char* name) {
    if(!name)return false;
    const char expected[]="steamvr";
    for(unsigned i=0;i<7;++i) {
        const char c=name[i];
        if(!c || (c>='A'&&c<='Z'?c-'A'+'a':c)!=expected[i])return false;
    }
    return !name[7] || name[7]==' ' || name[7]=='/';
}
// Own exactly one background-client initialization, never an existing client's
// context. All methods run on the session/render thread, outside loader lock.
template<class Ops> class SteamReader {
    Ops ops_;
    bool loaded_=false,initialized_=false,ready_=false;
public:
    Ops& Backend(){return ops_;}
    void Stop() {
        ready_=false;
        if(initialized_){ops_.Shutdown();initialized_=false;}
        if(loaded_){ops_.Unload();loaded_=false;}
    }
    bool Start(const char* runtime) {
        Stop();
        if(!SteamVR(runtime) || !ops_.Load())return false;
        loaded_=true;
        if(!ops_.Initialize()){Stop();return false;}
        initialized_=true;
        if(!ops_.System()){Stop();return false;}
        ready_=true;return true;
    }
    float Read() {
        if(!ready_)return 0;
        const float value=ops_.Frequency();
        return std::isfinite(value)&&value>=1&&value<=1000?value:0;
    }
    // No destructor shutdown: static destruction may execute under loader lock.
};
}

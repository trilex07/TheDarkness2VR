#pragma once
#include <cmath>
#include <cstdint>

namespace d2vr_adaptive {
enum class Mode {Off=0,Auto=1,Half=2};
enum class Status {Off,Full,Half,Unsupported,UnknownRefresh,UnsupportedRefresh,RuntimeError};
enum class Result {Applied,Unsupported,Failed};
struct Runtime {
    void* context=nullptr;
    Result (*setHalf)(void*,bool)=nullptr;
    // Bind only a documented session-scoped pacing adapter. This is not a
    // display-refresh request, thread sleep, or permission to skip XR frames.
};
struct Work {
    uint64_t frame=0;
    double cpu=0,gpu=0; // complete application work spans in seconds; no wait-frame pacing
};
struct Snapshot {
    Mode mode=Mode::Off;
    Status status=Status::Off;
    double refresh=0,fps=0,target=0;
    bool half=false;
    uint64_t frame=0;
};
// Presentation-local two-party acknowledgement. A fresh stereo XR submission
// must belong to the same *forwarded* successful native Present. Nested COM
// forwarding cannot open or close its parent's receipt. The host keeps this
// object thread-local; the returned serial, not a later global frame ID, owns it.
class Presentation {
    uint64_t serial_=0,active_=0,frame_=0;
    bool conflicting_=false;
public:
    uint64_t Begin() {
        if(active_)return 0;
        if(++serial_==0)++serial_;
        frame_=0;conflicting_=false;return active_=serial_;
    }
    void StereoSubmitted(uint64_t frame) {
        if(active_ && frame) {
            if(frame_ && frame_!=frame)conflicting_=true;
            frame_=frame;
        }
    }
    uint64_t End(uint64_t serial,bool nativeSuccess) {
        if(!serial || serial!=active_)return 0;
        const auto frame=nativeSuccess && !conflicting_ ? frame_:0;
        active_=frame_=0;return frame;
    }
};
class Controller {
    struct Stamp {uint64_t frame=0;double time=0;} history_[128];
    unsigned historyNext_=0;
    Mode mode_=Mode::Off;
    Status status_=Status::Off;
    Runtime runtime_;
    bool half_=false;
    double refresh_=0,fps_=0,threshold_=0;
    uint64_t frame_=0,workFrame_=0;
    double time_=0,windowStart_=0,badSeconds_=0,changed_=0;
    unsigned intervals_=0;
    double goodSince_=0,goodLast_=0;
    static bool Time(double t){return std::isfinite(t) && t>0;}
    static double Threshold(double hz) {
        if(std::fabs(hz-90)<=.5)return 80;
        if(std::fabs(hz-120)<=.5)return 100;
        if(std::fabs(hz-72)<=.5)return 60;
        return 0; // rates without an agreed policy are not silently rounded
    }
    void ResetSamples() {
        frame_=workFrame_=0;time_=windowStart_=fps_=badSeconds_=0;intervals_=0;
        goodSince_=goodLast_=0;historyNext_=0;
        for(auto& stamp:history_)stamp={};
    }
    bool Request(bool half,double now) {
        if(half==half_)return true;
        if(!runtime_.setHalf){status_=Status::Unsupported;return false;}
        const auto result=runtime_.setHalf(runtime_.context,half);
        if(result!=Result::Applied) {
            status_=result==Result::Unsupported && !half_ ? Status::Unsupported:Status::RuntimeError;
            return false; // never claim a rejected transition took effect
        }
        half_=half;changed_=now;badSeconds_=goodSince_=goodLast_=0;
        status_=half ? Status::Half:Status::Full;return true;
    }
public:
    void DiscardFrames(){ResetSamples();}
    Snapshot State()const {
        return {mode_,status_,refresh_,fps_,refresh_>0 ? refresh_/(half_?2:1):0,half_,frame_};
    }
    // The previous session must be authoritatively terminal before forgetting
    // its pacing owner. A live adapter must be released via Configure(Off).
    void SessionEnded() {
        runtime_={};half_=false;refresh_=threshold_=changed_=0;ResetSamples();
        status_=mode_==Mode::Off ? Status::Off:Status::UnknownRefresh;
    }
    bool Bind(Runtime runtime,double now) {
        if(!Time(now))return false;
        if(half_ && !Request(false,now))return false; // do not abandon a live half-rate owner
        runtime_=runtime;refresh_=threshold_=0;ResetSamples();changed_=now;
        status_=mode_==Mode::Off ? Status::Off:Status::UnknownRefresh;return true;
    }
    bool Configure(Mode mode,double hz,double now) {
        if(!Time(now) || (mode!=Mode::Off && mode!=Mode::Auto && mode!=Mode::Half))return false;
        const bool validRate=std::isfinite(hz) && hz>=1 && hz<=1000;
        const double rate=validRate ? hz:0;
        if(mode==mode_ && rate==refresh_)return status_!=Status::RuntimeError;
        // A changed refresh or disabled mode must first release old pacing.
        // Keep reporting an error/active half if that release fails.
        if(half_ && (mode==Mode::Off || rate!=refresh_) && !Request(false,now))return false;
        mode_=mode;refresh_=rate;threshold_=Threshold(rate);ResetSamples();changed_=now;
        if(mode==Mode::Off){status_=Status::Off;return true;}
        if(!runtime_.setHalf){status_=Status::Unsupported;return true;}
        if(!refresh_){status_=Status::UnknownRefresh;return true;}
        if(!threshold_){status_=Status::UnsupportedRefresh;return true;}
        status_=half_ ? Status::Half:Status::Full;
        return mode!=Mode::Half || Request(true,now);
    }
    // Exactly one sample per newly rendered stereo application frame. The
    // caller supplies its identity only after successful XR layer submission;
    // compositor/reprojection frames have no identity here and cannot count.
    void Frame(uint64_t id,double now) {
        if(mode_==Mode::Off)return;
        if(!id || !Time(now)){ResetSamples();return;}
        if(id==frame_)return;
        if(id<frame_ || (time_ && now<=time_)){ResetSamples();return;}
        const bool gap=time_ && now-time_>.5;
        frame_=id;time_=now;
        history_[historyNext_]={id,now};historyNext_=(historyNext_+1)%128;
        if(!windowStart_ || gap) {
            windowStart_=now;intervals_=0;badSeconds_=goodSince_=goodLast_=0;fps_=0;return;
        }
        ++intervals_;
        const double elapsed=now-windowStart_;
        if(elapsed<1)return;
        fps_=intervals_/elapsed;windowStart_=now;intervals_=0;
        if(mode_!=Mode::Auto || status_!=Status::Full || !threshold_ || !runtime_.setHalf || half_)return;
        badSeconds_=fps_<threshold_-1e-6 ? badSeconds_+elapsed:0;
        // Two complete slow windows plus minimum residence; one hitch cannot
        // switch pacing. Full-rate recovery below never consults capped FPS.
        if(badSeconds_>=2 && now-changed_>=3)Request(true,now);
    }
    // Timestamp queries may arrive later. Match the exact accepted app frame,
    // reject repeats/out-of-order/stale/partial samples, and require both full
    // CPU and GPU work. A D3D9-only GPU marker does not meet this contract.
    void Workload(const Work& work,double now) {
        if(mode_!=Mode::Auto || status_!=Status::Half || !half_ || !runtime_.setHalf || !refresh_)return;
        if(!Time(now) || !work.frame || work.frame<=workFrame_)return;
        double submitted=0;
        for(const auto& stamp:history_)if(stamp.frame==work.frame){submitted=stamp.time;break;}
        if(!submitted){goodSince_=goodLast_=0;return;} // unknown IDs cannot poison the sample watermark
        const double budget=.85/refresh_;
        if(!submitted || submitted<=changed_ || now<submitted || now-submitted>.5 ||
           !std::isfinite(work.cpu) || !std::isfinite(work.gpu) || work.cpu<=0 || work.gpu<=0 ||
           work.cpu>budget || work.gpu>budget) {
            goodSince_=goodLast_=0;workFrame_=work.frame;return;
        }
        const bool consecutive=workFrame_ && work.frame-workFrame_==1;
        workFrame_=work.frame;
        if(!goodSince_ || !consecutive || submitted<=goodLast_ || submitted-goodLast_>.25)goodSince_=submitted;
        goodLast_=submitted;
        if(submitted-goodSince_>=3 && now-changed_>=3)Request(false,now);
    }
};
inline const char* Name(Status status) {
    switch(status) {
        case Status::Off:return "OFF";
        case Status::Full:return "FULL";
        case Status::Half:return "HALF";
        case Status::Unsupported:return "UNSUPPORTED";
        case Status::UnknownRefresh:return "REFRESH UNKNOWN";
        case Status::UnsupportedRefresh:return "REFRESH UNSUPPORTED";
        default:return "RUNTIME ERROR";
    }
}
}

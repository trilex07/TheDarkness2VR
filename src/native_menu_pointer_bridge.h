#pragma once
#include "vr_menu_pointer.h"
#include "native_menu_mouse_merge.h"

namespace d2vr_menu_native {
// Owner-thread coordinator. Surface must describe an actually submitted native
// menu, with cursor coordinates already expressed in native HWND client pixels.
// This owns values only: it neither finds objects nor installs an input hook.
struct PointerFrame {
    d2vr_menu::Actions pointer;
    d2vr_menu::NavigationActions navigation;
};
class PointerBridge {
    d2vr_menu::Capture capture_;
    d2vr_menu::Navigation navigation_;
    MouseButtonMerge buttons_;
    d2vr_menu::Actions pointer_;
    uint64_t surface_=0,frame_=0,receiver_=0;
    uint32_t owner_=0,device_=0,window_=0;
    double time_=0;
    bool active_=false,draining_=false,polling_=false,conflict_=false;
public:
    bool Owned()const{return buttons_.Owned();}
    unsigned Pending()const{return buttons_.Pending();}
    void Cancel() {
        if(polling_)conflict_=true;
        capture_.Reset();navigation_.Reset();buttons_.Cancel();
        pointer_={};active_=false;surface_=0;
        draining_=buttons_.Owned();
    }
    // submitted is the renderer's successful xrEndFrame receipt, not a guessed
    // menu-visible flag. No edge may originate from an unsubmitted surface.
    PointerFrame Update(const d2vr_menu::Surface& surface,const d2vr_menu::Aim& aim,
                        bool focused,bool submitted) {
        PointerFrame result;
        if(polling_){conflict_=true;return result;}
        if(!focused || !submitted || !aim.menu || !aim.tracked ||
           !std::isfinite(aim.time) || aim.time<=0 ||
           !std::isfinite(aim.trigger) || aim.trigger<0 || aim.trigger>1 ||
           (time_ && aim.time<time_) || (frame_ && aim.frame<frame_) ||
           (frame_ && aim.frame>frame_ && aim.time<=time_)) {
            Cancel();return result;
        }
        d2vr_menu::Hit hit;
        if(!d2vr_menu::Project(surface,aim,hit)) {Cancel();return result;}
        if(surface_ && surface_!=surface.identity)Cancel();
        // A committed press belongs to its old menu/device until its release
        // drains. Never deliver a new click to a replacement menu meanwhile.
        if(draining_)return result;
        if(aim.frame==frame_)return result;
        const auto pointer=capture_.Update(surface,aim);
        // GetDeviceData supplies one cursor position to its subsequent event
        // decoder. Do not misplace an undelivered click if another XR update
        // has moved the ray before that poll. Cancel instead of clicking a
        // different widget; a held native button still receives its release.
        if(buttons_.Pending() && pointer_.move && pointer.move &&
           (pointer_.hit.x!=pointer.hit.x || pointer_.hit.y!=pointer.hit.y)) {
            Cancel();return result;
        }
        frame_=aim.frame;time_=aim.time;surface_=surface.identity;
        active_=true;pointer_=pointer;
        if((pointer.press && !buttons_.Queue(true)) ||
           (pointer.release && !buttons_.Queue(false))) {Cancel();return result;}
        result.pointer=pointer;
        result.navigation=navigation_.Update(surface,aim,pointer.captured);
        return result;
    }
    // Invoke only inside the admitted current native poll, on its window's
    // owner thread, AFTER forwarding the original API exactly once. The host
    // owns all supplied writable extents through return. No pointers retained.
    // receiver is the host's nonzero lifetime identity, changed on destruction/
    // replacement even if native pointer addresses are reused.
    // moveCursor must apply the native cursor position before button decoding;
    // failure/reentrancy cancels undelivered clicks without dropping a release.
    template<class Move> bool Poll(uint32_t owner,uint32_t device,uint32_t window,uint64_t receiver,
        uint32_t result,uint32_t flags,uint32_t stateBytes,uint8_t* immediate,
        MouseEvent* events,uint32_t& count,uint32_t capacity,double now,
        uint32_t timestamp,uint32_t sequence,Move moveCursor) {
        if(polling_){conflict_=true;return false;}
        if(!std::isfinite(now) || now<time_ || now-time_>.25)Cancel();
        if((result!=0 && result!=1) || flags || stateBytes!=20 || !owner ||
           !device || !window || !receiver || !immediate || count>capacity ||
           (capacity && !events)) {Cancel();return false;}
        if(buttons_.Owned() && (owner!=owner_ || device!=device_ || window!=window_ || receiver!=receiver_)) {
            Cancel();return false;
        }
        if(!active_ && !buttons_.Owned() && !buttons_.Pending())return false;
        polling_=true;conflict_=false;
        bool moved=true;
        try {
            if(active_ && pointer_.move)moved=moveCursor(pointer_.hit.x,pointer_.hit.y);
        } catch(...) {polling_=false;Cancel();return false;}
        polling_=false;
        if(!moved || conflict_){Cancel();return false;}
        const bool applied=buttons_.Poll(result,flags,stateBytes,immediate,events,
                                         count,capacity,timestamp,sequence);
        if(buttons_.Owned()){owner_=owner;device_=device;window_=window;receiver_=receiver;}
        else {owner_=device_=window_=0;receiver_=0;draining_=false;}
        return applied;
    }
};
}

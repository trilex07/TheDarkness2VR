#pragma once
#include <cstddef>
#include <cstdint>

namespace d2vr_menu_native {
// D46 MouseDeviceWin consumes the 20-byte x86 DIDEVICEOBJECTDATA layout.
// This value type owns no native memory or API objects.
struct MouseEvent {
    uint32_t offset,data,time,sequence,appData;
};
static_assert(sizeof(MouseEvent)==20,"native x86 mouse event extent");

// Owner-thread transaction used AFTER native immediate and buffered polling,
// BEFORE 9A5D90 compares states/decodes events. The host must supply fresh native
// buffers and preserve its HRESULT; this class grants no receiver lifetime.
// Only button zero is merged. Motion, wheel, other buttons and physical memory
// outside that byte retain their original values. No OS calls or allocation.
class MouseButtonMerge {
    static constexpr unsigned Limit=32;
    bool pending_[Limit]{};
    unsigned first_=0,size_=0;
    bool owned_=false;
public:
    bool Owned()const{return owned_;}
    unsigned Pending()const{return size_;}
    // Enqueue every logical edge, including press+release between native polls.
    // A full queue rejects the new edge without mutating earlier input. The
    // host must Cancel on rejection rather than silently accepting a click.
    bool Queue(bool down) {
        const bool last=size_ ? pending_[(first_+size_-1)%Limit]:owned_;
        if(last==down)return true;
        if(size_==Limit)return false;
        pending_[(first_+size_)%Limit]=down;++size_;return true;
    }
    // Discard undelivered clicks, but retain a release for a committed press.
    // It is delivered at the next successful non-overflow native poll.
    void Cancel() {
        first_=size_=0;
        if(owned_){pending_[0]=false;size_=1;}
    }
    // Failed/peek/unknown-format calls are never modified, nor do they consume
    // pending input. The host passes the unmodified native poll HRESULT.
    // DI_BUFFEROVERFLOW (1) can rebuild events from immediate states, so queued
    // synthetic edges wait for a normal poll instead of being lost in rebuild.
    bool Poll(uint32_t result,uint32_t flags,uint32_t stateBytes,
              uint8_t* immediate,MouseEvent* events,uint32_t& count,
              uint32_t capacity,uint32_t time,uint32_t sequence) {
        if((result!=0 && result!=1) || flags || stateBytes!=20 || !immediate ||
           count>capacity || (capacity && !events))return false;
        const bool physical=(immediate[12]&0x80)!=0;
        uint32_t output=0;
        for(uint32_t i=0;i<count;++i) {
            // While held synthetically, both physical edges are masked; their
            // immediate state still decides whether our release needs an event.
            if(owned_ && events[i].offset==12)continue;
            if(output!=i)events[output]=events[i];
            ++output;
        }
        if(result==0) {
            while(size_) {
                const bool next=pending_[first_];
                const bool before=physical||owned_,after=physical||next;
                if(before!=after) {
                    if(output==capacity)break; // keep ownership and edge pending
                    events[output++]={12,after ? 0x80u:0u,time,sequence,0};
                }
                owned_=next;first_=(first_+1)%Limit;--size_;
            }
        }
        // Preserve all native bits other than the merged down bit. Never touch
        // the native previous-state buffer: it contains the last merged poll.
        immediate[12]=uint8_t((immediate[12]&0x7f)|((physical||owned_)?0x80:0));
        count=output;return true;
    }
};
}

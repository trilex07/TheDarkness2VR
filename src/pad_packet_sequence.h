#pragma once
#include <windows.h>
#include <xinput.h>
#include <cstring>

namespace d2vr_input {
// One returned port has one packet sequence even when its physical/XR source
// changes. The game's 89BB0F check compares only this number before emitting
// axis events. Payload bytes and all stock bindings are left untouched.
// Caller serializes access; this object owns no device, native or XR pointer.
class PadPacketSequence {
    XINPUT_GAMEPAD previous_{};
    DWORD packet_=0;
    bool valid_=false;
public:
    void Reset(){valid_=false;}
    void Apply(XINPUT_STATE& value) {
        if(!valid_) {
            packet_=value.dwPacketNumber+1;
            valid_=true;
        } else if(std::memcmp(&previous_,&value.Gamepad,sizeof(previous_))) {
            ++packet_;
        }
        previous_=value.Gamepad;
        value.dwPacketNumber=packet_;
    }
};
}

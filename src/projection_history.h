#pragma once
#include <cstdint>

namespace d2vr_projection_history {
struct Matrix {float values[16]{};bool valid=false;};
// STP and its matching raw WVP can arrive in separate constant commands.
// Replay must start at the live history BEFORE this shader's first command,
// rather than borrowing the last live object's WVP from the end of the frame.
struct History {
    Matrix live,begin,replay;
    uint64_t capturedFrame=0,replayFrame=0;
    Matrix& ForUpload(bool replaying,bool recording,uint64_t frame) {
        if(replaying) {
            if(replayFrame!=frame) {
                replay=capturedFrame==frame?begin:Matrix{};
                replayFrame=frame;
            }
            return replay;
        }
        if(recording && capturedFrame!=frame) {
            begin=live;capturedFrame=frame;
        }
        return live;
    }
};
}

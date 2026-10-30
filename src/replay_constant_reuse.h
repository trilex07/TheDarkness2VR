#pragma once
#include "replay_constant_batch.h"

namespace d2vr_replay_batch {
// Replay-local D3D9 float state, AFTER all eye transformations. Owns copied
// bits, never source pointers. An entire packet is omitted only if EVERY row
// is known and bit-identical to a successful D3D_OK write in this replay.
// No splitting, readback, cross-frame reuse or reuse of raw/live-eye bytes.
class ConstantReuse {
    float values_[2][256*4];
    bool known_[2][256];
    unsigned limits_[2]{};
    bool enabled_=false,busy_=false;
    uint64_t generation_=0;
public:
    uint64_t skipped[2]{};
    void Clear() {if(!enabled_)return;memset(known_,0,sizeof(known_));++generation_;}
    void Reset(bool enabled,unsigned vsLimit,unsigned psLimit) {
        enabled_=enabled;limits_[0]=vsLimit>256?256:vsLimit;
        limits_[1]=psLimit>224?224:psLimit;
        skipped[0]=skipped[1]=0;busy_=false;Clear();
    }
    void Abort(){Clear();enabled_=false;busy_=false;}
    void Invalidate(Stage stage,unsigned first,unsigned count) {
        if(!enabled_)return;
        const unsigned bank=stage==Stage::Vertex?0:1;
        if(stage==Stage::None || first>=limits_[bank] || count>limits_[bank]-first){Clear();return;}
        memset(known_[bank]+first,0,count);++generation_;
    }
    template<class Send> long Forward(Stage stage,unsigned first,const float* source,unsigned count,Send send) {
        if(!enabled_)return send(stage,first,source,count);
        if(busy_){Clear();return send(stage,first,source,count);}
        const unsigned bank=stage==Stage::Vertex?0:1;
        if(stage==Stage::None || !source || !count || first>=limits_[bank] || count>limits_[bank]-first) {
            Clear();return send(stage,first,source,count);
        }
        bool known=true;
        for(unsigned row=first;row<first+count;++row)if(!known_[bank][row]){known=false;break;}
        if(known && !memcmp(values_[bank]+first*4,source,size_t(count)*16)) {
            ++skipped[bank];return 0; // exact D3D_OK state already materialized
        }
        // Consume knowledge BEFORE entering a possibly reentrant driver. A
        // rejected/throwing write can never certify new bits. Callback clears
        // invalidate the generation; the outer call must not repopulate it.
        Invalidate(stage,first,count);const auto generation=generation_;busy_=true;
        const long result=send(stage,first,source,count);busy_=false;
        if(result==0 && enabled_ && generation==generation_) {
            memcpy(values_[bank]+first*4,source,size_t(count)*16);
            memset(known_[bank]+first,1,count);
        }
        return result;
    }
};
}

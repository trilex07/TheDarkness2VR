#pragma once

namespace d2vr_projection_estimate {
// The same eight accepted upload samples as the original estimator. Maintain
// their sorted multiset instead of sorting two fresh arrays at every WVP.
// This is CPU-local arithmetic, with no device-state or source-page cache.
class Window {
    float ring_[2][8]{}, sorted_[2][8]{};
    unsigned count_=0, next_=0;
    void Insert(unsigned axis,float value) {
        unsigned at=count_;
        if(count_==8) {
            const float old=ring_[axis][next_];
            if(old==value)return; // identical multiset; cursor still advances
            at=0;
            while(sorted_[axis][at]!=old)++at;
            for(unsigned i=at;i+1<count_;++i)sorted_[axis][i]=sorted_[axis][i+1];
            at=count_-1;
        }
        while(at && sorted_[axis][at-1]>value) {
            sorted_[axis][at]=sorted_[axis][at-1];--at;
        }
        sorted_[axis][at]=value;
        ring_[axis][next_]=value;
    }
public:
    bool Push(float px,float py,float& outX,float& outY) {
        // Reject NaNs, infinities and boundary values exactly as before; none
        // of them may advance the ring or enter the sorted multiset.
        if(!(px>0.2f && px<8.0f && py>0.2f && py<16.0f))return false;
        Insert(0,px);Insert(1,py);
        next_=(next_+1)%8;
        if(count_<8)++count_;
        if(count_<3)return false;
        // Keep publishing even for unchanged samples: an exact inverse-
        // projection observation may have overwritten the published output.
        outX=sorted_[0][count_/2];outY=sorted_[1][count_/2];
        return true;
    }
};
}

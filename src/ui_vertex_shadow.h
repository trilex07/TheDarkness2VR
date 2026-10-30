#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include "mapped_vertex_copy.h"
namespace d2vr_ui_vertices {
// Only bytes copied while the native writable Lock is still owned can be used.
// DISCARD starts a new validity generation, even outside the updated range.
class Shadow {
    struct Range{size_t first,last;};
    std::vector<uint8_t> bytes_;
    std::vector<Range> ranges_;
    Range pending_{};
    bool staged_=false;
    bool streaming_=false;
public:
    void Resize(size_t size){streaming_=d2vr_mapped_copy::Supported();bytes_.resize(size);ranges_.reserve(64);Clear();}
    void Clear(){ranges_.clear();staged_=false;}
    bool Stage(size_t offset,size_t size,const void* data,bool discard){
        if(discard)Clear();
        if(staged_ || offset>bytes_.size() || size>bytes_.size()-offset || !data || !size){Clear();return false;}
        d2vr_mapped_copy::Copy(bytes_.data()+offset,data,size,streaming_);pending_={offset,offset+size};staged_=true;return true;
    }
    bool Commit(bool success){
        if(!success){Clear();return false;}
        if(!staged_)return false;
        staged_=false;
        auto first=ranges_.begin();while(first!=ranges_.end() && first->last<pending_.first)++first;
        auto last=first;
        while(last!=ranges_.end() && last->first<=pending_.last){
            pending_.first=(std::min)(pending_.first,last->first);pending_.last=(std::max)(pending_.last,last->last);++last;
        }
        if(first!=last){*first=pending_;ranges_.erase(first+1,last);return true;}
        if(ranges_.size()>=1024){Clear();return false;}
        try{ranges_.insert(first,pending_);return true;}catch(...){Clear();return false;}
    }
    const uint8_t* Read(size_t offset,size_t size)const{
        if(staged_ || !size || offset>bytes_.size() || size>bytes_.size()-offset)return nullptr;
        for(const auto& range:ranges_)if(range.first<=offset && range.last>=offset+size)return bytes_.data()+offset;
        return nullptr;
    }
};
}

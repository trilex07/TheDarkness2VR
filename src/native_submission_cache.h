#pragma once
#include "submission_epoch.h"
#include "replay_constant_reuse.h"

namespace d2vr_submission {
using Stage=d2vr_replay_batch::Stage;
inline Epoch gate;
inline bool requested=false; // fixed by LoadConfig before the device is published
inline const void* deviceIdentity=nullptr; // published before installing the first device hook
inline void CheckDevice(const void* device){if(!device || device!=deviceIdentity)gate.Deny(Epoch::DeviceReplaced);}
inline thread_local bool internalBlockExpected=false;
// One shot, consumed BEFORE entering the driver. A recursive application
// CreateStateBlock must never inherit the mod-owned snapshot exemption.
class InternalStateBlockCreation {
    bool active_;
public:
    InternalStateBlockCreation():active_(requested){if(active_)internalBlockExpected=true;}
    ~InternalStateBlockCreation(){if(active_)internalBlockExpected=false;}
    InternalStateBlockCreation(const InternalStateBlockCreation&)=delete;
};
inline bool ConsumeInternalStateBlock(){const bool expected=internalBlockExpected;internalBlockExpected=false;return expected;}
struct Packet {
    Epoch::Ticket ticket;
    uint64_t serial=0;
    Stage stage=Stage::None;
    unsigned first=0,count=0;
    bool captured=false;
};
// Caller-thread float state only. No resources, original application pointers,
// Windows calls or allocations. Capture must receive recorder-OWNED bytes or
// the final eye-transformed local buffer; never an unchecked native pointer.
class Constants {
    d2vr_replay_batch::ConstantReuse values_;
    float pending_[256*4];
    uint64_t epoch_=0,serial_=0;
    bool busy_=false;
    void Sync(Epoch& domain) {
        const auto value=domain.Generation();
        if(value!=epoch_){values_.Clear();epoch_=value;}
    }
    static unsigned Bank(Stage stage){return stage==Stage::Vertex?0:1;}
    static bool Range(Stage stage,unsigned first,unsigned count) {
        const unsigned limit=stage==Stage::Vertex?256:224;
        return stage!=Stage::None && count && first<limit && count<=limit-first;
    }
public:
    uint64_t forwarded[2]{},skipped[2]{},uncaptured[2]{};
    Constants(){values_.Reset(true,256,224);}
    Packet Begin(Epoch& domain,uint32_t thread,Stage stage,unsigned first,unsigned count) {
        auto ticket=domain.Begin(thread);
        if(!ticket)return {};
        if(busy_){domain.Invalidate();return {};}
        Sync(domain);
        return {ticket,++serial_,stage,first,count,false};
    }
    bool Capture(Epoch& domain,Packet& packet,const void* owned,size_t bytes,
                 unsigned first,unsigned count) {
        packet.captured=false;
        if(!packet.ticket || packet.serial!=serial_ || busy_ || !domain.CanReuse(packet.ticket) ||
           !Range(packet.stage,first,count) || !owned || bytes!=size_t(count)*16)return false;
        std::memcpy(pending_,owned,bytes);
        packet.first=first;packet.count=count;packet.captured=true;
        return true;
    }
    template<class Send> long Forward(Epoch& domain,Packet packet,Send send) {
        if(!packet.ticket)return send();
        Sync(domain);
        const unsigned bank=Bank(packet.stage);
        if(busy_ || packet.serial!=serial_ || !domain.CanReuse(packet.ticket)) {
            domain.Invalidate();++forwarded[bank];
            const long result=send();domain.Invalidate();Sync(domain);return result;
        }
        if(!packet.captured) {
            // An unrecorded/failed source copy must not leave older float rows
            // certified. Forward the ORIGINAL pointer/HRESULT without reading it.
            values_.Invalidate(packet.stage,packet.first,packet.count);
            ++uncaptured[bank];++forwarded[bank];busy_=true;
            const long result=send();busy_=false;domain.Invalidate();Sync(domain);return result;
        }
        busy_=true;
        const long result=values_.Forward(packet.stage,packet.first,pending_,packet.count,
            [&](Stage,unsigned,const float*,unsigned)->long {++forwarded[bank];return send();});
        busy_=false;
        if(!domain.CanReuse(packet.ticket) || result!=0) {
            domain.Invalidate();Sync(domain);
        }
        skipped[bank]=values_.skipped[bank];
        return result;
    }
};
// Frame-owned bindings. Successful command publication owns every non-null
// COM identity in the recorder ledger until the epoch changes BEFORE release.
// Texture/depth binds are intentionally excluded from this admission contract.
inline bool bindingsRequested=false;
inline void InvalidateBindings(){if(requested && bindingsRequested)gate.Invalidate();}
class FrameBindings {
    struct Value {uintptr_t a=0,b=0,c=0;};
    Value values_[36];
    uint64_t valid_=0,epoch_=0;
    bool busy_=false;
    void Sync(Epoch& domain){if(epoch_!=domain.Generation()){valid_=0;epoch_=domain.Generation();}}
public:
    static constexpr int VertexShader=0,PixelShader=1,Declaration=2,Indices=3;
    static int Stream(unsigned n){return n<16?int(4+n):-1;}
    static int Frequency(unsigned n){return n<16?int(20+n):-1;}
    uint64_t forwarded=0,skipped=0,unowned=0;
    template<class Send,class Receipt> long Forward(Epoch& domain,Epoch::Ticket ticket,
        int key,uintptr_t a,uintptr_t b,uintptr_t c,Send send,Receipt published){
        Sync(domain);
        if(!ticket || key<0 || key>=36 || busy_ || !domain.CanReuse(ticket)){
            domain.Invalidate();++forwarded;
            const long result=send();domain.Invalidate();Sync(domain);return result;
        }
        const uint64_t bit=uint64_t(1)<<key;
        const auto old=values_[key];
        if((valid_&bit) && old.a==a && old.b==b && old.c==c){++skipped;return 0;}
        valid_&=~bit;busy_=true;++forwarded;
        const long result=send();busy_=false;
        if(result==0 && domain.CanReuse(ticket) && published()){
            values_[key]={a,b,c};valid_|=bit;
        }else{++unowned;domain.Invalidate();Sync(domain);}
        return result;
    }
};
inline thread_local FrameBindings bindings;
template<class Send,class Receipt> long ForwardBinding(const void* device,uint32_t thread,bool recording,
    int key,uintptr_t a,uintptr_t b,uintptr_t c,Send send,Receipt published){
    if(!requested || !bindingsRequested)return send();
    CheckDevice(device);
    if(!gate.Enabled() || gate.Paused())return send();
    if(!recording){gate.Invalidate();const long result=send();gate.Invalidate();return result;}
    return bindings.Forward(gate,gate.Begin(thread),key,a,b,c,send,published);
}
inline thread_local Constants constants;
// Invalidates successful cache writes around every raw renderer/restore entry.
inline void Invalidate(){if(requested)gate.Invalidate();}
}

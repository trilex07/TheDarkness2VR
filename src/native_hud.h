#pragma once
#include <cstdint>
namespace d2vr_hud {
// Explicit guidance temporarily releases the global HUD hide. Never force a
// HUD visible when the native owner had hidden it itself.
inline bool GuidanceActive(uint64_t now,uint64_t until) {
    return now<until && until-now<=8000;
}
struct Owner {uint32_t avatar=0,player=0,playerRef=0,hud=0,hudRef=0;
    bool operator==(const Owner& b)const {return avatar==b.avatar && player==b.player &&
        playerRef==b.playerRef && hud==b.hud && hudRef==b.hudRef;}
};
// Borrowed only during the admitted native view callback's strong avatar lease.
// 6B0CD0: Avatar::GetPlayer +4E4; 43BCB0: Player::GetAvatar +74.
// 7928C0: existing Player::GetHudStatus +60; never invoke its lazy factory.
template<class Read> bool Resolve(uint32_t avatar,Read read,Owner& out) {
    Owner s;s.avatar=avatar;
    const auto field=[&](uint32_t p,uint32_t offset,uint32_t& v) {
        return p>=0x10000 && !(p&3) && p<=UINT32_MAX-offset-3 && read(p+offset,v);
    };
    uint32_t ref=0,back=0,table=0;
    if(!field(avatar,0,table) || table!=0x1001630 ||
       !field(avatar,0x4e4,s.playerRef) || !field(s.playerRef,0,s.player) ||
       !field(s.player,0x74,ref) || !field(ref,0,back) || back!=avatar ||
       !field(s.player,0x60,s.hudRef) || !field(s.hudRef,0,s.hud) ||
       !field(s.hud,0,table) || table!=0xfe3b88 || s.hud>UINT32_MAX-0x13c)return false;
    out=s;return true;
}
struct Override {
    Owner owner;bool held=false;uint8_t saved=0;
    template<class ReadByte,class WriteByte>
    bool Update(const Owner& current,bool hide,ReadByte read,WriteByte write) {
        uint8_t value=0;
        if(!read(current.hud+0x13c,value) || value>1)return false;
        // Never dereference or restore a previous/freed owner.
        if(held && !(owner==current))held=false;
        if(hide) {
            if(!held){owner=current;saved=value;}
            if(value && !write(current.hud+0x13c,0))return false;
            held=true;return true;
        }
        if(held) {
            if(!value && !write(current.hud+0x13c,saved))return false;
            held=false;
        }
        return true;
    }
};
}

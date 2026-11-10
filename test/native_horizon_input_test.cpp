#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include "horizon_relocations.h"

static unsigned checks=0;
static void Check(bool value,const char* name) {
    ++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",name);std::exit(1);}
}
struct Body {
    unsigned char* p=nullptr;size_t size=0;
    explicit Body(const char* name) {
        FILE* f=nullptr;fopen_s(&f,name,"rb");Check(f!=nullptr,"fixture file");
        fseek(f,0,SEEK_END);size=ftell(f);rewind(f);
        p=static_cast<unsigned char*>(VirtualAlloc(nullptr,size,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        Check(p && fread(p,1,size,f)==size,"fixture allocation/read");fclose(f);
    }
    void Seal() {DWORD old=0;Check(VirtualProtect(p,size,PAGE_EXECUTE_READ,&old)!=0,"CPU RX");Check(FlushInstructionCache(GetCurrentProcess(),p,size)!=0,"CPU code visibility");}
    ~Body(){if(p)VirtualFree(p,0,MEM_RELEASE);}
};
static void Relative(Body& b,unsigned offset,const Body& target) {
    const auto displacement=int32_t(target.p-(b.p+offset+4));
    std::memcpy(b.p+offset,&displacement,4);
}
using Axis=float(__thiscall*)(void*,unsigned,unsigned*,unsigned*,int,bool);
using Tick=void(__thiscall*)(void*);
using Event=bool(__thiscall*)(void*,void*);
struct InputEvent {uint32_t type,index;float value;};
struct AxisDescriptor {uint32_t type;float gain,deadzone,rawScale;uint32_t index,pad[3];};
struct State {
    struct Entry {uint32_t type;float value;} entry[41];
    float previous[41];uint16_t invert;
};
static_assert(offsetof(State,previous)==0x148 && offsetof(State,invert)==0x1ec,"actual state layout");

int main() {
    Body axis("axis.bin"),store("store.bin"),tick("tick.bin"),reset("reset.bin"),event("event.bin"),constants("constants.bin");
    for(const auto& r:relocations){const auto address=uint32_t(reinterpret_cast<uintptr_t>(constants.p+r.constant));std::memcpy(axis.p+r.offset,&address,4);}
    Relative(event,0x14,reset);Relative(event,0x29,store);
    axis.Seal();store.Seal();tick.Seal();reset.Seal();event.Seal();
    DWORD old=0;Check(VirtualProtect(constants.p,constants.size,PAGE_READONLY,&old)!=0,"CPU constants readonly");
    const auto analog=reinterpret_cast<Axis>(axis.p);
    const auto advance=reinterpret_cast<Tick>(tick.p),clear=reinterpret_cast<Tick>(reset.p);
    const auto dispatch=reinterpret_cast<Event>(event.p);
    alignas(16) unsigned char nativeOwner[0x300]{};
    uint32_t nullReceiver=0;
    *reinterpret_cast<uint32_t*>(nativeOwner+0x54)=uint32_t(reinterpret_cast<uintptr_t>(&nullReceiver));
    auto& state=*reinterpret_cast<State*>(nativeOwner+0x60);
    unsigned map[1]={0};AxisDescriptor descriptor{2,1.f,.1f,1.f/32767.f,4,{0,0,0}};
    unsigned char device[0x180]{};
    *reinterpret_cast<void**>(device+0x12c)=map;*reinterpret_cast<void**>(device+0x13c)=&descriptor;
    auto mapped=[&](int raw,bool negate=false){InputEvent e{0,0x8d,99};e.value=analog(device,0,&e.type,&e.index,raw,negate);return e;};
    clear(&state);Check(state.entry[4].value==0 && state.invert==0,"native reset");
    for(int sign:{-1,1}) {
        auto e=mapped(sign*16384);
        Check(e.type==2 && e.index==4 && sign*e.value>.4f && sign*e.value<.5f,"actual analog descriptor/normalization");
        Check(!dispatch(nativeOwner,&e),"null downstream returns false after storing state");
        Check(state.entry[4].type==2 && state.entry[4].value==e.value,"actual LOOK_Y store");
        for(unsigned i=0;i<120;++i)advance(&state);
        Check(state.entry[4].value==e.value,"negative control: absent neutral retains analog across ticks");
        const auto neutral=mapped(0);Check(neutral.type==2 && neutral.index==4 && neutral.value==0,"actual neutral retains action identity");
        dispatch(nativeOwner,const_cast<InputEvent*>(&neutral));
        for(unsigned i=0;i<1000;++i){advance(&state);Check(state.entry[4].value==0,"actual neutral stays zero");}
        auto inverted=mapped(sign*16384,true);Check(inverted.value*sign<-.4f,"native inversion");
    }
    for(unsigned index=0;index<41;++index) {
        InputEvent relative{4,index,.75f};dispatch(nativeOwner,&relative);
        state.invert=0x101;advance(&state);
        Check(state.previous[index]==.75f && state.entry[index].value==0 && !state.invert,"type4 delta clears after saving previous");
    }
    for(int raw:{-32767,-16384,-1,0,1,16384,32767}) {
        auto e=mapped(raw);dispatch(nativeOwner,&e);const auto untouched=state.entry[4].value;
        InputEvent other{1,9,1.f};dispatch(nativeOwner,&other);advance(&state);
        Check(state.entry[4].value==untouched,"other action cannot clear or rewrite LOOK_Y");
        InputEvent resetEvent{0,0x3e,0};Check(dispatch(nativeOwner,&resetEvent),"explicit reset action handled");
        for(const auto& x:state.entry)Check(x.type==0 && x.value==0,"explicit reset clears all actions");
    }
    InputEvent unsupported{1,0x29,1.f};State before=state;
    Check(!dispatch(nativeOwner,&unsupported) && !std::memcmp(&state,&before,sizeof(State)),"out-of-range positive action does not store");
    std::printf("PASS %u checks: actual analog -> event -> typed state, neutral/release/tick/reset/inversion and retained-value negative controls; native camera/game not run\n",checks);
}

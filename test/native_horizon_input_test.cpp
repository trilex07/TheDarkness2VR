#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include "pad_packet_sequence.h"
#include "horizon_relocations.h"

static bool g_unifiedPadPackets=false;
static int g_enableVRGamepad=1;
#include "actual_packet_finalizer.inl"
static XINPUT_STATE xrSource{};
static bool xrSourceValid=true;
static bool CopyXrPad(XINPUT_STATE& out){out=xrSource;return xrSourceValid;}
static void ResetNativeAim(){}
#include "actual_packet_selector.inl"

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
    const auto displacement=int32_t(uint32_t(reinterpret_cast<uintptr_t>(target.p))-uint32_t(reinterpret_cast<uintptr_t>(b.p+offset+4)));
    std::memcpy(b.p+offset,&displacement,4);
}
static void RelativeAddress(Body& b,unsigned offset,uintptr_t target) {
    const auto displacement=uint32_t(target)-uint32_t(reinterpret_cast<uintptr_t>(b.p+offset+4));
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
static XINPUT_STATE supplied{};
static DWORD apiResult=ERROR_SUCCESS;
static Axis actualAxis=nullptr;
static Event actualEvent=nullptr;
static void* fakeAxisOwner=nullptr;
static void* fakeInputOwner=nullptr;
static unsigned builderCalls=0,neutralCalls=0;
static void __fastcall BasePoll(void*,void*,float) {}
static int Unexpected() {std::fprintf(stderr,"FAIL unsupported native poller housekeeping path\n");std::exit(1);}
static DWORD WINAPI InputApi(DWORD port,XINPUT_STATE* output) {
    Check(port==0,"native poller port");*output=supplied;return apiResult;
}
static void __fastcall RawBuilder(void*,void*,const XINPUT_STATE* oldValue,const XINPUT_STATE* newValue,int force) {
    ++builderCalls;
    // Exact 55FDB1..55FDC4 / BD3037 branch: negated right Y; changed-only,
    // except connection force. Other native raw actions are outside this fixture.
    if(force || oldValue->Gamepad.sThumbRY!=newValue->Gamepad.sThumbRY) {
        InputEvent e{0,0x8d,99};
        e.value=actualAxis(fakeAxisOwner,0,&e.type,&e.index,-int(newValue->Gamepad.sThumbRY),false);
        if(e.value==0)++neutralCalls;
        actualEvent(fakeInputOwner,&e);
    }
}

int main() {
    Body axis("axis.bin"),store("store.bin"),tick("tick.bin"),reset("reset.bin"),event("event.bin"),constants("constants.bin"),poll("poll.bin");
    for(const auto& r:relocations){const auto address=uint32_t(reinterpret_cast<uintptr_t>(constants.p+r.constant));std::memcpy(axis.p+r.offset,&address,4);}
    Relative(event,0x14,reset);Relative(event,0x29,store);
    for(const auto& r:pollRelocations) {
        uintptr_t target=reinterpret_cast<uintptr_t>(&Unexpected);
        if(r.target==0x89AF20)target=reinterpret_cast<uintptr_t>(&BasePoll);
        if(r.target==0xE441D2)target=reinterpret_cast<uintptr_t>(&InputApi);
        if(r.target==0x55FC60)target=reinterpret_cast<uintptr_t>(&RawBuilder);
        RelativeAddress(poll,r.offset,target);
    }
    axis.Seal();store.Seal();tick.Seal();reset.Seal();event.Seal();
    poll.Seal();
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
    actualAxis=analog;actualEvent=dispatch;fakeAxisOwner=device;fakeInputOwner=nativeOwner;
    const auto nativePoll=reinterpret_cast<void(__thiscall*)(void*,float)>(poll.p);
    auto handoff=[&](bool enabled) {
        alignas(16) unsigned char pollOwner[0x240]{};pollOwner[6]=pollOwner[7]=1;
        clear(&state);builderCalls=neutralCalls=0;apiResult=ERROR_SUCCESS;
        g_unifiedPadPackets=true;XINPUT_STATE empty{};FinalizePadPacket(0,ERROR_DEVICE_NOT_CONNECTED,&empty);
        g_unifiedPadPackets=enabled;
        XINPUT_STATE physicalSource{};physicalSource.dwPacketNumber=7;physicalSource.Gamepad.sThumbRY=16384;
        xrSource={};xrSource.dwPacketNumber=7;
        Check(ActualSelect(0,physicalSource,ERROR_SUCCESS,&supplied)==ERROR_SUCCESS && supplied.Gamepad.sThumbRY==16384,"actual selector prioritizes active physical Y");
        const auto physical=supplied.Gamepad;
        FinalizePadPacket(0,ERROR_SUCCESS,&supplied);Check(!std::memcmp(&physical,&supplied.Gamepad,sizeof(physical)),"physical payload preserved");
        nativePoll(pollOwner,1.f/60);Check(state.entry[4].value<-.4f && builderCalls==1,"actual native physical Y received");
        physicalSource.Gamepad={};
        Check(ActualSelect(0,physicalSource,ERROR_SUCCESS,&supplied)==ERROR_SUCCESS && supplied.Gamepad.sThumbRY==0 && supplied.dwPacketNumber==7,"actual selector switches to colliding XR neutral");
        const auto xr=supplied.Gamepad;
        FinalizePadPacket(0,ERROR_SUCCESS,&supplied);Check(!std::memcmp(&xr,&supplied.Gamepad,sizeof(xr)),"XR neutral payload preserved");
        nativePoll(pollOwner,1.f/60);
        if(enabled)Check(state.entry[4].value==0 && builderCalls==2 && neutralCalls==1,"GREEN: unified packet delivers actual neutral");
        else Check(state.entry[4].value<-.4f && builderCalls==1 && neutralCalls==0,"RED: equal source packet drops actual neutral");
        xrSource.dwPacketNumber=8;xrSource.Gamepad.wButtons=XINPUT_GAMEPAD_A;
        Check(ActualSelect(0,physicalSource,ERROR_SUCCESS,&supplied)==ERROR_SUCCESS && supplied.Gamepad.wButtons==XINPUT_GAMEPAD_A,"actual selector later XR button unchanged");
        FinalizePadPacket(0,ERROR_SUCCESS,&supplied);nativePoll(pollOwner,1.f/60);
        Check(enabled ? state.entry[4].value==0 : state.entry[4].value<-.4f,"later unrelated packet cannot repair skipped zero axis event");
        if(!enabled)std::puts("RED reproduced: actual89BB0F skips neutral; later button packet leaves typed LOOK_Y nonzero");
        else std::puts("GREEN actual poller -> native analog/event: neutral reached typed LOOK_Y; no axis/button mutation");
    };
    handoff(false);handoff(true);
    // Production finalizer gate and bookkeeping controls, without the native poller.
    for(bool enabled:{false,true})for(DWORD port:{0u,1u})for(int vr:{0,1}) {
        g_unifiedPadPackets=enabled;g_enableVRGamepad=vr;
        XINPUT_STATE v{};v.dwPacketNumber=123;v.Gamepad.wButtons=0xffff;v.Gamepad.sThumbRY=-1234;
        const auto b=v.Gamepad;FinalizePadPacket(port,ERROR_SUCCESS,&v);
        Check(!std::memcmp(&b,&v.Gamepad,sizeof(b)),"all finalizer payload bytes unchanged");
        if(!enabled || port || !vr)Check(v.dwPacketNumber==123,"default OFF/nonzero port/disabled VR pass through");
    }
    g_unifiedPadPackets=true;g_enableVRGamepad=1;
    XINPUT_STATE wrap{};wrap.dwPacketNumber=0xffffffffu;
    FinalizePadPacket(0,ERROR_DEVICE_NOT_CONNECTED,&wrap);FinalizePadPacket(0,ERROR_SUCCESS,&wrap);
    Check(wrap.dwPacketNumber==0,"DWORD wrap");
    auto stable=wrap;stable.dwPacketNumber=0xabcdef;FinalizePadPacket(0,ERROR_SUCCESS,&stable);
    Check(stable.dwPacketNumber==0,"unchanged payload stable despite incoming counter");
    stable.Gamepad.bRightTrigger=255;FinalizePadPacket(0,ERROR_SUCCESS,&stable);
    Check(stable.dwPacketNumber==1 && stable.Gamepad.bRightTrigger==255,"trigger edge increments without rebinding");
    std::printf("PASS %u checks: native typed input + real poller RED/GREEN + production finalizer; original camera integration/game not run\n",checks);
}

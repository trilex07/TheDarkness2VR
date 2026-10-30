#include "../src/native_menu_mouse_merge.h"
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace d2vr_menu_native;
static_assert(sizeof(DIDEVICEOBJECTDATA)==sizeof(MouseEvent),"x86 SDK event extent");
static_assert(offsetof(DIDEVICEOBJECTDATA,dwOfs)==offsetof(MouseEvent,offset),"offset ABI");
static_assert(offsetof(DIDEVICEOBJECTDATA,dwData)==offsetof(MouseEvent,data),"data ABI");
static_assert(offsetof(DIDEVICEOBJECTDATA,dwTimeStamp)==offsetof(MouseEvent,time),"timestamp ABI");
static_assert(offsetof(DIDEVICEOBJECTDATA,dwSequence)==offsetof(MouseEvent,sequence),"sequence ABI");
static_assert(offsetof(DIDEVICEOBJECTDATA,uAppData)==offsetof(MouseEvent,appData),"app data ABI");
static_assert(sizeof(DIMOUSESTATE2)==20&&offsetof(DIMOUSESTATE2,rgbButtons)==12,"mouse state ABI");
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
    MouseButtonMerge m;uint8_t state[20]{};MouseEvent e[40]{};uint32_t n=0;
    CHECK(m.Queue(true));CHECK(m.Queue(false));CHECK(m.Pending()==2);
    CHECK(m.Poll(0,0,20,state,e,n,40,123,456));CHECK(n==2);
    CHECK(e[0].offset==12&&e[0].data==128&&e[1].data==0);
    CHECK(e[0].time==123&&e[0].sequence==456&&e[0].appData==0);
    CHECK(!m.Owned()&&!m.Pending()&&!state[12]);
    // A laser release never releases a held physical mouse button.
    state[12]=0x81;n=0;CHECK(m.Queue(true));CHECK(m.Poll(0,0,20,state,e,n,40,1,1));
    CHECK(m.Owned()&&n==0&&state[12]==0x81);
    CHECK(m.Queue(false));CHECK(m.Poll(0,0,20,state,e,n,40,1,1));
    CHECK(!m.Owned()&&n==0&&state[12]==0x81);
    // A physical release while the laser holds cannot release the native button.
    state[12]=0;n=0;CHECK(m.Queue(true));CHECK(m.Poll(0,0,20,state,e,n,40,1,1));
    e[0]={12,0,2,3,0};e[1]={0,9,4,5,0};e[2]={13,128,6,7,0};n=3;
    CHECK(m.Poll(0,0,20,state,e,n,40,1,1));CHECK(n==2&&e[0].offset==0&&e[1].offset==13);
    CHECK(state[12]==128);m.Cancel();state[12]=0;n=0;
    CHECK(m.Poll(0,0,20,state,e,n,40,1,1));CHECK(n==1&&e[0].data==0&&!m.Owned());
    // A full buffer defers a press; cancel removes that undelivered click.
    CHECK(m.Queue(true));state[12]=0;n=1;e[0]={0,11,12,13,14};const auto saved=e[0];
    CHECK(m.Poll(0,0,20,state,e,n,1,1,1));CHECK(m.Pending()==1&&!m.Owned());
    CHECK(n==1&&!memcmp(e,&saved,20)&&!state[12]);m.Cancel();CHECK(!m.Pending());
    // Overflow must not consume a click that native state rebuild could erase.
    CHECK(m.Queue(true));CHECK(m.Queue(false));n=0;
    CHECK(m.Poll(1,0,20,state,e,n,40,1,1));CHECK(m.Pending()==2&&!n&&!state[12]);
    CHECK(m.Poll(0,0,20,state,e,n,40,1,1));CHECK(n==2&&!m.Pending());
    // Partial delivery across polls preserves a quick click's release.
    state[12]=0;n=0;CHECK(m.Queue(true));CHECK(m.Queue(false));
    CHECK(m.Poll(0,0,20,state,e,n,1,1,1));
    CHECK(n==1&&e[0].data==128&&m.Owned()&&m.Pending()==1);
    state[12]=0;n=0;CHECK(m.Poll(1,0,20,state,e,n,1,1,1));
    CHECK(n==0&&state[12]==128&&m.Owned()&&m.Pending()==1);
    state[12]=0;n=0;CHECK(m.Poll(0,0,20,state,e,n,1,1,1));
    CHECK(n==1&&e[0].data==0&&!m.Owned()&&!m.Pending());
    // Cancel a committed hold with a full unrelated-event buffer: retain its
    // release, not a dangling native press with lost ownership bookkeeping.
    state[12]=0;n=0;CHECK(m.Queue(true));CHECK(m.Poll(0,0,20,state,e,n,1,1,1));
    m.Cancel();state[12]=0;n=1;e[0]={8,120,1,1,0};
    CHECK(m.Poll(0,0,20,state,e,n,1,1,1));CHECK(m.Owned()&&m.Pending()==1&&state[12]==128);
    state[12]=0;n=0;CHECK(m.Poll(0,0,20,state,e,n,1,1,1));
    CHECK(!m.Owned()&&!m.Pending()&&e[0].data==0);
    // Zero-capacity calls can commit an ownership change that needs no event
    // (physical button already down), without touching an event pointer.
    state[12]=128;n=0;CHECK(m.Queue(true));
    CHECK(m.Poll(0,0,20,state,nullptr,n,0,1,1));CHECK(m.Owned()&&!m.Pending());
    m.Cancel();CHECK(m.Poll(0,0,20,state,nullptr,n,0,1,1));CHECK(!m.Owned()&&!m.Pending());
    // Invalid calls are complete no-ops, including queue/ownership and buffers.
    CHECK(m.Queue(true));
    for(unsigned kind=0;kind<7;++kind){
        memset(state,0x5a,sizeof(state));memset(e,0x3c,sizeof(e));n=1;
        uint8_t oldState[20];MouseEvent oldEvents[40];memcpy(oldState,state,20);memcpy(oldEvents,e,sizeof(e));
        CHECK(!m.Poll(kind==0?0x8007001Eu:(kind==1?2:0),kind==2?1:0,kind==3?16:20,
            kind==4?nullptr:state,kind==5?nullptr:e,n,kind==6?0:40,1,1));
        CHECK(n==1&&m.Pending()==1&&!m.Owned());CHECK(!memcmp(state,oldState,20));CHECK(!memcmp(e,oldEvents,sizeof(e)));
    }
    m.Cancel();
    // Bounded queue: explicit rejection preserves all earlier transitions.
    for(unsigned i=0;i<32;++i)CHECK(m.Queue((i&1)==0));
    CHECK(!m.Queue(true));CHECK(m.Pending()==32);m.Cancel();CHECK(!m.Pending());
    // Exhaustive physical/laser transitions through a small native event buffer.
    for(unsigned mask=0;mask<4096;++mask){
        MouseButtonMerge merge;bool physicalBefore=false,logicalBefore=false;
        for(unsigned step=0;step<6;++step){
            const bool physical=(mask>>(step*2))&1,logical=(mask>>(step*2+1))&1;
            memset(state,0,20);state[12]=physical?128:0;state[13]=0xa5;
            n=0;if(physical!=physicalBefore)e[n++]={12,physical?128u:0u,step,step,0};
            e[n++]={8,120,step,step,0}; // unrelated wheel event must survive
            CHECK(merge.Queue(logical));CHECK(merge.Poll(0,0,20,state,e,n,40,step,step));
            bool observed=physicalBefore||logicalBefore;unsigned wheel=0;
            for(uint32_t i=0;i<n;++i){if(e[i].offset==12)observed=(e[i].data&128)!=0;else{CHECK(e[i].offset==8&&e[i].data==120);++wheel;}}
            CHECK(wheel==1);CHECK(observed==(physical||logical));
            CHECK(bool(state[12]&128)==observed&&state[13]==0xa5);CHECK(!merge.Pending());
            physicalBefore=physical;logicalBefore=logical;
        }
    }
    std::printf("PASS native menu mouse merge: %u checks\n",checks);
}

#include "../src/unarmed_trigger_dpad.h"
#include <cstdio>
using namespace d2vr_input;
static int failures=0;
#define CHECK(c) do{ if(!(c)){ std::printf("FAIL line %d: %s\n",__LINE__,#c); ++failures; } }while(0)
int main() {
    const auto E=HandSlot::Empty, A=HandSlot::Armed, U=HandSlot::Unknown;
    { // right trigger, empty main hand -> DPAD_DOWN pulse, trigger hidden
        UnarmedTriggerDpad m; uint8_t l=0,r=255; int f=0;
        auto b=m.Update(l,r,A,E,true,1000,&f);
        CHECK(b==UnarmedTriggerDpad::kDpadDown); CHECK(f==2); CHECK(r==0);
        r=255; b=m.Update(l,r,A,A,true,1050,&f); // weapon now drawn, still held
        CHECK(b==UnarmedTriggerDpad::kDpadDown); CHECK(f==0); CHECK(r==0);
        r=255; b=m.Update(l,r,A,A,true,1200,&f);
        CHECK(b==0); CHECK(r==0); // held press never becomes a shot
        r=0; m.Update(l,r,A,A,true,1300); r=255; b=m.Update(l,r,A,A,true,1400);
        CHECK(b==0); CHECK(r==255); // new press with weapon fires normally
    }
    { // left trigger, empty off hand -> DPAD_RIGHT
        UnarmedTriggerDpad m; uint8_t l=200,r=0; int f=0;
        auto b=m.Update(l,r,E,A,true,0,&f);
        CHECK(b==UnarmedTriggerDpad::kDpadRight); CHECK(f==1); CHECK(l==0); CHECK(r==0);
    }
    { // armed hands pass through, no D-pad
        UnarmedTriggerDpad m; uint8_t l=255,r=255;
        CHECK(m.Update(l,r,A,A,true,0)==0); CHECK(l==255); CHECK(r==255);
    }
    { // unknown native state or no gameplay context -> pass through
        UnarmedTriggerDpad m; uint8_t l=255,r=255;
        CHECK(m.Update(l,r,U,U,true,0)==0); CHECK(l==255); CHECK(r==255);
        UnarmedTriggerDpad n; l=255; r=255;
        CHECK(n.Update(l,r,E,E,false,0)==0); CHECK(l==255); CHECK(r==255);
    }
    { // partial pull on empty hand is hidden but does not fire until threshold
        UnarmedTriggerDpad m; uint8_t l=0,r=90; int f=0;
        CHECK(m.Update(l,r,A,E,true,0,&f)==0); CHECK(f==0); CHECK(r==0);
        r=200; CHECK(m.Update(l,r,A,E,true,10,&f)==UnarmedTriggerDpad::kDpadDown); CHECK(f==2);
    }
    { // holding trigger does not re-fire; release + press fires again
        UnarmedTriggerDpad m; uint8_t l=0,r=255; int f=0;
        m.Update(l,r,A,E,true,0,&f); CHECK(f==2);
        r=255; m.Update(l,r,A,E,true,500,&f); CHECK(f==0);
        r=0; m.Update(l,r,A,E,true,600,&f);
        r=255; CHECK(m.Update(l,r,A,E,true,700,&f)==UnarmedTriggerDpad::kDpadDown); CHECK(f==2);
    }
    { // both at once
        UnarmedTriggerDpad m; uint8_t l=255,r=255; int f=0;
        CHECK(m.Update(l,r,E,E,true,0,&f)==(UnarmedTriggerDpad::kDpadDown|UnarmedTriggerDpad::kDpadRight)); CHECK(f==3);
    }
    if(failures) { std::printf("%d failure(s)\n",failures); return 1; }
    std::printf("unarmed_trigger_dpad: all tests passed\n"); return 0;
}

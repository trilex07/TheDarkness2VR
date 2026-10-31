#include "../src/unarmed_trigger_dpad.h"
#include <cstdio>
using namespace d2vr_input;
static int failures=0;
#define CHECK(c) do{ if(!(c)){ std::printf("FAIL line %d: %s\n",__LINE__,#c); ++failures; } }while(0)
int main() {
    const auto E=HandSlot::Empty, A=HandSlot::Armed, U=HandSlot::Unknown;
    using T=UnarmedTriggerDpad;
    { // left trigger + empty left hand alternates DOWN, UP; trigger hidden until release
        T m; uint8_t l=255,r=0; int f=0;
        CHECK(m.Update(l,r,E,A,true,1000,&f)==T::kDpadDown); CHECK(f==1); CHECK(l==0);
        l=255; CHECK(m.Update(l,r,A,A,true,1050,&f)==T::kDpadDown); CHECK(f==0); CHECK(l==0); // weapon arrived, still held
        l=0; m.Update(l,r,A,A,true,1300);
        l=0; m.Update(l,r,E,A,true,1400);
        l=255; CHECK(m.Update(l,r,E,A,true,1500,&f)==T::kDpadUp); CHECK(f==1);
        l=0; m.Update(l,r,E,A,true,1700);
        l=255; CHECK(m.Update(l,r,E,A,true,1800,&f)==T::kDpadDown); CHECK(f==1);
    }
    { // armed left hand fires normally
        T m; uint8_t l=255,r=0;
        CHECK(m.Update(l,r,A,A,true,0)==0); CHECK(l==255);
    }
    { // right trigger always passes through, even with an empty main hand
        T m; uint8_t l=0,r=255; int f=0;
        CHECK(m.Update(l,r,A,E,true,0,&f)==0); CHECK(f==0); CHECK(r==255);
    }
    { // unknown state / no context pass through
        T m; uint8_t l=255,r=255;
        CHECK(m.Update(l,r,U,U,true,0)==0); CHECK(l==255);
        T n; l=255; CHECK(n.Update(l,r,E,E,false,0)==0); CHECK(l==255);
    }
    { // partial pull on empty left hand hidden, fires at threshold
        T m; uint8_t l=90,r=0; int f=0;
        CHECK(m.Update(l,r,E,A,true,0,&f)==0); CHECK(f==0); CHECK(l==0);
        l=200; CHECK(m.Update(l,r,E,A,true,10,&f)==T::kDpadDown); CHECK(f==1);
    }
    { // double tap X: first tap passes, second within window -> D-pad, X hidden until release
        using D=DoubleTapDpad; D d; uint16_t b; uint16_t f=0;
        b=D::kX; d.Update(b,D::kDpadRight,true,1000,&f); CHECK(b==D::kX); CHECK(f==0);
        b=0; d.Update(b,D::kDpadRight,true,1080,&f); CHECK(b==0);
        b=D::kX; d.Update(b,D::kDpadRight,true,1200,&f); CHECK(f==D::kDpadRight); CHECK(b==D::kDpadRight);
        b=D::kX; d.Update(b,D::kDpadRight,true,1300,&f); CHECK(f==0); CHECK(b==D::kDpadRight); // still hidden, pulse
        b=D::kX; d.Update(b,D::kDpadRight,true,1400,&f); CHECK(b==0);                          // hidden, pulse over
        b=0; d.Update(b,D::kDpadRight,true,1500,&f); CHECK(b==0);
        b=D::kX; d.Update(b,D::kDpadRight,true,1600,&f); CHECK(b==D::kX); CHECK(f==0);        // third press = new first tap
    }
    { // empty main hand -> draw
        using D=DoubleTapDpad; D d; uint16_t b; uint16_t f=0;
        b=D::kX; d.Update(b,D::kDpadDown,true,0); b=0; d.Update(b,D::kDpadDown,true,50);
        b=D::kX; d.Update(b,D::kDpadDown,true,200,&f); CHECK(f==D::kDpadDown); CHECK(b==D::kDpadDown);
    }
    { // slow taps and holds never fire; hold X stays X (eat hearts)
        using D=DoubleTapDpad; D d; uint16_t b; uint16_t f=0;
        b=D::kX; d.Update(b,D::kDpadRight,true,0); b=0; d.Update(b,D::kDpadRight,true,100);
        b=D::kX; d.Update(b,D::kDpadRight,true,600,&f); CHECK(f==0); CHECK(b==D::kX);
        for(uint64_t t=650;t<2000;t+=50){ b=D::kX; d.Update(b,D::kDpadRight,true,t,&f); CHECK(f==0); CHECK(b==D::kX); }
    }
    { // outside gameplay context nothing changes
        using D=DoubleTapDpad; D d; uint16_t b; uint16_t f=0;
        b=D::kX; d.Update(b,D::kDpadRight,false,0); b=0; d.Update(b,D::kDpadRight,false,50);
        b=D::kX; d.Update(b,D::kDpadRight,false,100,&f); CHECK(f==0); CHECK(b==D::kX);
    }
    { // configurable target, OFF passes X through, parser
        using D=DoubleTapDpad; D d; uint16_t b; uint16_t f=0;
        b=D::kX; d.Update(b,0x8000,true,10); b=0; d.Update(b,0x8000,true,50);
        b=D::kX; d.Update(b,0x8000,true,100,&f); CHECK(f==0x8000); CHECK(b==0x8000);
        D o; b=D::kX; o.Update(b,0,true,10); b=0; o.Update(b,0,true,50);
        b=D::kX; o.Update(b,0,true,100,&f); CHECK(f==0); CHECK(b==D::kX);
        uint16_t bits=0; bool byHand=false;
        CHECK(ParsePadButton("dpad_right",bits,byHand) && bits==0x0008 && !byHand);
        CHECK(ParsePadButton("Y",bits,byHand) && bits==0x8000);
        CHECK(ParsePadButton("AUTO",bits,byHand) && byHand);
        CHECK(ParsePadButton("OFF",bits,byHand) && bits==0);
        CHECK(!ParsePadButton("bogus",bits,byHand));
    }
    { // toggle classification
        CHECK(ClassifyFirearm(1,0)==WeaponClass::OneHanded);
        CHECK(ClassifyFirearm(6,1)==WeaponClass::OneHanded);
        CHECK(ClassifyFirearm(4,0)==WeaponClass::TwoHanded);
        CHECK(ClassifyFirearm(7,0)==WeaponClass::TwoHanded);
        CHECK(ClassifyFirearm(2,0)==WeaponClass::Unknown);
        CHECK(ClassifyFirearm(2,1)==WeaponClass::Unknown);
        CHECK(ClassifyFirearm(0,0)==WeaponClass::Unknown);
        CHECK(ToggleTarget(WeaponClass::TwoHanded,A,0)==0x0004);
        CHECK(ToggleTarget(WeaponClass::OneHanded,A,0)==0x0008);
        CHECK(ToggleTarget(WeaponClass::OneHanded,E,0)==0x0002);
        CHECK(ToggleTarget(WeaponClass::Unknown,A,0x0008)==0x0004);
        CHECK(ToggleTarget(WeaponClass::Unknown,U,0x0004)==0x0008);
    }
    if(failures) { std::printf("%d failure(s)\n",failures); return 1; }
    std::printf("unarmed_trigger_dpad: all tests passed\n"); return 0;
}

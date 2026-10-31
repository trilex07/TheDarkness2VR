// Host test: dual weapon wheels, R3 hold gate, crouch toggle, rasteriser.
// Windows: test/build_dual_weapon_wheel.ps1 (MSVC /W4 /WX). Linux: g++ -std=c++17.
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../src/dual_weapon_wheel.h"
#include "../src/weapon_wheel_raster.h"

static int g_fail=0, g_checks=0;
#define CHECK(c) do{ ++g_checks; if(!(c)){ ++g_fail; std::printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#c);} }while(0)

using namespace d2vr_wheel;

static void HoldGateTests() {
    HoldGate g; bool fwd=false;
    // Tap: hidden while pressed, then a short pulse after release.
    CHECK(g.Update(true,false,true,1000,fwd)==HoldGate::Event::None); CHECK(!fwd);
    CHECK(g.Update(true,false,true,1100,fwd)==HoldGate::Event::None); CHECK(!fwd);
    CHECK(g.Update(false,false,true,1150,fwd)==HoldGate::Event::None); CHECK(fwd);
    g.Update(false,false,true,1240,fwd); CHECK(fwd);
    g.Update(false,false,true,1251,fwd); CHECK(!fwd);
    // Hold: opens after kHoldMs, release commits, R3 never forwarded.
    g.Reset();
    g.Update(true,false,true,0,fwd);
    CHECK(g.Update(true,false,true,219,fwd)==HoldGate::Event::None); CHECK(!fwd);
    CHECK(g.Update(true,false,true,220,fwd)==HoldGate::Event::Open); CHECK(!fwd && g.open);
    CHECK(g.Update(true,false,true,2000,fwd)==HoldGate::Event::None); CHECK(!fwd);
    CHECK(g.Update(false,false,true,2010,fwd)==HoldGate::Event::Commit); CHECK(!fwd && !g.open);
    g.Update(false,false,true,2050,fwd); CHECK(!fwd); // no tap after a commit
    // Recenter chord: L3 during the pending press -> raw pass-through, no open.
    g.Reset();
    g.Update(true,false,true,0,fwd); CHECK(!fwd);
    g.Update(true,true,true,50,fwd); CHECK(fwd);
    CHECK(g.Update(true,true,true,400,fwd)==HoldGate::Event::None); CHECK(fwd && !g.open);
    g.Update(false,false,true,450,fwd); CHECK(!fwd);
    // L3 first, then R3: raw from the first poll.
    g.Reset(); g.Update(true,true,true,0,fwd); CHECK(fwd);
    // L3 while open cancels.
    g.Reset(); g.Update(true,false,true,0,fwd);
    CHECK(g.Update(true,false,true,300,fwd)==HoldGate::Event::Open);
    CHECK(g.Update(true,true,true,310,fwd)==HoldGate::Event::Cancel); CHECK(!g.open);
    // Context loss while open cancels; not allowed at press -> raw.
    g.Reset(); g.Update(true,false,true,0,fwd); g.Update(true,false,true,300,fwd);
    CHECK(g.Update(true,false,false,310,fwd)==HoldGate::Event::Cancel);
    g.Reset(); g.Update(true,false,false,0,fwd); CHECK(fwd);
    CHECK(g.Update(true,false,true,500,fwd)==HoldGate::Event::None); CHECK(fwd && !g.open);
}

static void FlickTests() {
    using D=StickFlick::Dir;
    StickFlick t; D act=D::None;
    CHECK(t.Update(0,-.5f,true,0,act)==D::None && act==D::None);
    CHECK(t.Update(.3f,-.8f,true,10,act)==D::Down && act==D::Down);   // slightly sideways still counts
    CHECK(t.Update(0,-1.f,true,50,act)==D::None && act==D::Down);
    CHECK(t.Update(0,-1.f,true,200,act)==D::None && act==D::None);    // held: one pulse only
    CHECK(t.Update(0,.9f,true,210,act)==D::None);                     // not rearmed via centre
    t.Update(0,0,true,300,act);
    CHECK(t.Update(0,.9f,true,310,act)==D::Up && act==D::Up);         // jump
    t.Update(0,.1f,true,500,act);
    CHECK(t.Update(-.9f,-.8f,true,600,act)==D::None);                 // diagonal beyond 45 deg: turning
    CHECK(t.Update(0,-.9f,false,700,act)==D::None && act==D::None);   // no context
    CHECK(t.Update(0,-.9f,true,710,act)==D::None);                    // must rearm after context returns
    t.Update(0,0,true,720,act); CHECK(t.Update(0,-.9f,true,730,act)==D::Down);
    t.Reset(); CHECK(t.Update(NAN,NAN,true,0,act)==D::None);
}

static void SectorTests() {
    CHECK(SectorFromOffset(0,0,2,.04f)==-1);
    CHECK(SectorFromOffset(0,-.07f,2,.04f)==0);   // up
    CHECK(SectorFromOffset(0,.07f,2,.04f)==1);    // down
    CHECK(SectorFromOffset(.07f,0,4,.04f)==1);    // right, clockwise
    CHECK(SectorFromOffset(-.07f,0,4,.04f)==3);
    CHECK(SectorFromOffset(.01f,.07f,1,.04f)==0);
    CHECK(SectorFromOffset(.07f,0,0,.04f)==-1);
    CHECK(SectorFromOffset(NAN,0,2,.04f)==-1);
    // Must agree with the graphics icon centres for every count.
    for(int n=1;n<=8;++n) for(int i=0;i<n;++i) {
        const auto c=d2vr_weapon_wheel_graphics::SlotCenter(i,n);
        CHECK(SectorFromOffset(c.x,c.y,n,.01f)==i);
    }
    HandPick p; p.Update(0,0,2,.04f); CHECK(p.hover==-1 && !p.touched);
    p.Update(0,.08f,2,.04f); CHECK(p.hover==1 && p.touched);
    p.Update(0,0,2,.04f); CHECK(p.hover==-1 && p.touched);
}

static Loadout L(bool o0,bool o1,bool o2,int h0,int h1,bool swapped=false) {
    Loadout l; l.valid=true; l.owned[0]=o0; l.owned[1]=o1; l.owned[2]=o2;
    l.hand[0]=h0; l.hand[1]=h1; l.swapped=swapped; return l;
}

static void LoadoutTests() {
    // Headset 23:08: two one-handed weapons, right hand alternating 1/2.
    auto two=L(false,true,true,2,8);
    auto m=WeaponMenu(two);
    CHECK(m.count==2 && m.slot[0]==1 && m.slot[1]==2 && m.equipped==1);
    m=WeaponMenu(L(true,true,true,0,8));
    CHECK(m.count==3 && m.slot[2]==0 && m.equipped==2);
    m=WeaponMenu(L(true,true,true,1,2,true));               // swapped: right one-handed is slot 2
    CHECK(m.slot[0]==2 && m.slot[1]==1 && m.equipped==1);
    CHECK(WeaponMenu(Loadout{}).count==0);
    CHECK(WeaponMenu(L(true,true,true,kSlotOther,8)).equipped==-1);

    using R=Equip::Result;
    Equip e; CHECK(e.Step(two,0)==R::Idle && !e.Bits(0));
    // 2 -> 1: DPAD_LEFT, native hand changes, done.
    e.Start(1); CHECK(e.Step(two,1000)==R::Pending && e.Bits(1000)==kDpadLeft && e.presses==1);
    CHECK(e.Step(L(false,true,true,1,8),1100)==R::Pending);       // still inside the pulse
    CHECK(e.Bits(1149)==kDpadLeft && e.Bits(1150)==0);
    CHECK(e.Step(L(false,true,true,1,8),1200)==R::Done && !e.Active());
    // Lands on the wrong weapon first: second press.
    auto three=L(true,true,true,0,8);
    e.Start(2); e.Step(three,0); CHECK(e.bits==kDpadLeft);
    CHECK(e.Step(L(true,true,true,1,8),200)==R::Pending && e.presses==2 && e.Bits(200)==kDpadLeft);
    CHECK(e.Step(L(true,true,true,2,8),400)==R::Done);
    // Two-handed uses DPAD_RIGHT; empty hand draws with DPAD_DOWN.
    e.Start(0); e.Step(L(true,true,true,1,2),0); CHECK(e.bits==kDpadRight);
    e.Start(1); e.Step(L(true,true,true,8,8),0); CHECK(e.bits==kDpadDown);
    // No change -> retry after settle, give up after kMaxPresses.
    e.Start(1); uint64_t t=0;
    for(int i=0;i<10 && e.Active();++i) { e.Step(two,t); t+=Equip::kPulseMs+Equip::kSettleMs; }
    CHECK(!e.Active() && e.presses==Equip::kMaxPresses);
    // Unowned target fails without pressing; unknown loadout fails after settle.
    e.Start(0); CHECK(e.Step(two,0)==R::Failed && e.presses==0);
    Loadout unknown; e.Start(1); CHECK(e.Step(unknown,10)==R::Pending); CHECK(e.Step(unknown,700)==R::Failed);

    uint16_t bits=1;
    CHECK(ParseCrouchButton("r3",bits) && bits==kR3);
    CHECK(ParseCrouchButton("B",bits) && bits==0x2000);
    CHECK(ParseCrouchButton("X",bits) && bits==0x4000);
    CHECK(ParseCrouchButton("off",bits) && bits==0);
    CHECK(!ParseCrouchButton("R",bits) && !ParseCrouchButton("R33",bits) && !ParseCrouchButton(nullptr,bits));
}

static void RasterTests(const char* ppm) {
    const int W=1024,H=512;
    std::vector<uint32_t> px(size_t(W)*H,0xDEADBEEF), scratch(d2vr_wheel_raster::ScratchPixels(512));
    d2vr_wheel_raster::Target left{px.data(),W,H,W,0,0,512}, right{px.data(),W,H,W,512,0,512};
    d2vr_weapon_wheel_graphics::Wheel wl, wr;
    wl.count=1; wl.slots[0]={d2vr_weapon_icons::Icon::Pistol,true}; wl.equipped=0;
    wr.count=2; wr.slots[0]={d2vr_weapon_icons::Icon::DesertEagle,true};
    wr.slots[1]={d2vr_weapon_icons::Icon::Carbine,true}; wr.selected=1; wr.equipped=0;
    const unsigned vl=d2vr_wheel_raster::DrawWheel(left,wl,true,scratch.data());
    const unsigned vr=d2vr_wheel_raster::DrawWheel(right,wr,false,scratch.data());
    CHECK(vl>0 && vr>0 && vl<=d2vr_weapon_wheel_graphics::MaxVerticesPerWheel && vr<=d2vr_weapon_wheel_graphics::MaxVerticesPerWheel);
    CHECK(px[0]==0 && px[size_t(W)*H-1]==0);          // corners cleared and transparent
    const uint32_t centre=px[size_t(256)*W+256];
    CHECK((centre>>24)>200);                           // centre disc opaque
    bool premult=true;
    for(uint32_t p:px) { const unsigned a=p>>24; if((p&255)>a||((p>>8)&255)>a||((p>>16)&255)>a+1) premult=false; }
    CHECK(premult);
    // Hover changes the image of the hovered sector only.
    const auto before=px;
    wr.selected=0; d2vr_wheel_raster::DrawWheel(right,wr,false,scratch.data());
    size_t changedLeft=0, changedRight=0;
    for(int y=0;y<H;++y) for(int x=0;x<W;++x) if(px[size_t(y)*W+x]!=before[size_t(y)*W+x]) (x<512?changedLeft:changedRight)++;
    CHECK(changedLeft==0 && changedRight>1000);
    // Degenerate / non-finite triangles are ignored.
    d2vr_weapon_wheel_graphics::Vertex n{NAN,0,1,1,1,1}, z{0,0,1,1,1,1};
    const auto copy=px; d2vr_wheel_raster::Triangle(left,n,z,z); d2vr_wheel_raster::Triangle(left,z,z,z);
    CHECK(copy==px);
    CHECK(d2vr_wheel_raster::DrawWheel(left,wl,true,nullptr)==0);
    d2vr_wheel_raster::Target bad{px.data(),W,H,W,600,0,512};
    CHECK(d2vr_wheel_raster::DrawWheel(bad,wl,true,scratch.data())==0);
    // Seam check: a fully covered opaque region must be uniform (no cracks).
    {
        std::vector<uint32_t> img(64*64,0), sc(d2vr_wheel_raster::ScratchPixels(64));
        d2vr_wheel_raster::Target sq{img.data(),64,64,64,0,0,64};
        d2vr_weapon_wheel_graphics::Wheel one; one.count=1; one.slots[0]={d2vr_weapon_icons::Icon::EmptyHand,false};
        d2vr_wheel_raster::DrawWheel(sq,one,false,sc.data());
        CHECK((img[32*64+5]>>24)>200);
    }
    if(ppm) {
        if(FILE* f=std::fopen(ppm,"wb")) {
            std::fprintf(f,"P6\n%d %d\n255\n",W,H);
            for(uint32_t p:px) { // composite over a mid-grey "world" for review
                const float a=(p>>24)/255.f; unsigned char rgb[3];
                rgb[0]=(unsigned char)(((p>>16)&255)+90*(1-a)); rgb[1]=(unsigned char)(((p>>8)&255)+90*(1-a));
                rgb[2]=(unsigned char)((p&255)+90*(1-a)); std::fwrite(rgb,1,3,f);
            }
            std::fclose(f);
        }
    }
}

int main(int argc,char** argv) {
    HoldGateTests(); FlickTests(); SectorTests(); LoadoutTests();
    RasterTests(argc>1 ? argv[1] : nullptr);
    std::printf("%s: %d checks, %d failed\n",g_fail?"FAIL":"PASS",g_checks,g_fail);
    return g_fail?1:0;
}

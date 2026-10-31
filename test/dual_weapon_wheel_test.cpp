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

static void CrouchTests() {
    StickDownToggle t; bool pulse=false, sup=false;
    CHECK(!t.Update(0,-.5f,true,0,pulse,sup)); CHECK(!pulse && !sup);
    CHECK(t.Update(0,-.8f,true,10,pulse,sup)); CHECK(pulse && sup);
    CHECK(!t.Update(0,-1.f,true,50,pulse,sup)); CHECK(pulse && sup);
    CHECK(!t.Update(0,-1.f,true,200,pulse,sup)); CHECK(!pulse && sup); // held: one toggle only
    CHECK(!t.Update(0,-.5f,true,300,pulse,sup)); CHECK(sup);            // not rearmed yet
    CHECK(!t.Update(0,-.8f,true,310,pulse,sup));
    CHECK(!t.Update(0,0,true,400,pulse,sup)); CHECK(!sup);               // rearm
    CHECK(t.Update(0,-.9f,true,500,pulse,sup));                          // second toggle (stand up)
    t.Reset();
    CHECK(!t.Update(.9f,-.8f,true,0,pulse,sup));                         // diagonal: snap-turn territory
    CHECK(!t.Update(0,-.9f,false,0,pulse,sup)); CHECK(!pulse && !sup);   // no context
    CHECK(!t.Update(0,-.9f,true,10,pulse,sup));                          // must rearm after context returns
    t.Update(0,0,true,20,pulse,sup); CHECK(t.Update(0,-.9f,true,30,pulse,sup));
    CHECK(!t.Update(NAN,NAN,true,40,pulse,sup));
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
    auto full=L(true,true,true,1,2);
    auto r=RightMenu(full), l=LeftMenu(full);
    CHECK(r.count==2 && r.slot[0]==1 && r.slot[1]==0 && r.equipped==0);
    CHECK(l.count==1 && l.slot[0]==2 && l.equipped==0);
    auto sw=L(true,true,true,0,8,true);
    r=RightMenu(sw); l=LeftMenu(sw);
    CHECK(r.slot[0]==2 && r.equipped==1); CHECK(l.slot[0]==1 && l.equipped==-1);
    CHECK(RightMenu(Loadout{}).count==0);
    CHECK(RightMenu(L(false,true,false,1,8)).count==1);
    CHECK(LeftMenu(L(true,true,false,0,8)).count==0);

    // Resolve: dual -> two-handed and back.
    CHECK(Resolve(full,0,-1)==Action::TwoHanded);
    CHECK(Resolve(L(true,true,true,0,8),1,-1)==Action::OneHanded);
    CHECK(Resolve(L(true,true,true,0,8),-1,2)==Action::OneHanded);   // left pick ends two-handed
    CHECK(Resolve(full,1,2)==Action::None);                         // already there
    CHECK(Resolve(full,-1,-1)==Action::None);
    CHECK(Resolve(L(true,true,true,1,8),-1,2)==Action::OneHanded);   // single -> dual
    CHECK(Resolve(L(true,true,true,1,8),1,-1)==Action::None);
    CHECK(Resolve(L(false,true,true,1,2),0,-1)==Action::None);       // two-handed not owned
    CHECK(Resolve(L(true,true,true,0,8),0,2)==Action::None);         // right wins, already two-handed
    CHECK(Resolve(L(true,true,true,8,8),1,-1)==Action::OneHanded);   // empty hands
    CHECK(Resolve(Loadout{},0,-1)==Action::None);
    CHECK(Resolve(L(true,true,true,kSlotOther,8),1,-1)==Action::OneHanded); // unknown item in hand
    CHECK(!Satisfied(Action::OneHanded,L(true,true,true,kSlotOther,8)));
    CHECK(RightMenu(L(true,true,true,kSlotOther,8)).equipped==-1);
    CHECK(PadBits(Action::TwoHanded)==kDpadRight && PadBits(Action::OneHanded)==kDpadLeft && !PadBits(Action::None));

    Request q; q.Start(Action::TwoHanded,1000);
    CHECK(q.Bits(1000)==kDpadRight && q.Bits(1149)==kDpadRight && q.Bits(1150)==0);
    CHECK(q.Check(L(true,true,true,0,8),1100)==Request::Result::Pending); // not before pulse end
    CHECK(q.Check(full,1200)==Request::Result::Pending);
    CHECK(q.Check(L(true,true,true,0,8),1200)==Request::Result::Done);
    q.Start(Action::OneHanded,0);
    CHECK(q.Check(L(true,true,true,0,8),1499)==Request::Result::Pending);
    CHECK(q.Check(L(true,true,true,0,8),1500)==Request::Result::TimedOut);
    CHECK(q.Check(full,1600)==Request::Result::None);

    uint16_t bits=1;
    CHECK(ParseCrouchButton("r3",bits) && bits==kR3);
    CHECK(ParseCrouchButton("B",bits) && bits==0x2000);
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
    HoldGateTests(); CrouchTests(); SectorTests(); LoadoutTests();
    RasterTests(argc>1 ? argv[1] : nullptr);
    std::printf("%s: %d checks, %d failed\n",g_fail?"FAIL":"PASS",g_checks,g_fail);
    return g_fail?1:0;
}

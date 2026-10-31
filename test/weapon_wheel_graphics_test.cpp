#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
#include "../src/weapon_wheel_graphics.h"

using namespace d2vr_weapon_wheel_graphics;
static unsigned long checks;
static std::size_t maximum;
static void Check(bool condition) {
    ++checks;
    if(!condition) { std::printf("FAIL assertion %lu\n",checks);std::exit(1); }
}
static std::size_t SelectedFillVertices(const std::vector<Vertex>& vertices) {
    std::size_t count=0;
    for(const auto& v:vertices)
        if(v.r==.26f && v.g==.022f && v.b==.035f && v.a==.95f) ++count;
    return count;
}
static void Validate(const std::vector<Vertex>& vertices,const Placement& p) {
    Check(!vertices.empty() && vertices.size()%3==0 && vertices.size()<=MaxVerticesPerWheel);
    if(vertices.size()>maximum) maximum=vertices.size();
    for(const auto& v:vertices) {
        Check(std::isfinite(v.x) && std::isfinite(v.y));
        const float x=(v.x-p.x)/p.radius,y=(v.y-p.y)/p.radius;
        Check(x*x+y*y<=1.00001f);
        Check(v.r>=0 && v.r<=1 && v.g>=0 && v.g<=1 &&
              v.b>=0 && v.b<=1 && v.a>=0 && v.a<=1);
    }
    for(std::size_t i=0;i<vertices.size();i+=3) {
        const auto& a=vertices[i];const auto& b=vertices[i+1];const auto& c=vertices[i+2];
        Check((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)>1e-10f);
    }
}
static void WritePreview(const char* path,const std::vector<Vertex>& vertices) {
    FILE* file=nullptr;
    fopen_s(&file,path,"wb");Check(file!=nullptr);
    std::fputs("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 1280 720\">"
               "<rect width=\"1280\" height=\"720\" fill=\"#141214\"/>",file);
    for(std::size_t i=0;i<vertices.size();i+=3) {
        const auto& a=vertices[i];const auto& b=vertices[i+1];const auto& c=vertices[i+2];
        std::fprintf(file,"<polygon points=\"%.3f,%.3f %.3f,%.3f %.3f,%.3f\" "
                     "fill=\"rgb(%d,%d,%d)\" fill-opacity=\"%.4f\"/>",
                     640+a.x*580,360+a.y*580,640+b.x*580,360+b.y*580,
                     640+c.x*580,360+c.y*580,int(a.r*255+.5f),int(a.g*255+.5f),
                     int(a.b*255+.5f),a.a);
    }
    // Preview annotation only: runtime mesh remains font/label independent.
    std::fputs("<g fill=\"#eae2d5\" font-family=\"Segoe UI, sans-serif\" text-anchor=\"middle\">"
               "<text x=\"350\" y=\"63\" font-size=\"21\" letter-spacing=\"6\">LEFT HAND</text>"
               "<text x=\"930\" y=\"63\" font-size=\"21\" letter-spacing=\"6\">RIGHT HAND</text>"
               "<g font-size=\"14\" letter-spacing=\"1\">"
               "<text x=\"350\" y=\"243\">M1911</text><text x=\"930\" y=\"243\">M1911</text>"
               "<text x=\"521\" y=\"409\">MICRO UZI</text><text x=\"1101\" y=\"409\">MICRO UZI</text>"
               "<text x=\"350\" y=\"581\">DEFENDER</text><text x=\"930\" y=\"581\">DEFENDER</text>"
               "<text x=\"178\" y=\"430\">EMPTY</text><text x=\"758\" y=\"430\">EMPTY</text>"
               "</g><text x=\"640\" y=\"652\" font-size=\"12\" letter-spacing=\"2\" fill=\"#928881\">"
               "ILLUSTRATIVE LOADOUT</text><text x=\"640\" y=\"684\" font-size=\"14\" letter-spacing=\"3\" fill=\"#928881\">"
               "HOLD R3  &#183;  REACH TO SELECT  &#183;  RELEASE TO EQUIP</text></g></svg>",file);
    Check(std::fclose(file)==0);
}
int main(int argc,char** argv) {
    std::vector<Vertex> vertices;vertices.reserve(MaxVerticesDual);
    auto emit=[&](Vertex a,Vertex b,Vertex c) {
        vertices.push_back(a);vertices.push_back(b);vertices.push_back(c);
    };
    // Every supported slot count, valid/neutral selection, handedness and icon;
    // availability covers all enabled, all disabled and alternating subsets.
    for(int count=1;count<=8;++count) for(int selected=-1;selected<count;++selected)
    for(int handed=0;handed<2;++handed) for(int icon=0;icon<static_cast<int>(d2vr_weapon_icons::Icon::Count);++icon)
    for(int availability=0;availability<4;++availability) {
        Wheel wheel;wheel.count=count;wheel.selected=selected;wheel.equipped=selected;
        for(int i=0;i<count;++i) {
            wheel.slots[i].available=availability==0 ||
                (availability==2 && i%2==0) || (availability==3 && i%2!=0);
            wheel.slots[i].icon=static_cast<d2vr_weapon_icons::Icon>(icon);
        }
        vertices.clear();BuildWheel(wheel,handed!=0,emit,Placement{0,0,1});
        Validate(vertices,Placement{0,0,1});
        const bool hasSelection=selected>=0 && wheel.slots[selected].available;
        Check((SelectedFillVertices(vertices)>0)==hasSelection);
        for(int i=0;i<count;++i) {
            const Point q=SlotCenter(i,count,Placement{0,0,1});
            Check(std::fabs(q.x*q.x+q.y*q.y-IconRadius*IconRadius)<1e-5f);
        }
    }
    // Placement must work independently for two world-space hand anchors.
    Wheel placed;placed.selected=0;placed.equipped=0;
    for(auto& slot:placed.slots) slot.available=true;
    vertices.clear();BuildWheel(placed,true,emit,Placement{-.27f,.18f,.18f});
    Validate(vertices,Placement{-.27f,.18f,.18f});
    const auto equippedCount=vertices.size();
    placed.equipped=-1;vertices.clear();BuildWheel(placed,true,emit,Placement{-.27f,.18f,.18f});
    Check(equippedCount==vertices.size()+24); // one eight-triangle equipped dot
    placed.selected=99;vertices.clear();BuildWheel(placed,true,emit);
    Check(SelectedFillVertices(vertices)==0);
    // Invalid layout/count must never publish nonfinite geometry.
    for(const Placement p:{Placement{0,0,0},Placement{0,0,-1},
                            Placement{std::numeric_limits<float>::infinity(),0,1},
                            Placement{0,0,std::numeric_limits<float>::quiet_NaN()}}) {
        vertices.clear();BuildWheel(placed,false,emit,p);Check(vertices.empty());
    }
    for(int count:{-1,0}) {placed.count=count;vertices.clear();BuildWheel(placed,false,emit);Check(vertices.empty());}
    placed.count=9;vertices.clear();BuildWheel(placed,false,emit,Placement{0,0,1});Validate(vertices,Placement{0,0,1});
    const auto invalidCenter=SlotCenter(0,0);Check(invalidCenter.x==0 && invalidCenter.y==0);
    const auto top=SlotCenter(0,4),right=SlotCenter(1,4);
    Check(std::fabs(top.x)<1e-5f && top.y<0 && right.x>0 && std::fabs(right.y)<1e-5f);
    Wheel left,rightWheel;left.selected=0;rightWheel.selected=2;left.equipped=1;rightWheel.equipped=0;
    const d2vr_weapon_icons::Icon loadout[]={d2vr_weapon_icons::Icon::M1911,
        d2vr_weapon_icons::Icon::MicroUzi,d2vr_weapon_icons::Icon::Defender,d2vr_weapon_icons::Icon::EmptyHand};
    for(int i=0;i<4;++i) {
        left.slots[i].available=i!=2;rightWheel.slots[i].available=true;
        left.slots[i].icon=rightWheel.slots[i].icon=loadout[i];
    }
    vertices.clear();BuildDual(left,rightWheel,emit);
    Check(!vertices.empty() && vertices.size()<=MaxVerticesDual);
    for(const auto& v:vertices) Check(v.x>=-1 && v.x<=1 && v.y>=-1 && v.y<=1);
    if(argc>1) WritePreview(argv[1],vertices);
    std::printf("PASS %lu geometry checks; max vertices/wheel=%zu; dual vertices=%zu\n",
                checks,maximum,vertices.size());
}

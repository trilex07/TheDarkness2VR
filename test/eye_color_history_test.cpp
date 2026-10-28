#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::printf("FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using HRESULT=long;
using BOOL=int;
#define FAILED(x) ((x)<0)
#define SUCCEEDED(x) ((x)>=0)
constexpr int FALSE=0,D3DTEXF_NONE=0;
struct D3DSURFACE_DESC {unsigned Width=10,Height=10,Format=1,MultiSampleType=0,MultiSampleQuality=0;};
struct IDirect3DSurface9 {
    unsigned refs=1,pixel=0;bool owned=false;
    void AddRef(){++refs;}
    void Release(){CHECK(refs);if(!--refs && owned)delete this;}
    HRESULT GetDesc(D3DSURFACE_DESC* d){*d=D3DSURFACE_DESC{};return 0;}
};
struct IDirect3DDevice9 {
    bool failCopy=false,failCreate=false;unsigned copies=0;
    HRESULT StretchRect(IDirect3DSurface9* src,void*,IDirect3DSurface9* dst,void*,int) {
        ++copies;if(failCopy)return -1;dst->pixel=src->pixel;return 0;
    }
    HRESULT CreateRenderTarget(unsigned,unsigned,unsigned,unsigned,unsigned,BOOL,IDirect3DSurface9** output,void*) {
        if(failCreate)return -1;*output=new IDirect3DSurface9;(*output)->owned=true;return 0;
    }
};
static bool g_had3DCameraThisFrame=true,g_hadWorldDrawThisFrame=false;
@IMPLEMENTATION@
int main() {
    IDirect3DDevice9 dev;IDirect3DSurface9 output,other;
    for(unsigned frame=1;frame<=1000;++frame) {
        output.pixel=2*frame;CHECK(g_rightColorHistory.Save(&dev,&output));
        output.pixel=2*frame+1; // current live/left pixels
        const unsigned copies=dev.copies;
        g_hadWorldDrawThisFrame=true;
        CHECK(SeedActualCaller(&dev,&output));
        CHECK(output.pixel==2*frame+1 && dev.copies==copies); // world draw regenerates right
        g_hadWorldDrawThisFrame=false;
        CHECK(SeedActualCaller(&dev,&output));
        CHECK(output.pixel==2*frame && dev.copies==copies+1); // camera-only/UI must seed right
        output.pixel+=10000;CHECK(g_rightColorHistory.Save(&dev,&output)); // right UI composited
        output.pixel=3;CHECK(SeedRightEyeBackground(&dev,&output,false));
        CHECK(output.pixel==2*frame+10000);
    }
    dev.failCopy=true;CHECK(!SeedRightEyeBackground(&dev,&output,false));
    CHECK(!g_rightColorHistory.Save(&dev,&output));
    CHECK(output.refs==1 && !g_rightColorHistory.pixels && !g_rightColorHistory.target);
    dev.failCopy=false;dev.failCreate=true;CHECK(!g_rightColorHistory.Save(&dev,&output));
    dev.failCreate=false;output.pixel=8;CHECK(g_rightColorHistory.Save(&dev,&output));
    other.pixel=99;CHECK(SeedRightEyeBackground(&dev,&other,false));
    CHECK(other.pixel==99 && output.refs==1); // never seed an unrelated target
    CHECK(g_rightColorHistory.Save(&dev,&other));CHECK(other.refs==2);
    g_rightColorHistory.Release();CHECK(other.refs==1);
    CHECK(!g_rightColorHistory.Save(&dev,nullptr));
    std::puts("PASS actual-source right-eye history:1000 world/camera-only/UI segments, no copy for world draws, copy/create failure, target change and ownership release; simulated pixels, not HMD evidence");
    return 0;
}

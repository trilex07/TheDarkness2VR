#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstdint>
struct IDirect3DDevice9{};
struct IDirect3DSurface9{unsigned color;};
using D3DTEXTUREFILTERTYPE=unsigned;
struct D3DRECT {long x1,y1,x2,y2;};
using D3DCOLOR=DWORD;
static unsigned checks=0,starts=0,forwards=0,timing=0;
struct RecLockGuard {};
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL line%d %s\n",__LINE__,#x);exit(1);}}while(0)
static bool g_insidePresentation=false,g_preSceneWriteMissed=false,captureFailure=false;
static int g_submitMode=4,g_stereoMode=1;
static DWORD g_renderThreadId=GetCurrentThreadId();
static volatile LONG g_needRecBegin=1;
static unsigned color=0x12,snapshotColor=0;
static std::vector<unsigned> commands;
namespace d2vr_replay {
static bool g_recActive=false,g_recOverflow=false;
static void RecBeginFrame(IDirect3DDevice9*) {
    ++starts;snapshotColor=color;commands.clear();g_recActive=!captureFailure;
}
}
static struct {void Begin(IDirect3DDevice9*){++timing;}} g_gpuTiming;
static constexpr unsigned OP_Clear=1,OP_ColorFill=2,OP_StretchRect=3;
static std::vector<RECT> rectangles;
static uint32_t RecPush(const void* data,size_t bytes){CHECK(bytes==sizeof(RECT));rectangles.push_back(*static_cast<const RECT*>(data));return uint32_t((rectangles.size()-1)*sizeof(RECT));}
template<class... A>static void RecEmit(unsigned op,unsigned mask,DWORD a,DWORD b,A... args){
    if(op==OP_Clear){CHECK(a==1);commands.push_back(b);}
    else {
        const DWORD values[]={DWORD(args)...};
        if(op==OP_ColorFill){CHECK(mask<=1);if(mask)CHECK(values[0]/sizeof(RECT)<rectangles.size());commands.push_back(b);}
        else {CHECK(op==OP_StretchRect&&mask<=3);CHECK(values[0]==2);if(mask&1)CHECK(values[1]/sizeof(RECT)<rectangles.size());if(mask&2)CHECK(values[2]/sizeof(RECT)<rectangles.size());commands.push_back(reinterpret_cast<IDirect3DSurface9*>(a)->color);}
    }
}
static void RecEmitData(unsigned,const D3DRECT*,size_t,DWORD flags,D3DCOLOR c,DWORD,DWORD,void*,unsigned,DWORD){CHECK(flags==1);commands.push_back(c);}
using d2vr_replay::g_recActive;
using d2vr_replay::g_recOverflow;
static HRESULT WINAPI Forward(IDirect3DDevice9*,DWORD,const D3DRECT*,DWORD,D3DCOLOR c,float,DWORD){++forwards;color=c;return S_OK;}
static HRESULT WINAPI ForwardFill(IDirect3DDevice9*,IDirect3DSurface9* surface,const RECT*,D3DCOLOR c){++forwards;color=surface->color=c;return S_OK;}
static HRESULT WINAPI ForwardStretch(IDirect3DDevice9*,IDirect3DSurface9* source,const RECT*,IDirect3DSurface9* target,const RECT*,D3DTEXTUREFILTERTYPE){++forwards;color=target->color=source->color;return S_OK;}
static struct {decltype(&Forward) Clear=Forward;decltype(&ForwardFill) ColorFill=ForwardFill;decltype(&ForwardStretch) StretchRect=ForwardStretch;} g_orig;
@IMPLEMENTATION@
static void Reset(){g_insidePresentation=g_preSceneWriteMissed=captureFailure=false;g_submitMode=4;g_stereoMode=1;g_renderThreadId=GetCurrentThreadId();g_needRecBegin=1;g_recActive=false;starts=forwards=timing=0;commands.clear();rectangles.clear();color=0x12;}
int main(){
    IDirect3DDevice9 device;D3DRECT rect{0,0,8,8};
    for(unsigned frame=0;frame<1000;++frame){
        Reset();CHECK(Hook_Clear(&device,0,nullptr,1,frame+1,1,0)==S_OK);
        CHECK(starts==1&&timing==1&&forwards==1&&snapshotColor==0x12);
        CHECK(commands.size()==1&&commands[0]==frame+1&&color==frame+1);
        CHECK(Hook_Clear(&device,1,&rect,1,frame+2,1,0)==S_OK);
        CHECK(starts==1&&forwards==2&&commands.size()==2&&commands[1]==frame+2);
        // BeginScene must continue this stream rather than release the clear.
        CHECK(!g_needRecBegin&&g_recActive&&!g_preSceneWriteMissed);
        unsigned right=0xdeadbeef;for(auto c:commands)right=c;CHECK(right==color);
    }
    for(unsigned gate=0;gate<4;++gate){
        Reset();switch(gate){case 0:g_insidePresentation=true;break;case 1:g_submitMode=0;break;case 2:g_stereoMode=0;break;default:++g_renderThreadId;}
        CHECK(Hook_Clear(&device,0,nullptr,1,7,1,0)==S_OK);CHECK(!starts&&commands.empty()&&forwards==1);
    }
    Reset();captureFailure=true;Hook_Clear(&device,0,nullptr,1,7,1,0);
    CHECK(g_preSceneWriteMissed&&!g_recActive&&commands.empty()&&forwards==1&&!timing);
    RECT area{0,0,8,8};IDirect3DSurface9 source{0x34},target{0};
    for(unsigned kind=0;kind<2;++kind)for(unsigned mask=0;mask<4;++mask){
        Reset();
        if(kind==0)CHECK(Hook_ColorFill(&device,&target,(mask&1)?&area:nullptr,0x56)==S_OK);
        else CHECK(Hook_StretchRect(&device,&source,(mask&1)?&area:nullptr,&target,(mask&2)?&area:nullptr,2)==S_OK);
        CHECK(starts==1&&timing==1&&snapshotColor==0x12&&forwards==1);
        CHECK(commands.size()==1&&commands.back()==color&&target.color==color);
        CHECK(!g_needRecBegin&&g_recActive&&!g_preSceneWriteMissed);
        Hook_Clear(&device,0,nullptr,1,0x78,1,0);
        CHECK(starts==1&&forwards==2&&commands.size()==2&&commands.back()==0x78);
        for(unsigned gate=0;gate<5;++gate){
            Reset();switch(gate){case 0:g_insidePresentation=true;break;case 1:g_submitMode=0;break;case 2:g_stereoMode=0;break;case 3:++g_renderThreadId;break;default:g_needRecBegin=0;}
            if(kind==0)Hook_ColorFill(&device,&target,&area,0x56);
            else Hook_StretchRect(&device,&source,&area,&target,&area,2);
            CHECK(!starts&&!timing&&commands.empty()&&forwards==1);
        }
        Reset();captureFailure=true;
        if(kind==0)Hook_ColorFill(&device,&target,&area,0x56);
        else Hook_StretchRect(&device,&source,&area,&target,&area,2);
        CHECK(g_preSceneWriteMissed&&!g_recActive&&commands.empty()&&forwards==1&&!timing);
    }
    printf("PASS pre-scene writes actual hooks: %u checks, 1000 clear sequences plus full/rectangular fill/copy, pre-state/order/payload, one start, owner gates and capture failure; CPU fixture, not HMD proof\n",checks);
}

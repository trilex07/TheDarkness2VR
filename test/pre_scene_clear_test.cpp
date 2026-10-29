#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
struct IDirect3DDevice9{};
struct D3DRECT {long x1,y1,x2,y2;};
using D3DCOLOR=DWORD;
static unsigned checks=0,starts=0,forwards=0,timing=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL line%d %s\n",__LINE__,#x);exit(1);}}while(0)
static bool g_insidePresentation=false,g_preSceneClearMissed=false,captureFailure=false;
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
static constexpr unsigned OP_Clear=1;
template<class... A>static void RecEmit(unsigned,unsigned,DWORD flags,D3DCOLOR c,A...){CHECK(flags==1);commands.push_back(c);}
static void RecEmitData(unsigned,const D3DRECT*,size_t,DWORD flags,D3DCOLOR c,DWORD,DWORD,void*,unsigned,DWORD){CHECK(flags==1);commands.push_back(c);}
using d2vr_replay::g_recActive;
static HRESULT WINAPI Forward(IDirect3DDevice9*,DWORD,const D3DRECT*,DWORD,D3DCOLOR c,float,DWORD){++forwards;color=c;return S_OK;}
static struct {decltype(&Forward) Clear=Forward;} g_orig;
@IMPLEMENTATION@
static void Reset(){g_insidePresentation=g_preSceneClearMissed=captureFailure=false;g_submitMode=4;g_stereoMode=1;g_renderThreadId=GetCurrentThreadId();g_needRecBegin=1;g_recActive=false;starts=forwards=timing=0;commands.clear();color=0x12;}
int main(){
    IDirect3DDevice9 device;D3DRECT rect{0,0,8,8};
    for(unsigned frame=0;frame<1000;++frame){
        Reset();CHECK(Hook_Clear(&device,0,nullptr,1,frame+1,1,0)==S_OK);
        CHECK(starts==1&&timing==1&&forwards==1&&snapshotColor==0x12);
        CHECK(commands.size()==1&&commands[0]==frame+1&&color==frame+1);
        CHECK(Hook_Clear(&device,1,&rect,1,frame+2,1,0)==S_OK);
        CHECK(starts==1&&forwards==2&&commands.size()==2&&commands[1]==frame+2);
        // BeginScene must continue this stream rather than release the clear.
        CHECK(!g_needRecBegin&&g_recActive&&!g_preSceneClearMissed);
        unsigned right=0xdeadbeef;for(auto c:commands)right=c;CHECK(right==color);
    }
    for(unsigned gate=0;gate<4;++gate){
        Reset();switch(gate){case 0:g_insidePresentation=true;break;case 1:g_submitMode=0;break;case 2:g_stereoMode=0;break;default:++g_renderThreadId;}
        CHECK(Hook_Clear(&device,0,nullptr,1,7,1,0)==S_OK);CHECK(!starts&&commands.empty()&&forwards==1);
    }
    Reset();captureFailure=true;Hook_Clear(&device,0,nullptr,1,7,1,0);
    CHECK(g_preSceneClearMissed&&!g_recActive&&commands.empty()&&forwards==1&&!timing);
    printf("PASS pre-scene clear actual source: %u checks, 1000 full/rectangular clear sequences; pre-state before forwarding, one record start, owner gates and capture failure\n",checks);
}

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "../src/replay_constant_window.h"
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define HR(x) CHECK(SUCCEEDED(x))
// Actual driver-state contract, supplementary to the extracted production-loop
// regression. No game, visible window, Present, or frame-time claim.
int main() {
    static_assert(sizeof(void*)==4,"x86 required");
    wchar_t path[MAX_PATH];CHECK(GetSystemDirectoryW(path,MAX_PATH));
    CHECK(!wcscat_s(path,L"\\d3d9.dll"));HMODULE module=LoadLibraryW(path);CHECK(module);
    auto create=reinterpret_cast<HRESULT(WINAPI*)(UINT,IDirect3D9Ex**)>(GetProcAddress(module,"Direct3DCreate9Ex"));CHECK(create);
    IDirect3D9Ex* api=nullptr;HR(create(D3D_SDK_VERSION,&api));
    HWND window=CreateWindowExW(0,L"STATIC",L"D2VR material constants",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(window);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.BackBufferWidth=pp.BackBufferHeight=64;
    IDirect3DDevice9Ex* dev=nullptr;HR(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,window,0x54,&pp,nullptr,&dev));
    IDirect3DTexture9* texture=nullptr;HR(dev->CreateTexture(4,4,1,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture,nullptr));
    unsigned calls[2]{};
    for(unsigned round=0;round<200;++round) {
        float expectedVS[16],expectedPS[16];
        for(unsigned i=0;i<16;++i){expectedVS[i]=float(round*16+i)/1024;expectedPS[i]=float(round+i)/256;}
        for(unsigned mode=0;mode<2;++mode) {
            d2vr_replay_batch::ConstantWindow pending;pending.Reset(256,224);
            auto send=[&](d2vr_replay_batch::Stage stage,unsigned first,const float* data,unsigned n)->long {
                ++calls[mode];return stage==d2vr_replay_batch::Stage::Vertex ?
                    dev->SetVertexShaderConstantF(first,data,n):dev->SetPixelShaderConstantF(first,data,n);
            };
            auto bind=[&](unsigned kind) {
                if(!mode)pending.Flush(send);
                if(kind==0)HR(dev->SetTexture(0,texture));
                if(kind==1)HR(dev->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT));
                if(kind==2)HR(dev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE));
            };
            for(unsigned i=0;i<4;++i) {
                pending.Upload(d2vr_replay_batch::Stage::Vertex,i,expectedVS+i*4,1,send);bind(i%3);
                pending.Upload(d2vr_replay_batch::Stage::Pixel,i,expectedPS+i*4,1,send);bind((i+1)%3);
            }
            pending.Flush(send); // observer/draw boundary
            float actual[16];HR(dev->GetVertexShaderConstantF(0,actual,4));CHECK(!memcmp(actual,expectedVS,sizeof(actual)));
            HR(dev->GetPixelShaderConstantF(0,actual,4));CHECK(!memcmp(actual,expectedPS,sizeof(actual)));
            IDirect3DBaseTexture9* bound=nullptr;HR(dev->GetTexture(0,&bound));CHECK(bound==texture);bound->Release();
            DWORD sampler=0,stage=0;HR(dev->GetSamplerState(0,D3DSAMP_MINFILTER,&sampler));CHECK(sampler==D3DTEXF_POINT);
            HR(dev->GetTextureStageState(0,D3DTSS_COLOROP,&stage));CHECK(stage==D3DTOP_MODULATE);
        }
    }
    CHECK(calls[0]==1600 && calls[1]==400);
    HR(dev->SetTexture(0,nullptr));texture->Release();dev->Release();api->Release();DestroyWindow(window);FreeLibrary(module);
    printf("PASS actual x86 HAL/Ex flags=0x54: 400 material sequences, exact VS/PS bits and bindings; constant calls %u -> %u (synthetic only)\n",calls[0],calls[1]);
}

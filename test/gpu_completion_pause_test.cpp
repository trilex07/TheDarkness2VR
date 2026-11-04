#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/gpu_completion_pause.h"
#include <wrl/client.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
using Microsoft::WRL::ComPtr;
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define HR(x) CHECK(SUCCEEDED(x))
static double Cpu(){FILETIME c,e,k,u;CHECK(GetThreadTimes(GetCurrentThread(),&c,&e,&k,&u));ULARGE_INTEGER a{},b{};a.LowPart=k.dwLowDateTime;a.HighPart=k.dwHighDateTime;b.LowPart=u.dwLowDateTime;b.HighPart=u.dwHighDateTime;return double(a.QuadPart+b.QuadPart)*.0001;}
static double Now(){LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return 1000.0*double(t.QuadPart)/double(f.QuadPart);}
int main(){
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    HR(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
    d2vr_gpu_wait::CompletionPause pause,invalid;
    CHECK(!invalid.Prepare(nullptr,nullptr));CHECK(!invalid.Pause(0));
    ComPtr<ID3D11DeviceContext> deferred;HR(device->CreateDeferredContext(0,&deferred));
    CHECK(!invalid.Prepare(device.Get(),deferred.Get()));
    CHECK(pause.Prepare(device.Get(),context.Get()));
    D3D11_TEXTURE2D_DESC desc{};desc.Width=2676;desc.Height=2728;desc.MipLevels=1;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
    ComPtr<ID3D11Texture2D> a,b;HR(device->CreateTexture2D(&desc,nullptr,&a));HR(device->CreateTexture2D(&desc,nullptr,&b));
    D3D11_QUERY_DESC qd{D3D11_QUERY_EVENT,0};ComPtr<ID3D11Query> query;HR(device->CreateQuery(&qd,&query));
    // A/B/A/B on the same device, queue depth and full-resolution GPU copies.
    for(unsigned phase=0;phase<4;++phase){
        bool event=(phase&1)!=0;std::vector<double> wall;unsigned long polls=0,wakes=0;double cpu=Cpu();
        for(unsigned i=0;i<400;++i){
            for(unsigned copy=0;copy<16;++copy){context->CopyResource(b.Get(),a.Get());context->CopyResource(a.Get(),b.Get());}
            context->End(query.Get());context->Flush();double start=Now();HRESULT result;
            while((result=context->GetData(query.Get(),nullptr,0,D3D11_ASYNC_GETDATA_DONOTFLUSH))==S_FALSE){
                CHECK(Now()-start<2000);++polls;
                if(event){if(pause.Pause())++wakes;}else SwitchToThread();
            }
            CHECK(result==S_OK);wall.push_back(Now()-start);
        }
        cpu=Cpu()-cpu;std::sort(wall.begin(),wall.end());double total=0;for(auto w:wall)total+=w;
        printf("phase=%u mode=%s frames=400 CPU_ms=%.3f wait_mean=%.4f median=%.4f p95=%.4f polls=%lu wakes=%lu\n",phase,event?"event":"yield",cpu,total/400,wall[200],wall[380],polls,wakes);
    }
    // Early/zero-timeout wakes remain advisory: every reuse above required S_OK.
    CHECK(pause.Pause(0) || SUCCEEDED(device->GetDeviceRemovedReason()));
    CHECK(pause.Drain());
    printf("PASS actual x86 D3D11 GPU completion event; original-query ownership retained in 1600 iterations\n");
}

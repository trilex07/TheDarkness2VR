#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <initializer_list>
using Microsoft::WRL::ComPtr;
#define CHECK(x) do{if(!(x)){printf("FAIL line%d %s\n",__LINE__,#x);exit(1);}}while(0)
#define HR(x) CHECK(SUCCEEDED(x))
constexpr UINT W=128;
struct Vertex {float x,y,z,rhw;DWORD color;};
int main(){
 char path[MAX_PATH];CHECK(GetSystemDirectoryA(path,MAX_PATH));strcat_s(path,"\\d3d9.dll");auto dll=LoadLibraryA(path);CHECK(dll);
 auto create=reinterpret_cast<HRESULT(WINAPI*)(UINT,IDirect3D9Ex**)>(GetProcAddress(dll,"Direct3DCreate9Ex"));CHECK(create);
 ComPtr<IDirect3D9Ex> api;HR(create(D3D_SDK_VERSION,&api));D3DADAPTER_IDENTIFIER9 adapter{};HR(api->GetAdapterIdentifier(0,0,&adapter));printf("Adapter=%s driver=%s\n",adapter.Description,adapter.Driver);
 struct Format {const char* name;D3DFORMAT format;};
 const Format formats[]={{"A8R8G8B8",D3DFMT_A8R8G8B8},{"A16B16G16R16F",D3DFMT_A16B16G16R16F},{"D24S8",D3DFMT_D24S8},{"INTZ",D3DFORMAT(MAKEFOURCC('I','N','T','Z'))}};
 for(auto f:formats)for(unsigned n:{2u,4u,8u}){DWORD levels=0;HRESULT hr=api->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,f.format,TRUE,D3DMULTISAMPLE_TYPE(n),&levels);printf("MSAA format=%s samples=%u hr=%08X qualityLevels=%lu\n",f.name,n,unsigned(hr),levels);}
 HWND win=CreateWindowA("STATIC","Coverage GPU audit",WS_OVERLAPPEDWINDOW,0,0,W,W,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);CHECK(win);
 D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=win;pp.BackBufferWidth=W;pp.BackBufferHeight=W;pp.BackBufferFormat=D3DFMT_A8R8G8B8;
 ComPtr<IDirect3DDevice9Ex> dev;HR(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,win,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,nullptr,&dev));
 HR(dev->SetVertexShader(nullptr));HR(dev->SetPixelShader(nullptr));HR(dev->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE));HR(dev->SetTexture(0,nullptr));HR(dev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1));HR(dev->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE));HR(dev->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));HR(dev->SetRenderState(D3DRS_ZENABLE,FALSE));HR(dev->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE));HR(dev->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,TRUE));
 ComPtr<IDirect3DSurface9> resolved,read;HR(dev->CreateRenderTarget(W,W,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&resolved,nullptr));HR(dev->CreateOffscreenPlainSurface(W,W,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&read,nullptr));
 double errors[5]{};unsigned counts[5]{};
 for(unsigned mode=0;mode<5;++mode){const float scale=mode==1?2.f:mode==3?1.25f:mode==4?1.5f:1.f;const UINT extent=UINT(W*scale);D3DMULTISAMPLE_TYPE samples=mode==2?D3DMULTISAMPLE_4_SAMPLES:D3DMULTISAMPLE_NONE;
  ComPtr<IDirect3DSurface9> target;HR(dev->CreateRenderTarget(extent,extent,D3DFMT_A8R8G8B8,samples,0,FALSE,&target,nullptr));
  for(unsigned phase=0;phase<32;++phase){const double intercept=31.+double(phase)/32.;
   auto vertex=[&](double x,double y){return Vertex{float(x*scale-.5),float(y*scale-.5),.5f,1,0xffffffff};};
   // Half-plane clipped to y=0. The sloped boundary is y=.37*x+intercept.
   Vertex v[]={vertex(0,0),vertex(W,0),vertex(0,intercept),vertex(W,.37*W+intercept)};
   HR(dev->SetRenderTarget(0,target.Get()));D3DVIEWPORT9 vp{0,0,extent,extent,0,1};HR(dev->SetViewport(&vp));HR(dev->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0));HR(dev->BeginScene());HR(dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Vertex)));HR(dev->EndScene());
   HR(dev->StretchRect(target.Get(),nullptr,resolved.Get(),nullptr,(mode==1 || mode>=3)?D3DTEXF_LINEAR:D3DTEXF_NONE));HR(dev->GetRenderTargetData(resolved.Get(),read.Get()));D3DLOCKED_RECT lock{};HR(read->LockRect(&lock,nullptr,D3DLOCK_READONLY));
   for(UINT y=0;y<W;++y)for(UINT x=0;x<W;++x){unsigned red=(reinterpret_cast<const DWORD*>(static_cast<const char*>(lock.pBits)+y*lock.Pitch)[x]>>16)&255;
    // Verify no blur away from the actual geometric boundary.
    const double distance=(double(y)+.5)-(.37*(double(x)+.5)+intercept);
    if(distance < -1.5)CHECK(red==255);else if(distance>1.5)CHECK(red==0);
    if(std::abs(distance)>1.5)continue;
    unsigned covered=0;for(unsigned sy=0;sy<32;++sy)for(unsigned sx=0;sx<32;++sx)covered+=(double(y)+(sy+.5)/32.)<.37*(double(x)+(sx+.5)/32.)+intercept;
    double ideal=covered/1024.;double e=red/255.-ideal;errors[mode]+=e*e;++counts[mode];
   }
   HR(read->UnlockRect());
  }
  printf("Coverage mode=%s phases=32 edgeSamples=%u RMS_error=%.6f interiors=exact\n",mode==0?"native1x":mode==1?"SSAA2xDimensions":mode==2?"MSAA4x":mode==3?"SSAA1.25xDimensions":"SSAA1.5xDimensions",counts[mode],sqrt(errors[mode]/counts[mode]));
 }
 CHECK(errors[1]<errors[0]*.5);CHECK(errors[2]<errors[0]);
 printf("MSAA4x 50-percent error-energy efficacy gate=%s; not selected for integration\n",errors[2]<errors[0]*.5?"PASS":"FAIL");
 // A D3D9 texture itself has no sample-count argument. Sampleable INTZ depth
 // and MRT resolve ordering remain separate integration blockers, regardless
 // of CheckDeviceMultiSampleType's result. This is NOT a game/MSAA candidate.
 puts("PASS actual hardware coverage improves without filtering flat interiors; native game integration NOT RUN");DestroyWindow(win);return 0;
}


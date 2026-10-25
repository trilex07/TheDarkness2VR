// Real D3D11 WARP execution of the vendored per-eye NIS sharpen path.
#include <windows.h>
#include <d3d11.h>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../src/nis_runtime.h"
#include "../src/nis/NIS_Scaler_CS.h"
#include "../src/nis/NIS_Sharpen_CS.h"
#pragma comment(lib,"d3d11.lib")
static void Say(const char* s) { std::printf("%s\n",s); }
static void Check(bool ok,const char* why) { if(!ok) { std::printf("FAIL %s\n",why); std::exit(1); } }
int main() {
    ID3D11Device* dev=nullptr; ID3D11DeviceContext* ctx=nullptr;
    D3D_FEATURE_LEVEL level{};
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
            D3D11_SDK_VERSION,&dev,&level,&ctx)),"WARP device");
    Check(level>=D3D_FEATURE_LEVEL_11_0,"compute 5.0 support");
    NisRuntime nis{};
    Check(NisInit(&nis,dev,kNisScalerCS,sizeof(kNisScalerCS),
        kNisSharpenCS,sizeof(kNisSharpenCS),nullptr,Say),"embedded NIS init");
    constexpr UINT W=256,H=256;
    std::vector<unsigned char> pixels(W*H*4);
    for(UINT y=0;y<H;++y) for(UINT x=0;x<W;++x) {
        const unsigned ramp=x%64;
        auto v=(ramp<24)?65:(ramp>40?180:65+(int)(ramp-24)*7);
        if((y/32)&1) v=245-v;
        for(int c=0;c<3;++c) pixels[4*(y*W+x)+c]=(unsigned char)v;
        pixels[4*(y*W+x)+3]=255;
    }
    D3D11_TEXTURE2D_DESC td{};td.Width=W;td.Height=H;td.MipLevels=1;td.ArraySize=1;
    td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.SampleDesc.Count=1;
    td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{};init.pSysMem=pixels.data();init.SysMemPitch=W*4;
    ID3D11Texture2D* source=nullptr; ID3D11ShaderResourceView* view=nullptr;
    Check(SUCCEEDED(dev->CreateTexture2D(&td,&init,&source)),"source texture");
    Check(SUCCEEDED(dev->CreateShaderResourceView(source,nullptr,&view)),"source SRV");
    Check(NisEnsureOutput(&nis,dev,W,H,W,H),"two persistent outputs");
    for(int eye=0;eye<2;++eye)
        Check(NisDispatchEye(&nis,ctx,view,W,H,eye,W,H,.2f,true),"NIS eye dispatch");
    td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* readback[2]{};
    for(int eye=0;eye<2;++eye) {
        Check(SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&readback[eye])),"staging texture");
        ctx->CopyResource(readback[eye],nis.outTex[eye]);
    }
    std::vector<unsigned char> output[2];
    for(int eye=0;eye<2;++eye) {
        D3D11_MAPPED_SUBRESOURCE m{};
        Check(SUCCEEDED(ctx->Map(readback[eye],0,D3D11_MAP_READ,0,&m)),"readback map");
        output[eye].resize(W*H*4);
        for(UINT y=0;y<H;++y) std::memcpy(output[eye].data()+y*W*4,
                                           (const unsigned char*)m.pData+y*m.RowPitch,W*4);
        ctx->Unmap(readback[eye],0);
    }
    Check(output[0]==output[1],"eyes have identical pixel results for same source");
    int changed=0;for(size_t i=0;i<pixels.size();++i) if(pixels[i]!=output[0][i]) ++changed;
    std::printf("WARP debug changed=%d input=%u output=%u edgeIn=%u edgeOut=%u\n", changed,pixels[0],output[0][0],pixels[4*(W*16+15)],output[0][4*(W*16+15)]);
    Check(changed>W,"sharpen pass changed actual pixels");
    std::printf("PASS per-eye WARP NIS: %d changed byte components\n",changed);
    for(auto* r:readback) r->Release();view->Release();source->Release();
    NisShutdown(&nis);ctx->Release();dev->Release();
}

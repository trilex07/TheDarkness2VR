#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <vector>
#include <cstdio>
#include "../src/foveated_sharpen.h"
#include "../src/nis_runtime.h"
#include "../src/nis/NIS_Scaler_CS.h"
#include "../src/nis/NIS_Sharpen_CS.h"
#include "../src/gaze_nis_copy_base.inl"

int main() {
    ID3D11Device* dev=nullptr;ID3D11DeviceContext* ctx=nullptr;
    D3D_FEATURE_LEVEL level{};
    if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
             D3D11_SDK_VERSION,&dev,&level,&ctx)))return 1;
    constexpr UINT w=256,h=256;
    std::vector<unsigned char> pixels(w*h*4);
    for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x){
        const auto i=(y*w+x)*4;
        pixels[i]=x<128?0:255;
        pixels[i+1]=y<128?0:255;
        pixels[i+2]=((x/8+y/8)&1)?60:190;
        pixels[i+3]=255;
    }
    D3D11_TEXTURE2D_DESC td{};td.Width=w;td.Height=h;td.MipLevels=1;td.ArraySize=1;
    td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.SampleDesc.Count=1;
    td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=pixels.data();initial.SysMemPitch=w*4;
    ID3D11Texture2D* src=nullptr;ID3D11ShaderResourceView* srv=nullptr;
    if(FAILED(dev->CreateTexture2D(&td,&initial,&src)) ||
       FAILED(dev->CreateShaderResourceView(src,nullptr,&srv)))return 2;
    NisRuntime nis{};
    if(!NisInit(&nis,dev,kNisScalerCS,sizeof(kNisScalerCS),
                kNisSharpenCS,sizeof(kNisSharpenCS),nullptr,nullptr) ||
       !NisEnsureOutput(&nis,dev,w,h,w,h))return 3;
    const auto outer=d2vr_fovea::Around(128,128,w,h,.40f);
    const auto inner=d2vr_fovea::Around(128,128,w,h,.22f);
    const auto levels=d2vr_fovea::Grade(.35f,1.f);
    if(!NisCopyBaseAndSharpenGaze(&nis,ctx,src,srv,w,h,0,outer,inner,levels))return 4;
    td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* staging=nullptr;
    if(FAILED(dev->CreateTexture2D(&td,nullptr,&staging)))return 5;
    ctx->CopyResource(staging,nis.outTex[0]);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(ctx->Map(staging,0,D3D11_MAP_READ,0,&mapped)))return 6;
    unsigned outsideDiff=0,insideDiff=0;
    for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x){
        const auto* got=static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch+x*4;
        const auto* old=&pixels[(y*w+x)*4];
        const bool diff=memcmp(got,old,4)!=0;
        const bool inside=x>=outer.x && x<outer.x+outer.w && y>=outer.y && y<outer.y+outer.h;
        if(diff){if(inside)++insideDiff;else ++outsideDiff;}
    }
    ctx->Unmap(staging,0);
    staging->Release();srv->Release();src->Release();NisShutdown(&nis);
    ctx->Release();dev->Release();
    std::printf("outsideDiff=%u insideDiff=%u\n",outsideDiff,insideDiff);
    return outsideDiff==0 && insideDiff>0 ? 0 : 7;
}

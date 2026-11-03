// Persistent per-eye preprocessing scratch. Never writes the submitted source.
// NIS reads this image after FXAA; its output periphery still copies ORIGINAL.
#include "gaze_fxaa_cs.h"
struct GazeFxaaRuntime {
    ID3D11ComputeShader* shader=nullptr;
    ID3D11Buffer* constants=nullptr;
    ID3D11Texture2D* image[2]{};
    ID3D11ShaderResourceView* srv[2]{};
    ID3D11UnorderedAccessView* uav[2]{};
    UINT width=0,height=0;
};
static GazeFxaaRuntime g_gazeFxaa;
static void GazeFxaaShutdown() {
    auto& r=g_gazeFxaa;
    for(int i=0;i<2;++i) {
        if(r.srv[i])r.srv[i]->Release();
        if(r.uav[i])r.uav[i]->Release();
        if(r.image[i])r.image[i]->Release();
    }
    if(r.constants)r.constants->Release();
    if(r.shader)r.shader->Release();
    r=GazeFxaaRuntime{};
}
static bool GazeFxaaPrepare(ID3D11Device* dev,ID3D11DeviceContext* ctx,
    ID3D11Texture2D* original,ID3D11ShaderResourceView* source,int eye,
    const d2vr_fovea::Rect& inner,const d2vr_fovea::Rect& outer,float cx,float cy) {
    if(!dev || !ctx || !original || !source || eye<0 || eye>1 || inner.Empty() || outer.Empty())return false;
    D3D11_TEXTURE2D_DESC d{};original->GetDesc(&d);
    if(d.Format!=DXGI_FORMAT_B8G8R8A8_UNORM || d.SampleDesc.Count!=1 || d.MipLevels!=1 || d.ArraySize!=1)return false;
    auto& r=g_gazeFxaa;
    if(r.width!=d.Width || r.height!=d.Height)GazeFxaaShutdown();
    if(!r.shader) {
        if(FAILED(dev->CreateComputeShader(kGazeFxaaCS,sizeof(kGazeFxaaCS),nullptr,&r.shader)))return false;
        D3D11_BUFFER_DESC cb{};cb.ByteWidth=48;cb.Usage=D3D11_USAGE_DYNAMIC;
        cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;cb.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        if(FAILED(dev->CreateBuffer(&cb,nullptr,&r.constants)))return false;
        r.width=d.Width;r.height=d.Height;
    }
    if(!r.constants)return false;
    if(!r.image[eye]) {
        d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
        d.CPUAccessFlags=0;d.MiscFlags=0;
        if(FAILED(dev->CreateTexture2D(&d,nullptr,&r.image[eye])) ||
           FAILED(dev->CreateShaderResourceView(r.image[eye],nullptr,&r.srv[eye])) ||
           FAILED(dev->CreateUnorderedAccessView(r.image[eye],nullptr,&r.uav[eye])))return false;
    }
    if(!r.srv[eye] || !r.uav[eye])return false;
    struct Params { UINT w,h,x,y;float cx,cy,inner,outer;UINT ew,eh;float pad[2]; };
    static_assert(sizeof(Params)==48,"HLSL layout");
    Params p{d.Width,d.Height,outer.x,outer.y,
        cx,cy,
        std::min(inner.w,inner.h)*.5f,std::min(outer.w,outer.h)*.5f,outer.w,outer.h,{0,0}};
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(ctx->Map(r.constants,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return false;
    memcpy(mapped.pData,&p,sizeof(p));ctx->Unmap(r.constants,0);
    // Full copy provides the NIS neighborhood beyond the dispatch rectangle.
    // Resources are unbound after each dispatch, avoiding SRV/UAV/copy hazards.
    ctx->CopyResource(r.image[eye],original);
    ctx->CSSetShader(r.shader,nullptr,0);
    ctx->CSSetConstantBuffers(0,1,&r.constants);
    ctx->CSSetSamplers(0,1,&g_nisEye.sampler);
    ctx->CSSetShaderResources(0,1,&source);
    ctx->CSSetUnorderedAccessViews(0,1,&r.uav[eye],nullptr);
    ctx->Dispatch((outer.w+7)/8,(outer.h+7)/8,1);
    ID3D11ShaderResourceView* noSrv=nullptr;ID3D11UnorderedAccessView* noUav=nullptr;
    ctx->CSSetShaderResources(0,1,&noSrv);ctx->CSSetUnorderedAccessViews(0,1,&noUav,nullptr);
    ctx->CSSetShader(nullptr,nullptr,0);
    return true;
}

#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include "smaa_EdgeVS.h"
#include "smaa_WeightVS.h"
#include "smaa_BlendVS.h"
#include "smaa_EdgePS.h"
#include "smaa_WeightPS.h"
#include "smaa_BlendPS.h"
#include "../third_party/smaa/Textures/AreaTex.h"
#include "../third_party/smaa/Textures/SearchTex.h"

namespace d2vr_smaa {
using Microsoft::WRL::ComPtr;
// Owner-thread only, completed independent eye images. Commands are immutable
// for an input pair: record once, execute against CURRENT pixels every frame.
// ExecuteCommandList(TRUE) preserves the shared immediate-context state. No
// temporal feedback, CPU readback, image scaling, or D3D9 state is involved.
class Eyes {
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> deferred_;
    ComPtr<ID3D11VertexShader> vs_[3];
    ComPtr<ID3D11PixelShader> ps_[3];
    ComPtr<ID3D11SamplerState> samplers_[2];
    ComPtr<ID3D11RasterizerState> raster_;
    ComPtr<ID3D11ShaderResourceView> area_,search_;
    ComPtr<ID3D11Texture2D> sources_[2],output_[2],scratch_[2];
    ComPtr<ID3D11ShaderResourceView> sourceViews_[2],scratchViews_[2];
    ComPtr<ID3D11RenderTargetView> outputViews_[2],scratchTargets_[2];
    ComPtr<ID3D11Buffer> metrics_;
    ComPtr<ID3D11CommandList> commands_;
    bool failed_=false;
    uint32_t width_=0,height_=0;
    template<class T> bool Lookup(const unsigned char* data,UINT width,UINT height,
                                  UINT pitch,DXGI_FORMAT format,ComPtr<T>& view) {
        D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;
        desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=format;
        desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init{data,pitch,0};ComPtr<ID3D11Texture2D> texture;
        return SUCCEEDED(device_->CreateTexture2D(&desc,&init,&texture)) &&
               SUCCEEDED(device_->CreateShaderResourceView(texture.Get(),nullptr,&view));
    }
    bool Init(ID3D11Device* device) {
        device_=device;
        if(FAILED(device->CreateDeferredContext(0,&deferred_)))return false;
        const BYTE* vertex[]={kSmaaEdgeVS,kSmaaWeightVS,kSmaaBlendVS};
        const size_t vertexSize[]={sizeof(kSmaaEdgeVS),sizeof(kSmaaWeightVS),sizeof(kSmaaBlendVS)};
        const BYTE* pixel[]={kSmaaEdgePS,kSmaaWeightPS,kSmaaBlendPS};
        const size_t pixelSize[]={sizeof(kSmaaEdgePS),sizeof(kSmaaWeightPS),sizeof(kSmaaBlendPS)};
        for(int i=0;i<3;++i)if(FAILED(device->CreateVertexShader(vertex[i],vertexSize[i],nullptr,&vs_[i])) ||
            FAILED(device->CreatePixelShader(pixel[i],pixelSize[i],nullptr,&ps_[i])))return false;
        D3D11_SAMPLER_DESC sampler{};sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.MaxLOD=D3D11_FLOAT32_MAX;sampler.ComparisonFunc=D3D11_COMPARISON_NEVER;
        for(int i=0;i<2;++i){sampler.Filter=i?D3D11_FILTER_MIN_MAG_MIP_POINT:D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            if(FAILED(device->CreateSamplerState(&sampler,&samplers_[i])))return false;}
        D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;
        if(FAILED(device->CreateRasterizerState(&raster,&raster_)))return false;
        return Lookup(areaTexBytes,AREATEX_WIDTH,AREATEX_HEIGHT,AREATEX_PITCH,DXGI_FORMAT_R8G8_UNORM,area_) &&
               Lookup(searchTexBytes,SEARCHTEX_WIDTH,SEARCHTEX_HEIGHT,SEARCHTEX_PITCH,DXGI_FORMAT_R8_UNORM,search_);
    }
    void ReleasePair() {
        commands_.Reset();metrics_.Reset();
        for(int i=0;i<2;++i){sourceViews_[i].Reset();sources_[i].Reset();outputViews_[i].Reset();output_[i].Reset();
            scratchViews_[i].Reset();scratchTargets_[i].Reset();scratch_[i].Reset();}
        width_=height_=0;
    }
    bool Build(ID3D11Texture2D* left,ID3D11Texture2D* right,const D3D11_TEXTURE2D_DESC& input) {
        ReleasePair();
        sources_[0]=left;sources_[1]=right;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=input.Width;desc.Height=input.Height;
        desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
        desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        for(int i=0;i<2;++i) {
            if(FAILED(device_->CreateShaderResourceView(sources_[i].Get(),nullptr,&sourceViews_[i])))return false;
            desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
            if(FAILED(device_->CreateTexture2D(&desc,nullptr,&output_[i])) ||
               FAILED(device_->CreateRenderTargetView(output_[i].Get(),nullptr,&outputViews_[i])))return false;
            desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
            if(FAILED(device_->CreateTexture2D(&desc,nullptr,&scratch_[i])) ||
               FAILED(device_->CreateRenderTargetView(scratch_[i].Get(),nullptr,&scratchTargets_[i])) ||
               FAILED(device_->CreateShaderResourceView(scratch_[i].Get(),nullptr,&scratchViews_[i])))return false;
        }
        const float metrics[]={1.f/desc.Width,1.f/desc.Height,float(desc.Width),float(desc.Height)};
        D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(metrics);cb.Usage=D3D11_USAGE_IMMUTABLE;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        D3D11_SUBRESOURCE_DATA init{metrics,0,0};
        if(FAILED(device_->CreateBuffer(&cb,&init,&metrics_)))return false;
        auto* ctx=deferred_.Get();ctx->ClearState();
        ctx->IASetInputLayout(nullptr);ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->RSSetState(raster_.Get());D3D11_VIEWPORT vp{0,0,float(desc.Width),float(desc.Height),0,1};ctx->RSSetViewports(1,&vp);
        ID3D11Buffer* buffer=metrics_.Get();ctx->VSSetConstantBuffers(0,1,&buffer);ctx->PSSetConstantBuffers(0,1,&buffer);
        ID3D11SamplerState* samplers[]={samplers_[0].Get(),samplers_[1].Get()};ctx->PSSetSamplers(0,2,samplers);
        const float clear[4]{};ID3D11ShaderResourceView* nulls[3]{};
        for(int eye=0;eye<2;++eye)for(int stage=0;stage<3;++stage) {
            ctx->PSSetShaderResources(0,3,nulls);ctx->OMSetRenderTargets(0,nullptr,nullptr);
            auto* target=stage==2?outputViews_[eye].Get():scratchTargets_[stage].Get();
            if(stage<2)ctx->ClearRenderTargetView(target,clear); // discard in edge PS must leave zero edges
            ctx->OMSetRenderTargets(1,&target,nullptr);
            ctx->VSSetShader(vs_[stage].Get(),nullptr,0);ctx->PSSetShader(ps_[stage].Get(),nullptr,0);
            ID3D11ShaderResourceView* views[3]{};
            if(stage==0)views[0]=sourceViews_[eye].Get();
            else if(stage==1){views[0]=scratchViews_[0].Get();views[1]=area_.Get();views[2]=search_.Get();}
            else {views[0]=sourceViews_[eye].Get();views[1]=scratchViews_[1].Get();}
            ctx->PSSetShaderResources(0,3,views);ctx->Draw(3,0);
        }
        ctx->PSSetShaderResources(0,3,nulls);ctx->OMSetRenderTargets(0,nullptr,nullptr);
        if(FAILED(ctx->FinishCommandList(FALSE,&commands_)))return false;
        width_=desc.Width;height_=desc.Height;++builds;return true;
    }
public:
    uint64_t builds=0,executions=0;
    Eyes()=default;Eyes(const Eyes&)=delete;Eyes& operator=(const Eyes&)=delete;
    void Shutdown() {ReleasePair();deferred_.Reset();for(int i=0;i<3;++i){vs_[i].Reset();ps_[i].Reset();}
        for(auto& s:samplers_)s.Reset();raster_.Reset();area_.Reset();search_.Reset();device_.Reset();failed_=false;}
    bool Process(ID3D11Device* device,ID3D11DeviceContext* immediate,ID3D11Texture2D* left,ID3D11Texture2D* right,
                 ID3D11Texture2D* output[2]) {
        if(!output)return false;output[0]=output[1]=nullptr;
        if(failed_ || !device || !immediate || !left || !right ||
           immediate->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
        for(int i=0;i<2;++i)if(left==output_[i].Get() || right==output_[i].Get() ||
            left==scratch_[i].Get() || right==scratch_[i].Get())return false;
        ComPtr<ID3D11Device> owner,contextOwner;left->GetDevice(&owner);immediate->GetDevice(&contextOwner);
        if(owner.Get()!=device || contextOwner.Get()!=device)return false;
        right->GetDevice(&owner);if(owner.Get()!=device)return false;
        D3D11_TEXTURE2D_DESC a{},b{};left->GetDesc(&a);right->GetDesc(&b);
        if(!a.Width || !a.Height || a.Width>4096 || a.Height>4096 || a.Width!=b.Width || a.Height!=b.Height ||
           a.Format!=DXGI_FORMAT_B8G8R8A8_UNORM || b.Format!=a.Format || a.MipLevels!=1 || b.MipLevels!=1 ||
           a.ArraySize!=1 || b.ArraySize!=1 || a.SampleDesc.Count!=1 || b.SampleDesc.Count!=1 ||
           !(a.BindFlags&D3D11_BIND_SHADER_RESOURCE) || !(b.BindFlags&D3D11_BIND_SHADER_RESOURCE))return false;
        if(device_.Get() && device_.Get()!=device)Shutdown();
        if((!device_ && !Init(device)) ||
           ((!commands_ || sources_[0].Get()!=left || sources_[1].Get()!=right || width_!=a.Width || height_!=a.Height) &&
            !Build(left,right,a))) {Shutdown();failed_=true;return false;}
        immediate->ExecuteCommandList(commands_.Get(),TRUE);++executions;
        output[0]=output_[0].Get();output[1]=output_[1].Get();return true;
    }
};
}

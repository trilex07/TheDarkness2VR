#include <cstdio>
#include <cstdlib>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <string>
#include "../../src/smaa_eye.h"
using Microsoft::WRL::ComPtr;
static void Check(bool ok,const char* name){if(!ok){std::fprintf(stderr,"FAIL %s\n",name);std::exit(1);}}
static ComPtr<ID3D11Texture2D> Texture(ID3D11Device* device,UINT width,UINT height,const std::vector<uint32_t>& pixels) {
    D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
    D3D11_SUBRESOURCE_DATA init{pixels.data(),width*4,0};ComPtr<ID3D11Texture2D> result;
    Check(SUCCEEDED(device->CreateTexture2D(&d,&init,&result)),"input texture");return result;
}
static std::vector<uint32_t> Read(ID3D11Device* dev,ID3D11DeviceContext* ctx,ID3D11Texture2D* source) {
    D3D11_TEXTURE2D_DESC d{};source->GetDesc(&d);const UINT w=d.Width,h=d.Height;
    d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;
    ComPtr<ID3D11Texture2D> staging;Check(SUCCEEDED(dev->CreateTexture2D(&d,nullptr,&staging)),"staging");
    ctx->CopyResource(staging.Get(),source);D3D11_MAPPED_SUBRESOURCE m{};
    Check(SUCCEEDED(ctx->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)),"read fixture only");
    std::vector<uint32_t> out(w*h);for(UINT y=0;y<h;++y)std::memcpy(out.data()+y*w,(char*)m.pData+y*m.RowPitch,w*4);
    ctx->Unmap(staging.Get(),0);return out;
}
int main(int argc,char** argv) {
    ComPtr<ID3D11Device> dev;ComPtr<ID3D11DeviceContext> ctx;D3D_FEATURE_LEVEL level{};
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&dev,&level,&ctx)),"hardware D3D11");
    ComPtr<IDXGIDevice> dxgi;dev.As(&dxgi);ComPtr<IDXGIAdapter> adapter;dxgi->GetAdapter(&adapter);DXGI_ADAPTER_DESC ad{};adapter->GetDesc(&ad);
    std::wprintf(L"Hardware: %ls\n",ad.Description);
    if(argc==6 && std::string(argv[1])=="--image") {
        const UINT iw=std::strtoul(argv[2],nullptr,10),ih=std::strtoul(argv[3],nullptr,10);
        Check(iw && ih && iw<=4096 && ih<=4096,"offline image bounds");
        std::vector<uint32_t> pixels(size_t(iw)*ih);std::ifstream input(argv[4],std::ios::binary|std::ios::ate);
        Check(input && input.tellg()==std::streamoff(pixels.size()*4),"exact BGRA image length");input.seekg(0);
        Check(bool(input.read((char*)pixels.data(),pixels.size()*4)),"read BGRA fixture");
        auto texture=Texture(dev.Get(),iw,ih,pixels);d2vr_smaa::Eyes filter;ID3D11Texture2D* images[2]{};
        Check(filter.Process(dev.Get(),ctx.Get(),texture.Get(),texture.Get(),images),"offline image filter");
        const auto result=Read(dev.Get(),ctx.Get(),images[0]);
        std::ofstream output(argv[5],std::ios::binary);Check(bool(output.write((char*)result.data(),result.size()*4)),"write filtered fixture");
        return 0;
    }
    Check(argc==1,"usage: no args for tests, --image WIDTH HEIGHT INPUT.bgra OUTPUT.bgra for offline fixture");
    constexpr UINT w=128,h=128;std::vector<uint32_t> l(w*h,0xff1f5a8f),r(w*h,0xffc82d0b);
    auto left=Texture(dev.Get(),w,h,l),right=Texture(dev.Get(),w,h,r);
    d2vr_smaa::Eyes filter;ID3D11Texture2D* out[2]{};
    // A non-default immediate state must survive every cached-list execution.
    D3D11_VIEWPORT viewport{7,9,53,67,.2f,.8f};ctx->RSSetViewports(1,&viewport);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ComPtr<ID3D11RenderTargetView> oldTarget;Check(SUCCEEDED(dev->CreateRenderTargetView(left.Get(),nullptr,&oldTarget)),"old target");
    auto* target=oldTarget.Get();ctx->OMSetRenderTargets(1,&target,nullptr);
    Check(filter.Process(dev.Get(),ctx.Get(),left.Get(),right.Get(),out),"first pair");
    Check(Read(dev.Get(),ctx.Get(),out[0])==l && Read(dev.Get(),ctx.Get(),out[1])==r,"flat pixels exact and independent eyes");
    D3D11_VIEWPORT actual{};UINT count=1;ctx->RSGetViewports(&count,&actual);
    Check(count==1 && !std::memcmp(&actual,&viewport,sizeof(actual)),"viewport restored");
    D3D11_PRIMITIVE_TOPOLOGY topology;ctx->IAGetPrimitiveTopology(&topology);Check(topology==D3D11_PRIMITIVE_TOPOLOGY_LINELIST,"topology restored");
    ComPtr<ID3D11RenderTargetView> after;ctx->OMGetRenderTargets(1,&after,nullptr);Check(after.Get()==oldTarget.Get(),"target restored");
    ctx->OMSetRenderTargets(0,nullptr,nullptr);
    std::fill(l.begin(),l.end(),0xff6f9fa3);ctx->UpdateSubresource(left.Get(),0,nullptr,l.data(),w*4,0);
    Check(filter.Process(dev.Get(),ctx.Get(),left.Get(),right.Get(),out),"repeat pair");
    Check(Read(dev.Get(),ctx.Get(),out[0])==l && Read(dev.Get(),ctx.Get(),out[1])==r && filter.builds==1,"current pixels with cached commands, no prior-eye contamination");
    // Stair-step diagonal: flat interiors must stay exact, edge receives coverage.
    for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x)l[y*w+x]=x>y*.61f+13?0xffeeeeee:0xff121212;
    ctx->UpdateSubresource(left.Get(),0,nullptr,l.data(),w*4,0);
    Check(filter.Process(dev.Get(),ctx.Get(),left.Get(),right.Get(),out),"diagonal pair");
    const auto aa=Read(dev.Get(),ctx.Get(),out[0]);size_t changed=0;
    for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x)if(aa[y*w+x]!=l[y*w+x]){
        ++changed;Check(std::fabs(float(x)-(y*.61f+13))<3.1f,"changes confined to detected edge");}
    Check(changed>50 && Read(dev.Get(),ctx.Get(),out[1])==r,"diagonal coverage with other eye exact");
    Check(!filter.Process(dev.Get(),ctx.Get(),out[0],right.Get(),out) && !out[0] && !out[1],"reject feedback and clear outputs");
    Check(!filter.Process(dev.Get(),ctx.Get(),nullptr,right.Get(),out),"reject null input");
    auto replacement=Texture(dev.Get(),w,h,l);Check(filter.Process(dev.Get(),ctx.Get(),replacement.Get(),right.Get(),out) && filter.builds==2,"new identity rebuild");
    auto wrong=Texture(dev.Get(),64,64,std::vector<uint32_t>(4096,0xff888888));
    Check(!filter.Process(dev.Get(),ctx.Get(),wrong.Get(),right.Get(),out) && !out[0] && !out[1],"mixed-size pair rejected together");
    Check(filter.Process(dev.Get(),ctx.Get(),replacement.Get(),right.Get(),out),"valid pair recovers after rejection");
    filter.Shutdown();Check(filter.Process(dev.Get(),ctx.Get(),left.Get(),right.Get(),out),"device-reset lifecycle");
    std::printf("PASS hardware SMAA: %zu edge pixels, exact flats, independent eyes, live pixels, state restore, pair rejection/reset\n",changed);
    // Full-size dispatch timing on THIS GPU, not a game/HMD frame-time result.
    const UINT bw=3232,bh=3300;std::vector<uint32_t> bench(bw*bh);
    for(UINT y=0;y<bh;++y)for(UINT x=0;x<bw;++x)bench[y*bw+x]=((x/13+y/17)&1)?0xffeeeeee:0xff121212;
    auto bl=Texture(dev.Get(),bw,bh,bench),br=Texture(dev.Get(),bw,bh,bench);
    Check(filter.Process(dev.Get(),ctx.Get(),bl.Get(),br.Get(),out),"full-size pair");
    ComPtr<ID3D11Query> disjoint,start,end;D3D11_QUERY_DESC q{D3D11_QUERY_TIMESTAMP_DISJOINT,0};
    Check(SUCCEEDED(dev->CreateQuery(&q,&disjoint)),"timing disjoint");q.Query=D3D11_QUERY_TIMESTAMP;
    Check(SUCCEEDED(dev->CreateQuery(&q,&start)) && SUCCEEDED(dev->CreateQuery(&q,&end)),"timestamps");
    constexpr int frames=40;ctx->Begin(disjoint.Get());ctx->End(start.Get());
    const auto cpuStart=std::chrono::steady_clock::now();
    for(int i=0;i<frames;++i)Check(filter.Process(dev.Get(),ctx.Get(),bl.Get(),br.Get(),out),"cached benchmark pair");
    const double cpu=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-cpuStart).count()/frames;
    ctx->End(end.Get());ctx->End(disjoint.Get());ctx->Flush();D3D11_QUERY_DATA_TIMESTAMP_DISJOINT data{};
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    HRESULT hr;do{hr=ctx->GetData(disjoint.Get(),&data,sizeof(data),0);if(hr==S_FALSE)Sleep(1);}while(hr==S_FALSE && std::chrono::steady_clock::now()<deadline);
    UINT64 a=0,b=0;Check(hr==S_OK && !data.Disjoint && data.Frequency && ctx->GetData(start.Get(),&a,sizeof(a),0)==S_OK && ctx->GetData(end.Get(),&b,sizeof(b),0)==S_OK,"valid timing");
    std::printf("SMAA fixture 3232x3300 BOTH eyes: GPU mean %.3fms, CPU enqueue mean %.3fms; NOT game FPS\n",double(b-a)*1000/data.Frequency/frames,cpu);
}

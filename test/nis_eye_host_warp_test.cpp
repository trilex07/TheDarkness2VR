#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <vector>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static ID3D11Device* g_dev11=nullptr;
static ID3D11DeviceContext* g_ctx11=nullptr;
static int g_nisMode=2;
static float g_nisSharpness=.8f;
static UINT g_scW=0,g_scH=0;
static unsigned failures=0;
static void Log(const char* format,...) {
    char text[512];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);
    puts(text);if(strstr(text,"filter disabled"))++failures;
}
#include "../src/nis_eye.inl"
static ID3D11Texture2D* Source(UINT w,UINT h,unsigned color) {
    std::vector<unsigned> pixels(w*h,color);
    D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{};data.pSysMem=pixels.data();data.SysMemPitch=w*4;
    ID3D11Texture2D* source=nullptr;CHECK(SUCCEEDED(g_dev11->CreateTexture2D(&d,&data,&source)));return source;
}
static std::vector<unsigned> Read(ID3D11Texture2D* texture) {
    D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* staging=nullptr;CHECK(SUCCEEDED(g_dev11->CreateTexture2D(&d,nullptr,&staging)));
    g_ctx11->CopyResource(staging,texture);D3D11_MAPPED_SUBRESOURCE m{};
    CHECK(SUCCEEDED(g_ctx11->Map(staging,0,D3D11_MAP_READ,0,&m)));
    std::vector<unsigned> pixels(d.Width*d.Height);
    for(UINT y=0;y<d.Height;++y)memcpy(pixels.data()+y*d.Width,(char*)m.pData+y*m.RowPitch,d.Width*4);
    g_ctx11->Unmap(staging,0);staging->Release();return pixels;
}
static void Constant(ID3D11Texture2D* texture,unsigned expected) {
    for(unsigned pixel:Read(texture))for(unsigned shift=0;shift<24;shift+=8)
        CHECK(abs(int((pixel>>shift)&255)-int((expected>>shift)&255))<=2);
}
int main() {
    CHECK(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&g_dev11,nullptr,&g_ctx11)));
    auto left=Source(66,35,0xff406080),right=Source(66,35,0xffb09070);
    ID3D11Texture2D* output[2]{};CHECK(NisEyeProcess(left,right,output));
    CHECK(output[0]!=output[1]);Constant(output[0],0xff406080);Constant(output[1],0xffb09070);
    auto persistent=output[0];auto srv=g_nisEyeSource[0];
    for(unsigned frame=0;frame<10;++frame)CHECK(NisEyeProcess(left,right,output));
    CHECK(output[0]==persistent && g_nisEyeSource[0]==srv);
    CHECK(g_nisEye.numOutputCreates==1 && g_nisEye.numShaderInits==1);
    std::vector<unsigned> pattern(66*35);
    for(unsigned y=0;y<35;++y)for(unsigned x=0;x<66;++x) {
        const unsigned v=unsigned(128+60*std::sin(x*.7)*std::sin(y*.6));
        pattern[y*66+x]=0xff000000|v|(v<<8)|(v<<16);
    }
    g_ctx11->UpdateSubresource(left,0,nullptr,pattern.data(),66*4,0);
    g_nisSharpness=.2f;CHECK(NisEyeProcess(left,right,output));auto low=Read(output[0]);
    g_nisSharpness=.8f;CHECK(NisEyeProcess(left,right,output));auto high=Read(output[0]);
    unsigned changed=0;for(unsigned i=0;i<low.size();++i)changed+=low[i]!=high[i];CHECK(changed>100);
    Constant(output[1],0xffb09070); // changing strength/source in eye0 cannot contaminate eye1
    NisEyeShutdown();CHECK(!g_nisEyeSource[0] && !g_nisEyeSourceOwner[1]);
    D3D11_TEXTURE2D_DESC invalidDesc{};left->GetDesc(&invalidDesc);invalidDesc.BindFlags=0;
    ID3D11Texture2D* invalid=nullptr;CHECK(SUCCEEDED(g_dev11->CreateTexture2D(&invalidDesc,nullptr,&invalid)));
    CHECK(!NisEyeProcess(invalid,right,output) && !output[0] && !output[1]);
    CHECK(g_nisEyeFailed && failures==1);
    for(unsigned i=0;i<1000;++i)CHECK(!NisEyeProcess(left,right,output));
    CHECK(failures==1 && !output[0] && !output[1]); // fault does not retry or return stale output
    NisEyeShutdown();CHECK(NisEyeProcess(left,right,output));CHECK(!g_nisEyeFailed);
    NisEyeShutdown();invalid->Release();left->Release();right->Release();
    left=Source(2676,2728,0xff406080);right=Source(2676,2728,0xffb09070);
    CHECK(NisEyeProcess(left,right,output));Constant(output[0],0xff406080);Constant(output[1],0xffb09070);
    CHECK(g_nisEye.outW==2676 && g_nisEye.outH==2728);NisEyeShutdown();
    left->Release();right->Release();g_ctx11->ClearState();g_ctx11->Release();g_dev11->Release();
    printf("PASS actual NIS eye host WARP: independent eyes/full borders,20%% vs80%% changed %u pixels, persistent resources, failure latch1000 attempts, shutdown recovery, native2676x2728; not game/FPS evidence\n",changed);
}

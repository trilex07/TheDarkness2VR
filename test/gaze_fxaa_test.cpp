#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include "../src/foveated_sharpen.h"
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static ID3D11Device* g_dev11=nullptr;
static ID3D11DeviceContext* g_ctx11=nullptr;
static int g_nisMode=2;
static float g_nisSharpness=.8f,g_foveaPeriphery=0,g_foveaSharpness=1,g_foveaSize=.3f;
static UINT g_scW=0,g_scH=0;
static bool g_foveatedSharpen=true,g_foveatedCopyBase=true;
static bool g_gazeFxaaEnabled=true,g_foveaCenterFallback=true;
static bool tracked=false;
static float gazeX[2]{.25f,.75f},gazeY[2]{.3f,.7f};
static bool FoveaGazePixel(int eye,uint32_t w,uint32_t h,float& x,float& y) {
    x=gazeX[eye]*w;y=gazeY[eye]*h;return tracked;
}
static void Log(const char* format,...) {
    va_list args;va_start(args,format);vprintf(format,args);puts("");va_end(args);
}
#include "../src/nis_eye.inl"
static std::vector<unsigned> Read(ID3D11Texture2D* texture) {
    D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);
    d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* staging=nullptr;CHECK(SUCCEEDED(g_dev11->CreateTexture2D(&d,nullptr,&staging)));
    g_ctx11->CopyResource(staging,texture);D3D11_MAPPED_SUBRESOURCE m{};
    CHECK(SUCCEEDED(g_ctx11->Map(staging,0,D3D11_MAP_READ,0,&m)));
    std::vector<unsigned> pixels(d.Width*d.Height);
    for(UINT y=0;y<d.Height;++y)memcpy(pixels.data()+y*d.Width,(char*)m.pData+y*m.RowPitch,d.Width*4);
    g_ctx11->Unmap(staging,0);staging->Release();return pixels;
}
static ID3D11Texture2D* Source(UINT w,UINT h,const std::vector<unsigned>& pixels) {
    D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=pixels.data();initial.SysMemPitch=w*4;
    ID3D11Texture2D* texture=nullptr;CHECK(SUCCEEDED(g_dev11->CreateTexture2D(&d,&initial,&texture)));return texture;
}
static void CheckOutside(ID3D11Texture2D* image,const std::vector<unsigned>& original,UINT w,UINT h,const d2vr_fovea::Rect& r) {
    const auto got=Read(image);
    for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x)
        if(x<r.x || x>=r.x+r.w || y<r.y || y>=r.y+r.h)CHECK(got[y*w+x]==original[y*w+x]);
}
int main() {
    CHECK(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&g_dev11,nullptr,&g_ctx11)));
    for(const auto size:{std::pair<UINT,UINT>{258,259},{517,293},{2676,2728}}) {
        const UINT w=size.first,h=size.second;
        std::vector<unsigned> left(w*h),right(w*h,0xff805020);
        for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x)
            left[y*w+x]=0x7f000000|(((x+2*y)%19<9)?0xeeeeee:0x202020);
        auto l=Source(w,h,left),r=Source(w,h,right);
        ID3D11Texture2D* output[2]{};
        tracked=false;CHECK(NisEyeProcess(l,r,output));
        CHECK(g_gazeFxaa.image[0]!=g_gazeFxaa.image[1]);
        const auto center=d2vr_fovea::Around(w*.5f,h*.5f,w,h,g_foveaSize*1.7f);
        CheckOutside(output[0],left,w,h,center);
        CHECK(Read(l)==left && Read(r)==right);
        const auto scratch=Read(g_gazeFxaa.image[0]);
        unsigned smoothed=0;
        for(UINT i=0;i<scratch.size();++i) {
            CHECK((scratch[i]>>24)==0x7f); // FXAA never repurposes native alpha
            smoothed+=scratch[i]!=left[i];
        }
        CHECK(smoothed>100);
        const auto antialiased=Read(output[0]);
        g_gazeFxaaEnabled=false;CHECK(NisEyeProcess(l,r,output));
        CHECK(Read(output[0])!=antialiased); // proves actual NIS consumes FXAA
        g_gazeFxaaEnabled=true;
        auto persistent=g_gazeFxaa.image[0];
        tracked=true;
        for(unsigned frame=0;frame<4;++frame) {
            gazeX[0]=frame%2?.01f:.99f;gazeY[0]=frame%2?.99f:.01f;
            CHECK(NisEyeProcess(l,r,output));
            CheckOutside(output[0],left,w,h,d2vr_fovea::Around(gazeX[0]*w,gazeY[0]*h,w,h,g_foveaSize*1.7f));
            CHECK(g_gazeFxaa.image[0]==persistent);
            for(auto pixel:Read(output[1]))CHECK(pixel==0xff805020); // independent right eye stays flat
        }
        CHECK(Read(l)==left && Read(r)==right);
        NisEyeShutdown();CHECK(!g_gazeFxaa.shader && !g_gazeFxaa.image[0] && !g_nisEyeSource[1]);
        CHECK(NisEyeProcess(l,r,output)); // Reset/reinitialization
        NisEyeShutdown();l->Release();r->Release();
        printf("PASS FXAA->NIS %ux%u: changed scratch pixels=%u; center/tracked borders, original periphery, independent eyes, immutable sources, persistence/reset\n",w,h,smoothed);
    }
    g_ctx11->ClearState();g_ctx11->Release();g_dev11->Release();
    printf("PASS %u checks; WARP pixel evidence, NOT headset or temporal shimmer proof\n",checks);
}

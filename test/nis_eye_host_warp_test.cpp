#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <vector>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <dxgi.h>
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
static void RetireTiming() {
    g_ctx11->Flush(); // Standalone fixture has no Present/runtime submitting work.
    for(unsigned i=0;i<2000 && g_nisEye.timingPending;++i) {
        NisTimingPoll(&g_nisEye,g_ctx11);if(g_nisEye.timingPending)Sleep(1);
    }
    CHECK(!g_nisEye.timingPending);
}
static void HardwareCost(ID3D11Texture2D* left,ID3D11Texture2D* right) {
    RetireTiming();
    for(unsigned pattern=0;pattern<2;++pattern) {
        if(pattern) {
            std::vector<unsigned> pixels(2676*2728);unsigned random=12345;
            for(auto& pixel:pixels){random^=random<<13;random^=random>>17;random^=random<<5;pixel=0xff000000|(random&0xffffff);}
            g_ctx11->UpdateSubresource(left,0,nullptr,pixels.data(),2676*4,0);
            std::reverse(pixels.begin(),pixels.end());
            g_ctx11->UpdateSubresource(right,0,nullptr,pixels.data(),2676*4,0);
        }
        double samples[20]{},total=0;
        for(unsigned i=0;i<24;++i) {
            const auto before=g_nisEye.timingFrames;ID3D11Texture2D* output[2]{};
            CHECK(NisEyeProcess(left,right,output));RetireTiming();
            CHECK(g_nisEye.timingFrames==before+1);
            if(i>=4){samples[i-4]=g_nisEye.msGpu;total+=g_nisEye.msGpu;}
        }
        std::sort(samples,samples+20);
        printf("NIS_COST native2676x2728 pair strength=0.80 pattern=%s samples=20 meanMs=%.6f medianMs=%.6f maxMs=%.6f; dispatch bracket only, isolated GPU\n",
            pattern?"noise":"constant",total/20,(samples[9]+samples[10])/2,samples[19]);
    }
}
int main(int argc,char** argv) {
    const bool hardware=argc==2 && !strcmp(argv[1],"--hardware");
    CHECK(SUCCEEDED(D3D11CreateDevice(nullptr,hardware?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&g_dev11,nullptr,&g_ctx11)));
    IDXGIDevice* dxgi=nullptr;CHECK(SUCCEEDED(g_dev11->QueryInterface(__uuidof(IDXGIDevice),reinterpret_cast<void**>(&dxgi))));
    IDXGIAdapter* adapter=nullptr;CHECK(SUCCEEDED(dxgi->GetAdapter(&adapter)));
    DXGI_ADAPTER_DESC adapterDesc{};CHECK(SUCCEEDED(adapter->GetDesc(&adapterDesc)));
    printf("NIS fixture adapter=%ls hardware=%d\n",adapterDesc.Description,int(hardware));adapter->Release();dxgi->Release();
    auto left=Source(66,35,0xff406080),right=Source(66,35,0xffb09070);
    ID3D11Texture2D* output[2]{};CHECK(NisEyeProcess(left,right,output));
    CHECK(output[0]!=output[1]);Constant(output[0],0xff406080);Constant(output[1],0xffb09070);
    auto persistent=output[0];auto srv=g_nisEyeSource[0];
    for(unsigned frame=0;frame<10;++frame)CHECK(NisEyeProcess(left,right,output));
    CHECK(output[0]==persistent && g_nisEyeSource[0]==srv);
    CHECK(g_nisEye.numOutputCreates==1 && g_nisEye.numShaderInits==1);
    CHECK(g_nisEye.configUploaded && g_nisEye.numConfigUploads==1); // 22 dispatches, one actual upload
    std::vector<unsigned> pattern(66*35);
    for(unsigned y=0;y<35;++y)for(unsigned x=0;x<66;++x) {
        const unsigned v=unsigned(128+60*std::sin(x*.7)*std::sin(y*.6));
        pattern[y*66+x]=0xff000000|v|(v<<8)|(v<<16);
    }
    g_ctx11->UpdateSubresource(left,0,nullptr,pattern.data(),66*4,0);
    g_nisSharpness=.2f;CHECK(NisEyeProcess(left,right,output));auto low=Read(output[0]);
    g_nisSharpness=.8f;CHECK(NisEyeProcess(left,right,output));auto high=Read(output[0]);
    CHECK(g_nisEye.numConfigUploads==3); // each changed config uploads once for both eyes
    unsigned changed=0;for(unsigned i=0;i<low.size();++i)changed+=low[i]!=high[i];CHECK(changed>100);
    Constant(output[1],0xffb09070); // changing strength/source in eye0 cannot contaminate eye1
    for(unsigned mode=2;mode<=3;++mode) {
        g_nisMode=mode;g_scW=132;g_scH=70;
        for(unsigned sample=0;sample<8;++sample) {
            g_nisSharpness=sample*.15f; // includes clamped >1 strength
            CHECK(NisEyeProcess(left,right,output));
            auto cachedLeft=Read(output[0]),cachedRight=Read(output[1]);
            const auto uploads=g_nisEye.numConfigUploads;
            CHECK(NisEyeProcess(left,right,output) && uploads==g_nisEye.numConfigUploads);
            // Force a fresh real driver upload as the reference. Every output
            // pixel, including borders of non-tile-aligned eyes, must match.
            g_nisEye.configUploaded=false;
            CHECK(NisEyeProcess(left,right,output));
            CHECK(g_nisEye.numConfigUploads==uploads+1);
            CHECK(Read(output[0])==cachedLeft && Read(output[1])==cachedRight);
        }
    }
    g_nisMode=2;g_nisSharpness=.8f;CHECK(NisEyeProcess(left,right,output));
    // Use a real non-mappable replacement only inside this fixture to exercise
    // Map failure. Production never exposes/writes this privately owned buffer.
    auto originalBuffer=g_nisEye.cb;
    D3D11_BUFFER_DESC badDesc{};badDesc.ByteWidth=sizeof(NISConfig);
    badDesc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;badDesc.Usage=D3D11_USAGE_DEFAULT;
    ID3D11Buffer* badBuffer=nullptr;CHECK(SUCCEEDED(g_dev11->CreateBuffer(&badDesc,nullptr,&badBuffer)));
    g_nisEye.cb=badBuffer;g_nisEye.configUploaded=false;
    const auto uploadedBeforeFailure=g_nisEye.numConfigUploads;
    CHECK(!NisDispatchEye(&g_nisEye,g_ctx11,g_nisEyeSource[0],66,35,0,66,35,.8f,true));
    CHECK(!g_nisEye.configUploaded && g_nisEye.numConfigUploads==uploadedBeforeFailure);
    g_nisEye.cb=originalBuffer;badBuffer->Release();
    CHECK(NisEyeProcess(left,right,output) && g_nisEye.numConfigUploads==uploadedBeforeFailure+1);
    CHECK(Read(output[0])==high);
    NisEyeShutdown();CHECK(!g_nisEyeSource[0] && !g_nisEyeSourceOwner[1]);
    CHECK(!g_nisEye.configUploaded && !g_nisEye.numConfigUploads);
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
    CHECK(g_nisEye.outW==2676 && g_nisEye.outH==2728);
    CHECK(g_nisEye.timingPending && !g_nisEye.timingActive);
    NisTimingBegin(&g_nisEye,g_ctx11); // pending receipt cannot be overwritten
    CHECK(!g_nisEye.timingActive);
    NisTimingEnd(&g_nisEye,g_ctx11); // unmatched End is inert
    CHECK(g_nisEye.timingPending);
    g_ctx11->Flush(); // fixture submission only; runtime polling never flushes
    for(unsigned i=0;i<2000 && g_nisEye.timingPending;++i) {
        NisTimingPoll(&g_nisEye,g_ctx11);if(g_nisEye.timingPending)Sleep(1);
    }
    CHECK(!g_nisEye.timingPending && g_nisEye.timingFrames==1 && g_nisEye.msGpu>=0);
    NisTimingBegin(&g_nisEye,g_ctx11);CHECK(g_nisEye.timingActive);
    NisTimingBegin(&g_nisEye,g_ctx11);CHECK(g_nisEye.timingActive);
    NisTimingEnd(&g_nisEye,g_ctx11);CHECK(!g_nisEye.timingActive && g_nisEye.timingPending);
    NisTimingEnd(&g_nisEye,g_ctx11);CHECK(g_nisEye.timingPending);
    if(hardware)HardwareCost(left,right);
    NisEyeShutdown();CHECK(!g_nisEye.timingPending && !g_nisEye.timingActive && !g_nisEye.timingFrames);
    left->Release();right->Release();g_ctx11->ClearState();g_ctx11->Release();g_dev11->Release();
    printf("PASS actual NIS eye host %s: independent eyes/full borders,20%% vs80%% changed %u pixels, persistent resources,22 dispatches/one config upload,16 cached-vs-forced-upload pixel comparisons across sharpness/mode/resize,real Map failure/recovery, failure latch1000 attempts, shutdown recovery, native2676x2728; not game/FPS evidence\n",hardware?"HARDWARE":"WARP",changed);
}

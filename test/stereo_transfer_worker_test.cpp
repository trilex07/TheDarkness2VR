#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/stereo_transfer_worker.h"
#include <dxgi.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do {if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x); std::exit(1);}}while(0)
#define HR(x) do {HRESULT h=(x);if(FAILED(h)){std::printf("FAIL line %d hr=%08lx: %s\n",__LINE__,h,#x);std::exit(1);}}while(0)
using d2vr_transfer::Ref;
template<class T> Ref<T> Adopt(T* p) {Ref<T> owned(p);if(p)p->Release();return owned;}
template<class T> Ref<T> CreateShader(ID3D11Device* dev,const char* source,const char* target);
static Ref<ID3DBlob> Compile(const char* source,const char* target) {
    ID3DBlob *code=nullptr,*errors=nullptr;
    HRESULT hr=D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,"main",target,
        D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
    if(errors) {std::printf("%s\n",static_cast<const char*>(errors->GetBufferPointer()));errors->Release();}
    HR(hr);return Adopt(code);
}
static void Scenario(IDirect3DDevice9Ex* dev9,ID3D11Device* dev11,ID3D11DeviceContext* ctx) {
    IDirect3DSurface9* rtRaw=nullptr;
    HR(dev9->CreateRenderTarget(64,64,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&rtRaw,nullptr));
    auto rt=Adopt(rtRaw); HR(dev9->SetRenderTarget(0,rt.Get()));
    HANDLE sharedHandle=nullptr; IDirect3DTexture9* texRaw=nullptr;
    HR(dev9->CreateTexture(64,64,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texRaw,&sharedHandle));
    auto shared9=Adopt(texRaw); IDirect3DSurface9* surfRaw=nullptr;
    HR(shared9->GetSurfaceLevel(0,&surfRaw));auto surf=Adopt(surfRaw);
    IDirect3DQuery9* q9Raw=nullptr;HR(dev9->CreateQuery(D3DQUERYTYPE_EVENT,&q9Raw));auto q9=Adopt(q9Raw);
    ID3D11Texture2D* tex11Raw=nullptr;
    HR(dev11->OpenSharedResource(sharedHandle,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&tex11Raw)));
    auto shared11=Adopt(tex11Raw);
    ID3D11ShaderResourceView* srvRaw=nullptr;HR(dev11->CreateShaderResourceView(shared11.Get(),nullptr,&srvRaw));auto srv=Adopt(srvRaw);
    D3D11_TEXTURE2D_DESC td{};shared11->GetDesc(&td);
    td.MiscFlags=0;td.BindFlags=D3D11_BIND_RENDER_TARGET;td.Usage=D3D11_USAGE_DEFAULT;td.CPUAccessFlags=0;
    Ref<ID3D11Texture2D> eyes[2];Ref<ID3D11RenderTargetView> targets[2];
    for(int e=0;e<2;++e) {
        ID3D11Texture2D* eye=nullptr;HR(dev11->CreateTexture2D(&td,nullptr,&eye));eyes[e]=Adopt(eye);
        ID3D11RenderTargetView* rtv=nullptr;HR(dev11->CreateRenderTargetView(eye,nullptr,&rtv));targets[e]=Adopt(rtv);
    }
    td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* stageRaw=nullptr;HR(dev11->CreateTexture2D(&td,nullptr,&stageRaw));auto stage=Adopt(stageRaw);
    ID3D11Query* q11Raw=nullptr;D3D11_QUERY_DESC qd{D3D11_QUERY_EVENT,0};
    HR(dev11->CreateQuery(&qd,&q11Raw));auto q11=Adopt(q11Raw);
    D3D11_BUFFER_DESC bd{};bd.ByteWidth=112;bd.Usage=D3D11_USAGE_DYNAMIC;
    bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    ID3D11Buffer* bufferRaw=nullptr;HR(dev11->CreateBuffer(&bd,nullptr,&bufferRaw));auto buffer=Adopt(bufferRaw);
    const auto vcode=Compile("struct O{float4 p:SV_Position;float2 uv:TEXCOORD0;};O main(uint i:SV_VertexID){O o;o.uv=float2((i<<1)&2,i&2);o.p=float4(o.uv.x*2-1,1-o.uv.y*2,0,1);return o;}","vs_4_0");
    const auto pcode=Compile("Texture2D t:register(t0);SamplerState s:register(s0);cbuffer C:register(b0){float4 tint;}float4 main(float4 p:SV_Position,float2 uv:TEXCOORD0):SV_Target{return t.Sample(s,uv)*tint;}","ps_4_0");
    ID3D11VertexShader* vsRaw=nullptr;HR(dev11->CreateVertexShader(vcode->GetBufferPointer(),vcode->GetBufferSize(),nullptr,&vsRaw));auto vs=Adopt(vsRaw);
    ID3D11PixelShader* psRaw=nullptr;HR(dev11->CreatePixelShader(pcode->GetBufferPointer(),pcode->GetBufferSize(),nullptr,&psRaw));auto ps=Adopt(psRaw);
    D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
    ID3D11SamplerState* samplerRaw=nullptr;HR(dev11->CreateSamplerState(&sd,&samplerRaw));auto sampler=Adopt(samplerRaw);
    D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
    ID3D11RasterizerState* rasterRaw=nullptr;HR(dev11->CreateRasterizerState(&rd,&rasterRaw));auto raster=Adopt(rasterRaw);
    d2vr_transfer::Queue queue;bool fault=false;
    auto packet=[&](int eye) {
        auto j=std::make_shared<d2vr_transfer::Job>();
        j->producer=q9;j->reader=q11;j->context=Ref<ID3D11DeviceContext>(ctx);
        j->target=targets[eye];j->constants=buffer;j->vs=vs;j->ps=ps;
        j->sampler=sampler;j->raster=raster;j->source=srv;
        j->size=112;j->width=j->height=64;
        float tint[4]={1,1,1,1};memcpy(j->bytes.data(),tint,sizeof(tint));return j;
    };
    auto issue=[&] {
        HR(dev9->StretchRect(rt.Get(),nullptr,surf.Get(),nullptr,D3DTEXF_NONE));
        HR(q9->Issue(D3DISSUE_END));
        const HRESULT flushed=q9->GetData(nullptr,0,D3DGETDATA_FLUSH);
        CHECK(flushed==S_OK || flushed==S_FALSE);
    };
    auto verify=[&](int eye,DWORD expected) {
        ctx->CopyResource(stage.Get(),eyes[eye].Get());D3D11_MAPPED_SUBRESOURCE map{};
        HR(ctx->Map(stage.Get(),0,D3D11_MAP_READ,0,&map));
        const DWORD actual=*reinterpret_cast<const DWORD*>(static_cast<const unsigned char*>(map.pData)+32*map.RowPitch+32*4);
        ctx->Unmap(stage.Get(),0);
        if(actual!=expected) std::printf("eye=%d expected=%08lx actual=%08lx\n",eye,expected,actual);
        CHECK(actual==expected);
    };
    for(unsigned frame=0;frame<100;++frame) {
        const DWORD left=0xff000000|(frame+1)<<16|0x002233;
        const DWORD right=0xff000000|0x00bb0000|(frame+1)<<8|0x000055;
        HR(dev9->ColorFill(rt.Get(),nullptr,left));issue();
        auto l=packet(0);
        {
            d2vr_transfer::Pending pending;CHECK(pending.Start(queue,l,fault));
            // Model main-thread right replay while the worker consumes LEFT.
            for(int draw=0;draw<16;++draw) HR(dev9->ColorFill(rt.Get(),nullptr,right));
            if(frame%2) CHECK(pending.Finish()); // other half tests destructor join
        }
        CHECK(!fault && l->readerComplete && l->thread!=GetCurrentThreadId());
        l.reset();verify(0,left);
        issue();auto r=packet(1);
        {
            d2vr_transfer::Pending pending;CHECK(pending.Start(queue,r,fault));
            HR(dev9->ColorFill(rt.Get(),nullptr,left)); // restore live image in parallel
            CHECK(pending.Finish());
        }
        CHECK(!fault && r->readerComplete);r.reset();
        verify(1,right);verify(0,left);
    }
    // Invalid work must latch the overwrite fault, including on an early return.
    {
        d2vr_transfer::Pending pending;
        CHECK(pending.Start(queue,std::make_shared<d2vr_transfer::Job>(),fault));
    }
    CHECK(fault);
    d2vr_transfer::Pending refused;CHECK(!refused.Start(queue,packet(0),fault));
    queue.Stop();ctx->ClearState();ctx->Flush();
    IDirect3DSurface9* bb=nullptr;HR(dev9->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&bb));
    HR(dev9->SetRenderTarget(0,bb));bb->Release();
}
int main() {
    static_assert(sizeof(void*)==4,"x86 required");
    wchar_t path[MAX_PATH];CHECK(GetSystemDirectoryW(path,MAX_PATH));CHECK(!wcscat_s(path,L"\\d3d9.dll"));
    HMODULE module=LoadLibraryW(path);CHECK(module);
    const auto create=reinterpret_cast<HRESULT(WINAPI*)(UINT,IDirect3D9Ex**)>(GetProcAddress(module,"Direct3DCreate9Ex"));CHECK(create);
    IDirect3D9Ex* apiRaw=nullptr;HR(create(D3D_SDK_VERSION,&apiRaw));auto api=Adopt(apiRaw);
    HWND window=CreateWindowExW(0,L"STATIC",L"D2VR isolated interop worker test",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(window);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth=pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_UNKNOWN;pp.hDeviceWindow=window;
    IDirect3DDevice9Ex* d9Raw=nullptr;
    HR(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_PUREDEVICE|D3DCREATE_MULTITHREADED,&pp,nullptr,&d9Raw));auto d9=Adopt(d9Raw);
    D3DDEVICE_CREATION_PARAMETERS created{};HR(d9->GetCreationParameters(&created));
    CHECK(created.BehaviorFlags==0x54); // exact game baseline 0x50 plus worker synchronization
    IDXGIFactory1* factoryRaw=nullptr;HR(CreateDXGIFactory1(__uuidof(IDXGIFactory1),reinterpret_cast<void**>(&factoryRaw)));auto factory=Adopt(factoryRaw);
    LUID luid{};HR(api->GetAdapterLUID(0,&luid));Ref<IDXGIAdapter1> adapter;
    for(UINT i=0;;++i) {
        IDXGIAdapter1* candidate=nullptr;if(factory->EnumAdapters1(i,&candidate)!=S_OK) break;
        auto owned=Adopt(candidate);DXGI_ADAPTER_DESC1 desc{};HR(candidate->GetDesc1(&desc));
        if(desc.AdapterLuid.HighPart==luid.HighPart && desc.AdapterLuid.LowPart==luid.LowPart) {adapter=owned;break;}
    }
    CHECK(adapter.Get());ID3D11Device* d11Raw=nullptr;ID3D11DeviceContext* ctxRaw=nullptr;
    HR(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,0,D3D11_SDK_VERSION,&d11Raw,nullptr,&ctxRaw));auto d11=Adopt(d11Raw);auto ctx=Adopt(ctxRaw);
    Scenario(d9.Get(),d11.Get(),ctx.Get());
    HR(d9->ResetEx(&pp,nullptr)); // all worker/shared-resource references have been released
    Scenario(d9.Get(),d11.Get(),ctx.Get());
    ctx={};d11={};d9={};api={};DestroyWindow(window);FreeLibrary(module);
    std::puts("PASS actual x86 D3D9Ex/D3D11 worker: 400 eye transfers, pixel equality, concurrent D3D9 work, RAII join, failure latch, Reset/recreate");
    std::puts("No game/OpenXR/Steam calls; this does not prove frame-time improvement or headset acceptance.");
}

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/stereo_transfer_worker.h"
#include "../src/shared_reader_fence.h"
#include <dxgi.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do {if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x); std::exit(1);}}while(0)
#define HR(x) do {HRESULT h=(x);if(FAILED(h)){std::printf("FAIL line %d hr=%08lx: %s\n",__LINE__,h,#x);std::exit(1);}}while(0)
#include "actual_transfer_history.inl"
using d2vr_transfer::Ref;
struct ReaderOps {
    ID3D11DeviceContext* context;ID3D11Query* query;
    unsigned ends=0;
    bool Valid() const {return context && query;}
    const void* Context() const {return context;}
    const void* Query() const {return query;}
    uint32_t Thread() const {return GetCurrentThreadId();}
    void Flush(){context->Flush();}
    d2vr_shared_reader::Poll GetData(){
        const auto hr=context->GetData(query,nullptr,0,D3D11_ASYNC_GETDATA_DONOTFLUSH);
        return hr==S_OK?d2vr_shared_reader::Poll::Ready:
            hr==S_FALSE?d2vr_shared_reader::Poll::Pending:d2vr_shared_reader::Poll::Failed;
    }
    uint64_t Now() const {return GetTickCount64();}
    void PauseThread(){SwitchToThread();}
    void End(){++ends;context->End(query);}
};
template<class T> Ref<T> Adopt(T* p) {Ref<T> owned(p);if(p)p->Release();return owned;}
template<class T> Ref<T> CreateShader(ID3D11Device* dev,const char* source,const char* target);
static Ref<ID3DBlob> Compile(const char* source,const char* target) {
    ID3DBlob *code=nullptr,*errors=nullptr;
    HRESULT hr=D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,"main",target,
        D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
    if(errors) {std::printf("%s\n",static_cast<const char*>(errors->GetBufferPointer()));errors->Release();}
    HR(hr);return Adopt(code);
}
static void Scenario(IDirect3DDevice9Ex* dev9,ID3D11Device* dev11,ID3D11DeviceContext* ctx,
                     bool deferred=false,UINT width=64,UINT height=64,bool ownerProducer=false,bool split=false,
                     bool overlapOwner=false,unsigned frames=100,bool reuseHistory=false) {
    IDirect3DSurface9* rtRaw=nullptr;
    HR(dev9->CreateRenderTarget(width,height,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&rtRaw,nullptr));
    auto rt=Adopt(rtRaw); HR(dev9->SetRenderTarget(0,rt.Get()));
    struct Input {
        Ref<IDirect3DTexture9> tex9;Ref<IDirect3DSurface9> surface;
        Ref<IDirect3DQuery9> producer;Ref<ID3D11Texture2D> tex11;
        Ref<ID3D11ShaderResourceView> srv;Ref<ID3D11Query> reader;
        d2vr_shared_reader::Fence fence;
        std::shared_ptr<d2vr_transfer::Job> retained;
    } inputs[2];
    for(unsigned i=0;i<(split?2u:1u);++i) {
        auto& in=inputs[i];HANDLE handle=nullptr;IDirect3DTexture9* tex=nullptr;
        HR(dev9->CreateTexture(width,height,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&tex,&handle));in.tex9=Adopt(tex);
        IDirect3DSurface9* surface=nullptr;HR(tex->GetSurfaceLevel(0,&surface));in.surface=Adopt(surface);
        IDirect3DQuery9* producer=nullptr;HR(dev9->CreateQuery(D3DQUERYTYPE_EVENT,&producer));in.producer=Adopt(producer);
        ID3D11Texture2D* image=nullptr;HR(dev11->OpenSharedResource(handle,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&image)));in.tex11=Adopt(image);
        ID3D11ShaderResourceView* srv=nullptr;HR(dev11->CreateShaderResourceView(image,nullptr,&srv));in.srv=Adopt(srv);
        ID3D11Query* reader=nullptr;D3D11_QUERY_DESC desc{D3D11_QUERY_EVENT,0};HR(dev11->CreateQuery(&desc,&reader));in.reader=Adopt(reader);
    }
    D3D11_TEXTURE2D_DESC td{};inputs[0].tex11->GetDesc(&td);
    td.MiscFlags=0;td.BindFlags=D3D11_BIND_RENDER_TARGET;td.Usage=D3D11_USAGE_DEFAULT;td.CPUAccessFlags=0;
    Ref<ID3D11Texture2D> eyes[2];Ref<ID3D11RenderTargetView> targets[2];
    for(int e=0;e<2;++e) {
        ID3D11Texture2D* eye=nullptr;HR(dev11->CreateTexture2D(&td,nullptr,&eye));eyes[e]=Adopt(eye);
        ID3D11RenderTargetView* rtv=nullptr;HR(dev11->CreateRenderTargetView(eye,nullptr,&rtv));targets[e]=Adopt(rtv);
    }
    td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* stageRaw=nullptr;HR(dev11->CreateTexture2D(&td,nullptr,&stageRaw));auto stage=Adopt(stageRaw);
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
    if(overlapOwner) {
        CHECK(ownerProducer);
        D3DDEVICE_CREATION_PARAMETERS created{};
        HR(dev9->GetCreationParameters(&created));
        CHECK((created.BehaviorFlags & D3DCREATE_MULTITHREADED)==0);
    }
    d2vr_transfer::Queue queue;bool fault=false;
    auto inputFor=[&](const std::shared_ptr<d2vr_transfer::Job>& job)->Input& {
        for(auto& in:inputs)if(in.srv.Get()==job->source.Get())return in;
        CHECK(false);return inputs[0];
    };
    auto track=[&](const std::shared_ptr<d2vr_transfer::Job>& job) {
        if(!deferred)return;
        auto& in=inputFor(job);ReaderOps ops{ctx,in.reader.Get()};
        CHECK(!in.retained);in.retained=job;CHECK(in.fence.TrackIssuedRead(ops));
    };
    auto packet=[&](int eye) {
        auto j=std::make_shared<d2vr_transfer::Job>();
        auto& in=inputs[split?eye:0];
        if(!ownerProducer)j->producer=in.producer;
        j->producerComplete=ownerProducer;j->reader=in.reader;j->context=Ref<ID3D11DeviceContext>(ctx);
        j->target=targets[eye];j->constants=buffer;j->vs=vs;j->ps=ps;
        j->sampler=sampler;j->raster=raster;j->source=in.srv;
        j->size=112;j->width=width;j->height=height;j->deferReader=deferred;
        float tint[4]={1,1,1,1};memcpy(j->bytes.data(),tint,sizeof(tint));return j;
    };
    auto retire=[&](const std::shared_ptr<d2vr_transfer::Job>& job) {
        CHECK(job->readerSubmitted && job->result==S_OK);
        if(!deferred) {CHECK(job->readerComplete);return;}
        CHECK(!job->readerComplete);
        auto& in=inputFor(job);ReaderOps readerOps{ctx,in.reader.Get()};
        CHECK(in.retained==job && in.fence.pending);
        CHECK(in.fence.Wait(true,readerOps));CHECK(!in.fence.pending && !readerOps.ends);
        in.retained.reset();
    };
    auto issue=[&](int eye) {
        auto& in=inputs[split?eye:0];
        auto& q9=in.producer;
        CHECK(!in.retained && !in.fence.pending); // native overwrite cannot bypass retirement
        HR(dev9->StretchRect(rt.Get(),nullptr,in.surface.Get(),nullptr,D3DTEXF_NONE));
        HR(q9->Issue(D3DISSUE_END));
        HRESULT flushed=q9->GetData(nullptr,0,D3DGETDATA_FLUSH);
        CHECK(flushed==S_OK || flushed==S_FALSE);
        if(ownerProducer && !overlapOwner) {
            const auto start=GetTickCount64();
            while(flushed==S_FALSE) {
                CHECK(GetTickCount64()-start<2000);SwitchToThread();
                flushed=q9->GetData(nullptr,0,0);
            }
            CHECK(flushed==S_OK);
        }
    };
    auto waitOwner=[&](int eye) {
        if(!overlapOwner) return;
        auto& in=inputs[split?eye:0];
        // The source RT changes while the flushed copy is still in flight.
        // Sampling before S_OK would observe this later color.
        const DWORD distract=eye?0xff001122:0xff221100;
        for(int draw=0;draw<16;++draw) HR(dev9->ColorFill(rt.Get(),nullptr,distract));
        const auto start=GetTickCount64();
        HRESULT hr=S_FALSE;
        while((hr=in.producer->GetData(nullptr,0,0))==S_FALSE) {
            CHECK(GetTickCount64()-start<2000);
            SwitchToThread();
        }
        CHECK(hr==S_OK);
    };
    auto verify=[&](int eye,DWORD expected) {
        ctx->CopyResource(stage.Get(),eyes[eye].Get());D3D11_MAPPED_SUBRESOURCE map{};
        HR(ctx->Map(stage.Get(),0,D3D11_MAP_READ,0,&map));
        const DWORD actual=*reinterpret_cast<const DWORD*>(static_cast<const unsigned char*>(map.pData)+32*map.RowPitch+32*4);
        ctx->Unmap(stage.Get(),0);
        if(actual!=expected) std::printf("eye=%d expected=%08lx actual=%08lx\n",eye,expected,actual);
        CHECK(actual==expected);
    };
    if(overlapOwner) {
        // Dropping the lease before publish must retire the issued query.
        // The following frames reissue it and still sample the stretched color.
        HR(dev9->ColorFill(rt.Get(),nullptr,0xff112233));
        issue(0);
        auto abandoned=packet(0);
        abandoned->producerComplete=false;
        {
            d2vr_transfer::Pending pending;
            CHECK(pending.Stage(queue,abandoned,inputs[0].producer.Get(),fault));
            CHECK(pending.Staged() && abandoned->thread==0 && !abandoned->readerSubmitted);
        }
        CHECK(!fault && !inputs[0].fence.pending);
    }
    EyeColorHistory history;
    DWORD previousRight=0;
    CHECK(!reuseHistory || (split && deferred && overlapOwner));
    for(unsigned frame=0;frame<frames;++frame) {
        const DWORD left=0xff000000|(frame+1)<<16|0x002233;
        const DWORD right=0xff000000|0x00bb0000|(frame+1)<<8|0x000055;
        const bool uiOnly=reuseHistory && history.pixels && frame%3==1;
        const DWORD expectedRight=uiOnly?previousRight:right;
        HR(dev9->ColorFill(rt.Get(),nullptr,left));issue(0);
        auto l=packet(0);
        if(overlapOwner) CHECK(!l->producer.Get() && l->producerComplete);
        {
            d2vr_transfer::Pending pending;
            if(overlapOwner) {
                l->producerComplete=false;
                CHECK(pending.Stage(queue,l,inputs[0].producer.Get(),fault));
                CHECK(!pending.Start(queue,l,fault) && l->thread==0);
                waitOwner(0);CHECK(pending.Poll()==S_OK);
                CHECK(pending.Promote() && !pending.Staged());
                CHECK(!pending.Promote());
            } else CHECK(pending.Start(queue,l,fault));
            track(l);
            // Model main-thread right replay while the worker consumes LEFT.
            if(reuseHistory) CHECK(history.Restore(dev9,rt.Get())); // queued BEFORE slot-one overwrite
            if(uiOnly) {
                RECT ui={0,0,8,8};HR(dev9->ColorFill(rt.Get(),&ui,0xffabcdef));
            } else for(int draw=0;draw<16;++draw) HR(dev9->ColorFill(rt.Get(),nullptr,right));
            if(frame%2) CHECK(pending.Finish()); // other half tests destructor join
        }
        CHECK(!fault && l->thread!=GetCurrentThreadId());
        // Same-context eye copies may be queued before reader completion.
        // Map is a test readback gate; production must still fence shared reuse.
        if(!split) {
            if(frame%2) {verify(0,left);retire(l);}
            else {retire(l);verify(0,left);}
            l.reset();
        }
        // The second input may be written while the first reader receipt is
        // still pending. No wait/map/poll of the first input precedes this copy.
        if(split)CHECK(inputs[0].retained==l && inputs[0].fence.pending);
        // Alternate the exact-copy adoption with production fallback Save:
        // fallback must detach the old shared alias before it writes.
        if(reuseHistory && frame%5==4) {
            CHECK(history.Save(dev9,rt.Get()) && !history.adopted);
            CHECK(history.pixels!=inputs[1].surface.Get());
        }
        issue(1);auto r=packet(1);
        if(reuseHistory && frame%5!=4) {
            history.Adopt(rt.Get(),inputs[1].surface.Get());
            CHECK(history.adopted && history.pixels==inputs[1].surface.Get());
        }
        {
            d2vr_transfer::Pending pending;
            if(overlapOwner) {
                r->producerComplete=false;
                CHECK(pending.Stage(queue,r,inputs[split?1:0].producer.Get(),fault));
                waitOwner(1);CHECK(pending.Poll()==S_OK);
                CHECK(pending.Promote() && !pending.Staged());
            } else CHECK(pending.Start(queue,r,fault));
            track(r);
            if(reuseHistory) HR(dev9->StretchRect(inputs[0].surface.Get(),nullptr,rt.Get(),nullptr,D3DTEXF_NONE));
            else HR(dev9->ColorFill(rt.Get(),nullptr,left)); // restore live image in parallel
            CHECK(pending.Finish());
        }
        CHECK(!fault);verify(1,expectedRight);verify(0,left);retire(r);r.reset();
        previousRight=expectedRight;
        if(split){retire(l);l.reset();}
    }
    history.Release(); // COM history ownership drains before slots/Reset.
    CHECK(!history.target && !history.pixels);
    // A completed receipt must carry no D3D9 interface; an uncompleted one
    // must carry its exact query. Reject both contradictory packets.
    {
        auto invalid=packet(0);invalid->producerComplete=!ownerProducer;
        invalid->Run();CHECK(invalid->result==E_INVALIDARG && !invalid->readerSubmitted);
    }
    // Invalid work must latch the overwrite fault, including on an early return.
    {
        d2vr_transfer::Pending pending;
        CHECK(pending.Start(queue,std::make_shared<d2vr_transfer::Job>(),fault));
    }
    CHECK(fault);
    d2vr_transfer::Pending refused;CHECK(!refused.Start(queue,packet(0),fault));
    bool deferredFault=false;
    {
        auto invalid=std::make_shared<d2vr_transfer::Job>();invalid->deferReader=true;
        d2vr_transfer::Pending pending;CHECK(pending.Start(queue,invalid,deferredFault));
    }
    CHECK(deferredFault); // submitted flag alone must never turn failure into success
    queue.Stop();ctx->ClearState();ctx->Flush();
    IDirect3DSurface9* bb=nullptr;HR(dev9->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&bb));
    HR(dev9->SetRenderTarget(0,bb));bb->Release();
    printf("PASS interop scenario deferred=%d split=%d overlap=%d size=%ux%u: %u eye transfers, pixels and explicit reader retirement\n",
        int(deferred),int(split),int(overlapOwner),width,height,frames*2);
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
    Scenario(d9.Get(),d11.Get(),ctx.Get(),true);
    Scenario(d9.Get(),d11.Get(),ctx.Get(),true,2676,2728);
    HR(d9->ResetEx(&pp,nullptr));
    Scenario(d9.Get(),d11.Get(),ctx.Get());
    Scenario(d9.Get(),d11.Get(),ctx.Get(),true,2676,2728);
    d9={};d9Raw=nullptr;
    HR(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_PUREDEVICE,&pp,nullptr,&d9Raw));
    d9=Adopt(d9Raw);HR(d9->GetCreationParameters(&created));CHECK(created.BehaviorFlags==0x50);
    Scenario(d9.Get(),d11.Get(),ctx.Get(),false,64,64,true);
    Scenario(d9.Get(),d11.Get(),ctx.Get(),true,2676,2728,true);
    HR(d9->ResetEx(&pp,nullptr));
    Scenario(d9.Get(),d11.Get(),ctx.Get(),true,2676,2728,true);
    Scenario(d9.Get(),d11.Get(),ctx.Get(),true,2676,2728,true,true);
    Scenario(d9.Get(),d11.Get(),ctx.Get(),true,64,64,true,true);
    Scenario(d9.Get(),d11.Get(),ctx.Get(),false,64,64,true,false,true);
    Scenario(d9.Get(),d11.Get(),ctx.Get(),true,64,64,true,false,true);
    Scenario(d9.Get(),d11.Get(),ctx.Get(),true,2676,2728,true,false,true,10);
    Scenario(d9.Get(),d11.Get(),ctx.Get(),true,64,64,true,true,true);
    Scenario(d9.Get(),d11.Get(),ctx.Get(),true,2676,2728,true,true,true,10);
    HR(d9->ResetEx(&pp,nullptr));
    Scenario(d9.Get(),d11.Get(),ctx.Get(),true,64,64,true,false,true,10);
    Scenario(d9.Get(),d11.Get(),ctx.Get(),true,64,64,true,true,true,40,true);
    HR(d9->ResetEx(&pp,nullptr));
    Scenario(d9.Get(),d11.Get(),ctx.Get(),true,2676,2728,true,true,true,10,true);
    HR(d9->ResetEx(&pp,nullptr));
    puts("PASS combined actual-source adopted history + native 0x50 + D3D11 split readers: UI persistence, seed-before-overwrite, fallback detachment, live slot restore, resize/recreate and Reset");
    ctx={};d11={};d9={};api={};DestroyWindow(window);FreeLibrary(module);
    std::puts("PASS actual x86 D3D9Ex/D3D11 worker: 2760 eye transfers (1060 at 2676x2728), including single-owner overlap on native 0x50, two independent shared inputs, MT 0x54 workers, sampled pixel equality, concurrent D3D9 work, query retirement, RAII join, failure latch, Reset/recreate");
    std::puts("No game/OpenXR/Steam calls; this does not prove frame-time improvement or headset acceptance.");
}

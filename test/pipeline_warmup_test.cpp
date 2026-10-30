#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
static bool g_startupShaderPool=true;
static unsigned g_startupShaderCopies=2;
static void Log(const char* format,...) {va_list args;va_start(args,format);vprintf(format,args);va_end(args);puts("");}
#include "../src/shader_preload_pool.inl"
#include "../src/pipeline_warmup_d3d9.inl"
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define HR(x) do{HRESULT h=(x);if(FAILED(h)){printf("FAIL %d hr=%08lx: %s\n",__LINE__,h,#x);exit(1);}}while(0)
using namespace d2vr_pipeline_warmup;

static std::vector<uint32_t> Compile(const char* source,const char* target) {
    ID3DBlob *code=nullptr,*errors=nullptr;
    HRESULT hr=D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,"main",target,0,0,&code,&errors);
    if(errors){printf("%s\n",static_cast<char*>(errors->GetBufferPointer()));errors->Release();}HR(hr);
    std::vector<uint32_t> result(code->GetBufferSize()/4);memcpy(result.data(),code->GetBufferPointer(),code->GetBufferSize());code->Release();return result;
}
static Record Sample(const std::vector<uint32_t>& vs,const std::vector<uint32_t>& ps) {
    Record r;r.vs=vs;r.ps=ps;r.state=EmptyState();
    r.state.targets[0]=D3DFMT_A8R8G8B8;r.state.elementCount=1;
    r.state.elements[0]={0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0};
    return r;
}

struct FakeOps {
    std::vector<IUnknown*> vs,ps;
    Prepared prepared=Prepared::Ready;
    unsigned draws=0,failAt=~0u;
    uint64_t now=0,step=0;
    std::vector<std::pair<IUnknown*,IUnknown*>> pairs;
    uint64_t Now(){now+=step;return now;}
    std::vector<IUnknown*> Vertex(const std::vector<uint32_t>&){return vs;}
    std::vector<IUnknown*> Pixel(const std::vector<uint32_t>&){return ps;}
    Prepared Prepare(const State&){return prepared;}
    bool Draw(IUnknown* v,IUnknown* p){if(draws++==failAt)return false;pairs.push_back({v,p});return true;}
};

static void Pure(const std::vector<uint32_t>& vs,const std::vector<uint32_t>& ps) {
    Manifest manifest;
    const Record a=Sample(vs,ps);Record b=a;b.state.alphaTest=1;Record depthOnly=a;depthOnly.ps.clear();
    CHECK(manifest.Add(a) && !manifest.Add(a) && manifest.Add(b) && manifest.Add(depthOnly) && manifest.Size()==3);
    Record bad=a;bad.state.elementCount=0;CHECK(!manifest.Add(bad));
    bad=a;bad.state.elements[0].stream=Streams;CHECK(!manifest.Add(bad));
    bad=a;bad.state.frequency[1]=0xC0000001u;CHECK(!manifest.Add(bad));
    bad=a;bad.state.textures[2]={6,D3DFMT_A8R8G8B8,0,0};CHECK(!manifest.Add(bad));
    bad=a;bad.state.textures[2]={4,D3DFMT_A8R8G8B8,D3DUSAGE_RENDERTARGET,0};CHECK(!manifest.Add(bad));
    bad=a;bad.state.elements[5].offset=4;CHECK(!manifest.Add(bad)); // bytes after elementCount must be zero
    bad=a;bad.state.targets[0]=0;CHECK(!manifest.Add(bad));
    bad=a;std::swap(bad.vs,bad.ps);CHECK(!manifest.Add(bad));
    uint64_t version=0;auto data=manifest.Serialize(version);
    std::vector<Record> parsed;CHECK(version==3 && Parse(data.data(),data.size(),parsed) && parsed.size()==3);
    CHECK(parsed[0]==a && parsed[1]==b && parsed[2]==depthOnly);
    for(size_t cut:{size_t(1),size_t(3),data.size()-1}) CHECK(!Parse(data.data(),cut,parsed) && parsed.empty());
    auto trailing=data;trailing.push_back(0);CHECK(!Parse(trailing.data(),trailing.size(),parsed));
    auto wrong=data;wrong[2]=StateWords+1;CHECK(!Parse(wrong.data(),wrong.size(),parsed));
    wrong=data;wrong[0]^=1;CHECK(!Parse(wrong.data(),wrong.size(),parsed));
    wrong=data;wrong[4]=PixelVersion;CHECK(!Parse(wrong.data(),wrong.size(),parsed)); // VS slot holding PS bytecode
    CHECK(StreamStride(a.state,0)==12 && StreamStride(a.state,1)==0);

    IUnknown* v[3]={reinterpret_cast<IUnknown*>(1),reinterpret_cast<IUnknown*>(2),reinterpret_cast<IUnknown*>(3)};
    IUnknown* p[2]={reinterpret_cast<IUnknown*>(10),reinterpret_cast<IUnknown*>(11)};
    FakeOps ops;ops.vs={v[0],v[1],v[2]};ops.ps={p[0],p[1]};
    auto r=Warm({a},ops,1000);
    CHECK(r.warmed==1 && r.draws==3 && !r.failed && ops.pairs.size()==3);
    CHECK(ops.pairs[0].first==v[0] && ops.pairs[1].first==v[1] && ops.pairs[2].first==v[2]); // every VS copy
    CHECK(ops.pairs[0].second==p[0] && ops.pairs[1].second==p[1]);                            // every PS copy
    FakeOps nullPs;nullPs.vs={v[0]};r=Warm({depthOnly},nullPs,1000);
    CHECK(r.draws==1 && nullPs.pairs[0].second==nullptr);
    FakeOps missing;missing.vs={v[0]};r=Warm({a},missing,1000);
    CHECK(r.missingObjects==1 && r.draws==0 && !r.failed);
    FakeOps unsupported;unsupported.vs={v[0]};unsupported.ps={p[0]};unsupported.prepared=Prepared::Unsupported;
    r=Warm({a,b},unsupported,1000);CHECK(r.unsupported==2 && r.draws==0 && !r.failed);
    FakeOps lost=unsupported;lost.prepared=Prepared::DeviceFailure;r=Warm({a,b},lost,1000);
    CHECK(r.failed && r.draws==0 && r.warmed==0);
    FakeOps drawFailure;drawFailure.vs={v[0],v[1]};drawFailure.ps={p[0]};drawFailure.failAt=1;
    r=Warm({a,b},drawFailure,1000);CHECK(r.failed && r.draws==1 && r.warmed==0);
    FakeOps slow;slow.vs={v[0]};slow.ps={p[0]};slow.step=600;r=Warm({a,b,depthOnly},slow,1000);
    CHECK(r.timedOut && r.warmed==1);
    puts("PASS manifest validation/dedupe/round trip, every pooled copy, null PS, missing objects, unsupported, failure stop, budget");
}

struct DeviceSnapshot {
    IDirect3DSurface9* target=nullptr;IDirect3DSurface9* depth=nullptr;IDirect3DVertexShader9* vs=nullptr;IDirect3DPixelShader9* ps=nullptr;
    IDirect3DVertexDeclaration9* declaration=nullptr;IDirect3DBaseTexture9* texture=nullptr;IDirect3DIndexBuffer9* indices=nullptr;
    IDirect3DVertexBuffer9* stream=nullptr;UINT offset=0,stride=0,frequency[Streams]{};D3DVIEWPORT9 viewport{};
    DWORD states[5]{},srgbTexture=0,second=0;
    void Read(IDirect3DDevice9* d) {
        HR(d->GetRenderTarget(0,&target));d->GetDepthStencilSurface(&depth);HR(d->GetVertexShader(&vs));HR(d->GetPixelShader(&ps));
        HR(d->GetVertexDeclaration(&declaration));HR(d->GetTexture(0,&texture));HR(d->GetIndices(&indices));
        HR(d->GetStreamSource(0,&stream,&offset,&stride));HR(d->GetViewport(&viewport));
        for(UINT i=0;i<Streams;++i)HR(d->GetStreamSourceFreq(i,&frequency[i]));
        const D3DRENDERSTATETYPE ids[5]={D3DRS_SRGBWRITEENABLE,D3DRS_ALPHATESTENABLE,D3DRS_ALPHAFUNC,D3DRS_FOGENABLE,D3DRS_CLIPPLANEENABLE};
        for(int i=0;i<5;++i)HR(d->GetRenderState(ids[i],&states[i]));
        HR(d->GetSamplerState(0,D3DSAMP_SRGBTEXTURE,&srgbTexture));
        IDirect3DSurface9* rt1=nullptr;if(SUCCEEDED(d->GetRenderTarget(1,&rt1))&&rt1){second=1;rt1->Release();}
    }
    bool Same(const DeviceSnapshot& o) const {
        return target==o.target && depth==o.depth && vs==o.vs && ps==o.ps && declaration==o.declaration && texture==o.texture &&
            indices==o.indices && stream==o.stream && offset==o.offset && stride==o.stride && !memcmp(frequency,o.frequency,sizeof(frequency)) &&
            !memcmp(&viewport,&o.viewport,sizeof(viewport)) && !memcmp(states,o.states,sizeof(states)) && srgbTexture==o.srgbTexture && second==o.second;
    }
    void Release() {
        for(IUnknown* u:{static_cast<IUnknown*>(target),static_cast<IUnknown*>(depth),static_cast<IUnknown*>(vs),static_cast<IUnknown*>(ps),
            static_cast<IUnknown*>(declaration),static_cast<IUnknown*>(texture),static_cast<IUnknown*>(indices),static_cast<IUnknown*>(stream)}) if(u)u->Release();
        *this=DeviceSnapshot{};
    }
};
static std::vector<IUnknown*> Lookup(void* owner,const std::vector<uint32_t>& code) {
    auto* s=static_cast<d2vr_shader_preload::State*>(owner);return s->pool.PendingObjects(s->device,code);
}
static void Save(const std::wstring& path,const std::vector<uint32_t>& data) {
    HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);CHECK(f!=INVALID_HANDLE_VALUE);
    DWORD written=0;CHECK(WriteFile(f,data.data(),DWORD(data.size()*4),&written,nullptr));CHECK(written==data.size()*4);CloseHandle(f);
}

int wmain(int argc,wchar_t** argv) {
    static_assert(sizeof(void*)==4,"x86 required");
    const bool hardware=argc>1;
    const wchar_t* gameplayManifest=argc==3?argv[2]:nullptr;
    const auto vs=Compile("float4x4 m;struct O{float4 p:POSITION;float2 t:TEXCOORD0;};O main(float3 p:POSITION,float2 t:TEXCOORD0){O o;o.p=mul(float4(p,1),m);o.t=t;return o;}","vs_3_0");
    const auto ps=Compile("sampler2D s;float4 main(float2 t:TEXCOORD0):COLOR{return tex2D(s,t);}","ps_3_0");
    const auto otherPs=Compile("float4 main():COLOR{return float4(0,1,0,1);}","ps_3_0");
    Pure(vs,ps);

    wchar_t system[MAX_PATH];CHECK(GetSystemDirectoryW(system,MAX_PATH));CHECK(!wcscat_s(system,L"\\d3d9.dll"));
    HMODULE module=LoadLibraryW(system);CHECK(module);
    IDirect3D9* api=nullptr;IDirect3D9Ex* apiEx=nullptr;
    if(hardware) {
        auto create=reinterpret_cast<HRESULT(WINAPI*)(UINT,IDirect3D9Ex**)>(GetProcAddress(module,"Direct3DCreate9Ex"));CHECK(create);
        HR(create(D3D_SDK_VERSION,&apiEx));api=apiEx;
    } else {
        auto create=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(module,"Direct3DCreate9"));CHECK(create);
        api=create(D3D_SDK_VERSION);CHECK(api);
    }
    HWND window=CreateWindowExW(0,L"STATIC",L"D2VR pipeline warmup fixture",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(window);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.BackBufferWidth=pp.BackBufferHeight=64;
    pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24S8;
    IDirect3DDevice9* device=nullptr;IDirect3DDevice9Ex* deviceEx=nullptr;
    if(hardware) {HR(apiEx->CreateDeviceEx(0,D3DDEVTYPE_HAL,window,0x54,&pp,nullptr,&deviceEx));device=deviceEx;}
    else HR(api->CreateDevice(0,D3DDEVTYPE_REF,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device));

    if(gameplayManifest) {
        CHECK(hardware);
        const auto data=ReadManifestFile(gameplayManifest);
        std::vector<Record> gameplay;CHECK(Parse(data.data(),data.size(),gameplay));
        wchar_t temporary[MAX_PATH];CHECK(GetTempPathW(MAX_PATH,temporary));
        const auto dir=std::wstring(temporary)+L"d2vr-gameplay-"+std::to_wstring(GetCurrentProcessId());
        const auto seed=dir+L"-gameplay-seed.bin",cache=dir+L"-gameplay-cache.bin";
        d2vr_shader_preload::Pool<IUnknown> archive(device);
        for(const auto& record:gameplay) {
            CHECK(archive.Remember(device,record.vs));
            if(!record.ps.empty())CHECK(archive.Remember(device,record.ps));
        }
        uint64_t version=0;Save(seed,archive.Archive(version));
        auto pool=d2vr_shader_preload::PrepareState(device,cache,seed,2);
        const auto pending=pool->pool.Pending();
        auto result=WarmDevice(device,gameplay,pool.get(),&Lookup,&Lookup,8000);
        const auto& r=result.result;
        std::printf("GAMEPLAY records=%u warmed=%u draws=%u unsupported=%u missing=%u textureFailures=%u timedOut=%d failed=%d driverComplete=%d restored=%d\n",
            r.records,r.warmed,r.draws,r.unsupported,r.missingObjects,result.substitutedTextures,int(r.timedOut),int(r.failed),int(result.completed),int(result.restored));
        CHECK(result.began && result.completed && result.restored && !r.failed && !r.timedOut);
        CHECK(r.warmed==gameplay.size() && r.unsupported==0 && r.missingObjects==0);
        CHECK(pool->pool.Pending()==pending);pool.reset();
        HR(deviceEx->ResetEx(&pp,nullptr));
        CHECK(DeleteFileW(seed.c_str()));
        std::puts("PASS actual-source gameplay manifest: all records warmed with exact formats, borrowed shaders, cleanup before ResetEx; not an in-game FPS claim");
    }

    // Engine-like first draw: learn the effective pipeline from public device state.
    IDirect3DVertexShader9* gameVs=nullptr;IDirect3DPixelShader9* gamePs=nullptr;IDirect3DPixelShader9* gameOther=nullptr;
    HR(device->CreateVertexShader(reinterpret_cast<const DWORD*>(vs.data()),&gameVs));
    HR(device->CreatePixelShader(reinterpret_cast<const DWORD*>(ps.data()),&gamePs));
    HR(device->CreatePixelShader(reinterpret_cast<const DWORD*>(otherPs.data()),&gameOther));
    const D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
        {0,12,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
    IDirect3DVertexDeclaration9* declaration=nullptr;HR(device->CreateVertexDeclaration(elements,&declaration));
    IDirect3DTexture9* texture=nullptr;const D3DPOOL gamePool=hardware?D3DPOOL_DEFAULT:D3DPOOL_MANAGED; // Ex devices reject MANAGED
    HR(device->CreateTexture(4,4,1,0,D3DFMT_A8R8G8B8,gamePool,&texture,nullptr));
    IDirect3DVertexBuffer9* buffer=nullptr;HR(device->CreateVertexBuffer(3*20,0,0,gamePool,&buffer,nullptr));
    HR(device->SetVertexShader(gameVs));HR(device->SetPixelShader(gamePs));HR(device->SetVertexDeclaration(declaration));
    HR(device->SetTexture(0,texture));HR(device->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,TRUE));
    HR(device->SetRenderState(D3DRS_ALPHATESTENABLE,TRUE));HR(device->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATER));
    HR(device->SetStreamSource(0,buffer,0,20));
    Learner session(GetCurrentThreadId(),L"");
    d2vr_pipeline_warmup::learner.store(&session);
    HR(device->BeginScene());
    ObserveDraw(device,gameVs,gamePs);HR(device->DrawPrimitive(D3DPT_TRIANGLELIST,0,1));
    ObserveDraw(device,gameVs,gamePs);
    // A stale tracked identity still records the DEVICE binding; the duplicate is rejected.
    ObserveDraw(device,gameVs,gameOther);ObserveDraw(device,gameVs,gamePs);
    std::thread([&]{ObserveDraw(device,gameVs,nullptr);}).join(); // foreign thread never learns
    HR(device->EndScene());
    CHECK(session.learned==1 && session.manifest.Size()==1 && session.captureFailures==0 && session.seen.size()==2);
    // Background persistence through the production Tick, then whole-file admission.
    wchar_t cwd[MAX_PATH];CHECK(GetCurrentDirectoryW(MAX_PATH,cwd));
    const std::wstring prefix=std::wstring(cwd)+L"\\pipeline-"+std::to_wstring(GetCurrentProcessId());
    session.path=prefix+L"-manifest.bin";
    Tick(0);
    for(const auto deadline=GetTickCount64()+5000;session.writing.load() && GetTickCount64()<deadline;)Sleep(10);
    CHECK(!session.writing && session.saved==session.manifest.Version() && session.writeFailures==0);
    // Ownership follows the presenting thread; the previous thread stops learning.
    std::thread([&]{Tick(0);}).join();
    ObserveDraw(device,gameVs,nullptr);CHECK(session.seen.size()==2);
    Tick(0);ObserveDraw(device,gameVs,nullptr);CHECK(session.seen.size()==3 && session.learned==1);
    const auto onDisk=ReadManifestFile(session.path);
    d2vr_pipeline_warmup::learner.store(nullptr);
    uint64_t version=0;auto data=session.manifest.Serialize(version);
    CHECK(onDisk==data);
    std::vector<Record> records;CHECK(Parse(onDisk.data(),onDisk.size(),records) && records.size()==1);
    const Record& learned=records[0];
    CHECK(learned.vs==vs && learned.ps==ps && learned.state.elementCount==2 && learned.state.elements[1].offset==12);
    CHECK(learned.state.alphaTest==1 && learned.state.alphaFunc==D3DCMP_GREATER && learned.state.textures[0].type==D3DRTYPE_TEXTURE &&
          learned.state.textures[0].format==D3DFMT_A8R8G8B8 && learned.state.textures[0].srgb==1 && learned.state.depth==D3DFMT_D24S8);
    puts("PASS learned exact bytecode + effective declaration/targets/depth/render/sampler state from the first live draw only");

    // Startup pool owns two copies of each shader; warmup borrows them only.
    const auto cache=prefix+L"-learned.bin",seed=prefix+L"-seed.bin";
    d2vr_shader_preload::Pool<IUnknown> package(device);CHECK(package.Remember(device,vs));CHECK(package.Remember(device,ps));
    Save(seed,package.Archive(version));
    auto pool=d2vr_shader_preload::PrepareState(device,cache,seed,2);
    CHECK(pool->pool.Pending()==4);
    const auto vsBefore=pool->pool.PendingObjects(device,vs),psBefore=pool->pool.PendingObjects(device,ps);
    CHECK(vsBefore.size()==2 && psBefore.size()==2);

    // Render targets are not part of D3DSBT_ALL. Preserve a preexisting MRT.
    IDirect3DSurface9* secondTarget=nullptr;
    HR(device->CreateRenderTarget(64,64,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&secondTarget,nullptr));
    HR(device->SetRenderTarget(1,secondTarget));
    DeviceSnapshot before,after;before.Read(device);
    auto report=WarmDevice(device,records,pool.get(),&Lookup,&Lookup,5000);
    after.Read(device);
    CHECK(report.began && report.completed && report.restored && report.result.warmed==1 && report.result.draws==2 &&
          report.result.missingObjects==0 && report.substitutedTextures==0);
    CHECK(before.Same(after));after.Release();
    CHECK(pool->pool.PendingObjects(device,vs)==vsBefore && pool->pool.PendingObjects(device,ps)==psBefore && pool->pool.Pending()==4);

    Record missing=Sample(vs,otherPs);Record exotic=learned;exotic.state.targets[0]=D3DFMT_UYVY;
    report=WarmDevice(device,{missing,exotic},pool.get(),&Lookup,&Lookup,5000);
    after.Read(device);
    CHECK(report.restored && report.result.missingObjects==1 && report.result.unsupported==1 && report.result.draws==0 && !report.result.failed);
    CHECK(before.Same(after));after.Release();before.Release();

    if(hardware) {
        constexpr D3DFORMAT intz=static_cast<D3DFORMAT>(0x5a544e49);
        Com<IDirect3DSurface9> directDepth;
        const HRESULT direct=device->CreateDepthStencilSurface(16,16,intz,D3DMULTISAMPLE_NONE,0,TRUE,&directDepth.p,nullptr);
        Com<IDirect3DTexture9> depthTexture;
        const HRESULT textureResult=device->CreateTexture(16,16,1,D3DUSAGE_DEPTHSTENCIL,intz,D3DPOOL_DEFAULT,&depthTexture.p,nullptr);
        std::printf("INTZ directSurface=0x%08lX texture=0x%08lX\n",static_cast<unsigned long>(direct),static_cast<unsigned long>(textureResult));
        if(SUCCEEDED(textureResult)) {
            Record nativeDepth=learned;nativeDepth.state.depth=uint32_t(intz);
            DeviceSnapshot previous,current;previous.Read(device);
            auto native=WarmDevice(device,{nativeDepth},pool.get(),&Lookup,&Lookup,5000);
            CHECK(native.result.warmed==1 && native.result.unsupported==0 && native.completed && native.restored);
            current.Read(device);CHECK(previous.Same(current));current.Release();
            nativeDepth.state.textures[0]={D3DRTYPE_TEXTURE,uint32_t(intz),D3DUSAGE_DEPTHSTENCIL,0};
            native=WarmDevice(device,{nativeDepth},pool.get(),&Lookup,&Lookup,5000);
            CHECK(native.result.warmed==1 && native.result.unsupported==0 && native.completed && native.restored);
            current.Read(device);CHECK(previous.Same(current));current.Release();previous.Release();
            std::puts("PASS actual INTZ: texture-backed depth and sampler, exact format, state restoration; private depth owner released before ResetEx");
        } else std::puts("SKIP INTZ: hardware does not support native INTZ textures");
    }

    // Equal-format MRT slots still require distinct private surfaces.
    {
        DeviceWarm mrt(device);CHECK(mrt.Begin());
        auto state=learned.state;state.targets[1]=state.targets[0];
        CHECK(mrt.Prepare(state)==Prepared::Ready);
        Com<IDirect3DSurface9> rt0,rt1;
        HR(device->GetRenderTarget(0,&rt0.p));HR(device->GetRenderTarget(1,&rt1.p));
        CHECK(rt0.p!=rt1.p);
        CHECK(mrt.Draw(vsBefore[0],psBefore[0]));CHECK(mrt.Complete(5000));CHECK(mrt.Restore());
    }

    // Exception unwinding must restore bindings before releasing private resources.
    before.Read(device);
    try {
        DeviceWarm interrupted(device);CHECK(interrupted.Begin());
        CHECK(interrupted.Prepare(learned.state)==Prepared::Ready);
        throw 7;
    } catch(int) {}
    after.Read(device);CHECK(before.Same(after));after.Release();before.Release();
    HR(device->SetRenderTarget(1,nullptr));secondTarget->Release();

    // A non-Ex device cannot Reset while private DEFAULT-pool resources exist.
    HR(device->SetTexture(0,nullptr));HR(device->SetStreamSource(0,nullptr,0,0));
    HR(device->SetVertexShader(nullptr));HR(device->SetPixelShader(nullptr));HR(device->SetVertexDeclaration(nullptr));
    d2vr_shader_preload::state.store(pool.get());
    d2vr_shader_preload::BeginReset(device);
    const HRESULT reset=hardware?deviceEx->ResetEx(&pp,nullptr):device->Reset(&pp);
    d2vr_shader_preload::EndReset(device,reset);HR(reset);
    d2vr_shader_preload::Tick(0);
    // Engine creation still receives the warmed pool objects themselves.
    auto original=[](IDirect3DDevice9*,const DWORD*,IDirect3DVertexShader9**){return E_FAIL;};
    IDirect3DVertexShader9* served=nullptr;HR(d2vr_shader_preload::Create(device,reinterpret_cast<const DWORD*>(vs.data()),&served,original));
    CHECK(served==static_cast<IDirect3DVertexShader9*>(vsBefore[1]));served->Release();
    d2vr_shader_preload::state.store(nullptr);pool.reset();

    declaration->Release();texture->Release();buffer->Release();gameVs->Release();gamePs->Release();gameOther->Release();
    device->Release();api->Release();DestroyWindow(window);FreeLibrary(module);
    for(int i=0;i<500 && pool && pool->writing.load();++i)Sleep(10);
    DeleteFileW(cache.c_str());CHECK(DeleteFileW(seed.c_str()));CHECK(DeleteFileW(session.path.c_str()));
    printf("PASS actual x86 D3D9 %s: pooled objects warmed without creation or ownership change, D3DSBT_ALL state restored, missing/unsupported skipped, private resources released before Reset\n",
        hardware?"HAL/Ex flags=0x54":"REF");
    puts("No game/FPS/driver-compilation claim; runtime hitch reduction requires the in-game producer-flush trace.");
}

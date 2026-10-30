// Diagnostic-only recorder. No original function, resource, or HRESULT is changed.
namespace d2vr_command_history {
using namespace ::d2vr_command_history;
static Ring<32768> history;
static std::atomic<unsigned> status{0}, ownerTid{0}, foreignEpoch{0};
static std::atomic<bool> exportRequested{false};
static bool configured=false;
static IDirect3DDevice9* ownerDevice=nullptr; // writer only, never dereferenced by worker
static Pipeline pipeline;
static unsigned observedEpoch=0, observedGeneration=~0u, snapshots=0, snapshotFrame=~0u, drawId=0;
static bool dirty=true;
static PFN_SBApply originalApply=nullptr;
static uint64_t frequency=0, threshold=0, armQpc=0, snapshotTicks=0;
static HANDLE armEvent=nullptr, exportEvent=nullptr;
static const D3DRENDERSTATETYPE renderStates[16]={D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,
 D3DRS_ZFUNC,D3DRS_ALPHATESTENABLE,D3DRS_ALPHAFUNC,D3DRS_ALPHAREF,
 D3DRS_ALPHABLENDENABLE,D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_BLENDOP,
 D3DRS_CULLMODE,D3DRS_FOGENABLE,D3DRS_LIGHTING,D3DRS_COLORWRITEENABLE,
 D3DRS_SRGBWRITEENABLE,D3DRS_STENCILENABLE};
static const D3DSAMPLERSTATETYPE samplerStates[6]={D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,
 D3DSAMP_MIPFILTER,D3DSAMP_SRGBTEXTURE,D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV};
static uint64_t Clock(){LARGE_INTEGER t{};QueryPerformanceCounter(&t);return uint64_t(t.QuadPart);}
static bool Owns(IDirect3DDevice9* dev,bool claim=false) {
    if(!configured || status.load(std::memory_order_acquire)!=1)return false;
    const unsigned tid=GetCurrentThreadId();
    unsigned expected=0;
    if(claim && ownerTid.compare_exchange_strong(expected,tid))ownerDevice=dev;
    if(ownerTid.load()!=tid){if(ownerTid.load())foreignEpoch.fetch_add(1);return false;}
    if(ownerDevice!=dev){dirty=true;return false;}
    if(g_captureResetting){dirty=true;pipeline={};return false;}
    const unsigned epoch=foreignEpoch.load(),generation=unsigned(g_captureResetGeneration);
    if(epoch!=observedEpoch || generation!=observedGeneration){
        pipeline={};dirty=true;observedEpoch=epoch;observedGeneration=generation;
    }
    return true;
}
static bool Building(IDirect3DDevice9* dev){return d2vr_stateblock::Active(dev);}
static void Invalidate(){dirty=true;pipeline={};}
static void NoteStateRead(uint64_t begin) {
    const uint64_t end=Clock();snapshotTicks+=end-begin;
    if(end-begin<frequency/4000)return; // record diagnostic reads only above0.25ms
    Event event{};event.begin=begin;event.frame=unsigned(g_frameId);event.draw=drawId;
    event.tid=ownerTid.load();event.generation=unsigned(g_captureResetGeneration);
    event.device=uint32_t(uintptr_t(ownerDevice));event.eye=uint32_t(g_replayEye);
    event.operation=StateRead;event.pipeline=pipeline;
    history.End(history.Begin(event),end,0,threshold,true);
    if(history.frozen)status.store(2,std::memory_order_release);
}
static bool Snapshot(IDirect3DDevice9* dev) {
    const unsigned frame=unsigned(g_frameId);
    if(frame!=snapshotFrame){snapshotFrame=frame;snapshots=0;}
    if(++snapshots>4){pipeline={};return false;} // report unknown, never guess inherited bindings
    const uint64_t begin=Clock();
    pipeline={};
    IDirect3DVertexShader9* vs=nullptr;IDirect3DPixelShader9* ps=nullptr;
    IDirect3DVertexDeclaration9* declaration=nullptr;
    if(SUCCEEDED(dev->GetVertexShader(&vs))){pipeline.vs=uint32_t(uintptr_t(vs));pipeline.known|=1;}
    if(SUCCEEDED(dev->GetPixelShader(&ps))){pipeline.ps=uint32_t(uintptr_t(ps));pipeline.known|=2;}
    if(SUCCEEDED(dev->GetVertexDeclaration(&declaration))){pipeline.declaration=uint32_t(uintptr_t(declaration));pipeline.known|=4;}
    DWORD value=0;
    if(SUCCEEDED(dev->GetFVF(&value))){pipeline.fvf=value;pipeline.known|=8;}
    if(vs)vs->Release();if(ps)ps->Release();if(declaration)declaration->Release();
    for(unsigned i=0;i<4;++i){IDirect3DSurface9* target=nullptr;
        HRESULT hr=dev->GetRenderTarget(i,&target);
        if(SUCCEEDED(hr) || hr==D3DERR_NOTFOUND){pipeline.rt[i]=uint32_t(uintptr_t(target));pipeline.known|=16u<<i;}
        if(target)target->Release();}
    IDirect3DSurface9* depth=nullptr;HRESULT hr=dev->GetDepthStencilSurface(&depth);
    if(SUCCEEDED(hr) || hr==D3DERR_NOTFOUND){pipeline.depth=uint32_t(uintptr_t(depth));pipeline.known|=256;}
    if(depth)depth->Release();
    for(unsigned i=0;i<8;++i){IDirect3DBaseTexture9* texture=nullptr;
        if(SUCCEEDED(dev->GetTexture(i,&texture))){pipeline.textures[i]=uint32_t(uintptr_t(texture));pipeline.known|=512u<<i;}
        if(texture)texture->Release();}
    for(unsigned i=0;i<16;++i)if(SUCCEEDED(dev->GetRenderState(renderStates[i],&value))){pipeline.render[i]=value;pipeline.renderKnown|=1u<<i;}
    for(unsigned i=0;i<6;++i)if(SUCCEEDED(dev->GetSamplerState(0,samplerStates[i],&value))){pipeline.sampler[i]=value;pipeline.samplerKnown|=1u<<i;}
    dirty=false;NoteStateRead(begin);return true;
}
// The accepted configuration intentionally leaves StateBlockHook OFF. Query
// the actual two shader bindings on each draw in that case; never label a stale
// setter mirror "effective". Other mirrored fields are unknown unless freshly
// queried at this operation. Four full snapshots/frame bound extra driver reads.
static bool QueryShaders(IDirect3DDevice9* dev) {
    const uint64_t begin=Clock();
    IDirect3DVertexShader9* vs=nullptr;IDirect3DPixelShader9* ps=nullptr;
    const uint32_t oldVS=pipeline.vs,oldPS=pipeline.ps;
    pipeline.known=0;pipeline.renderKnown=0;pipeline.samplerKnown=0;
    if(SUCCEEDED(dev->GetVertexShader(&vs))){pipeline.vs=uint32_t(uintptr_t(vs));pipeline.known|=1;}
    if(SUCCEEDED(dev->GetPixelShader(&ps))){pipeline.ps=uint32_t(uintptr_t(ps));pipeline.known|=2;}
    if(vs)vs->Release();if(ps)ps->Release();
    NoteStateRead(begin);
    return pipeline.vs!=oldVS || pipeline.ps!=oldPS;
}
struct Scope {
    uint64_t ticket=0;unsigned epoch=0;
    Scope(IDirect3DDevice9* dev,Op operation,int eye,uint32_t caller=0,
          uint32_t a=0,uint32_t b=0,uint32_t c=0,uint32_t d=0,uint32_t e=0,uint32_t f=0) {
        if(!Owns(dev,operation>=Draw && operation<=DrawIndexedUP) || status.load(std::memory_order_acquire)!=1)return;
        if(exportRequested.load()){history.frozen=true;status.store(2,std::memory_order_release);return;}
        epoch=foreignEpoch.load();
        if(!originalApply){
            const bool changed=QueryShaders(dev);
            if(status.load()!=1)return;
            const uint32_t vs=pipeline.vs,ps=pipeline.ps,known=pipeline.known;
            if((dirty || changed || operation==CreateTexture || operation==ProducerFlush) && !Snapshot(dev)){
                pipeline.vs=vs;pipeline.ps=ps;pipeline.known=known;
            }
        }else if(dirty)Snapshot(dev);
        if(status.load()!=1)return;
        Event event{};event.begin=Clock();event.frame=unsigned(g_frameId);
        if(operation>=Draw && operation<=DrawIndexedUP)++drawId;
        event.draw=drawId;event.tid=ownerTid.load();event.generation=unsigned(g_captureResetGeneration);
        event.device=uint32_t(uintptr_t(dev));event.operation=operation;event.eye=uint32_t(eye);
        event.caller=caller;event.args[0]=a;event.args[1]=b;event.args[2]=c;
        event.args[3]=d;event.args[4]=e;event.args[5]=f;event.pipeline=pipeline;
        ticket=history.Begin(event);
    }
    HRESULT Finish(HRESULT result){
        if(ticket){history.End(ticket,Clock(),uint32_t(result),threshold,epoch!=foreignEpoch.load());
            if(history.frozen)status.store(2,std::memory_order_release);ticket=0;}
        return result;
    }
};
// Called only after the native bind succeeds. StateBlock construction changes
// the block, not the effective pipeline. The original call is never skipped.
static OrigFns originals;
static HRESULT STDMETHODCALLTYPE VS(IDirect3DDevice9* d,IDirect3DVertexShader9* s){
    HRESULT hr=originals.SetVertexShader(d,s);if(Owns(d)&&SUCCEEDED(hr)&&!Building(d)){pipeline.vs=uint32_t(uintptr_t(s));pipeline.known|=1;}return hr;}
static HRESULT STDMETHODCALLTYPE PS(IDirect3DDevice9* d,IDirect3DPixelShader9* s){
    HRESULT hr=originals.SetPixelShader(d,s);if(Owns(d)&&SUCCEEDED(hr)&&!Building(d)){pipeline.ps=uint32_t(uintptr_t(s));pipeline.known|=2;}return hr;}
static HRESULT STDMETHODCALLTYPE Declaration(IDirect3DDevice9* d,IDirect3DVertexDeclaration9* s){
    HRESULT hr=originals.SetVertexDeclaration(d,s);if(Owns(d)&&SUCCEEDED(hr)&&!Building(d)){pipeline.declaration=uint32_t(uintptr_t(s));pipeline.known|=4;pipeline.known&=~8u;}return hr;}
static HRESULT STDMETHODCALLTYPE FVF(IDirect3DDevice9* d,DWORD v){
    HRESULT hr=originals.SetFVF(d,v);if(Owns(d)&&SUCCEEDED(hr)&&!Building(d)){pipeline.fvf=v;pipeline.known|=8;pipeline.known&=~4u;}return hr;}
static HRESULT STDMETHODCALLTYPE RT(IDirect3DDevice9* d,DWORD i,IDirect3DSurface9* s){
    HRESULT hr=originals.SetRenderTarget(d,i,s);if(Owns(d)&&SUCCEEDED(hr)&&i<4){pipeline.rt[i]=uint32_t(uintptr_t(s));pipeline.known|=16u<<i;}return hr;}
static HRESULT STDMETHODCALLTYPE Depth(IDirect3DDevice9* d,IDirect3DSurface9* s){
    HRESULT hr=originals.SetDepthStencilSurface(d,s);if(Owns(d)&&SUCCEEDED(hr)){pipeline.depth=uint32_t(uintptr_t(s));pipeline.known|=256;}return hr;}
static HRESULT STDMETHODCALLTYPE Texture(IDirect3DDevice9* d,DWORD i,IDirect3DBaseTexture9* s){
    HRESULT hr=originals.SetTexture(d,i,s);if(Owns(d)&&SUCCEEDED(hr)&&!Building(d)&&i<8){pipeline.textures[i]=uint32_t(uintptr_t(s));pipeline.known|=512u<<i;}return hr;}
static HRESULT STDMETHODCALLTYPE Render(IDirect3DDevice9* d,D3DRENDERSTATETYPE s,DWORD v){
    HRESULT hr=originals.SetRenderState(d,s,v);if(Owns(d)&&SUCCEEDED(hr)&&!Building(d))for(unsigned i=0;i<16;++i)if(renderStates[i]==s){pipeline.render[i]=v;pipeline.renderKnown|=1u<<i;break;}return hr;}
static HRESULT STDMETHODCALLTYPE Sampler(IDirect3DDevice9* d,DWORD stage,D3DSAMPLERSTATETYPE s,DWORD v){
    HRESULT hr=originals.SetSamplerState(d,stage,s,v);if(Owns(d)&&SUCCEEDED(hr)&&!Building(d)&&stage==0)for(unsigned i=0;i<6;++i)if(samplerStates[i]==s){pipeline.sampler[i]=v;pipeline.samplerKnown|=1u<<i;break;}return hr;}
static HRESULT STDMETHODCALLTYPE Apply(IDirect3DStateBlock9* sb){
    HRESULT hr=originalApply(sb); // forwarding order and HRESULT preserved
    if(status.load()==1){
        if(ownerTid.load()==GetCurrentThreadId())Invalidate();
        else if(ownerTid.load())foreignEpoch.fetch_add(1);
    }
    return hr;
}
using CreateTextureFn=HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DTexture9**,HANDLE*);
static CreateTextureFn originalCreateTexture=nullptr;
static HRESULT STDMETHODCALLTYPE TextureCreate(IDirect3DDevice9* dev,UINT w,UINT h,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DTexture9** out,HANDLE* shared){
    Scope scope(dev,CreateTexture,g_replayEye,uint32_t(uintptr_t(_ReturnAddress())),w,h,levels,usage,format,pool);
    const HRESULT hr=originalCreateTexture(dev,w,h,levels,usage,format,pool,out,shared);
    if(scope.ticket && !history.frozen && SUCCEEDED(hr) && out)history.entries[(scope.ticket-1)%32768].resource=uint32_t(uintptr_t(*out));
    return scope.Finish(hr);
}
static HRESULT STDMETHODCALLTYPE VSCreate(IDirect3DDevice9* dev,const DWORD* code,IDirect3DVertexShader9** out){
    Scope scope(dev,CreateVS,g_replayEye,uint32_t(uintptr_t(_ReturnAddress())));
    HRESULT hr=originals.CreateVertexShader(dev,code,out);
    if(scope.ticket && !history.frozen && SUCCEEDED(hr) && out)history.entries[(scope.ticket-1)%32768].args[0]=uint32_t(uintptr_t(*out));
    return scope.Finish(hr);
}
static HRESULT STDMETHODCALLTYPE PSCreate(IDirect3DDevice9* dev,const DWORD* code,IDirect3DPixelShader9** out){
    Scope scope(dev,CreatePS,g_replayEye,uint32_t(uintptr_t(_ReturnAddress())));
    HRESULT hr=originals.CreatePixelShader(dev,code,out);
    if(scope.ticket && !history.frozen && SUCCEEDED(hr) && out)history.entries[(scope.ticket-1)%32768].args[0]=uint32_t(uintptr_t(*out));
    return scope.Finish(hr);
}
static HRESULT STDMETHODCALLTYPE Stretch(IDirect3DDevice9* dev,IDirect3DSurface9* a,const RECT* ar,IDirect3DSurface9* b,const RECT* br,D3DTEXTUREFILTERTYPE filter){
    Scope command(dev,Blit,g_replayEye,0,uint32_t(uintptr_t(a)),uint32_t(uintptr_t(b)),filter);
    return command.Finish(originals.StretchRect(dev,a,ar,b,br,filter));
}
static HRESULT STDMETHODCALLTYPE ClearCommand(IDirect3DDevice9* dev,DWORD count,const D3DRECT* rect,DWORD flags,D3DCOLOR color,float z,DWORD stencil){
    Scope command(dev,Clear,g_replayEye,0,flags,color,stencil,count);
    return command.Finish(originals.Clear(dev,count,rect,flags,color,z,stencil));
}
static HRESULT STDMETHODCALLTYPE SurfaceUpdate(IDirect3DDevice9* dev,IDirect3DSurface9* a,const RECT* r,IDirect3DSurface9* b,const POINT* p){
    Scope command(dev,UpdateSurface,g_replayEye,0,uint32_t(uintptr_t(a)),uint32_t(uintptr_t(b)));
    return command.Finish(originals.UpdateSurface(dev,a,r,b,p));
}
static HRESULT STDMETHODCALLTYPE TextureUpdate(IDirect3DDevice9* dev,IDirect3DBaseTexture9* a,IDirect3DBaseTexture9* b){
    Scope command(dev,UpdateTexture,g_replayEye,0,uint32_t(uintptr_t(a)),uint32_t(uintptr_t(b)));
    return command.Finish(originals.UpdateTexture(dev,a,b));
}
static HRESULT STDMETHODCALLTYPE Fill(IDirect3DDevice9* dev,IDirect3DSurface9* a,const RECT* r,D3DCOLOR color){
    Scope command(dev,ColorFill,g_replayEye,0,uint32_t(uintptr_t(a)),color);
    return command.Finish(originals.ColorFill(dev,a,r,color));
}
static DWORD WINAPI ExportWorker(void*) {
    // Control events never block the renderer. No disk writes until the external
    // collector has stopped ETW and explicitly signals export.
    WaitForSingleObject(armEvent,INFINITE);
    armQpc=Clock();status.store(1,std::memory_order_release);
    WaitForSingleObject(exportEvent,INFINITE);exportRequested.store(true);
    for(unsigned i=0;i<200 && status.load(std::memory_order_acquire)!=2;++i)Sleep(10);
    if(status.load()!=2){Log("[CommandHistory] export unavailable: no owner completion; buffer not read concurrently");return 0;}
    char root[MAX_PATH]{},path[MAX_PATH]{};
    GetEnvironmentVariableA("LOCALAPPDATA",root,MAX_PATH);
    sprintf_s(path,"%s\\Darkness2VR\\command-history-%lu.bin",root,GetCurrentProcessId());
    FILE* file=nullptr;fopen_s(&file,path,"wb");
    if(!file){Log("[CommandHistory] export failed path=%s",path);return 0;}
    // Header is twelve fixed-width uint64 values, then chronological Event records.
    uint64_t header[12]={0x3154534948323244ull,1,sizeof(Event),history.Count(),frequency,
        armQpc,history.trigger,history.written,history.overwritten,history.missingEnds,snapshotTicks,foreignEpoch.load()};
    bool ok=fwrite(header,sizeof(header),1,file)==1;
    for(size_t i=0;ok && i<history.Count();++i)ok=fwrite(&history.At(i),sizeof(Event),1,file)==1;
    ok=fclose(file)==0 && ok;
    Log("[CommandHistory] exported=%d path=%s events=%zu trigger=%llu snapshotTicks=%llu frequency=%llu",ok,path,history.Count(),history.trigger,snapshotTicks,frequency);
    status.store(3,std::memory_order_release);return 0;
}
static void Install(IDirect3DDevice9* dev) {
    if(configured || !GetPrivateProfileIntA("Diagnostics","CommandHistory",0,".\\Darkness2VR-diagnostic.ini"))return;
    LARGE_INTEGER f{};if(!QueryPerformanceFrequency(&f)||f.QuadPart<=0)return;
    char name[96];sprintf_s(name,"Local\\D2VRCommandHistoryArm-%lu",GetCurrentProcessId());
    armEvent=CreateEventA(nullptr,FALSE,FALSE,name);
    sprintf_s(name,"Local\\D2VRCommandHistoryExport-%lu",GetCurrentProcessId());
    exportEvent=CreateEventA(nullptr,FALSE,FALSE,name);
    if(!armEvent||!exportEvent)return;
    frequency=f.QuadPart;threshold=frequency*12/1000;
    if(!g_orig.SetVertexShader || !g_orig.SetPixelShader || !g_orig.SetRenderState ||
       !g_orig.SetTexture || !g_orig.SetDepthStencilSurface || !g_orig.SetRenderTarget ||
       !g_orig.SetFVF || !g_orig.SetVertexDeclaration || !g_orig.SetSamplerState ||
       !g_orig.CreateVertexShader || !g_orig.CreatePixelShader)return;
    originals=g_orig;
    originalCreateTexture=reinterpret_cast<CreateTextureFn>(PatchVTable(dev,23,reinterpret_cast<void*>(&TextureCreate)));
    if(!originalCreateTexture)return;
    configured=true;
    g_orig.SetVertexShader=VS;g_orig.SetPixelShader=PS;g_orig.SetVertexDeclaration=Declaration;
    g_orig.SetFVF=FVF;g_orig.SetRenderTarget=RT;g_orig.SetDepthStencilSurface=Depth;
    g_orig.SetTexture=Texture;g_orig.SetRenderState=Render;g_orig.SetSamplerState=Sampler;
    g_orig.CreateVertexShader=VSCreate;g_orig.CreatePixelShader=PSCreate;
    if(originals.StretchRect)g_orig.StretchRect=Stretch;
    if(originals.Clear)g_orig.Clear=ClearCommand;
    if(originals.UpdateSurface)g_orig.UpdateSurface=SurfaceUpdate;
    if(originals.UpdateTexture)g_orig.UpdateTexture=TextureUpdate;
    if(originals.ColorFill)g_orig.ColorFill=Fill;
    HANDLE worker=CreateThread(nullptr,0,ExportWorker,nullptr,0,nullptr);
    if(worker)CloseHandle(worker);
    else {
        g_orig=originals;PatchVTable(dev,23,reinterpret_cast<void*>(originalCreateTexture));
        configured=false;CloseHandle(armEvent);CloseHandle(exportEvent);
        armEvent=exportEvent=nullptr;
        Log("[CommandHistory] worker creation failed; original dispatch restored");
    }
    Log("[CommandHistory] configured=%d defaultOff=1 capacity=32768 eventBytes=%zu threshold=12ms; VR preserved, no renderer waits; collector must arm/export",configured,sizeof(Event));
}
} // namespace d2vr_command_history

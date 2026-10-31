// Requires <windows.h>, <d3d9.h> and Log(). No native EXE writes, no hooks.
#include "pipeline_warmup.h"
#include <atomic>
#include <memory>
#include <process.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace d2vr_pipeline_warmup {
template<class T> struct Com {
    T* p=nullptr;
    Com()=default;
    Com(const Com&)=delete;
    Com& operator=(const Com&)=delete;
    ~Com(){if(p)p->Release();}
};

template<class Shader> static bool ShaderCode(Shader* shader,uint32_t version,std::vector<uint32_t>& out) {
    UINT bytes=0;
    if(!shader || FAILED(shader->GetFunction(nullptr,&bytes)) || bytes<8 || (bytes&3) ||
       bytes>d2vr_startup_shader::MaxWords*4) return false;
    out.resize(bytes/4);
    if(FAILED(shader->GetFunction(out.data(),&bytes)) || bytes!=out.size()*4) return false;
    return ValidCode(out,version);
}

// Public device state only. Fixed-function, FVF and >4-stream pipelines are
// not recorded; any failed query rejects the record instead of guessing.
static bool Capture(IDirect3DDevice9* dev,Record& out) {
    out=Record{};out.state=EmptyState();
    State& s=out.state;
    Com<IDirect3DVertexShader9> vs;Com<IDirect3DPixelShader9> ps;
    if(FAILED(dev->GetVertexShader(&vs.p)) || !vs.p || !ShaderCode(vs.p,VertexVersion,out.vs)) return false;
    if(FAILED(dev->GetPixelShader(&ps.p))) return false;
    if(ps.p && !ShaderCode(ps.p,PixelVersion,out.ps)) return false;
    Com<IDirect3DVertexDeclaration9> declaration;
    if(FAILED(dev->GetVertexDeclaration(&declaration.p)) || !declaration.p) return false;
    D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH+1];UINT count=0;
    if(FAILED(declaration.p->GetDeclaration(nullptr,&count)) || count<2 || count>MaxElements+1 ||
       FAILED(declaration.p->GetDeclaration(elements,&count)) || count<2 || count>MaxElements+1 ||
       elements[count-1].Stream!=0xff) return false;
    s.elementCount=count-1;
    static_assert(sizeof(D3DVERTEXELEMENT9)==sizeof(Element),"element layout");
    std::memcpy(s.elements,elements,sizeof(Element)*s.elementCount);
    D3DCAPS9 caps{};
    if(FAILED(dev->GetDeviceCaps(&caps)) || !caps.NumSimultaneousRTs)return false;
    for(uint32_t i=0;i<std::min<uint32_t>(Targets,caps.NumSimultaneousRTs);++i) {
        Com<IDirect3DSurface9> target;D3DSURFACE_DESC desc{};
        const HRESULT hr=dev->GetRenderTarget(i,&target.p);
        if(FAILED(hr) && hr!=D3DERR_NOTFOUND)return false;
        if(SUCCEEDED(hr) && target.p) {
            if(FAILED(target.p->GetDesc(&desc))) return false;
            s.targets[i]=uint32_t(desc.Format);
        }
    }
    {
        Com<IDirect3DSurface9> depth;D3DSURFACE_DESC desc{};
        const HRESULT hr=dev->GetDepthStencilSurface(&depth.p);
        if(FAILED(hr) && hr!=D3DERR_NOTFOUND)return false;
        if(SUCCEEDED(hr) && depth.p) {
            if(FAILED(depth.p->GetDesc(&desc))) return false;
            s.depth=uint32_t(desc.Format);
        }
    }
    DWORD value=0;
    const struct {D3DRENDERSTATETYPE state;uint32_t* out;} states[]={
        {D3DRS_SRGBWRITEENABLE,&s.srgbWrite},{D3DRS_ALPHATESTENABLE,&s.alphaTest},{D3DRS_ALPHAFUNC,&s.alphaFunc},
        {D3DRS_FOGENABLE,&s.fog},{D3DRS_CLIPPLANEENABLE,&s.clipPlanes}};
    for(const auto& entry:states) {
        if(FAILED(dev->GetRenderState(entry.state,&value))) return false;
        *entry.out=entry.state==D3DRS_ALPHAFUNC||entry.state==D3DRS_CLIPPLANEENABLE?uint32_t(value):uint32_t(value!=0);
    }
    for(uint32_t i=0;i<Streams;++i) {
        UINT frequency=1;
        if(FAILED(dev->GetStreamSourceFreq(i,&frequency))) return false;
        s.frequency[i]=frequency;
    }
    for(uint32_t i=0;i<Samplers;++i) {
        Com<IDirect3DBaseTexture9> texture;
        if(FAILED(dev->GetTexture(i,&texture.p))) return false;
        if(!texture.p) continue;
        auto& t=s.textures[i];
        const auto type=texture.p->GetType();
        if(type==D3DRTYPE_TEXTURE || type==D3DRTYPE_CUBETEXTURE) {
            D3DSURFACE_DESC desc{};
            const HRESULT hr=type==D3DRTYPE_TEXTURE?static_cast<IDirect3DTexture9*>(texture.p)->GetLevelDesc(0,&desc):
                static_cast<IDirect3DCubeTexture9*>(texture.p)->GetLevelDesc(0,&desc);
            if(FAILED(hr)) return false;
            t.format=uint32_t(desc.Format);t.usage=desc.Usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL);
        } else if(type==D3DRTYPE_VOLUMETEXTURE) {
            D3DVOLUME_DESC desc{};
            if(FAILED(static_cast<IDirect3DVolumeTexture9*>(texture.p)->GetLevelDesc(0,&desc))) return false;
            t.format=uint32_t(desc.Format);
        } else return false;
        t.type=uint32_t(type);
        if(FAILED(dev->GetSamplerState(i,D3DSAMP_SRGBTEXTURE,&value))) return false;
        t.srgb=value!=0;
    }
    return ValidRecord(out);
}

// Executes on a device the engine does not own yet. Private 16x16 resources
// and one covering triangle. Recorded formats, declaration, alpha, fog, clip
// and samplers stay as captured. Cull, scissor and the depth test are forced
// only so the triangle is not discarded before the driver compiles; D3DSBT_ALL
// restores them. Restore() is mandatory.
class DeviceWarm {
    IDirect3DDevice9* dev_;
    struct Target {uint32_t format;bool depth;uint32_t slot;IDirect3DSurface9* surface;IDirect3DTexture9* depthTexture;};
    struct Bound {Texture key;IDirect3DBaseTexture9* texture;};
    struct Declaration {std::vector<Element> elements;IDirect3DVertexDeclaration9* object;};
    std::vector<Target> targets_;
    std::vector<Bound> textures_;
    std::vector<Declaration> declarations_;
    IDirect3DVertexBuffer9* vertices_=nullptr;
    IDirect3DIndexBuffer9* indices_=nullptr;
    IDirect3DStateBlock9* saved_=nullptr;
    IDirect3DSurface9* savedTargets_[Targets]{};
    uint32_t savedTargetCount_=0;
    bool savedStateReady_=false;
    IDirect3DSurface9* savedDepth_=nullptr;
    UINT savedFrequency_[Streams]{1,1,1,1};
    bool scene_=false;
    IDirect3DSurface9* Surface(uint32_t format,bool depth,uint32_t slot=0) {
        for(auto& t:targets_) if(t.format==format && t.depth==depth && t.slot==slot) return t.surface;
        IDirect3DSurface9* surface=nullptr;
        Com<IDirect3DTexture9> depthTexture;
        HRESULT hr;
        if(depth && format==0x5a544e49u) { // INTZ is a sampleable depth texture, not a standalone surface.
            hr=dev_->CreateTexture(16,16,1,D3DUSAGE_DEPTHSTENCIL,D3DFORMAT(format),D3DPOOL_DEFAULT,&depthTexture.p,nullptr);
            if(SUCCEEDED(hr) && depthTexture.p)hr=depthTexture.p->GetSurfaceLevel(0,&surface);
            else if(SUCCEEDED(hr))hr=E_FAIL;
        } else hr=depth?dev_->CreateDepthStencilSurface(16,16,D3DFORMAT(format),D3DMULTISAMPLE_NONE,0,TRUE,&surface,nullptr):
            dev_->CreateRenderTarget(16,16,D3DFORMAT(format),D3DMULTISAMPLE_NONE,0,FALSE,&surface,nullptr);
        if(FAILED(hr)) {if(surface)surface->Release();surface=nullptr;}
        Com<IDirect3DSurface9> owner;owner.p=surface;
        targets_.push_back({format,depth,slot,surface,depthTexture.p});
        owner.p=nullptr;depthTexture.p=nullptr;return surface;
    }
    IDirect3DBaseTexture9* TextureFor(const Texture& key) {
        for(auto& t:textures_) if(!std::memcmp(&t.key,&key,sizeof(key))) return t.texture;
        IDirect3DBaseTexture9* texture=nullptr;HRESULT hr=E_FAIL;
        if(key.type==D3DRTYPE_TEXTURE) {IDirect3DTexture9* t=nullptr;hr=dev_->CreateTexture(4,4,1,key.usage,D3DFORMAT(key.format),D3DPOOL_DEFAULT,&t,nullptr);texture=t;}
        else if(key.type==D3DRTYPE_CUBETEXTURE) {IDirect3DCubeTexture9* t=nullptr;hr=dev_->CreateCubeTexture(4,1,key.usage,D3DFORMAT(key.format),D3DPOOL_DEFAULT,&t,nullptr);texture=t;}
        else if(key.type==D3DRTYPE_VOLUMETEXTURE) {IDirect3DVolumeTexture9* t=nullptr;hr=dev_->CreateVolumeTexture(4,4,4,1,0,D3DFORMAT(key.format),D3DPOOL_DEFAULT,&t,nullptr);texture=t;}
        if(FAILED(hr)) texture=nullptr;
        Com<IDirect3DBaseTexture9> owner;owner.p=texture;
        textures_.push_back({key,texture});owner.p=nullptr;
        if(!texture) ++substitutedTextures;
        return texture;
    }
    IDirect3DVertexDeclaration9* DeclarationFor(const State& s) {
        std::vector<Element> elements(s.elements,s.elements+s.elementCount);
        for(auto& d:declarations_) if(d.elements.size()==elements.size() &&
            !std::memcmp(d.elements.data(),elements.data(),elements.size()*sizeof(Element))) return d.object;
        D3DVERTEXELEMENT9 native[MaxElements+1];
        std::memcpy(native,elements.data(),elements.size()*sizeof(Element));
        native[elements.size()]=D3DDECL_END();
        IDirect3DVertexDeclaration9* object=nullptr;
        if(FAILED(dev_->CreateVertexDeclaration(native,&object))) object=nullptr;
        Com<IDirect3DVertexDeclaration9> owner;owner.p=object;
        declarations_.push_back({std::move(elements),object});
        owner.p=nullptr;return object;
    }
    // DYNAMIC+DISCARD so a new layout does not wait on the previous draw's compile.
    bool WriteCoverage(const State& s) {
        void* mapped=nullptr;
        if(FAILED(vertices_->Lock(0,0,&mapped,D3DLOCK_DISCARD)) || !mapped) return false;
        auto* bytes=static_cast<uint8_t*>(mapped);
        std::memset(bytes,0,kVertexBytes);
        static const float clip[3][4]={
            {-1.f,-1.f,0.5f,1.f},{3.f,-1.f,0.5f,1.f},{-1.f,3.f,0.5f,1.f}};
        for(uint32_t stream=0;stream<Streams;++stream) {
            const uint32_t stride=StreamStride(s,stream);
            if(!stride) continue;
            uint8_t* base=bytes+stream*kStreamBytes;
            for(uint32_t v=0;v<3;++v) {
                uint8_t* vert=base+size_t(v)*stride;
                for(uint32_t i=0;i<s.elementCount;++i) {
                    const auto& e=s.elements[i];
                    if(e.stream!=stream) continue;
                    if((e.usage==D3DDECLUSAGE_POSITION || e.usage==D3DDECLUSAGE_POSITIONT) && e.type<=D3DDECLTYPE_FLOAT4) {
                        float tmp[4];
                        const uint32_t n=uint32_t(e.type)+1;
                        for(uint32_t c=0;c<n;++c) tmp[c]=clip[v][c];
                        std::memcpy(vert+e.offset,tmp,n*sizeof(float));
                    } else if(e.type==D3DDECLTYPE_D3DCOLOR) {
                        const uint32_t white=0xffffffffu;
                        std::memcpy(vert+e.offset,&white,sizeof(white));
                    }
                }
            }
        }
        return SUCCEEDED(vertices_->Unlock());
    }
public:
    static constexpr UINT kStreamBytes=3*0x1100;
    static constexpr UINT kVertexBytes=Streams*kStreamBytes;
    uint32_t substitutedTextures=0;
    explicit DeviceWarm(IDirect3DDevice9* dev):dev_(dev) {}
    DeviceWarm(const DeviceWarm&)=delete;
    DeviceWarm& operator=(const DeviceWarm&)=delete;
    ~DeviceWarm() {if(savedStateReady_)Restore();else Release();}
    uint64_t Now() const {return GetTickCount64();}
    // Captures the complete replaceable state first. Without a saved state
    // block nothing is mutated.
    bool Begin() {
        if(FAILED(dev_->CreateStateBlock(D3DSBT_ALL,&saved_)) || !saved_) {saved_=nullptr;return false;}
        D3DCAPS9 caps{};
        if(FAILED(dev_->GetDeviceCaps(&caps)) || !caps.NumSimultaneousRTs) return false;
        savedTargetCount_=std::min<uint32_t>(Targets,caps.NumSimultaneousRTs);
        for(uint32_t i=0;i<savedTargetCount_;++i) {
            const HRESULT hr=dev_->GetRenderTarget(i,&savedTargets_[i]);
            if(FAILED(hr) && hr!=D3DERR_NOTFOUND) return false;
            if(i==0 && !savedTargets_[i]) return false;
        }
        const HRESULT depthResult=dev_->GetDepthStencilSurface(&savedDepth_);
        if(FAILED(depthResult) && depthResult!=D3DERR_NOTFOUND) return false;
        for(uint32_t i=0;i<Streams;++i) if(FAILED(dev_->GetStreamSourceFreq(i,&savedFrequency_[i]))) return false;
        savedStateReady_=true;
        void* data=nullptr;
        if(FAILED(dev_->CreateVertexBuffer(kVertexBytes,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&vertices_,nullptr)) ||
           FAILED(vertices_->Lock(0,0,&data,D3DLOCK_DISCARD))) return false;
        if(!data)return false;
        std::memset(data,0,kVertexBytes);if(FAILED(vertices_->Unlock()))return false;
        if(FAILED(dev_->CreateIndexBuffer(3*sizeof(uint16_t),D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_DEFAULT,&indices_,nullptr)) ||
           FAILED(indices_->Lock(0,0,&data,0))) return false;
        if(!data)return false;
        auto* index=static_cast<uint16_t*>(data);
        index[0]=0;index[1]=1;index[2]=2;
        if(FAILED(indices_->Unlock()))return false;
        if(FAILED(dev_->BeginScene())) return false;
        scene_=true;
        // Identity in every register, after the state-block capture, so a shader
        // that transforms by any constant matrix still emits the clip positions
        // written below. Apply() restores the engine's constants.
        if(caps.MaxVertexShaderConst) {
            const UINT n=std::min<UINT>(256,caps.MaxVertexShaderConst);
            std::vector<float> constants(size_t(n)*4,0.f);
            for(UINT i=0;i<n;++i) constants[size_t(i)*4+(i&3)]=1.f;
            if(FAILED(dev_->SetVertexShaderConstantF(0,constants.data(),n)))
                Log("[PipelineWarmup] identity constants unavailable; coverage draw continues");
        }
        return true;
    }
    std::vector<IUnknown*> (*vertex)(void*,const std::vector<uint32_t>&)=nullptr;
    std::vector<IUnknown*> (*pixel)(void*,const std::vector<uint32_t>&)=nullptr;
    void* lookup=nullptr;
    std::vector<IUnknown*> Vertex(const std::vector<uint32_t>& code) {return vertex(lookup,code);}
    std::vector<IUnknown*> Pixel(const std::vector<uint32_t>& code) {return pixel(lookup,code);}
    Prepared Prepare(const State& s) {
        IDirect3DSurface9* color[Targets]{};
        for(uint32_t i=0;i<Targets;++i) if(s.targets[i] && !(color[i]=Surface(s.targets[i],false,i))) return Prepared::Unsupported;
        auto* declaration=DeclarationFor(s);
        if(!declaration) return Prepared::Unsupported;
        for(uint32_t i=savedTargetCount_;i<Targets;++i) if(color[i])return Prepared::Unsupported;
        if(FAILED(dev_->SetDepthStencilSurface(nullptr)))return Prepared::DeviceFailure;
        for(uint32_t i=0;i<savedTargetCount_;++i) if(FAILED(dev_->SetRenderTarget(i,color[i]))) return i?Prepared::Unsupported:Prepared::DeviceFailure;
        IDirect3DSurface9* depth=s.depth?Surface(s.depth,true):nullptr;
        if(s.depth && !depth)return Prepared::Unsupported;
        if(FAILED(dev_->SetDepthStencilSurface(depth))) return Prepared::DeviceFailure;
        const D3DVIEWPORT9 viewport={0,0,16,16,0.f,1.f};
        if(FAILED(dev_->SetViewport(&viewport)) ||
           FAILED(dev_->SetRenderState(D3DRS_SRGBWRITEENABLE,s.srgbWrite)) ||
           FAILED(dev_->SetRenderState(D3DRS_ALPHATESTENABLE,s.alphaTest)) ||
           FAILED(dev_->SetRenderState(D3DRS_ALPHAFUNC,s.alphaFunc)) ||
           FAILED(dev_->SetRenderState(D3DRS_FOGENABLE,s.fog)) ||
           FAILED(dev_->SetRenderState(D3DRS_CLIPPLANEENABLE,s.clipPlanes)) ||
           FAILED(dev_->SetVertexDeclaration(declaration)) ||
           FAILED(dev_->SetIndices(indices_))) return Prepared::DeviceFailure;
        for(uint32_t i=0;i<Samplers;++i) {
            const auto& t=s.textures[i];
            auto* texture=t.type?TextureFor(t):nullptr;
            if(t.type && !texture)return Prepared::Unsupported;
            if(FAILED(dev_->SetTexture(i,texture)) ||
               FAILED(dev_->SetSamplerState(i,D3DSAMP_SRGBTEXTURE,t.srgb))) return Prepared::DeviceFailure;
        }
        for(uint32_t i=0;i<Streams;++i) {
            const uint32_t stride=StreamStride(s,i);
            if(FAILED(dev_->SetStreamSource(i,stride?vertices_:nullptr,stride?i*kStreamBytes:0,stride))) return Prepared::DeviceFailure;
            if(FAILED(dev_->SetStreamSourceFreq(i,s.frequency[i]))) return Prepared::Unsupported;
        }
        if(FAILED(dev_->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE)) ||
           FAILED(dev_->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE)) ||
           FAILED(dev_->SetRenderState(D3DRS_ZENABLE,s.depth?D3DZB_TRUE:D3DZB_FALSE)) ||
           FAILED(dev_->SetRenderState(D3DRS_ZWRITEENABLE,FALSE)) ||
           FAILED(dev_->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL)) ||
           !WriteCoverage(s)) return Prepared::DeviceFailure;
        dev_->Clear(0,nullptr,D3DCLEAR_TARGET|(s.depth?D3DCLEAR_ZBUFFER:0),0,1.f,0);
        return Prepared::Ready;
    }
    bool Draw(IUnknown* vs,IUnknown* ps) {
        return SUCCEEDED(dev_->SetVertexShader(static_cast<IDirect3DVertexShader9*>(vs))) &&
               SUCCEEDED(dev_->SetPixelShader(static_cast<IDirect3DPixelShader9*>(ps))) &&
               SUCCEEDED(dev_->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,3,0,1));
    }
    // Waits for the queued driver work at startup, never during gameplay.
    bool Complete(uint64_t waitMs) {
        if(scene_) {const HRESULT hr=dev_->EndScene();scene_=false;if(FAILED(hr))return false;}
        Com<IDirect3DQuery9> query;
        if(FAILED(dev_->CreateQuery(D3DQUERYTYPE_EVENT,&query.p)) || !query.p || FAILED(query.p->Issue(D3DISSUE_END))) return false;
        const auto start=Now();
        for(;;) {
            const HRESULT hr=query.p->GetData(nullptr,0,D3DGETDATA_FLUSH);
            if(hr==S_OK) return true;
            if(hr!=S_FALSE || Now()-start>=waitMs) return false;
            Sleep(1);
        }
    }
    // Restores the engine-visible state, then frees every private resource.
    bool Restore() {
        bool ok=savedStateReady_ && saved_;
        if(scene_) {ok=SUCCEEDED(dev_->EndScene()) && ok;scene_=false;}
        if(savedStateReady_ && saved_) {
            for(uint32_t i=0;i<Streams;++i) ok=SUCCEEDED(dev_->SetStreamSourceFreq(i,savedFrequency_[i])) && ok;
            ok=SUCCEEDED(dev_->SetDepthStencilSurface(nullptr)) && ok;
            for(uint32_t i=1;i<savedTargetCount_;++i) ok=SUCCEEDED(dev_->SetRenderTarget(i,nullptr)) && ok;
            for(uint32_t i=0;i<savedTargetCount_;++i) ok=SUCCEEDED(dev_->SetRenderTarget(i,savedTargets_[i])) && ok;
            ok=SUCCEEDED(dev_->SetDepthStencilSurface(savedDepth_)) && ok;
            // Apply last: SetRenderTarget resets the viewport.
            ok=SUCCEEDED(saved_->Apply()) && ok;
        }
        Release();
        return ok;
    }
    void Release() {
        if(scene_) {dev_->EndScene();scene_=false;}
        for(auto& t:targets_) {if(t.surface)t.surface->Release();if(t.depthTexture)t.depthTexture->Release();}
        for(auto& t:textures_) if(t.texture) t.texture->Release();
        for(auto& d:declarations_) if(d.object) d.object->Release();
        targets_.clear();textures_.clear();declarations_.clear();
        if(vertices_) {vertices_->Release();vertices_=nullptr;}
        if(indices_) {indices_->Release();indices_=nullptr;}
        if(saved_) {saved_->Release();saved_=nullptr;}
        for(auto& target:savedTargets_)if(target){target->Release();target=nullptr;}
        savedStateReady_=false;savedTargetCount_=0;
        if(savedDepth_) {savedDepth_->Release();savedDepth_=nullptr;}
    }
};

struct WarmReport { Result result; bool began=false, completed=false, restored=false; uint32_t substitutedTextures=0; };
static WarmReport WarmDevice(IDirect3DDevice9* dev,const std::vector<Record>& records,void* lookup,
    std::vector<IUnknown*> (*vertex)(void*,const std::vector<uint32_t>&),
    std::vector<IUnknown*> (*pixel)(void*,const std::vector<uint32_t>&),uint64_t budgetMs) {
    WarmReport report;
    DeviceWarm warm(dev);warm.lookup=lookup;warm.vertex=vertex;warm.pixel=pixel;
    report.began=warm.Begin();
    if(report.began) {
        const auto start=warm.Now();
        report.result=Warm(records,warm,budgetMs);
        const auto used=warm.Now()-start;
        report.completed=!report.result.failed && warm.Complete(budgetMs>used?budgetMs-used:0);
    }
    report.substitutedTextures=warm.substitutedTextures;
    report.restored=warm.Restore();
    return report;
}

// Learning: first live draw of each pointer pair on the owning thread only.
struct Learner {
    std::atomic<DWORD> thread; // device-creation thread, then the Present thread
    std::wstring path;
    Manifest manifest;
    std::unordered_set<uint64_t> seen;
    std::unordered_map<uint64_t,uint8_t> captureAttempts;
    uintptr_t lastVs=~uintptr_t(0), lastPs=~uintptr_t(0);
    std::atomic<uint64_t> saved{0}, learned{0}, captureFailures{0}, writeFailures{0};
    std::atomic<bool> saturated{false};
    std::atomic<bool> writing{false};
    ULONGLONG nextTick=0;
    Learner(DWORD owner,std::wstring file):thread(owner),path(std::move(file)) {}
};
static std::atomic<Learner*> learner{nullptr}; // process lifetime
constexpr size_t MaxSeenPairs=65536;

inline void NoteCaptureFailure(Learner* l,uint64_t key) {
    ++l->captureFailures;
    l->lastVs=l->lastPs=~uintptr_t(0); // the next draw of this pair must retry
    auto& attempts=l->captureAttempts[key];
    if(++attempts>=2 || l->captureAttempts.size()>MaxSeenPairs) {
        l->seen.insert(key);
        l->captureAttempts.erase(key);
    }
}
// The seen key is inserted only after a usable snapshot. One failed capture
// stays eligible; a second failure, or a full manifest, is permanent.
inline void ObserveDraw(IDirect3DDevice9* dev,const void* vs,const void* ps) {
    Learner* l=learner.load(std::memory_order_acquire);
    if(!l || GetCurrentThreadId()!=l->thread.load(std::memory_order_relaxed)) return;
    if(l->saturated.load(std::memory_order_relaxed)) return;
    const auto v=reinterpret_cast<uintptr_t>(vs),p=reinterpret_cast<uintptr_t>(ps);
    if(v==l->lastVs && p==l->lastPs) return;
    l->lastVs=v;l->lastPs=p;
    if(!v || l->seen.size()>=MaxSeenPairs) return;
    const uint64_t key=(uint64_t(v)<<32)|uint32_t(p);
    if(l->seen.find(key)!=l->seen.end()) return;
    try {
        Record record;
        if(!Capture(dev,record)) {NoteCaptureFailure(l,key);return;}
        l->captureAttempts.erase(key);
        const auto admit=l->manifest.Admit(record);
        if(admit==Manifest::Admit::Added) ++l->learned;
        l->seen.insert(key);
        if(admit==Manifest::Admit::Rejected || l->manifest.Size()>=MaxRecords)
            l->saturated.store(true,std::memory_order_relaxed);
    } catch(...) {try{NoteCaptureFailure(l,key);}catch(...){++l->captureFailures;}}
}

static std::vector<uint32_t> ReadManifestFile(const std::wstring& path) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER size{};std::vector<uint32_t> data;
    if(GetFileSizeEx(file,&size) && size.QuadPart>=12 && size.QuadPart<=LONGLONG(MaxBytes) && !(size.QuadPart%4)) {
        try {data.resize(size_t(size.QuadPart)/4);} catch(...) {data.clear();}
        DWORD read=0;
        if(!data.empty() && (!ReadFile(file,data.data(),DWORD(size.QuadPart),&read,nullptr) || read!=DWORD(size.QuadPart))) data.clear();
    }
    CloseHandle(file);
    return data;
}
struct WriteJob {Learner* owner;std::vector<uint32_t> data;uint64_t version=0;};
static unsigned __stdcall WriteManifest(void* value) {
    std::unique_ptr<WriteJob> job(static_cast<WriteJob*>(value));bool ok=false;
    try {
        // No whole-manifest copy/serialization on the Present thread. The
        // process-lifetime learner owns immutable records until this completes.
        job->data=job->owner->manifest.Serialize(job->version);
        const std::wstring pending=job->owner->path+L".pending-"+std::to_wstring(GetCurrentProcessId());
        HANDLE file=CreateFileW(pending.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE) {
            DWORD written=0;const DWORD bytes=DWORD(job->data.size()*4);
            ok=WriteFile(file,job->data.data(),bytes,&written,nullptr) && written==bytes;
            CloseHandle(file);
            if(ok) ok=MoveFileExW(pending.c_str(),job->owner->path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
            if(!ok) DeleteFileW(pending.c_str());
        }
    } catch(...) {ok=false;}
    if(ok) job->owner->saved.store(job->version);
    else ++job->owner->writeFailures;
    job->owner->writing.store(false);
    return 0;
}
// Present-thread cadence; serialization and file I/O run on the worker.
static void Tick(ULONGLONG now=GetTickCount64()) {
    Learner* l=learner.load(std::memory_order_acquire);
    if(!l) return;
    // Learning follows the thread that presents, which also issues the draws.
    // A changed owner restarts the consecutive-pair filter, not the seen set.
    const DWORD current=GetCurrentThreadId();
    if(l->thread.exchange(current,std::memory_order_relaxed)!=current) l->lastVs=l->lastPs=~uintptr_t(0);
    if(now<l->nextTick) return;
    l->nextTick=now+2000;
    if(!l->manifest.Size() || l->saved.load()>=l->manifest.Version() || l->writing.exchange(true)) return;
    Log("[PipelineWarmup] saving records=%zu learned=%llu captureFailures=%llu writeFailures=%llu saturated=%d",
        l->manifest.Size(),l->learned.load(),l->captureFailures.load(),l->writeFailures.load(),int(l->saturated.load()));
    try {
        auto job=std::make_unique<WriteJob>();job->owner=l;
        auto handle=_beginthreadex(nullptr,0,&WriteManifest,job.get(),0,nullptr);
        if(handle) {job.release();CloseHandle(reinterpret_cast<HANDLE>(handle));}
        else {l->writing.store(false);++l->writeFailures;}
    } catch(...) {l->writing.store(false);++l->writeFailures;}
}
}

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include <array>
#include <cstring>
#include "@REPO@/src/shared_reader_fence.h"
static unsigned checks=0,releases=0,copies=0,allocations=0,drains=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL line%d %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned failAt=0,creationStep=0;
static bool Fail(){return failAt && ++creationStep==failAt;}
struct Object;
static std::vector<Object*> objects;
struct Object {
    unsigned refs=1;
    Object(){objects.push_back(this);}
    virtual ~Object()=default;
    void AddRef(){CHECK(refs);++refs;}
    void Release(){CHECK(refs);if(!--refs){++releases;Zero();}}
    virtual void Zero(){}
};
struct D3DSURFACE_DESC{UINT Width,Height,Format;};
struct IDirect3DSurface9:Object{
    D3DSURFACE_DESC desc{2676,2728,21};unsigned color=0;
    HRESULT GetDesc(D3DSURFACE_DESC* out){if(Fail())return E_FAIL;*out=desc;return S_OK;}
};
struct IDirect3DTexture9:Object{
    IDirect3DSurface9* surface=new IDirect3DSurface9;
    HRESULT GetSurfaceLevel(unsigned,IDirect3DSurface9** out){if(Fail())return E_FAIL;*out=surface;surface->AddRef();return S_OK;}
    void Zero()override{surface->Release();}
};
struct __declspec(uuid("33333333-3333-3333-3333-333333333333")) ID3D11Texture2D:Object{};
struct ID3D11ShaderResourceView:Object{
    ID3D11Texture2D* texture=nullptr;
    void Zero()override{if(texture)texture->Release();}
};
struct IDirect3DQuery9:Object{
    unsigned issues=0,polls=0,flushes=0;HRESULT result=S_OK;
    HRESULT Issue(DWORD){++issues;return result;}
    HRESULT GetData(void*,unsigned,DWORD flags){++polls;flushes+=flags!=0;return result;}
};
struct ID3D11Query:Object{unsigned polls=0,ends=0;HRESULT result=S_OK;};
struct D3D11_QUERY_DESC{unsigned Query,MiscFlags;};
static constexpr unsigned D3D11_QUERY_EVENT=0,D3D11_ASYNC_GETDATA_DONOTFLUSH=1,
    D3DUSAGE_RENDERTARGET=1,D3DFMT_A8R8G8B8=21,D3DPOOL_DEFAULT=0,D3DQUERYTYPE_EVENT=8,
    D3DBACKBUFFER_TYPE_MONO=0,D3DTEXF_NONE=0,D3DISSUE_END=1,D3DGETDATA_FLUSH=1;
struct Context:Object{
    unsigned flushes=0;bool lease=false;
    void End(ID3D11Query* q){CHECK(!lease);++q->ends;}
    void Flush(){CHECK(!lease);++flushes;}
    HRESULT GetData(ID3D11Query* q,void*,unsigned,DWORD flags){CHECK(!lease&&flags==D3D11_ASYNC_GETDATA_DONOTFLUSH);++q->polls;return q->result;}
} context;
static Context* g_ctx11=&context;
struct Device11{
    HRESULT OpenSharedResource(HANDLE,REFIID,void** out){if(Fail())return E_FAIL;*out=new ID3D11Texture2D;return S_OK;}
    HRESULT CreateShaderResourceView(ID3D11Texture2D* tex,void*,ID3D11ShaderResourceView** out){
        if(Fail())return E_FAIL;*out=new ID3D11ShaderResourceView;(*out)->texture=tex;tex->AddRef();return S_OK;
    }
    HRESULT CreateQuery(const D3D11_QUERY_DESC*,ID3D11Query** out){if(Fail())return E_FAIL;*out=new ID3D11Query;return S_OK;}
} device11;
static Device11* g_dev11=&device11;
struct IDirect3DDevice9{
    IDirect3DSurface9 input;
    HRESULT GetRenderTarget(unsigned,IDirect3DSurface9** out){*out=&input;input.AddRef();return S_OK;}
    HRESULT GetBackBuffer(unsigned,unsigned,unsigned,IDirect3DSurface9** out){*out=&input;input.AddRef();return S_OK;}
    HRESULT CreateTexture(UINT w,UINT h,unsigned,unsigned,unsigned,unsigned,IDirect3DTexture9** out,HANDLE* handle){
        ++allocations;if(Fail())return E_FAIL;*out=new IDirect3DTexture9;(*out)->surface->desc={w,h,21};*handle=*out;return S_OK;
    }
    HRESULT CreateQuery(unsigned,IDirect3DQuery9** out){if(Fail())return E_FAIL;*out=new IDirect3DQuery9;return S_OK;}
    HRESULT StretchRect(IDirect3DSurface9* src,void*,IDirect3DSurface9* dst,void*,unsigned){++copies;dst->color=src->color;return S_OK;}
};
using ID3D11DeviceContext=Context;
using ID3D11RenderTargetView=Object;
using ID3D11Buffer=Object;
using ID3D11VertexShader=Object;
using ID3D11PixelShader=Object;
using ID3D11RasterizerState=Object;
using ID3D11SamplerState=Object;
namespace d2vr_transfer{
@REF@
struct Job{
    Ref<IDirect3DQuery9> producer;
    Ref<Context> context;
    Ref<ID3D11Query> reader;
    Ref<Object> target,constants,vs,ps,raster,sampler;
    Ref<ID3D11ShaderResourceView> source,depth;
    std::array<unsigned char,256> bytes{};
    unsigned size=0,width=0,height=0;
    bool deferReader=false,producerComplete=false;
};
struct Queue{void Drain(){++drains;context.lease=false;}};
class Pending{
    std::shared_ptr<Job> job;
public:
    bool reject=false;
    bool Start(Queue&,std::shared_ptr<Job> input,bool&){
        if(reject)return false;CHECK(!context.lease);job=input;context.lease=true;return true;
    }
    void Finish(){if(job){context.lease=false;context.End(job->reader.Get());context.Flush();job.reset();}}
    ~Pending(){Finish();}
};
}
static bool g_zeroCopyEnabled=true,g_lastConsumerReaderFence=true;
static UINT g_srcW=0,g_srcH=0;
template<class... A>static void Log(A...){}
#include "@REPO@/src/shared_capture.inl"
static bool EnsureD11StereoPipeline(){CHECK(!context.lease);return true;}
static void EnsureDepthTexture11(){CHECK(!context.lease);}
static ID3D11ShaderResourceView* g_uploadSRV=nullptr;
static ID3D11ShaderResourceView* g_depthSRV11=nullptr;
static Object *g_eyeRTV[2],*g_stereoCB11,*g_stereoVS11,*g_stereoPS11,*g_stereoRS11,*g_stereoSamp11;
static int g_eyeFov[2]{};
static UINT g_eyeCacheW=2676,g_eyeCacheH=2728;
static bool g_deferStereoTransferReader=true;
static int g_stereoTransferMode=1;
struct Constants{float values[28]{};};
static Constants PrepareStereoEyeConstants(int,int,int,int,int,int,int){return {};}
@START_TRANSFER@
static void Track(SharedCaptureSlot& slot){
    CHECK(CanReadSharedUpload(slot.srv11));
    slot.deferred=std::make_shared<d2vr_transfer::Job>();
    SharedReaderFenceOps ops{slot};context.End(slot.reader);context.Flush();
    CHECK(slot.fence.TrackIssuedRead(ops));
}
int main(){
    IDirect3DDevice9 device;d2vr_transfer::Queue queue;
    g_stereoTransferQueue=&queue;
    CHECK(EnsureSharedCapture(&device));CHECK(PrepareSplitSharedCapture(&device));
    auto& left=g_sharedCapture[0];auto& right=g_sharedCapture[1];
    CHECK(left.tex9!=right.tex9&&left.surface9!=right.surface9&&left.reader!=right.reader&&left.producer!=right.producer&&left.srv11!=right.srv11);
    auto allocationsBefore=allocations;
    CHECK(EnsureSharedCapture(&device)&&PrepareSplitSharedCapture(&device)&&allocations==allocationsBefore);
    for(unsigned frame=0;frame<1000;++frame){
        left.reader->result=right.reader->result=S_OK;
        device.input.color=frame*2+1;CHECK(CaptureBackbufferToShared(&device,true,0));Track(left);
        std::weak_ptr<d2vr_transfer::Job> leftJob=left.deferred;
        left.reader->result=E_FAIL;const unsigned leftPolls=left.reader->polls;
        device.input.color=frame*2+2;CHECK(CaptureBackbufferToShared(&device,true,1));Track(right);
        CHECK(left.reader->polls==leftPolls&&!leftJob.expired());
        CHECK(left.surface9->color==frame*2+1&&right.surface9->color==frame*2+2);
        const unsigned copied=copies;
        CHECK(!CaptureBackbufferToShared(&device,true,0)&&copies==copied&&!leftJob.expired());
        left.reader->result=S_OK;
        CHECK(WaitSharedReader(left)&&leftJob.expired());
        CHECK(WaitSharedReader(right));
    }
    // Owner-produced mode still waits its own producer; never polls the peer.
    CHECK(CaptureBackbufferToShared(&device,false,1));
    CHECK(g_srcW==2676&&g_srcH==2728);
    CHECK(!CaptureBackbufferToShared(&device,true,2)&&!EnsureSharedCapture(&device,2));
    CHECK(SealSharedUpload(left.srv11)&&!CanReadSharedUpload(left.srv11));
    CHECK(CanReadSharedUpload(right.srv11));CHECK(WaitSharedReader(left));
    // A failed reader retains both resource lifetimes and that slot's packet.
    Track(left);Track(right);right.reader->result=E_FAIL;
    const unsigned retired=releases;auto* oldLeft=left.tex9;auto* oldRight=right.tex9;
    std::weak_ptr<d2vr_transfer::Job> retained=right.deferred;
    context.lease=true;CHECK(!ReleaseSharedCapture());
    CHECK(drains&&g_stereoTransferFault&&releases==retired&&left.tex9==oldLeft&&right.tex9==oldRight&&!retained.expired());
    CHECK(!CaptureBackbufferToShared(&device,true,0));
    right.reader->result=S_OK;CHECK(ReleaseSharedCapture()&&retained.expired());
    CHECK(!left.tex9&&!right.tex9&&!g_stereoTransferFault);
    // All allocation stages of the optional slot fail independently. Existing
    // slot zero remains usable; no per-frame retry or primary failure accrual.
    for(unsigned step=1;step<=7;++step){
        CHECK(EnsureSharedCapture(&device));g_zeroCopyFailures=2;
        failAt=step;creationStep=0;
        CHECK(!PrepareSplitSharedCapture(&device)&&g_splitCaptureUnavailable);
        const unsigned attempts=allocations;failAt=0;
        CHECK(!PrepareSplitSharedCapture(&device)&&allocations==attempts&&g_zeroCopyFailures==2);
        CHECK(EnsureSharedCapture(&device)&&CaptureBackbufferToShared(&device,true,0));
        CHECK(ReleaseSharedCapture()&&!g_splitCaptureUnavailable);g_zeroCopyFailures=0;
    }
    // Resize one eye after its prior read completes; retain the other's slot.
    CHECK(EnsureSharedCapture(&device)&&PrepareSplitSharedCapture(&device));
    auto* peer=right.tex9;Track(left);device.input.desc={1280,720,21};
    CHECK(EnsureSharedCapture(&device,0)&&left.width==1280&&right.tex9==peer);
    CHECK(EnsureSharedCapture(&device,1)&&right.width==1280);
    CHECK(ReleaseSharedCapture());
    // Exercise the production packet builder too: exact producer/source/reader
    // must follow the selected slot, including owner-producer mode and fallback.
    g_eyeRTV[0]=new Object;g_eyeRTV[1]=new Object;
    g_stereoCB11=new Object;g_stereoVS11=new Object;g_stereoPS11=new Object;
    g_stereoRS11=new Object;g_stereoSamp11=new Object;
    for(int mode:{1,3})for(unsigned split:{0u,1u}){
        g_stereoTransferMode=mode;
        CHECK(EnsureSharedCapture(&device)&&PrepareSplitSharedCapture(&device));
        for(unsigned eye=0;eye<2;++eye){
            const unsigned index=split?eye:0;
            auto& slot=g_sharedCapture[index];
            d2vr_transfer::Pending pending;std::shared_ptr<d2vr_transfer::Job> job;
            CHECK(StartStereoTransfer(&device,eye,pending,job,index));
            CHECK(context.lease&&job&&job->source.Get()==slot.srv11&&job->reader.Get()==slot.reader);
            CHECK(job->producer.Get()==(mode==3?nullptr:slot.producer));
            CHECK(job->producerComplete==(mode==3)&&job->deferReader&&slot.deferred==job);
            CHECK(job->target.Get()==g_eyeRTV[eye]&&job->width==g_eyeCacheW&&job->height==g_eyeCacheH&&g_uploadSRV==nullptr);
            pending.Finish();job.reset();CHECK(!context.lease&&slot.deferred);
            CHECK(WaitSharedReader(slot)&&!slot.deferred);
        }
        CHECK(ReleaseSharedCapture());
    }
    CHECK(EnsureSharedCapture(&device));
    d2vr_transfer::Pending refused;refused.reject=true;std::shared_ptr<d2vr_transfer::Job> refusedJob;
    CHECK(!StartStereoTransfer(&device,0,refused,refusedJob));
    CHECK(g_stereoTransferFault&&!left.deferred&&!left.fence.pending&&!context.lease);
    refusedJob.reset();CHECK(ReleaseSharedCapture());
    // Legacy readers have no draw-time receipt. Teardown must still fence
    // their last context read, including after a failed retirement attempt.
    g_lastConsumerReaderFence=false;CHECK(EnsureSharedCapture(&device));
    auto* legacyReader=left.reader;legacyReader->result=E_FAIL;
    CHECK(!left.fence.pending&&!left.deferred&&!ReleaseSharedCapture());
    CHECK(legacyReader->ends==1&&left.reader==legacyReader);
    legacyReader->result=S_OK;CHECK(ReleaseSharedCapture()&&legacyReader->ends==2);
    g_lastConsumerReaderFence=true;
    for(auto* resource:{g_eyeRTV[0],g_eyeRTV[1],g_stereoCB11,g_stereoVS11,g_stereoPS11,g_stereoRS11,g_stereoSamp11})resource->Release();
    for(auto* object:objects)if(object!=&device.input&&object!=&context)CHECK(object->refs==0);
    // Objects are retained by the fixture only to inspect use-after-release.
    for(auto* object:objects)if(object!=&device.input&&object!=&context)delete object;
    printf("PASS actual shared capture slots: %u checks, 1000 two-eye sequences; no peer query polling/overwrite, own-reader refusal, deferred retention, all creation failures, resize and all-or-nothing release; CPU model, no GPU/FPS claim\n",checks);
}

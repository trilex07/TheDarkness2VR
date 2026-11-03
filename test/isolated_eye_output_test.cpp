#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "../src/isolated_eye_output.h"
#include "../src/native_submission_cache.h"
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
namespace d2vr_record_sync {static bool busy=false;static std::atomic<bool> conflict=false;static bool Busy(){return busy;}}
namespace d2vr_replay {
#include "actual_records.inl"
static std::vector<Rec> g_rec;
static bool g_recActive=false,g_recOverflow=false;
static uint64_t g_wvpHistoryGeneration=1;
struct RecLockGuard{~RecLockGuard(){}};
}
#include "../src/isolated_eye_replay.inl"
using ApplyFn=HRESULT(STDMETHODCALLTYPE*)(IDirect3DStateBlock9*);
static ApplyFn g_origSBApply=nullptr;
static bool SyncActiveShader(IDirect3DDevice9*){return true;}
#include "actual_snapshot.inl"
static HRESULT STDMETHODCALLTYPE Set(IDirect3DDevice9* d,DWORD slot,IDirect3DBaseTexture9* t){return d->SetTexture(slot,t);}
static IDirect3DSurface9* Target(IDirect3DDevice9* d,unsigned w,unsigned h){
    IDirect3DSurface9* s=nullptr;CHECK(SUCCEEDED(d->CreateRenderTarget(w,h,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&s,nullptr)));return s;
}
static void Same(IDirect3DDevice9* d,IDirect3DSurface9* a,IDirect3DSurface9* b){
    D3DSURFACE_DESC desc{};CHECK(SUCCEEDED(a->GetDesc(&desc)));
    IDirect3DSurface9 *ra=nullptr,*rb=nullptr;
    CHECK(SUCCEEDED(d->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&ra,nullptr)));
    CHECK(SUCCEEDED(d->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&rb,nullptr)));
    CHECK(SUCCEEDED(d->GetRenderTargetData(a,ra)));CHECK(SUCCEEDED(d->GetRenderTargetData(b,rb)));
    D3DLOCKED_RECT la{},lb{};CHECK(SUCCEEDED(ra->LockRect(&la,nullptr,D3DLOCK_READONLY)));CHECK(SUCCEEDED(rb->LockRect(&lb,nullptr,D3DLOCK_READONLY)));
    for(unsigned y=0;y<desc.Height;++y)CHECK(!memcmp((char*)la.pBits+y*la.Pitch,(char*)lb.pBits+y*lb.Pitch,desc.Width*4));
    CHECK(SUCCEEDED(ra->UnlockRect()));CHECK(SUCCEEDED(rb->UnlockRect()));ra->Release();rb->Release();
}
static uint32_t Ptr(const void* p){return uint32_t(uintptr_t(p));}
static void Run(IDirect3DDevice9Ex* d,unsigned w,unsigned h,bool texture){
    using namespace d2vr_replay;
    IDirect3DTexture9* tex=nullptr;IDirect3DSurface9* source=nullptr;
    if(texture){CHECK(SUCCEEDED(d->CreateTexture(w,h,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&tex,nullptr)));CHECK(SUCCEEDED(tex->GetSurfaceLevel(0,&source)));}
    else source=Target(d,w,h);
    auto legacy=Target(d,w,h),left=Target(d,w,h),history=Target(d,w,h),saved=Target(d,w,h);
    CHECK(SUCCEEDED(d->ColorFill(source,nullptr,0xff123456)));CHECK(SUCCEEDED(d->ColorFill(legacy,nullptr,0xff123456)));
    d2vr_eye_output::Output out;CHECK(out.Prepare(d,source));CHECK(!out.Valid());CHECK(out.Seed(d,source));
    CHECK((out.SourceTexture()!=nullptr)==texture);
    const unsigned frames=w>1000?4:80;
    for(unsigned frame=0;frame<frames;++frame){
        // Legacy: backup left, seed prior right, save right, restore left.
        CHECK(SUCCEEDED(d->ColorFill(left,nullptr,0xffabcdef+frame)));
        CHECK(SUCCEEDED(d->StretchRect(left,nullptr,source,nullptr,D3DTEXF_NONE)));
        CHECK(SUCCEEDED(d->StretchRect(source,nullptr,saved,nullptr,D3DTEXF_NONE)));
        CHECK(SUCCEEDED(d->StretchRect(legacy,nullptr,source,nullptr,D3DTEXF_NONE)));
        RECT partial{int(frame%8),int(frame%8),int(frame%8+8),int(frame%8+8)};
        CHECK(SUCCEEDED(d->ColorFill(source,frame%7?&partial:nullptr,0xff220000+frame)));
        CHECK(SUCCEEDED(d->StretchRect(source,nullptr,history,nullptr,D3DTEXF_NONE)));
        CHECK(SUCCEEDED(d->StretchRect(source,nullptr,legacy,nullptr,D3DTEXF_NONE)));
        CHECK(SUCCEEDED(d->StretchRect(saved,nullptr,source,nullptr,D3DTEXF_NONE)));
        std::vector<Rec> original={{OP_SetRenderTarget,0,{0,Ptr(source)}},{OP_ColorFill,0,{Ptr(source),0xff220000+frame}},
            {OP_StretchRect,0,{Ptr(source),Ptr(saved)}},{OP_SetTexture,0,{0,Ptr(tex)}}};
        g_rec=original;out.Activate();EyeOutputRecordPatch patch;CHECK(patch.Begin(out));
        CHECK(g_rec[0].a[1]==Ptr(out.Right()));CHECK(g_rec[1].a[0]==Ptr(out.Right()));
        CHECK(g_rec[2].a[0]==Ptr(out.Right()) && g_rec[2].a[1]==Ptr(saved));
        CHECK(g_rec[3].a[1]==Ptr(out.Texture(tex)));
        // Actual snapshot Apply must substitute target BEFORE stateblock Apply,
        // preserving viewport and inherited sampler bindings on a pure device.
        CHECK(SUCCEEDED(d->SetRenderTarget(0,source)));
        D3DVIEWPORT9 vp{1,1,w-2,h-2,0,1};CHECK(SUCCEEDED(d->SetViewport(&vp)));
        CHECK(SUCCEEDED(d->SetTexture(0,tex)));CHECK(SUCCEEDED(d->SetTexture(257,tex)));
        DeviceSnapshot snapshot;CHECK(snapshot.Capture(d));CHECK(snapshot.Apply(d,source,out.Right()));
        D3DVIEWPORT9 actualViewport{};CHECK(SUCCEEDED(d->GetViewport(&actualViewport)));CHECK(!memcmp(&vp,&actualViewport,sizeof(vp)));
        CHECK(out.RemapInheritedTextures(d,Set));
        IDirect3DSurface9* bound=nullptr;CHECK(SUCCEEDED(d->GetRenderTarget(0,&bound)));CHECK(bound==out.Right());bound->Release();
        IDirect3DBaseTexture9* boundTex=nullptr;CHECK(SUCCEEDED(d->GetTexture(0,&boundTex)));CHECK(boundTex==out.Texture(tex));if(boundTex)boundTex->Release();
        boundTex=nullptr;CHECK(SUCCEEDED(d->GetTexture(257,&boundTex)));CHECK(boundTex==out.Texture(tex));if(boundTex)boundTex->Release();
        CHECK(SUCCEEDED(d->SetTexture(0,nullptr)));CHECK(SUCCEEDED(d->SetTexture(257,nullptr)));
        CHECK(SUCCEEDED(d->ColorFill(out.Surface(source),frame%7?&partial:nullptr,0xff220000+frame)));
        CHECK(patch.Restore());CHECK(!memcmp(g_rec.data(),original.data(),original.size()*sizeof(Rec)));
        out.Commit();CHECK(snapshot.Apply(d));
        if(frame<3 || frame+1==frames){Same(d,out.Right(),legacy);Same(d,source,left);}
    }
    for(auto op:{OP_ApplyStateBlock,OP_UpdateSurface,OP_UpdateTexture,OP_SetRenderTarget}){
        g_rec={{uint32_t(op),0,{0,0}}};
        if(op==OP_UpdateSurface)g_rec[0].a[1]=Ptr(source);
        if(op==OP_UpdateTexture){if(!tex)continue;g_rec[0].a[0]=Ptr(tex);}
        if(op==OP_SetRenderTarget){g_rec[0].a[0]=1;g_rec[0].a[1]=Ptr(source);}
        const auto before=g_rec;out.Activate();EyeOutputRecordPatch p;CHECK(!p.Begin(out));CHECK(!memcmp(before.data(),g_rec.data(),sizeof(Rec)));out.Deactivate();
    }
    g_rec={{OP_ColorFill,0,{Ptr(source)}}};
    for(unsigned gate=0;gate<6;++gate){
        out.Activate();
        g_recActive=gate==0;g_recOverflow=gate==1;d2vr_record_sync::busy=gate==2;d2vr_record_sync::conflict=gate==3;
        if(gate==4)out.Deactivate();
        if(gate==5)g_rec.resize(50001);
        EyeOutputRecordPatch p;CHECK(!p.Begin(out));CHECK(g_rec[0].a[0]==Ptr(source));
        g_recActive=g_recOverflow=d2vr_record_sync::busy=false;d2vr_record_sync::conflict=false;g_rec.resize(1);
    }
    out.Activate();
    {EyeOutputRecordPatch p;CHECK(p.Begin(out));}CHECK(g_rec[0].a[0]==Ptr(source));
    {EyeOutputRecordPatch p;CHECK(p.Begin(out));g_rec[0].a[0]=123;CHECK(!p.Restore());CHECK(g_recOverflow);g_recOverflow=false;}
    out.Invalidate();CHECK(!out.Active() && !out.Valid());CHECK(out.Seed(d,history));out.Release();
    IDirect3DTexture9* mip=nullptr;IDirect3DSurface9* mipSurface=nullptr;
    CHECK(SUCCEEDED(d->CreateTexture(64,64,0,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&mip,nullptr)));
    CHECK(SUCCEEDED(mip->GetSurfaceLevel(0,&mipSurface)));CHECK(!out.Prepare(d,mipSurface));mipSurface->Release();mip->Release();
    CHECK(SUCCEEDED(d->SetRenderTarget(0,left)));source->Release();if(tex)tex->Release();legacy->Release();history->Release();saved->Release();left->Release();
}
int main(){
    IDirect3D9Ex* api=nullptr;CHECK(SUCCEEDED(Direct3DCreate9Ex(D3D_SDK_VERSION,&api)));
    HWND window=CreateWindowExW(0,L"STATIC",L"Isolated eye offline",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(window);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.BackBufferWidth=pp.BackBufferHeight=64;pp.hDeviceWindow=window;
    IDirect3DDevice9Ex* d=nullptr;CHECK(SUCCEEDED(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,window,0x50,&pp,nullptr,&d)));
    for(unsigned cycle=0;cycle<2;++cycle){for(bool texture:{false,true}){Run(d,64,48,texture);Run(d,2676,2728,texture);}CHECK(SUCCEEDED(d->ResetEx(&pp,nullptr)));}
    d->Release();api->Release();DestroyWindow(window);
    printf("PASS isolated eye: %u checks, real x86 D3D9Ex 0x50, full-resolution pixels, partial/UI history, texture aliases, actual records/snapshot, rejection, RAII, Reset; no FPS/HMD claim\n",checks);
}

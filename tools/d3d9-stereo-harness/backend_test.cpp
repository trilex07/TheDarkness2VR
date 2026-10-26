// Real API conformance fixture for the default-OFF execution backend. Not a profiler.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <thread>
#include "../../src/image_present_observer.h"
#include "../../src/d3d9_backend.h"
#include "../../src/stereo_geometry.h"
using Microsoft::WRL::ComPtr;
static d2vr_image_present::Observer observer;
static uintptr_t imageBase=0;static size_t imageSize=0;
static bool Qualifies(d2vr_image_present::Method,uintptr_t caller) {
    return caller>=imageBase && caller-imageBase<imageSize;
}
static unsigned beforeCount=0,afterCount=0;
static void Before(IDirect3DSwapChain9*,uintptr_t,bool qualified,bool) {if(qualified) ++beforeCount;}
static void After(IDirect3DSwapChain9*,uintptr_t,bool qualified,bool,HRESULT) {if(qualified) ++afterCount;}
static void Log(const char* fmt,...) {
    if(std::strstr(fmt,"FAIL")) std::fputs("Present observer rejected native implementation\n",stderr);
}
static std::vector<unsigned char> TextSnapshot() {
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS*>(base+reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    auto section=IMAGE_FIRST_SECTION(pe);
    imageBase=reinterpret_cast<uintptr_t>(base);imageSize=pe->OptionalHeader.SizeOfImage;
    for(unsigned i=0;i<pe->FileHeader.NumberOfSections;++i,++section)
        if(!std::memcmp(section->Name,".text",5)) return {base+section->VirtualAddress,base+section->VirtualAddress+section->Misc.VirtualSize};
    ExitProcess(2);
}
static std::atomic<unsigned> checks{0};
static void Check(bool value,const char* name) {
    ++checks;
    if(!value) {std::fprintf(stderr,"FAIL %s error=%lu\n",name,GetLastError());ExitProcess(1);}
}
static void Ok(HRESULT hr,const char* name) {
    ++checks;
    if(hr!=D3D_OK) {std::fprintf(stderr,"FAIL %s hr=%08lX\n",name,static_cast<unsigned long>(hr));ExitProcess(1);}
}
static void Wait9(IDirect3DQuery9* query) {
    Ok(query->Issue(D3DISSUE_END),"producer fence");
    const ULONGLONG start=GetTickCount64();HRESULT hr;
    while((hr=query->GetData(nullptr,0,D3DGETDATA_FLUSH))==S_FALSE) {
        Check(GetTickCount64()-start<5000,"bounded producer wait");Sleep(1);
    }
    Ok(hr,"producer completed");
}
static void Wait11(ID3D11DeviceContext* ctx,ID3D11Query* query) {
    ctx->End(query);ctx->Flush();
    const ULONGLONG start=GetTickCount64();HRESULT hr;
    while((hr=ctx->GetData(query,nullptr,0,0))==S_FALSE) {
        Check(GetTickCount64()-start<5000,"bounded consumer wait");Sleep(1);
    }
    Ok(hr,"consumer completed before shared overwrite");
}
static ComPtr<ID3DBlob> Compile(const char* source,const char* profile) {
    ComPtr<ID3DBlob> code,errors;
    const HRESULT hr=D3DCompile(source,std::strlen(source),nullptr,nullptr,nullptr,"main",profile,
        D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
    if(FAILED(hr) && errors) std::fwrite(errors->GetBufferPointer(),1,errors->GetBufferSize(),stderr);
    Ok(hr,"SM3 shader compile");return code;
}
static void RenderPair(IDirect3DDevice9Ex* dev,ID3D11Device* dev11,ID3D11DeviceContext* ctx,UINT width) {
    HANDLE shared=nullptr;ComPtr<IDirect3DTexture9> texture;ComPtr<IDirect3DSurface9> target;
    Ok(dev->CreateTexture(width,32,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture,&shared),"shared9 RT");
    Check(shared!=nullptr,"shared handle");Ok(texture->GetSurfaceLevel(0,&target),"shared surface");
    ComPtr<ID3D11Texture2D> opened;
    Ok(dev11->OpenSharedResource(shared,IID_PPV_ARGS(&opened)),"native11 opens shared9 RT");
    D3D11_TEXTURE2D_DESC desc{};opened->GetDesc(&desc);
    Check(desc.Width==width && desc.Height==32 && desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM,"interop dimensions/format");
    ComPtr<ID3D11ShaderResourceView> srv;
    Ok(dev11->CreateShaderResourceView(opened.Get(),nullptr,&srv),"native11 source SRV");
    D3D11_TEXTURE2D_DESC cacheDesc=desc;cacheDesc.MiscFlags=0;
    cacheDesc.Usage=D3D11_USAGE_DEFAULT;cacheDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;cacheDesc.CPUAccessFlags=0;
    ComPtr<ID3D11Texture2D> cache[2];
    for(auto& c:cache) Ok(dev11->CreateTexture2D(&cacheDesc,nullptr,&c),"independent eye cache");
    ComPtr<IDirect3DSurface9> previous;Ok(dev->GetRenderTarget(0,&previous),"save live RT");
    ComPtr<IDirect3DStateBlock9> start;Ok(dev->CreateStateBlock(D3DSBT_ALL,&start),"save native state");
    ComPtr<IDirect3DQuery9> producer;Ok(dev->CreateQuery(D3DQUERYTYPE_EVENT,&producer),"producer event");
    D3D11_QUERY_DESC qd{D3D11_QUERY_EVENT,0};ComPtr<ID3D11Query> consumer;
    Ok(dev11->CreateQuery(&qd,&consumer),"consumer event");
    auto vsCode=Compile("row_major float4x4 m:register(c0); float4 main(float4 p:POSITION):POSITION{return mul(p,m);}","vs_3_0");
    auto psCode=Compile("float4 color:register(c0);float4 main():COLOR0{return color;}","ps_3_0");
    ComPtr<IDirect3DVertexShader9> vs;ComPtr<IDirect3DPixelShader9> ps;
    Ok(dev->CreateVertexShader(static_cast<const DWORD*>(vsCode->GetBufferPointer()),&vs),"native SM3 VS");
    Ok(dev->CreatePixelShader(static_cast<const DWORD*>(psCode->GetBufferPointer()),&ps),"native SM3 PS");
    const D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},D3DDECL_END()};
    ComPtr<IDirect3DVertexDeclaration9> decl;
    Ok(dev->CreateVertexDeclaration(elements,&decl),"position declaration");
    const float vertices[]={-.65f,-.65f,2, -.65f,.65f,2, .65f,-.65f,2, .65f,.65f,2};
    ComPtr<IDirect3DVertexBuffer9> vb;
    Ok(dev->CreateVertexBuffer(sizeof(vertices),D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&vb,nullptr),"dynamic native VB");
    void* mapped=nullptr;Ok(vb->Lock(0,sizeof(vertices),&mapped,D3DLOCK_DISCARD),"dynamic discard lock");
    std::memcpy(mapped,vertices,sizeof(vertices));Ok(vb->Unlock(),"dynamic unlock");
    ComPtr<IDirect3DSurface9> source;
    Ok(dev->CreateRenderTarget(width,32,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&source,nullptr),"native offscreen source RT0");
    Ok(dev->SetRenderTarget(0,source.Get()),"live offscreen RT");
    Ok(dev->SetDepthStencilSurface(nullptr),"no depth for fixture");
    D3DVIEWPORT9 vp{0,0,width,32,0,1};Ok(dev->SetViewport(&vp),"full viewport");
    Ok(dev->SetVertexDeclaration(decl.Get()),"bind declaration");
    Ok(dev->SetVertexShader(vs.Get()),"bind VS");Ok(dev->SetPixelShader(ps.Get()),"bind PS");
    Ok(dev->SetStreamSource(0,vb.Get(),0,3*sizeof(float)),"bind owned VB");
    Ok(dev->SetRenderState(D3DRS_ZENABLE,FALSE),"depth disabled");
    Ok(dev->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"cull disabled");
    Ok(dev->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE),"opaque material");
    const float originalColor[]={1,.25f,0,1},otherColor[]={0,0,1,1};
    Ok(dev->SetPixelShaderConstantF(0,originalColor,1),"original material constants");
    ComPtr<IDirect3DStateBlock9> material;Ok(dev->CreateStateBlock(D3DSBT_ALL,&material),"material state block");
    Ok(dev->SetPixelShaderConstantF(0,otherColor,1),"temporary other material");
    Ok(material->Apply(),"restore original material");
    float restored[4]{};Ok(dev->GetPixelShaderConstantF(0,restored,1),"verify state block constants");
    Check(std::memcmp(restored,originalColor,sizeof(restored))==0,"state block exact PS payload");
    float raw[16]={1,0,0,0, 0,1,0,0, 0,0,1,1, 0,0,0,0};
    for(int eye=0;eye<2;++eye) {
        float matrix[16];std::memcpy(matrix,raw,sizeof(matrix));
        d2vr_stereo::EyeShiftParams sp{};
        // Enlarged synthetic baseline makes a measurable disparity at 32px.
        d2vr_stereo::BuildPerEyeTranslation(eye,0,.5f,&sp.transX);
        d2vr_stereo::ApplyEyeShiftToMatrix(matrix,4,sp,true,1,1,nullptr);
        Ok(dev->SetVertexShaderConstantF(0,matrix,4),"per-eye production matrix math");
        Ok(dev->BeginScene(),"same-frame scene");
        Ok(dev->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0),"clear eye");
        Ok(dev->DrawPrimitive(D3DPT_TRIANGLESTRIP,0,2),"draw dynamic VB");
        Ok(dev->EndScene(),"end eye");
        Ok(dev->StretchRect(source.Get(),nullptr,target.Get(),nullptr,D3DTEXF_NONE),"production-style RT0-to-shared GPU copy");
        Wait9(producer.Get());
        ctx->CopyResource(cache[eye].Get(),opened.Get());
        Wait11(ctx,consumer.Get()); // retain existing reader-before-overwrite synchronization
    }
    // Readback is test verification ONLY. The renderer continues using GPU-only
    // shared surfaces and the original query synchronization; no CPU bridge.
    auto stagingDesc=cacheDesc;stagingDesc.Usage=D3D11_USAGE_STAGING;
    stagingDesc.BindFlags=0;stagingDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;Ok(dev11->CreateTexture2D(&stagingDesc,nullptr,&staging),"test-only readback");
    double centers[2]{};unsigned pixels[2]{};
    for(int eye=0;eye<2;++eye) {
        ctx->CopyResource(staging.Get(),cache[eye].Get());D3D11_MAPPED_SUBRESOURCE read{};
        Ok(ctx->Map(staging.Get(),0,D3D11_MAP_READ,0,&read),"verify cached eye pixels");
        double total=0;
        for(UINT y=0;y<32;++y) for(UINT x=0;x<width;++x) {
            const auto* pixel=static_cast<const unsigned char*>(read.pData)+y*read.RowPitch+4*x;
            if(pixel[2]>200) {Check(pixel[0]<4 && pixel[1]>=60 && pixel[1]<=68,"material color survived translation");++pixels[eye];total+=x+.5;}
        }
        ctx->Unmap(staging.Get(),0);Check(pixels[eye]>20,"eye contains actual geometry");centers[eye]=total/pixels[eye];
    }
    Check(centers[0]>centers[1] && std::fabs((centers[0]+centers[1])-width)<2,"correct per-eye disparity and center");
    Check(pixels[0]==pixels[1],"both eye caches retain geometry");
    std::printf("PASS GPU shared stereo %ux32: pixels=%u/%u centers=%.2f/%.2f; SM3 material, owned VB, state block\n",
        width,pixels[0],pixels[1],centers[0],centers[1]);
    Ok(dev->SetRenderTarget(0,previous.Get()),"restore live RT");Ok(start->Apply(),"restore live state");
    ctx->ClearState();ctx->Flush(); // no opened shared handles retained across Reset
}
static void WorkerUpload(ComPtr<IDirect3DDevice9Ex> device) {
    ComPtr<IDirect3DTexture9> texture;
    Ok(device->CreateTexture(16,16,1,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture,nullptr),"legal native worker texture");
    D3DLOCKED_RECT locked{};
    Ok(texture->LockRect(0,&locked,nullptr,D3DLOCK_DISCARD),"legal native worker lock");
    for(unsigned y=0;y<16;++y) std::memset(static_cast<unsigned char*>(locked.pBits)+y*locked.Pitch,0x3f,64);
    Ok(texture->UnlockRect(0),"legal native worker unlock");
}
static void Exercise(HMODULE module,d2vr_backend::Mode mode) {
    ComPtr<IDirect3D9Ex> api;Ok(d2vr_backend::CreateEx(module,D3D_SDK_VERSION,mode,&api),"system factory");
    HWND window=CreateWindowW(L"STATIC",L"D2vr backend fixture",WS_OVERLAPPED,0,0,48,32,nullptr,nullptr,nullptr,nullptr);
    Check(window!=nullptr,"hidden fixture window");
    const auto text=TextSnapshot();
    {
        D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
        pp.BackBufferWidth=32;pp.BackBufferHeight=32;pp.BackBufferFormat=D3DFMT_A8R8G8B8;pp.hDeviceWindow=window;
        pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
        ComPtr<IDirect3DDevice9Ex> dev;Ok(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,&pp,nullptr,&dev),"multithreaded9Ex device");
        ComPtr<IDirect3DDevice9On12> interop;const HRESULT qi=dev.As(&interop);
        Check(mode==d2vr_backend::Mode::System9On12 ? qi==S_OK : qi==E_NOINTERFACE,"requested backend identity");
        if(mode==d2vr_backend::Mode::System9On12) {
            // Inspect while the device is alive: the SYSTEM runtime unloads its
            // backend after the last factory/device reference is released.
            const HMODULE backend=GetModuleHandleW(L"d3d9on12.dll");
            Check(backend!=nullptr,"real translation module loaded");
            wchar_t backendPath[MAX_PATH]{},expected[MAX_PATH]{};
            Check(GetModuleFileNameW(backend,backendPath,MAX_PATH)>0,"translation module path");
            GetSystemDirectoryW(expected,MAX_PATH);wcscat_s(expected,L"\\d3d9on12.dll");
            Check(_wcsicmp(expected,backendPath)==0,"SYSTEM translation module, no drop-in replacement");
        }
        Check(d2vr_backend::DeviceMatches(mode,dev.Get()),"production device admission");
        Check(!d2vr_backend::DeviceMatches(static_cast<d2vr_backend::Mode>(2),dev.Get()),"unknown device backend rejected");
        Check(d2vr_backend::DeviceMatches(d2vr_backend::Mode::System9On12,dev.Get())==(mode==d2vr_backend::Mode::System9On12),"no silent native fallback");
        Check(!d2vr_backend::DeviceMatches(d2vr_backend::Mode::System9On12,nullptr),"missing translation device rejected");
        LUID luid{};Ok(api->GetAdapterLUID(0,&luid),"9Ex adapter LUID");
        if(interop) {ComPtr<ID3D12Device> d;Ok(interop->GetD3D12Device(IID_PPV_ARGS(&d)),"native12 identity");
            const LUID actual=d->GetAdapterLuid();Check(actual.HighPart==luid.HighPart && actual.LowPart==luid.LowPart,"9/12 same adapter");}
        ComPtr<IDXGIFactory1> factory;Ok(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"DXGI factory");
        ComPtr<IDXGIAdapter1> adapter;
        for(UINT n=0;;++n) {ComPtr<IDXGIAdapter1> item;if(FAILED(factory->EnumAdapters1(n,&item)))break;
            DXGI_ADAPTER_DESC1 desc{};Ok(item->GetDesc1(&desc),"adapter desc");
            if(desc.AdapterLuid.HighPart==luid.HighPart && desc.AdapterLuid.LowPart==luid.LowPart){adapter=item;break;}}
        Check(adapter!=nullptr,"native11 same adapter");
        DXGI_ADAPTER_DESC1 actualAdapter{};Ok(adapter->GetDesc1(&actualAdapter),"shared adapter identity");
        std::wprintf(L"GPU adapter: %ls; LUID=%08lX:%08lX vendor=%04X\n",actualAdapter.Description,
            static_cast<unsigned long>(luid.HighPart),luid.LowPart,actualAdapter.VendorId);
        ComPtr<ID3D11Device> dev11;ComPtr<ID3D11DeviceContext> context;
        Ok(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,
            D3D11_SDK_VERSION,&dev11,nullptr,&context),"native11 device");
        d2vr_image_present::Callbacks callbacks{};callbacks.qualify=&Qualifies;callbacks.before=&Before;callbacks.after=&After;callbacks.log=&Log;
        Check(observer.Start(dev.Get(),{true,imageBase,imageSize},callbacks),"actual production Present observer on native backend");
        for(UINT round=0;round<3;++round) {
            ComPtr<IDirect3DSwapChain9> chain;Ok(dev->CreateAdditionalSwapChain(&pp,&chain),"additional native swapchain");
            ComPtr<IDirect3DSwapChain9Ex> ex;Ok(chain.As(&ex),"additional SwapChain9Ex QI");
            ComPtr<IDirect3DDevice9> owner;Ok(chain->GetDevice(&owner),"swapchain owner");
            Check(owner.Get()==static_cast<IDirect3DDevice9*>(dev.Get()),"actual native owner retained");
            const HRESULT present=ex->Present(nullptr,nullptr,nullptr,nullptr,0);
            Check(present==D3D_OK || present==S_PRESENT_OCCLUDED,"real native Present HRESULT");
            Check(!observer.Failed() && observer.recursive.load()==0,"no native Present recursion or observer fault");
            Check(beforeCount==round+1 && afterCount==round+1,"one qualified before/after per native Present");
            Check(observer.entries.load()==observer.forwarded.load() && observer.forwarded.load()==observer.returned.load(),"all native Present entries forwarded/returned exactly once");
            // Preserve the game's existing MULTITHREADED ownership model rather
            // than removing its lock or moving renderer calls to a new worker.
            std::thread nativeWorker(WorkerUpload,dev);
            RenderPair(dev.Get(),dev11.Get(),context.Get(),pp.BackBufferWidth);
            nativeWorker.join();
            owner.Reset();ex.Reset();chain.Reset();
            pp.BackBufferWidth=pp.BackBufferWidth==32?48:32;
            observer.ResetBegin();
            const HRESULT reset=dev->ResetEx(&pp,nullptr);observer.ResetEnd(SUCCEEDED(reset));
            Ok(reset,"ResetEx after releasing capture objects");
        }
    }
        Check(TextSnapshot()==text,"main executable code unchanged after real observer/Reset");
        std::printf("PASS actual production observer entries=%llu forward=%llu returned=%llu ok=%llu statuses=%llu recursion=%llu\n",
            observer.entries.load(),observer.forwarded.load(),observer.returned.load(),observer.ok.load(),observer.statuses.load(),observer.recursive.load());
    DestroyWindow(window);
    std::printf("PASS backend=%d real D3D9Ex/11 sharing, additional-chain QI/Present, reset/recreate cycles\n",static_cast<int>(mode));
}
int main(int argc,char** argv) {
    static_assert(sizeof(void*)==4,"game ABI");
    for(int m=-1;m<3;++m) for(int guard=0;guard<2;++guard) for(int present=0;present<2;++present)
        for(int submit=0;submit<5;++submit) for(int shared=0;shared<2;++shared) {
            const bool expected=m==0 || (m==1 && guard && present && submit==4 && shared);
            Check(d2vr_backend::ProfileAllowed(static_cast<d2vr_backend::Mode>(m),guard!=0,present!=0,submit,shared!=0)==expected,"actual backend admission");
        }
    Check(d2vr_backend::CreateEx(nullptr,D3D_SDK_VERSION,d2vr_backend::Mode::System9On12,nullptr)==E_POINTER,"null factory output rejected");
    IDirect3D9Ex* rawOutput=reinterpret_cast<IDirect3D9Ex*>(1);
    Check(d2vr_backend::CreateEx(GetModuleHandleW(nullptr),D3D_SDK_VERSION,d2vr_backend::Mode::System9On12,&rawOutput)==HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND) && rawOutput==nullptr,"missing export clears output without fallback");
    ComPtr<IDirect3D9Ex> invalid;
    Check(d2vr_backend::CreateEx(nullptr,D3D_SDK_VERSION,d2vr_backend::Mode::System9On12,&invalid)==E_INVALIDARG,"missing module rejected");
    wchar_t path[MAX_PATH]{};Check(GetSystemDirectoryW(path,MAX_PATH)>0,"system path");wcscat_s(path,L"\\d3d9.dll");
    HMODULE module=LoadLibraryW(path);Check(module!=nullptr,"absolute SYSTEM runtime");
    wchar_t loaded[MAX_PATH]{};Check(GetModuleFileNameW(module,loaded,MAX_PATH)>0 && _wcsicmp(path,loaded)==0,"no drop-in runtime");
    Check(d2vr_backend::CreateEx(module,D3D_SDK_VERSION,static_cast<d2vr_backend::Mode>(2),&invalid)==E_INVALIDARG && !invalid,"unknown backend rejected");
    Check(argc==2,"explicit fixture backend");
    const auto mode=std::strcmp(argv[1],"on12")==0 ? d2vr_backend::Mode::System9On12 : d2vr_backend::Mode::Native;
    Exercise(module,mode);
    FreeLibrary(module);
    std::printf("PASS %u API/pixel/admission checks; no Steam/OpenXR or game files\n",checks.load());
    return 0;
}

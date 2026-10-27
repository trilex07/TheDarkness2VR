#define NOMINMAX
#include "../src/query_preload_pool.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned calls;
static bool forceFail=false;
static HRESULT STDMETHODCALLTYPE Original(IDirect3DDevice9* d,D3DQUERYTYPE t,IDirect3DQuery9** o){++calls;if(forceFail)return E_ACCESSDENIED;return d->CreateQuery(t,o);}
static DWORD DrawQuery(IDirect3DDevice9* d,IDirect3DQuery9* q) {
    struct Vertex{float x,y,z,w;DWORD color;};
    const Vertex triangle[]={{0,0,.5f,1,0xffffffff},{63,0,.5f,1,0xffffffff},{0,63,.5f,1,0xffffffff}};
    CHECK(q->GetType()==D3DQUERYTYPE_OCCLUSION && q->GetDataSize()==sizeof(DWORD));
    CHECK(SUCCEEDED(d->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0)));
    CHECK(SUCCEEDED(d->BeginScene()));CHECK(SUCCEEDED(q->Issue(D3DISSUE_BEGIN)));
    CHECK(SUCCEEDED(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE)));
    CHECK(SUCCEEDED(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE)));
    CHECK(SUCCEEDED(d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,triangle,sizeof(Vertex))));
    CHECK(SUCCEEDED(q->Issue(D3DISSUE_END)));CHECK(SUCCEEDED(d->EndScene()));
    DWORD pixels=0;HRESULT hr;const auto start=GetTickCount64();
    while((hr=q->GetData(&pixels,sizeof(pixels),D3DGETDATA_FLUSH))==S_FALSE && GetTickCount64()-start<3000)SwitchToThread();
    printf("query result hr=%08lx pixels=%lu\n",hr,pixels);
    CHECK(hr==S_OK);return pixels;
}
int main(int argc,char**) {
    const bool hardware=argc>1;
    wchar_t path[MAX_PATH];CHECK(GetSystemDirectoryW(path,MAX_PATH));CHECK(!wcscat_s(path,L"\\d3d9.dll"));
    const auto module=LoadLibraryW(path);CHECK(module);
    auto create=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(module,"Direct3DCreate9"));CHECK(create);
    IDirect3D9Ex* apiEx=nullptr;
    IDirect3D9* api=nullptr;
    if(hardware){auto exCreate=reinterpret_cast<HRESULT(WINAPI*)(UINT,IDirect3D9Ex**)>(GetProcAddress(module,"Direct3DCreate9Ex"));CHECK(exCreate && SUCCEEDED(exCreate(D3D_SDK_VERSION,&apiEx)));api=apiEx;}
    else api=create(D3D_SDK_VERSION);
    CHECK(api);
    HWND w=CreateWindowExW(0,L"STATIC",L"query regression",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(w);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=w;pp.BackBufferWidth=pp.BackBufferHeight=64;
    IDirect3DDevice9* device=nullptr;
    IDirect3DDevice9Ex* deviceEx=nullptr;
    if(hardware){CHECK(SUCCEEDED(apiEx->CreateDeviceEx(0,D3DDEVTYPE_HAL,w,0x54,&pp,nullptr,&deviceEx)));device=deviceEx;}
    else CHECK(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_REF,w,D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED,&pp,&device)));
    {
        d2vr_query_preload::Pool pool(device,w,1234,&Original,8);pool.EndReset(device,D3D_OK);
        CHECK(pool.Pending()==8 && calls==8);
        IDirect3DQuery9 *query=nullptr,*native=nullptr;
        CHECK(SUCCEEDED(pool.Create(device,D3DQUERYTYPE_OCCLUSION,&query,1234)) && calls==8);
        CHECK(SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_OCCLUSION,&native)) && query!=native);
        const auto visible=DrawQuery(device,query);CHECK(visible==DrawQuery(device,native));if(hardware)CHECK(visible>0);query->Release();native->Release();
        CHECK(SUCCEEDED(pool.Create(device,D3DQUERYTYPE_EVENT,&query,1234)) && calls==9);query->Release();
        CHECK(SUCCEEDED(pool.Create(device,D3DQUERYTYPE_OCCLUSION,&query,4321)) && calls==10);query->Release();
        const auto probe=device->CreateQuery(D3DQUERYTYPE_OCCLUSION,nullptr);
        CHECK(pool.Create(device,D3DQUERYTYPE_OCCLUSION,nullptr,1234)==probe && calls==11);
        for(unsigned i=0;i<7;++i){CHECK(SUCCEEDED(pool.Create(device,D3DQUERYTYPE_OCCLUSION,&query,1234)));query->Release();}
        CHECK(pool.Pending()==0 && calls==11);
        CHECK(SUCCEEDED(pool.Create(device,D3DQUERYTYPE_OCCLUSION,&query,1234)) && calls==12);query->Release();
        pool.BeginReset(device);pool.EndReset(device,E_FAIL);CHECK(pool.Pending()==0);
        CHECK(SUCCEEDED(pool.Create(device,D3DQUERYTYPE_OCCLUSION,&query,1234)) && calls==13);query->Release();
        CHECK(SUCCEEDED(device->Reset(&pp)));pool.EndReset(device,D3D_OK);CHECK(pool.Pending()==8 && calls==21);
        pool.BeginReset(device);CHECK(pool.Pending()==0);
        CHECK(SUCCEEDED(device->Reset(&pp)));pool.EndReset(device,D3D_OK);
        CHECK(SUCCEEDED(pool.Create(device,D3DQUERYTYPE_OCCLUSION,&query,1234)));
        CHECK(DrawQuery(device,query)==visible);query->Release();pool.BeginReset(device);
        if(hardware){CHECK(SUCCEEDED(deviceEx->ResetEx(&pp,nullptr)));pool.EndReset(device,D3D_OK);CHECK(pool.Pending()==8);pool.BeginReset(device);}
        CHECK(pool.hits==9);
        IDirect3DDevice9* other=nullptr;
        CHECK(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_REF,w,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&other)));
        const auto before=calls;
        CHECK(SUCCEEDED(pool.Create(other,D3DQUERYTYPE_OCCLUSION,&query,1234)) && calls==before+1);
        IDirect3DDevice9* owner=nullptr;CHECK(SUCCEEDED(query->GetDevice(&owner)) && owner==other);
        owner->Release();query->Release();other->Release();
        forceFail=true;
        query=nullptr;CHECK(pool.Create(device,D3DQUERYTYPE_OCCLUSION,&query,1234)==E_ACCESSDENIED && !query);
        pool.EndReset(device,D3D_OK);CHECK(pool.Pending()==0);
        CHECK(pool.Create(device,D3DQUERYTYPE_OCCLUSION,&query,1234)==E_ACCESSDENIED && !query);
        forceFail=false;
    }
    device->Release();api->Release();DestroyWindow(w);FreeLibrary(module);
    printf("PASS x86 D3D9 %s query inventory: query results equal, one-use, type/caller/probe bypass, exhaustion, Reset lifecycle; REF zero does not prove rasterization\n",hardware?"HAL/Ex":"REF");
}

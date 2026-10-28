#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
using ApplyFn=HRESULT(STDMETHODCALLTYPE*)(IDirect3DStateBlock9*);
static ApplyFn g_origSBApply=nullptr;
static void SyncActiveShader(IDirect3DDevice9*){}
#include "actual_snapshot.inl"
int main(int argc,char**) {
    wchar_t path[MAX_PATH];CHECK(GetSystemDirectoryW(path,MAX_PATH));wcscat_s(path,L"\\d3d9.dll");
    HMODULE dll=LoadLibraryW(path);CHECK(dll);
    auto create=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(dll,"Direct3DCreate9"));CHECK(create);
    auto* api=create(D3D_SDK_VERSION);CHECK(api);
    HWND w=CreateWindowExW(0,L"STATIC",L"snapshot test",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(w);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=w;pp.BackBufferWidth=pp.BackBufferHeight=64;
    IDirect3DDevice9* d=nullptr;CHECK(SUCCEEDED(api->CreateDevice(0,argc>1?D3DDEVTYPE_HAL:D3DDEVTYPE_REF,w,D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED,&pp,&d)));
    DeviceSnapshot reusable,legacy,end;
    IDirect3DStateBlock9* first=nullptr;
    for(unsigned i=0;i<1000;++i) {
        D3DVIEWPORT9 vp{0,0,32+i%32,32+i%32,0,1};float values[4]={float(i),2,3,4};
        CHECK(SUCCEEDED(d->SetViewport(&vp)));CHECK(SUCCEEDED(d->SetRenderState(D3DRS_ALPHABLENDENABLE,i%2)));
        CHECK(SUCCEEDED(d->SetVertexShaderConstantF(0,values,1)));
        CHECK(reusable.Capture(d,true));CHECK(legacy.Capture(d));
        if(!i)first=reusable.state;CHECK(reusable.state==first);
        CHECK(SUCCEEDED(d->SetRenderState(D3DRS_ALPHABLENDENABLE,!(i%2))));
        float dirty[4]={-1,-1,-1,-1};CHECK(SUCCEEDED(d->SetVertexShaderConstantF(0,dirty,1)));
        CHECK(end.Capture(d,true));
        reusable.Apply(d);DWORD state;float got[4];D3DVIEWPORT9 actual{};
        CHECK(SUCCEEDED(d->GetRenderState(D3DRS_ALPHABLENDENABLE,&state)) && state==i%2);
        CHECK(SUCCEEDED(d->GetVertexShaderConstantF(0,got,1)) && !memcmp(got,values,sizeof(got)));
        CHECK(SUCCEEDED(d->GetViewport(&actual)) && !memcmp(&actual,&vp,sizeof(vp)));
        legacy.Apply(d);CHECK(SUCCEEDED(d->GetVertexShaderConstantF(0,got,1)) && !memcmp(got,values,sizeof(got)));
        end.Apply(d);CHECK(SUCCEEDED(d->GetVertexShaderConstantF(0,got,1)) && !memcmp(got,dirty,sizeof(got)));
        reusable.Apply(d);CHECK(SUCCEEDED(d->GetVertexShaderConstantF(0,got,1)) && !memcmp(got,values,sizeof(got)));
        end.Release(true);
        reusable.Release(true);legacy.Release();
        CHECK(reusable.state==first && !reusable.targets[0] && !reusable.depth);
    }
    reusable.Release();end.Release();CHECK(!reusable.state && !reusable.owner && !end.state);
    CHECK(SUCCEEDED(d->Reset(&pp)));CHECK(reusable.Capture(d,true));reusable.Release();
    // ABBA order limits warmup/order bias. This measures fixture CPU cost,
    // not game frame time or GPU execution.
    LARGE_INTEGER frequency;CHECK(QueryPerformanceFrequency(&frequency));
    double elapsed[2]{};
    constexpr unsigned iterations=10000;
    for(unsigned round=0;round<8;++round) {
        const bool reuse=(round%4==1 || round%4==2);
        DeviceSnapshot measured;
        CHECK(measured.Capture(d,reuse));measured.Release(reuse);
        LARGE_INTEGER begin,finish;CHECK(QueryPerformanceCounter(&begin));
        for(unsigned i=0;i<iterations;++i) {
            CHECK(measured.Capture(d,reuse));measured.Release(reuse);
        }
        CHECK(QueryPerformanceCounter(&finish));
        elapsed[reuse?1:0]+=double(finish.QuadPart-begin.QuadPart)/double(frequency.QuadPart);
        measured.Release();
    }
    printf("BENCH CPU us/snapshot: recreate=%.3f reuse=%.3f; 40000 iterations each, ABBA; not game FPS\n",
        elapsed[0]*1e6/(4*iterations),elapsed[1]*1e6/(4*iterations));
    d->Release();api->Release();DestroyWindow(w);FreeLibrary(dll);
    puts("PASS actual x86 DeviceSnapshot: 1000 fresh captures, state/constant/viewport equality, same object, released surfaces, Reset recreation");
}

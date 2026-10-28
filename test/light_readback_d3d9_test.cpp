#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <unordered_map>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static std::unordered_map<const void*,const void*> g_surfToTex;
static std::unordered_map<const void*,uint32_t> g_lightEndHash;
static int g_lightRbFrames=-1,g_lightRbBudget=0;
static UINT FakeProfileInt(LPCSTR,LPCSTR,INT,LPCSTR){return 0;}
#define GetPrivateProfileIntA FakeProfileInt
static void Log(const char*,...){}
static uint32_t ReadbackTextureHash(IDirect3DDevice9*,IDirect3DTexture9*){return 7;}
#include "actual_readback.inl"
int main(int argc,char**) {
    wchar_t path[MAX_PATH];CHECK(GetSystemDirectoryW(path,MAX_PATH));wcscat_s(path,L"\\d3d9.dll");
    HMODULE dll=LoadLibraryW(path);CHECK(dll);
    auto create=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(dll,"Direct3DCreate9"));CHECK(create);
    IDirect3D9* api=create(D3D_SDK_VERSION);CHECK(api);
    HWND window=CreateWindowExW(0,L"STATIC",L"readback ownership test",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(window);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow=window;pp.BackBufferWidth=pp.BackBufferHeight=64;
    IDirect3DDevice9* device=nullptr;
    CHECK(SUCCEEDED(api->CreateDevice(0,argc>1?D3DDEVTYPE_HAL:D3DDEVTYPE_REF,window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED,&pp,&device)));
    for(unsigned cycle=0;cycle<20;++cycle) {
        const bool diagnostic=(cycle%2)!=0;
        g_lightRbFrames=diagnostic?1:0;
        IDirect3DTexture9* texture=nullptr;IDirect3DSurface9* surface=nullptr;
        CHECK(SUCCEEDED(device->CreateTexture(64,64,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture,nullptr)));
        CHECK(SUCCEEDED(texture->GetSurfaceLevel(0,&surface)));
        CollectLightReadbackTarget(surface);CollectLightReadbackTarget(surface);
        CHECK(g_surfToTex.size()==(diagnostic?1u:0u));
        surface->Release();texture->Release();
        if(diagnostic) {
            // A real DEFAULT-pool resource is still owned by the diagnostic.
            CHECK(FAILED(device->Reset(&pp)));
            ReleaseLightReadbackTargets();
        }
        CHECK(SUCCEEDED(device->Reset(&pp)));
        CHECK(g_surfToTex.empty());
    }
    ReleaseLightReadbackTargets();device->Release();api->Release();DestroyWindow(window);FreeLibrary(dll);
    puts("PASS actual x86 D3D9: 20 OFF/diagnostic cycles, retained DEFAULT RT blocks classic Reset, cleanup permits Reset");
}

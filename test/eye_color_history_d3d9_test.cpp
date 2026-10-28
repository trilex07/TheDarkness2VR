#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
@IMPLEMENTATION@
static void Fill(IDirect3DDevice9* dev,IDirect3DSurface9* surface,D3DCOLOR color) {
    CHECK(SUCCEEDED(dev->ColorFill(surface,nullptr,color)));
}
static void Pixels(IDirect3DDevice9* dev,IDirect3DSurface9* surface,D3DCOLOR color,
                   bool overlay=false) {
    D3DSURFACE_DESC desc{};CHECK(SUCCEEDED(surface->GetDesc(&desc)));
    IDirect3DSurface9* read=nullptr;
    CHECK(SUCCEEDED(dev->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&read,nullptr)));
    CHECK(SUCCEEDED(dev->GetRenderTargetData(surface,read)));
    D3DLOCKED_RECT lock{};CHECK(SUCCEEDED(read->LockRect(&lock,nullptr,D3DLOCK_READONLY)));
    for(unsigned y=0;y<desc.Height;++y)for(unsigned x=0;x<desc.Width;++x) {
        auto row=(const D3DCOLOR*)((const unsigned char*)lock.pBits+y*lock.Pitch);
        CHECK(row[x]==((overlay && x<8 && y<8)?0xffabcdef:color));
    }
    CHECK(SUCCEEDED(read->UnlockRect()));read->Release();
}
static IDirect3DSurface9* Target(IDirect3DDevice9* dev,unsigned w,unsigned h) {
    IDirect3DSurface9* target=nullptr;
    CHECK(SUCCEEDED(dev->CreateRenderTarget(w,h,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr)));
    return target;
}
int main(int argc,char**) {
    wchar_t path[MAX_PATH];CHECK(GetSystemDirectoryW(path,MAX_PATH));wcscat_s(path,L"\\d3d9.dll");
    HMODULE dll=LoadLibraryW(path);CHECK(dll);
    auto create=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(dll,"Direct3DCreate9"));CHECK(create);
    IDirect3D9* api=create(D3D_SDK_VERSION);CHECK(api);
    HWND window=CreateWindowExW(0,L"STATIC",L"offline history fixture",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(window);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.BackBufferWidth=pp.BackBufferHeight=64;
    IDirect3DDevice9* dev=nullptr;
    CHECK(SUCCEEDED(api->CreateDevice(0,argc>1?D3DDEVTYPE_HAL:D3DDEVTYPE_REF,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&dev)));
    unsigned checked=0;
    for(unsigned size=0;size<3;++size) {
        const unsigned w=size==0?64:size==1?257:2676,h=size==0?48:size==1?129:2728;
        auto target=Target(dev,w,h);
        const unsigned cycles=size==2?2:20;
        for(unsigned i=0;i<cycles;++i) {
            const D3DCOLOR right=0xff001100+i,left=0xff220000+i;
            Fill(dev,target,right);CHECK(FinishRightEyeHistory(dev,target,true,true));
            Fill(dev,target,left);CHECK(SeedRightEyeBackground(dev,target,true));Pixels(dev,target,left);
            CHECK(SeedRightEyeBackground(dev,target,false));Pixels(dev,target,right);
            RECT rect{0,0,8,8};CHECK(SUCCEEDED(dev->ColorFill(target,&rect,0xffabcdef)));
            CHECK(FinishRightEyeHistory(dev,target,true,true));
            Fill(dev,target,left);CHECK(SeedRightEyeBackground(dev,target,false));Pixels(dev,target,right,true);
            // Model an unsuccessful new world replay, then only UI segments.
            Fill(dev,target,left);CHECK(!FinishRightEyeHistory(dev,target,false,true));
            for(unsigned ui=0;ui<3;++ui) {
                CHECK(!SeedRightEyeBackground(dev,target,false));
                CHECK(!FinishRightEyeHistory(dev,target,true,false));
            }
            Pixels(dev,target,left); // rejected history must not overwrite newer live pixels
            CHECK(SeedRightEyeBackground(dev,target,true));
            Fill(dev,target,right);CHECK(FinishRightEyeHistory(dev,target,true,true));
            Fill(dev,target,left);CHECK(SeedRightEyeBackground(dev,target,false));Pixels(dev,target,right);
            ++checked;
        }
        g_rightColorHistory.Release();
        // System-memory sources cannot be StretchRect sources: verify actual
        // driver rejection invalidates history and fresh world replay recovers.
        IDirect3DSurface9* invalid=nullptr;
        CHECK(SUCCEEDED(dev->CreateOffscreenPlainSurface(w,h,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&invalid,nullptr)));
        CHECK(!FinishRightEyeHistory(dev,invalid,true,true));
        CHECK(g_rightColorHistory.rejected && !g_rightColorHistory.target && !g_rightColorHistory.pixels);
        invalid->Release();CHECK(!SeedRightEyeBackground(dev,target,false));
        CHECK(SeedRightEyeBackground(dev,target,true));
        Fill(dev,target,0xff123456);CHECK(FinishRightEyeHistory(dev,target,true,true));
        g_rightColorHistory.Release();target->Release();
        CHECK(SUCCEEDED(dev->Reset(&pp)));
        CHECK(!g_rightColorHistory.rejected);
    }
    dev->Release();api->Release();DestroyWindow(window);FreeLibrary(dll);
    std::printf("PASS actual-source x86 D3D9 %s history: %u cycles; full pixel readback, UI overlay, failed segment rejection/recovery, native2676x2728, real invalid-copy rejection and three Reset cycles; no game/XR/FPS evidence\n",argc>1?"HAL":"REF",checked);
}

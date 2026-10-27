// Hidden-window REF correctness smoke: real system D3D9 API, no GPU performance
// claim, Steam/OpenXR access, proxy loading, device sharing, or game injection.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "../src/render_command_worker.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::printf("FAIL %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
struct Vertex { float x,y,z,rhw; DWORD color; };
struct Reply { HRESULT hr = E_PENDING; DWORD value = 0; };
struct Packet {
    enum Op { Clear, Begin, End, State, ReadState, Draw, Reset } op;
    DWORD a=0,b=0;
    std::vector<Vertex> vertices;
    std::shared_ptr<Reply> reply;
};
static DWORD createdThread=0, destroyedThread=0;
static unsigned draws=0;
static HRESULT initialization=E_PENDING;
struct D3D9Owner {
    HMODULE library=nullptr;
    HWND window=nullptr;
    IDirect3D9* api=nullptr;
    IDirect3DDevice9* device=nullptr;
    D3DPRESENT_PARAMETERS pp{};
    DWORD owner=GetCurrentThreadId();
    D3D9Owner() {
        createdThread=owner;
        wchar_t path[MAX_PATH];
        if (!GetSystemDirectoryW(path,MAX_PATH) || wcscat_s(path,L"\\d3d9.dll")) return;
        library=LoadLibraryW(path); if (!library) return;
        auto create=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(library,"Direct3DCreate9"));
        if (!create || !(api=create(D3D_SDK_VERSION))) return;
        // Built-in window class, hidden and never activated; created/reset/destroyed
        // on the same worker as the D3D9 device, as required by D3D9.
        window=CreateWindowExW(0,L"STATIC",L"D2VR REF worker test",WS_POPUP,
            0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if (!window) return;
        pp.Windowed=TRUE; pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
        pp.BackBufferWidth=64; pp.BackBufferHeight=64;
        pp.BackBufferFormat=D3DFMT_UNKNOWN; pp.hDeviceWindow=window;
        initialization=api->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_REF,window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device);
    }
    ~D3D9Owner() {
        CHECK(GetCurrentThreadId()==owner);
        if(device) device->Release(); if(api) api->Release();
        if(window) DestroyWindow(window); if(library) FreeLibrary(library);
        destroyedThread=GetCurrentThreadId();
    }
    bool Execute(const Packet& p) {
        CHECK(GetCurrentThreadId()==owner);
        if(!device) return false;
        HRESULT hr=E_FAIL;
        switch(p.op) {
        case Packet::Clear: hr=device->Clear(0,nullptr,D3DCLEAR_TARGET,p.a,1,0); break;
        case Packet::Begin: hr=device->BeginScene(); break;
        case Packet::End: hr=device->EndScene(); break;
        case Packet::State: hr=device->SetRenderState(static_cast<D3DRENDERSTATETYPE>(p.a),p.b); break;
        case Packet::ReadState:
            if(p.reply) hr=device->GetRenderState(static_cast<D3DRENDERSTATETYPE>(p.a),&p.reply->value);
            break;
        case Packet::Reset: hr=device->Reset(&pp); break;
        case Packet::Draw:
            hr=device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
            if(SUCCEEDED(hr) && p.vertices.size()==3) {
                hr=device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,p.vertices.data(),sizeof(Vertex));
                if(SUCCEEDED(hr)) ++draws;
            } else hr=D3DERR_INVALIDCALL;
            break;
        }
        if(p.reply) p.reply->hr=hr;
        return SUCCEEDED(hr);
    }
};
int main() {
    static_assert(sizeof(void*)==4,"x86 required");
    using Queue=d2vr_render_worker::Queue<D3D9Owner,Packet,32>;
    Queue q;
    auto first=q.Submit({Packet::Clear,0xff000000});
    const auto init=q.Wait(first.ticket);
    if(!init.complete || init.firstFailure) {
        std::printf("FAIL REF initialization/use hr=0x%08lx\n",initialization); return 1;
    }
    CHECK(createdThread!=GetCurrentThreadId());
    const std::vector<Vertex> triangle={{0,0,0,1,0xffff0000},{32,0,0,1,0xff00ff00},{0,32,0,1,0xff0000ff}};
    for(unsigned i=0;i<1000;++i) {
        CHECK(q.Submit({Packet::Begin}).accepted);
        CHECK(q.Submit({Packet::State,D3DRS_CULLMODE,D3DCULL_NONE}).accepted);
        CHECK(q.Submit({Packet::Draw,0,0,triangle}).accepted);
        CHECK(q.Submit({Packet::End}).accepted);
    }
    auto reply=std::make_shared<Reply>();
    auto get=q.Submit({Packet::ReadState,D3DRS_CULLMODE,0,{},reply});
    auto done=q.Wait(get.ticket);
    CHECK(done.complete && !done.firstFailure);
    CHECK(reply->hr==D3D_OK && reply->value==D3DCULL_NONE);
    auto reset=q.Submit({Packet::Reset});
    CHECK(q.Wait(reset.ticket).complete && !q.Drain().firstFailure);
    CHECK(q.Submit({Packet::Begin}).accepted);
    CHECK(q.Submit({Packet::Draw,0,0,triangle}).accepted);
    CHECK(q.Submit({Packet::End}).accepted);
    q.Stop();
    CHECK(!q.Drain().firstFailure);
    CHECK(draws==1001 && createdThread==destroyedThread);
    std::puts("PASS x86 system D3D9 REF: 1001 draws, ordered getter, Reset, worker-owned lifetime");
    std::puts("NOT a game integration, pixel comparison, GPU benchmark or VR acceptance");
}

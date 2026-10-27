#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <atomic>
#include <string>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <thread>
static bool g_startupShaderPool=true;
static void Log(const char* format,...) {va_list args;va_start(args,format);vprintf(format,args);va_end(args);puts("");}
#include "../src/shader_preload_pool.inl"
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define HR(x) do{HRESULT h=(x);if(FAILED(h)){printf("FAIL %d hr=%08lx: %s\n",__LINE__,h,#x);exit(1);}}while(0)
using namespace d2vr_shader_preload;
struct Fake {std::atomic<unsigned> releases{0};void Release(){++releases;}};
static void Core() {
    int device=1,other=2;Fake a,b,rejected;
    const std::vector<uint32_t> code={0xfffe0300,0x0001fffe,123,65535};
    auto different=code;different[2]=124;
    {
        Pool<Fake> pool(&device);
        CHECK(pool.Remember(&device,code));CHECK(pool.Remember(&device,different));
        CHECK(!pool.Remember(&other,code));CHECK(!pool.Remember(&device,{1,2,3}));
        CHECK(pool.Adopt(&device,code,&a,0));CHECK(!pool.Adopt(&device,code,&rejected,0));
        rejected.Release();CHECK(pool.Adopt(&device,different,&b,0));
        int32_t result=99;
        CHECK(!pool.Take(&other,code,code[0],result));CHECK(!pool.Take(&device,code,0xffff0300,result));
        std::atomic<unsigned> winners{0};std::vector<std::thread> threads;
        for(int i=0;i<16;++i) threads.emplace_back([&]{int32_t hr=99;if(auto* got=pool.Take(&device,code,code[0],hr)){CHECK(got==&a && hr==0);++winners;}});
        for(auto& t:threads)t.join();CHECK(winners==1);
        CHECK(!pool.Take(&device,code,code[0],result));
        uint64_t version=0;const auto archive=pool.Archive(version);
        CHECK(version==2 && d2vr_startup_shader::Validate(reinterpret_cast<const unsigned char*>(archive.data()),archive.size()*4));
        pool.ClearObjects();CHECK(a.releases==0 && b.releases==1);
        a.Release();pool.ExpireOne();CHECK(a.releases==1 && b.releases==1);
    }
    CHECK(a.releases==1 && b.releases==1 && rejected.releases==1);
    Pool<Fake> bounded(&device);
    for(uint32_t i=0;i<MaxShaders;++i){auto unique=code;unique[2]=i;CHECK(bounded.Remember(&device,unique));}
    auto excess=code;excess[2]=uint32_t(MaxShaders);CHECK(!bounded.Remember(&device,excess));
    uint64_t version=0;auto archive=bounded.Archive(version);
    CHECK(ValidateArchive(reinterpret_cast<const unsigned char*>(archive.data()),archive.size()*4));
    archive[1]=uint32_t(MaxShaders+1);CHECK(!ValidateArchive(reinterpret_cast<const unsigned char*>(archive.data()),archive.size()*4));
    puts("PASS one-use ownership, wrong device/stage, exact code, 16-thread contention, cleanup/expiry release boundaries");
}
static std::vector<uint32_t> Compile(const char* source,const char* target) {
    ID3DBlob *code=nullptr,*errors=nullptr;
    HRESULT hr=D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,"main",target,0,0,&code,&errors);
    if(errors){printf("%s\n",static_cast<char*>(errors->GetBufferPointer()));errors->Release();}HR(hr);
    std::vector<uint32_t> result(code->GetBufferSize()/4);memcpy(result.data(),code->GetBufferPointer(),code->GetBufferSize());code->Release();return result;
}
static void Save(const std::wstring& path,const std::vector<uint32_t>& data) {
    HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);CHECK(f!=INVALID_HANDLE_VALUE);
    DWORD written=0;CHECK(WriteFile(f,data.data(),DWORD(data.size()*4),&written,nullptr));CHECK(written==data.size()*4);CloseHandle(f);
}
static void Same(IUnknown* shader,const std::vector<uint32_t>& expected,bool vertex) {
    UINT size=0;
    if(vertex) HR(static_cast<IDirect3DVertexShader9*>(shader)->GetFunction(nullptr,&size));
    else HR(static_cast<IDirect3DPixelShader9*>(shader)->GetFunction(nullptr,&size));
    CHECK(size==expected.size()*4);std::vector<uint32_t> actual(size/4);
    if(vertex) HR(static_cast<IDirect3DVertexShader9*>(shader)->GetFunction(actual.data(),&size));
    else HR(static_cast<IDirect3DPixelShader9*>(shader)->GetFunction(actual.data(),&size));
    CHECK(actual==expected);
}
int main(int argc,char**) {
    const bool hardware=argc>1;
    static_assert(sizeof(void*)==4,"x86 required");Core();
    const auto vs=Compile("float4 main(float4 p:POSITION):POSITION{return p;}","vs_3_0");
    const auto ps=Compile("float4 main():COLOR{return float4(1,0,0,1);}","ps_3_0");
    const auto otherPs=Compile("float4 main():COLOR{return float4(0,1,0,1);}","ps_3_0");
    std::vector<uint32_t> snapshot;CHECK(Snapshot(reinterpret_cast<const DWORD*>(vs.data()),snapshot) && snapshot==vs);
    void* page=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(page && vs.size()*4<4096);
    memcpy(page,vs.data(),vs.size()*4);DWORD old=0;CHECK(VirtualProtect(page,4096,PAGE_READWRITE|PAGE_GUARD,&old));
    CHECK(!Snapshot(static_cast<const DWORD*>(page),snapshot));MEMORY_BASIC_INFORMATION mbi{};
    CHECK(VirtualQuery(page,&mbi,sizeof(mbi)) && (mbi.Protect&PAGE_GUARD));CHECK(VirtualFree(page,0,MEM_RELEASE));
    wchar_t system[MAX_PATH];CHECK(GetSystemDirectoryW(system,MAX_PATH));CHECK(!wcscat_s(system,L"\\d3d9.dll"));
    HMODULE module=LoadLibraryW(system);CHECK(module);
    IDirect3D9* api=nullptr;IDirect3D9Ex* apiEx=nullptr;
    if(hardware) {
        auto create=reinterpret_cast<HRESULT(WINAPI*)(UINT,IDirect3D9Ex**)>(GetProcAddress(module,"Direct3DCreate9Ex"));CHECK(create);
        HR(create(D3D_SDK_VERSION,&apiEx));api=apiEx;
    } else {
        auto create=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(module,"Direct3DCreate9"));CHECK(create);
        api=create(D3D_SDK_VERSION);CHECK(api);
    }
    HWND window=CreateWindowExW(0,L"STATIC",L"D2VR shader ownership fixture",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(window);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.BackBufferWidth=pp.BackBufferHeight=64;
    IDirect3DDevice9* device=nullptr;IDirect3DDevice9Ex* deviceEx=nullptr;
    if(hardware) {HR(apiEx->CreateDeviceEx(0,D3DDEVTYPE_HAL,window,0x54,&pp,nullptr,&deviceEx));device=deviceEx;}
    else HR(api->CreateDevice(0,D3DDEVTYPE_REF,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device));
    wchar_t cwd[MAX_PATH];CHECK(GetCurrentDirectoryW(MAX_PATH,cwd));
    const std::wstring prefix=std::wstring(cwd)+L"\\fixture-"+std::to_wstring(GetCurrentProcessId());
    const auto cache=prefix+L"-learned.bin",seed=prefix+L"-seed.bin";
    Pool<Fake> package(device);CHECK(package.Remember(device,vs));CHECK(package.Remember(device,ps));uint64_t version;
    Save(seed,package.Archive(version));
    auto current=PrepareState(device,cache,seed);state.store(current.get());
    BeginReset(device);HR(device->Reset(&pp));EndReset(device,D3D_OK); // game's initial reset must retain preload hits
    unsigned creates=0;
    auto originalVS=[&](IDirect3DDevice9* d,const DWORD* c,IDirect3DVertexShader9** o){++creates;return d->CreateVertexShader(c,o);};
    auto originalPS=[&](IDirect3DDevice9* d,const DWORD* c,IDirect3DPixelShader9** o){++creates;return d->CreatePixelShader(c,o);};
    IDirect3DVertexShader9 *v1=nullptr,*v2=nullptr;IDirect3DPixelShader9 *p1=nullptr,*p2=nullptr;
    HR(Create(device,reinterpret_cast<const DWORD*>(vs.data()),&v1,originalVS));CHECK(creates==0);
    HR(Create(device,reinterpret_cast<const DWORD*>(ps.data()),&p1,originalPS));CHECK(creates==0);
    Same(v1,vs,true);Same(p1,ps,false);
    HR(Create(device,reinterpret_cast<const DWORD*>(vs.data()),&v2,originalVS));CHECK(creates==1); // cannot reuse consumed pool object
    HR(Create(device,reinterpret_cast<const DWORD*>(otherPs.data()),&p2,originalPS));CHECK(creates==2 && current->pool.Size()==3);
    auto fail=[](IDirect3DDevice9*,const DWORD*,IDirect3DVertexShader9**){return E_ACCESSDENIED;};
    CHECK(Create(device,static_cast<const DWORD*>(nullptr),&v1,fail)==E_ACCESSDENIED);
    current->nextTick=0;Tick();
    const auto deadline=GetTickCount64()+5000;
    while(current->writing.load() && GetTickCount64()<deadline) Sleep(10);
    CHECK(!current->writing && current->saved==3 && current->writeFailures==0);
    CHECK(ReadArchive(cache).size()>8);
    v1->Release();v2->Release();p1->Release();p2->Release();state.store(nullptr);current.reset();
    current=PrepareState(device,cache,seed);state.store(current.get());CHECK(current->pool.Size()==3);
    if(hardware) {BeginReset(device);HR(deviceEx->ResetEx(&pp,nullptr));EndReset(device,D3D_OK);}
    const unsigned before=creates;
    HR(Create(device,reinterpret_cast<const DWORD*>(otherPs.data()),&p2,originalPS));CHECK(creates==before);Same(p2,otherPs,false);
    BeginReset(device);EndReset(device,E_FAIL);
    CHECK(Create(device,reinterpret_cast<const DWORD*>(vs.data()),&v1,fail)==E_ACCESSDENIED);
    BeginReset(device);HR(device->Reset(&pp));EndReset(device,D3D_OK);Same(p2,otherPs,false);
    HR(Create(device,reinterpret_cast<const DWORD*>(ps.data()),&p1,originalPS));CHECK(creates==before); // unused shaders also survive Reset
    p1->Release();p2->Release();state.store(nullptr);current.reset();
    device->Release();api->Release();DestroyWindow(window);FreeLibrary(module);
    CHECK(DeleteFileW(cache.c_str()));CHECK(DeleteFileW(seed.c_str()));
    printf("PASS actual x86 D3D9 %s: startup objects served without creation, effective bytes, native misses/HRESULT, background persistence and second-start hit, Reset ownership, guard preservation\n",hardware?"HAL/Ex flags=0x54":"REF");
    puts("No game/FPS/first-draw performance claim.");
}

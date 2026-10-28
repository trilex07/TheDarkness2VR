// Narrow API-cost experiment, not a game/driver worker or frame-time profiler.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <algorithm>
#define CHECK(x) do{if(!(x)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
int main() {
    wchar_t path[MAX_PATH]{};CHECK(GetSystemDirectoryW(path,MAX_PATH));
    CHECK(!wcscat_s(path,L"\\d3d9.dll"));auto module=LoadLibraryW(path);CHECK(module);
    auto create=reinterpret_cast<HRESULT(WINAPI*)(UINT,IDirect3D9Ex**)>(GetProcAddress(module,"Direct3DCreate9Ex"));CHECK(create);
    IDirect3D9Ex* api=nullptr;CHECK(SUCCEEDED(create(D3D_SDK_VERSION,&api)));
    D3DADAPTER_IDENTIFIER9 adapter{};CHECK(SUCCEEDED(api->GetAdapterIdentifier(0,0,&adapter)));
    printf("adapter=%s driver=%s version=%08lx:%08lx\n",adapter.Description,adapter.Driver,adapter.DriverVersion.HighPart,adapter.DriverVersion.LowPart);
    auto window=CreateWindowW(L"STATIC",L"D2VR isolated D3D9 flag probe",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(window);
    IDirect3DDevice9Ex* devices[2]{};
    for(unsigned i=0;i<2;++i) {
        D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.BackBufferWidth=pp.BackBufferHeight=64;
        CHECK(SUCCEEDED(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,window,0x50|(i?D3DCREATE_MULTITHREADED:0),&pp,nullptr,&devices[i])));
        D3DDEVICE_CREATION_PARAMETERS actual{};CHECK(SUCCEEDED(devices[i]->GetCreationParameters(&actual)) && actual.BehaviorFlags==(0x50u|(i?4u:0u)));
    }
    LARGE_INTEGER frequency{};CHECK(QueryPerformanceFrequency(&frequency));
    double wall[2][8]{},cycles[2][8]{};
    for(unsigned round=0;round<9;++round)for(unsigned turn=0;turn<2;++turn) {
        unsigned which=turn^(round&1);auto* device=devices[which];
        float constants[32]{};LARGE_INTEGER start{},end{};ULONG64 c0=0,c1=0;
        CHECK(QueryThreadCycleTime(GetCurrentThread(),&c0));CHECK(QueryPerformanceCounter(&start));
        for(unsigned call=0;call<100000;++call) {
            constants[0]=float(call&255);constants[4]=float((call>>8)&255);
            CHECK(SUCCEEDED(device->SetVertexShaderConstantF(0,constants,8)));
            CHECK(SUCCEEDED(device->SetPixelShaderConstantF(0,constants,2)));
        }
        CHECK(QueryPerformanceCounter(&end));CHECK(QueryThreadCycleTime(GetCurrentThread(),&c1));
        if(round) {
            wall[which][round-1]=double(end.QuadPart-start.QuadPart)*1000/frequency.QuadPart;
            cycles[which][round-1]=double(c1-c0)/200000;
            printf("sample flags=0x%02x calls=200000 wallMs=%.6f cyclesPerCall=%.3f\n",which?0x54:0x50,wall[which][round-1],cycles[which][round-1]);
        }
    }
    for(unsigned i=0;i<2;++i){std::sort(wall[i],wall[i]+8);std::sort(cycles[i],cycles[i]+8);}
    const double a=(wall[0][3]+wall[0][4])/2,b=(wall[1][3]+wall[1][4])/2;
    printf("RESULT median0x50Ms=%.6f median0x54Ms=%.6f deltaMsPer1000Calls=%.6f cycles0x50=%.3f cycles0x54=%.3f\n",a,b,(b-a)/200,(cycles[0][3]+cycles[0][4])/2,(cycles[1][3]+cycles[1][4])/2);
    puts("No draws/Present/game/OpenXR/Steam. Setter-only API cost, not full engine/driver workload or FPS evidence.");
    for(auto* device:devices)device->Release();api->Release();DestroyWindow(window);FreeLibrary(module);return 0;
}

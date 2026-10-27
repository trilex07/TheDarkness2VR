#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdio>
#include "../../src/startup_shader_warmup.h"
struct DriverOps {
 IDirect3DDevice9* dev; LARGE_INTEGER frequency; double elapsed=0;unsigned created=0;
 uint64_t Now(){return GetTickCount64();}
 bool CreateAndRelease(const unsigned char* code,uint32_t){
  LARGE_INTEGER a,b;QueryPerformanceCounter(&a);HRESULT hr;bool good=false;
  if(d2vr_startup_shader::Word(code)==0xfffe0300u){IDirect3DVertexShader9*s=nullptr;hr=dev->CreateVertexShader(reinterpret_cast<const DWORD*>(code),&s);good=SUCCEEDED(hr)&&s;if(s)s->Release();}
  else{IDirect3DPixelShader9*s=nullptr;hr=dev->CreatePixelShader(reinterpret_cast<const DWORD*>(code),&s);good=SUCCEEDED(hr)&&s;if(s)s->Release();}
  QueryPerformanceCounter(&b);elapsed+=1000.*(b.QuadPart-a.QuadPart)/frequency.QuadPart;if(good)++created;return good;
 }
};
int main(int argc,char**argv){
 if(argc!=2)return 2;
 std::ifstream f(argv[1],std::ios::binary|std::ios::ate);if(!f)return 3;auto n=f.tellg();if(n<8||n>d2vr_startup_shader::MaxBytes)return 4;
 std::vector<DWORD> data((size_t(n)+3)/4);f.seekg(0);if(!f.read(reinterpret_cast<char*>(data.data()),n))return 5;
 auto bytes=reinterpret_cast<const unsigned char*>(data.data());if(!d2vr_startup_shader::Validate(bytes,size_t(n)))return 6;
 wchar_t system[MAX_PATH]{};GetSystemDirectoryW(system,MAX_PATH);wcscat_s(system,L"\\d3d9.dll");HMODULE dll=LoadLibraryW(system);if(!dll)return 7;
 using Factory=HRESULT(WINAPI*)(UINT,IDirect3D9Ex**);auto factory=reinterpret_cast<Factory>(GetProcAddress(dll,"Direct3DCreate9Ex"));if(!factory)return 8;
 IDirect3D9Ex*d3d=nullptr;if(FAILED(factory(D3D_SDK_VERSION,&d3d)))return 9;
 HWND wnd=CreateWindowExW(0,L"STATIC",L"D2VR shader fixture",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!wnd){d3d->Release();return 10;}
 D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=wnd;pp.BackBufferWidth=64;pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_UNKNOWN;
 IDirect3DDevice9Ex*dev=nullptr;HRESULT hr=d3d->CreateDeviceEx(0,D3DDEVTYPE_HAL,wnd,D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&pp,nullptr,&dev);
 if(FAILED(hr)){std::printf("CreateDeviceEx failed %08lx\n",hr);DestroyWindow(wnd);d3d->Release();return 11;}
 LARGE_INTEGER freq;QueryPerformanceFrequency(&freq);bool ok=true;
 for(int pass=0;pass<3;++pass){DriverOps ops{dev,freq};auto r=d2vr_startup_shader::Run(bytes,size_t(n),10000,ops);std::printf("pass=%d valid=%d created=%u failed=%u budgetStop=%d create_release_ms=%.6f\n",pass,int(r.valid),ops.created,r.failed,int(r.timedOut),ops.elapsed);ok=ok&&r.valid&&!r.failed&&!r.timedOut;}
 dev->Release();DestroyWindow(wnd);d3d->Release();FreeLibrary(dll);
 std::puts("First pass is NOT cold-cache proof; this fixture is NOT gameplay FPS or pixel validation.");return ok?0:12;
}

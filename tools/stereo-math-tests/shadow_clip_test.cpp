#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <fstream>
#include <cstdio>
#include "../../src/shadow_clip_coordinates.h"
static void Check(bool ok,const char* name) {if(!ok){std::fprintf(stderr,"FAIL %s\n",name);std::exit(1);}}
int main(int argc,char** argv) {
    Check(argc==2,"private shader required");
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);
    Check(bool(file),"input open");const auto size=size_t(file.tellg());
    Check(size==568*4,"input size");std::vector<uint32_t> code(size/4),out;
    file.seekg(0);file.read(reinterpret_cast<char*>(code.data()),size);
    Check(bool(file),"input read");
    Check(d2vr_shadow_clip::Build(code.data(),code.size(),out),"complete bytecode admission");
    Check(out.size()==code.size(),"length unchanged");
    for(size_t i=0;i<code.size();++i)if(i!=234 && i!=237)Check(out[i]==code[i],"unrelated words preserved");
    auto bad=code;bad[200]^=1;std::vector<uint32_t> rejected;
    Check(!d2vr_shadow_clip::Build(bad.data(),bad.size(),rejected)&&rejected.empty(),"unknown bytecode rejected");
    Check(!d2vr_shadow_clip::Build(code.data(),code.size()-1,rejected),"truncation rejected");
    auto d3d=Direct3DCreate9(D3D_SDK_VERSION);Check(d3d!=nullptr,"system D3D9");
    const auto window=CreateWindowA("STATIC","D2VR shader validation",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
    IDirect3DDevice9* dev=nullptr;
    Check(window!=nullptr,"test window");
    const HRESULT deviceResult=d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&dev);
    if(FAILED(deviceResult))std::fprintf(stderr,"CreateDevice HRESULT=%08lX\n",static_cast<unsigned long>(deviceResult));
    Check(SUCCEEDED(deviceResult),"driver device");
    IDirect3DPixelShader9* shader=nullptr;
    Check(SUCCEEDED(dev->CreatePixelShader(reinterpret_cast<const DWORD*>(out.data()),&shader)),"driver accepts replacement");
    std::ofstream saved("shadow-clip-candidate.bin",std::ios::binary);saved.write(reinterpret_cast<char*>(out.data()),out.size()*4);saved.close();
    shader->Release();dev->Release();d3d->Release();DestroyWindow(window);
    std::puts("PASS exact shader mutation, unknown/truncated rejection and D3D9 driver validation");
}

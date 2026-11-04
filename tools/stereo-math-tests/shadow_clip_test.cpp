#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#pragma comment(lib,"d3dcompiler.lib")
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
    Check(out.size()==code.size()+8,"runtime selector instructions added");
    for(size_t i=0;i<234;++i)Check(out[i]==code[i],"prefix preserved");
    for(size_t i=238;i<code.size();++i)Check(out[i+8]==code[i],"suffix preserved");
    Check(out[234]==0x02000001u && out[236]==0xa00000dfu && out[245]==0x8000000fu,"runtime selector and temporary register");
    std::vector<uint32_t> rejected;
    for(size_t i=0;i<code.size();++i){auto bad=code;bad[i]^=1;
        Check(!d2vr_shadow_clip::Build(bad.data(),bad.size(),rejected)&&rejected.empty(),"every single-word mutation rejected");}
    Check(!d2vr_shadow_clip::Build(code.data(),code.size()-1,rejected),"truncation rejected");
    IDirect3D9Ex* d3d=nullptr;Check(SUCCEEDED(Direct3DCreate9Ex(D3D_SDK_VERSION,&d3d)),"system D3D9Ex");
    const auto window=CreateWindowA("STATIC","D2VR shader validation",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
    pp.BackBufferWidth=pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_A8R8G8B8;
    IDirect3DDevice9Ex* dev=nullptr;
    Check(window!=nullptr,"test window");
    const HRESULT deviceResult=d3d->CreateDeviceEx(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,nullptr,&dev);
    if(FAILED(deviceResult))std::fprintf(stderr,"CreateDevice HRESULT=%08lX\n",static_cast<unsigned long>(deviceResult));
    Check(SUCCEEDED(deviceResult),"driver device");
    IDirect3DPixelShader9* shader=nullptr;
    Check(SUCCEEDED(dev->CreatePixelShader(reinterpret_cast<const DWORD*>(code.data()),&shader)),"driver accepts native");
    shader->Release();shader=nullptr;
    ID3DBlob* assembly=nullptr;
    Check(SUCCEEDED(D3DDisassemble(out.data(),out.size()*4,0,nullptr,&assembly)),"replacement disassembles");
    std::ofstream("shadow-clip-scoped.asm.txt")<<static_cast<const char*>(assembly->GetBufferPointer());assembly->Release();
    const auto shaderResult=dev->CreatePixelShader(reinterpret_cast<const DWORD*>(out.data()),&shader);
    std::printf("Replacement HRESULT=%08lX\n",static_cast<unsigned long>(shaderResult));
    Check(SUCCEEDED(shaderResult),"driver accepts replacement");
    shader->Release();shader=nullptr;
    // Execute the exact native/patched reconstruction prefix on the real GPU.
    // Only replace the later shadow taps with an RG observation of r0.xy.
    // This checks native selector=0 equivalence, independently of scene content.
    IDirect3DPixelShader9* prefix[2]{};
    for(unsigned mode=0;mode<2;++mode){
        const auto& source=mode?out:code;const size_t end=mode?246:238;
        std::vector<uint32_t> observe(source.begin(),source.begin()+end);
        observe.insert(observe.end(),{0x02000001u,0x80030800u,0x80e40000u,0x0000ffffu});
        Check(SUCCEEDED(dev->CreatePixelShader(reinterpret_cast<const DWORD*>(observe.data()),&prefix[mode])),"prefix GPU observation");
    }
    IDirect3DTexture9* depth=nullptr;
    Check(SUCCEEDED(dev->CreateTexture(64,64,1,D3DUSAGE_DYNAMIC,D3DFMT_R32F,D3DPOOL_DEFAULT,&depth,nullptr)),"depth fixture");
    D3DLOCKED_RECT lock{};Check(SUCCEEDED(depth->LockRect(0,&lock,nullptr,D3DLOCK_DISCARD)),"depth lock");
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)
        reinterpret_cast<float*>(static_cast<char*>(lock.pBits)+y*lock.Pitch)[x]=.5f;
    depth->UnlockRect(0);dev->SetTexture(0,depth);
    dev->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);dev->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
    dev->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
    dev->SetRenderState(D3DRS_ZENABLE,FALSE);dev->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    dev->SetRenderState(D3DRS_LIGHTING,FALSE);dev->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE,3);
    dev->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1|D3DFVF_TEXCOORDSIZE4(0));
    struct Vertex {float x,y,z,w,u,v,s,t;};
    const Vertex quad[]={{-.5f,-.5f,.5f,1,0,0,0,1},{63.5f,-.5f,.5f,1,1,0,0,1},
                         {-.5f,63.5f,.5f,1,0,1,0,1},{63.5f,63.5f,.5f,1,1,1,0,1}};
    IDirect3DSurface9* target=nullptr,*read=nullptr;
    Check(SUCCEEDED(dev->GetRenderTarget(0,&target)),"fixture target");
    Check(SUCCEEDED(dev->CreateOffscreenPlainSurface(64,64,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&read,nullptr)),"fixture readback");
    std::vector<unsigned char> pixels[3];
    for(unsigned mode=0;mode<3;++mode){
        const float select[4]={mode==2?1.f:0.f,0,0,0};
        Check(SUCCEEDED(dev->SetPixelShaderConstantF(223,select,1)),"selector upload");
        dev->SetPixelShader(prefix[mode?1:0]);
        Check(SUCCEEDED(dev->BeginScene()),"begin fixture");
        const auto draw=dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex));dev->EndScene();
        Check(SUCCEEDED(draw),"prefix draw");
        Check(SUCCEEDED(dev->GetRenderTargetData(target,read)),"prefix readback");
        Check(SUCCEEDED(read->LockRect(&lock,nullptr,D3DLOCK_READONLY)),"prefix lock");
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){
            const auto p=static_cast<const unsigned char*>(lock.pBits)+y*lock.Pitch+x*4;
            pixels[mode].push_back(p[1]);pixels[mode].push_back(p[2]);
        }read->UnlockRect();
    }
    Check(pixels[0]==pixels[1],"native selector GPU pixels are bit exact");
    size_t changed=0;for(size_t i=0;i<pixels[0].size();++i)changed+=pixels[0][i]!=pixels[2][i];
    Check(changed>1000,"corrected selector produces distinct NDC pixels");
    std::printf("PASS GPU native selector bit exact; corrected selector changed %zu observed channels\n",changed);
    read->Release();target->Release();depth->Release();prefix[0]->Release();prefix[1]->Release();
    std::ofstream saved("shadow-clip-candidate.bin",std::ios::binary);saved.write(reinterpret_cast<char*>(out.data()),out.size()*4);saved.close();
    dev->Release();d3d->Release();DestroyWindow(window);
    std::puts("PASS exact shader mutation, unknown/truncated rejection and D3D9 driver validation");
}

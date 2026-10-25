#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include "../../src/composite_shader_patch.h"

static unsigned checks;
static void Check(bool ok, const char* what) {
    ++checks;
    if (!ok) { std::fprintf(stderr,"FAIL: %s\n",what); std::exit(1); }
}
static const DWORD kDof[] = {0x04000012,0x80270003,0x80550000,0x80e40001,0x80e40002};
static const DWORD kFog[] = {0x04000012,0x80270001,0x80000000,0xa0e40005,0x80e40003};

// Original test shader, not a game asset. Frame/DOF textures -> two blends ->
// gain/offset -> output. The two blend instructions use the production operands.
static std::vector<DWORD> Fixture() {
    return {0xffff0300,
        0x0200001f,0x80000005,0x900f0000, // dcl_texcoord v0
        0x0200001f,0x90000000,0xa00f0800, // dcl_2d s0
        0x0200001f,0x90000000,0xa00f0803, // dcl_2d s3
        0x02000001,0x800f0000,0xa0e40000, // mov r0,c0 (fog,DOF,alpha,unused)
        0x03000042,0x800f0002,0x90e40000,0xa0e40800, // texld r2,v0,s0
        0x03000042,0x800f0001,0x90e40000,0xa0e40803, // texld r1,v0,s3
        0x04000012,0x80270003,0x80550000,0x80e40001,0x80e40002,
        0x04000012,0x80270001,0x80000000,0xa0e40005,0x80e40003,
        0x03000005,0x80070001,0x80e40001,0xa0e40007, // mul r1.xyz,r1,c7
        0x03000002,0x80070001,0x80e40001,0xa0e40008, // add r1.xyz,r1,c8
        0x02000001,0x80070800,0x80e40001, // mov oC0.xyz,r1
        0x02000001,0x80080800,0xa0aa0000, // mov oC0.w,c0.z
        0x0000ffff};
}

static void StructuralTests() {
    using namespace d2vr_composite;
    auto fixture=Fixture();
    const size_t fog=FindUniqueInstruction(fixture.data(),fixture.size(),kFog);
    Check(fog<fixture.size(),"blend located at an instruction boundary");
    std::vector<DWORD> result;
    Check(Build(fixture.data(),fixture.size(),true,true,result)==None && result.empty(),
        "unknown shader is never accepted by production hash gate");
    auto duplicate=fixture;duplicate.insert(duplicate.end()-1,kFog,kFog+5);
    Check(FindUniqueInstruction(duplicate.data(),duplicate.size(),kFog)==duplicate.size(),"duplicate rejected");
    const std::vector<DWORD> comment={0xffff0300,0x0005fffe,kFog[0],kFog[1],kFog[2],kFog[3],kFog[4],0x0000ffff};
    Check(FindUniqueInstruction(comment.data(),comment.size(),kFog)==comment.size(),"comment/CTAB match ignored");
    auto bad=fixture;bad.pop_back();
    Check(FindUniqueInstruction(bad.data(),bad.size(),kFog)==bad.size(),"missing END rejected");
    bad=fixture;bad.resize(fog+3);
    Check(FindUniqueInstruction(bad.data(),bad.size(),kFog)==bad.size(),"truncated operands rejected");
    bad=fixture;bad[fog+3]^=1;
    Check(FindUniqueInstruction(bad.data(),bad.size(),kFog)==bad.size(),"different fog source rejected");
    auto patched=fixture;BypassBlend(patched,fog);
    for(size_t i=0;i<fixture.size();++i)
        if(i<fog || i>fog+4 || i==fog+1) Check(patched[i]==fixture[i],"other instructions and mask unchanged");
}

static void CheckPrivateShader(IDirect3DDevice9* dev,const char* path) {
    using namespace d2vr_composite;
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    Check(bool(file),"private shader opened");
    const size_t bytes=static_cast<size_t>(file.tellg());
    Check(bytes%4==0 && bytes<=32768,"bounded private shader size");
    std::vector<DWORD> original(bytes/4);file.seekg(0);file.read(reinterpret_cast<char*>(original.data()),bytes);
    Check(bool(file),"private shader read");
    const auto saved=original;
    const uint32_t hash=Hash(original.data(),original.size());
    Check(hash==0x33DE98C1 || hash==0x75F88E11,"original complete shader identity");
    for(unsigned flags=0;flags<4;++flags) {
        std::vector<DWORD> patched;
        const unsigned expected=(flags&Dof)|((hash==0x33DE98C1)?(flags&HeightFog):0);
        Check(Build(original.data(),original.size(),(flags&Dof)!=0,(flags&HeightFog)!=0,patched)==expected,"independent production options");
        Check(original==saved,"original bytecode immutable");
        if(!expected) {Check(patched.empty(),"disabled/unsupported option produces no replacement");continue;}
        if(hash==0x33DE98C1 && flags==Dof)
            Check(Hash(patched.data(),patched.size())==0xA69BDC80,"fog OFF matches captured historical DOF shader exactly");
        Check(patched.size()==original.size(),"CTAB/token offsets preserved");
        const size_t dof=FindUniqueInstruction(original.data(),original.size(),kDof);
        const size_t fog=FindUniqueInstruction(original.data(),original.size(),kFog);
        for(size_t i=0;i<original.size();++i) {
            const bool edited=((expected&Dof) && i>=dof && i<dof+5 && i!=dof+1) ||
                              ((expected&HeightFog) && i>=fog && i<fog+5 && i!=fog+1);
            if(!edited) Check(patched[i]==original[i],"only requested blend tokens change");
        }
        IDirect3DPixelShader9* shader=nullptr;
        Check(SUCCEEDED(dev->CreatePixelShader(patched.data(),&shader)),"actual game candidate validated by D3D9 driver");
        shader->Release();
        std::printf("private shader %08X options=%u -> %08X, driver accepted\n",hash,flags,Hash(patched.data(),patched.size()));
    }
    original[original.size()-2]^=1;
    std::vector<DWORD> rejected;
    Check(Build(original.data(),original.size(),true,true,rejected)==None && rejected.empty(),"mutated whole-shader identity rejected");
}

static void PixelTests(IDirect3DDevice9* dev) {
    constexpr UINT W=8,H=4;
    IDirect3DTexture9 *frame=nullptr,*dof=nullptr;
    for(auto out:{&frame,&dof})
        Check(SUCCEEDED(dev->CreateTexture(2,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,out,nullptr)),"test texture");
    const DWORD colors[2]={0xff204060,0xffc08040};
    D3DLOCKED_RECT lock{};Check(SUCCEEDED(frame->LockRect(0,&lock,nullptr,0)),"frame upload");
    std::memcpy(lock.pBits,colors,sizeof(colors));frame->UnlockRect(0);
    Check(SUCCEEDED(dof->LockRect(0,&lock,nullptr,0)),"DOF upload");
    static_cast<DWORD*>(lock.pBits)[0]=static_cast<DWORD*>(lock.pBits)[1]=0xff808080;dof->UnlockRect(0);
    dev->SetTexture(0,frame);dev->SetTexture(3,dof);
    for(DWORD s:{0u,3u}) {
        dev->SetSamplerState(s,D3DSAMP_MINFILTER,D3DTEXF_POINT);dev->SetSamplerState(s,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
        dev->SetSamplerState(s,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);dev->SetSamplerState(s,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
        dev->SetSamplerState(s,D3DSAMP_SRGBTEXTURE,0);
    }
    dev->SetRenderState(D3DRS_ZENABLE,FALSE);dev->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);dev->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);
    const float blend[4]={.8f,.6f,1,0},fog[4]={.315762132f,.20836167f,.0511220694f,0};
    const float gain[4]={.5f,.5f,.5f,1},offset[4]={.1f,.1f,.1f,0};
    dev->SetPixelShaderConstantF(0,blend,1);dev->SetPixelShaderConstantF(5,fog,1);
    dev->SetPixelShaderConstantF(7,gain,1);dev->SetPixelShaderConstantF(8,offset,1);
    struct Vertex {float x,y,z,w,u,v;};
    const Vertex quad[]={{-.5f,-.5f,0,1,0,0},{W-.5f,-.5f,0,1,1,0},{-.5f,H-.5f,0,1,0,1},{W-.5f,H-.5f,0,1,1,1}};
    dev->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);
    IDirect3DSurface9 *target=nullptr,*readback=nullptr;
    Check(SUCCEEDED(dev->GetRenderTarget(0,&target)),"test RT");
    Check(SUCCEEDED(dev->CreateOffscreenPlainSurface(W,H,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr)),"readback surface");
    DWORD baselineContrast=0,clearContrast=0;
    for(unsigned flags=0;flags<4;++flags) {
        auto shader=Fixture();
        if(flags&d2vr_composite::Dof) d2vr_composite::BypassBlend(shader,d2vr_composite::FindUniqueInstruction(shader.data(),shader.size(),kDof));
        if(flags&d2vr_composite::HeightFog) d2vr_composite::BypassBlend(shader,d2vr_composite::FindUniqueInstruction(shader.data(),shader.size(),kFog));
        IDirect3DPixelShader9* ps=nullptr;
        Check(SUCCEEDED(dev->CreatePixelShader(shader.data(),&ps)),"fixture variant validated");
        Check(SUCCEEDED(dev->SetPixelShader(ps)),"fixture variant bound");
        for(int eye=0;eye<2;++eye) {
            Check(SUCCEEDED(dev->BeginScene()),"begin fixture");
            Check(SUCCEEDED(dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex))),"draw fixture");
            Check(SUCCEEDED(dev->EndScene()),"end fixture");
            Check(SUCCEEDED(dev->GetRenderTargetData(target,readback)),"fixture GPU readback");
            Check(SUCCEEDED(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY)),"fixture pixels");
            const auto* pixels=reinterpret_cast<const DWORD*>(static_cast<const char*>(lock.pBits)+lock.Pitch);
            for(int side=0;side<2;++side) {
                const DWORD pixel=pixels[side?6:1];
                for(int c=0;c<3;++c) {
                    const int shift=16-c*8;
                    float expected=float((colors[side]>>shift)&255)/255;
                    if(!(flags&d2vr_composite::Dof)) expected=expected*.4f+(128.f/255)*.6f;
                    if(!(flags&d2vr_composite::HeightFog)) expected=expected*.2f+fog[c]*.8f;
                    expected=expected*.5f+.1f;
                    Check(std::abs(int((pixel>>shift)&255)-int(std::lround(expected*255)))<=2,
                        "GPU pixels preserve independent blends and downstream color work");
                }
                Check((pixel>>24)==255,"output alpha retained");
            }
            const DWORD contrast=((pixels[6]>>16)&255)-((pixels[1]>>16)&255);
            if(flags==d2vr_composite::Dof) baselineContrast=contrast;
            if(flags==(d2vr_composite::Dof|d2vr_composite::HeightFog)) clearContrast=contrast;
            readback->UnlockRect();
        }
        dev->SetPixelShader(nullptr);ps->Release();
    }
    Check(clearContrast>baselineContrast*3,"removing fog restores contrast without sharpening or changing scene samples");
    std::printf("GPU fixture red contrast: fog=%lu, bypass=%lu (synthetic inputs, not a game capture)\n",baselineContrast,clearContrast);
    readback->Release();target->Release();dev->SetTexture(0,nullptr);dev->SetTexture(3,nullptr);frame->Release();dof->Release();
}

int main(int argc,char** argv) {
    StructuralTests();
    HWND window=CreateWindowExA(0,"STATIC","D2VR composite test",WS_POPUP,0,0,8,4,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
    Check(window!=nullptr,"hidden window");
    IDirect3D9* d3d=Direct3DCreate9(D3D_SDK_VERSION);Check(d3d!=nullptr,"D3D9 runtime");
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.hDeviceWindow=window;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth=8;pp.BackBufferHeight=4;pp.BackBufferFormat=D3DFMT_A8R8G8B8;
    IDirect3DDevice9* dev=nullptr;
    Check(SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&dev)),"D3D9 HAL device");
    PixelTests(dev);
    for(int i=1;i<argc;++i) CheckPrivateShader(dev,argv[i]);
    dev->Release();d3d->Release();DestroyWindow(window);
    std::printf("PASS: %u checks; real D3D9 GPU fixture; %d private game shaders; no Steam/OpenXR\n",checks,argc-1);
}

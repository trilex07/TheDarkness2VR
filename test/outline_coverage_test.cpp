#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <cmath>
#include "../src/outline_coverage.h"
using Microsoft::WRL::ComPtr;
#define CHECK(x) do{if(!(x)){printf("FAIL line %d %s\n",__LINE__,#x);return 1;}}while(0)
#define HR(x) do{HRESULT h=(x);if(FAILED(h)){printf("FAIL line %d hr=%08lx\n",__LINE__,h);return 1;}}while(0)
int main(int argc,char**argv) {
    CHECK(argc==2);std::ifstream file(argv[1],std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    CHECK(bytes.size()==379*4);std::vector<DWORD> code(379),replacement;
    memcpy(code.data(),bytes.data(),bytes.size());
    CHECK(d2vr_outline_coverage::Build(code.data(),code.size(),replacement));
    for(size_t i=0;i<code.size();++i){auto bad=code;bad[i]^=1;std::vector<DWORD> out={123};
        CHECK(!d2vr_outline_coverage::Build(bad.data(),bad.size(),out)&&out.empty());}
    std::vector<DWORD> out;
    CHECK(!d2vr_outline_coverage::Build<DWORD>(nullptr,379,out));
    CHECK(!d2vr_outline_coverage::Build(code.data(),378,out));
    ComPtr<ID3DBlob> text;HR(D3DDisassemble(replacement.data(),replacement.size()*4,0,nullptr,&text));
    std::ofstream("outline-patched.asm.txt")<<static_cast<const char*>(text->GetBufferPointer());
    char system[MAX_PATH];CHECK(GetSystemDirectoryA(system,MAX_PATH));strcat_s(system,"\\d3d9.dll");
    auto lib=LoadLibraryA(system);CHECK(lib);
    auto create=reinterpret_cast<HRESULT(WINAPI*)(UINT,IDirect3D9Ex**)>(GetProcAddress(lib,"Direct3DCreate9Ex"));CHECK(create);
    ComPtr<IDirect3D9Ex> api;HR(create(D3D_SDK_VERSION,&api));
    constexpr unsigned W=128,N=512;
    HWND win=CreateWindowA("STATIC","Outline coverage regression",WS_OVERLAPPEDWINDOW,0,0,W,W,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);CHECK(win);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=win;
    pp.BackBufferWidth=pp.BackBufferHeight=W;pp.BackBufferFormat=D3DFMT_A8R8G8B8;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    ComPtr<IDirect3DDevice9Ex> d;HR(api->CreateDeviceEx(0,D3DDEVTYPE_HAL,win,D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&pp,nullptr,&d));
    ComPtr<IDirect3DPixelShader9> ps[2];HR(d->CreatePixelShader(code.data(),&ps[0]));HR(d->CreatePixelShader(replacement.data(),&ps[1]));
    const char* vs="float4 s:register(c0);struct O{float4 p:POSITION;float2 uv:TEXCOORD0;float4 a:TEXCOORD1;float4 b:TEXCOORD2;float3 k:TEXCOORD3;float3 j:TEXCOORD4;};O main(float4 p:POSITION,float2 uv:TEXCOORD0){O o;o.p=p;o.uv=uv+s.xy;o.a=o.uv.xyxy+float4(-s.z,0,s.z,0);o.b=o.uv.xyxy+float4(0,-s.w,0,s.w);o.k=float3(0,8,0);o.j=0;return o;}";
    ComPtr<ID3DBlob> vb,err;HRESULT compiled=D3DCompile(vs,strlen(vs),nullptr,nullptr,nullptr,"main","vs_3_0",0,0,&vb,&err);
    if(err)printf("%s\n",static_cast<char*>(err->GetBufferPointer()));HR(compiled);
    ComPtr<IDirect3DVertexShader9> vertex;HR(d->CreateVertexShader(static_cast<DWORD*>(vb->GetBufferPointer()),&vertex));HR(d->SetVertexShader(vertex.Get()));
    D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT4,0,D3DDECLUSAGE_POSITION,0},{0,16,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
    ComPtr<IDirect3DVertexDeclaration9> decl;HR(d->CreateVertexDeclaration(elements,&decl));HR(d->SetVertexDeclaration(decl.Get()));
    struct V{float x,y,z,w,u,v;};const float hp=1.f/W;
    V q[]={{-1-hp,1+hp,0,1,0,0},{1-hp,1+hp,0,1,1,0},{-1-hp,-1+hp,0,1,0,1},{1-hp,-1+hp,0,1,1,1}};
    HR(d->SetRenderState(D3DRS_ZENABLE,FALSE));HR(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));HR(d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE));
    ComPtr<IDirect3DTexture9> depth,normal;HR(d->CreateTexture(N,N,1,D3DUSAGE_DYNAMIC,D3DFMT_R32F,D3DPOOL_DEFAULT,&depth,nullptr));
    HR(d->CreateTexture(N,N,1,D3DUSAGE_DYNAMIC,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&normal,nullptr));
    D3DLOCKED_RECT lock{};HR(depth->LockRect(0,&lock,nullptr,D3DLOCK_DISCARD));
    for(unsigned y=0;y<N;++y)for(unsigned x=0;x<N;++x)reinterpret_cast<float*>(static_cast<char*>(lock.pBits)+y*lock.Pitch)[x]=.7f;
    HR(depth->UnlockRect(0));HR(d->SetTexture(0,depth.Get()));HR(d->SetTexture(1,normal.Get()));
    for(unsigned s=0;s<2;++s)for(auto state:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER})HR(d->SetSamplerState(s,state,D3DTEXF_POINT));
    float constants[4*7]={};constants[12]=.1f;constants[13]=.2f;constants[14]=.3f;constants[15]=1;constants[16]=1;constants[17]=1;constants[19]=1;
    HR(d->SetPixelShaderConstantF(0,constants,7));
    ComPtr<IDirect3DSurface9> rt,read;HR(d->GetRenderTarget(0,&rt));HR(d->CreateOffscreenPlainSurface(W,W,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&read,nullptr));
    double variation[2]={},energy[2]={};std::vector<unsigned char> previous[2];
    for(unsigned flat=0;flat<2;++flat) {
        HR(normal->LockRect(0,&lock,nullptr,D3DLOCK_DISCARD));
        for(unsigned y=0;y<N;++y)for(unsigned x=0;x<N;++x) {
            const bool side=!flat && ((x+unsigned(.71*y))/11)%2;
            reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+y*lock.Pitch)[x]=side?0xffff8080u:0xff8080ffu;
        }
        HR(normal->UnlockRect(0));
        for(unsigned frame=0;frame<48;++frame)for(unsigned mode=0;mode<2;++mode) {
            HR(d->SetSamplerState(1,D3DSAMP_MINFILTER,D3DTEXF_POINT));
            HR(d->SetSamplerState(1,D3DSAMP_MAGFILTER,D3DTEXF_POINT));
            float shift[4]={float(frame)/48.f/W,float(frame)*.37f/48.f/W,1.f/W,1.f/W};HR(d->SetVertexShaderConstantF(0,shift,1));
            HR(d->SetPixelShader(ps[mode].Get()));HR(d->BeginScene());HR(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(V)));HR(d->EndScene());
            HR(d->GetRenderTargetData(rt.Get(),read.Get()));HR(read->LockRect(&lock,nullptr,D3DLOCK_READONLY));
            std::vector<unsigned char> pixels(W*W);
            for(unsigned y=0;y<W;++y)for(unsigned x=0;x<W;++x) {
                const DWORD c=reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+y*lock.Pitch)[x];
                CHECK((c&0x00ffffffu)==0x001a334du || (c&0x00ffffffu)==0x0019334cu || (c&0x00ffffffu)==0x0019334du);
                const unsigned a=c>>24;pixels[y*W+x]=static_cast<unsigned char>(a);
                if(flat)CHECK(a==0);
                else {energy[mode]+=a;if(frame)variation[mode]+=abs(int(a)-int(previous[mode][y*W+x]));}
            }
            HR(read->UnlockRect());previous[mode]=std::move(pixels);
        }
    }
    printf("Native variation %.0f energy %.0f; coverage variation %.0f energy %.0f; ratio %.4f\n",variation[0],energy[0],variation[1],energy[1],variation[1]/variation[0]);
    CHECK(variation[0]>0);CHECK(variation[1]<variation[0]*.8);CHECK(energy[1]>energy[0]*.8 && energy[1]<energy[0]*1.2);
    ComPtr<IDirect3DSurface9> full;
    HR(d->CreateRenderTarget(3345,3410,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&full,nullptr));
    HR(d->SetRenderTarget(0,full.Get()));
    D3DVIEWPORT9 viewport{0,0,3345,3410,0,1};HR(d->SetViewport(&viewport));
    ComPtr<IDirect3DQuery9> frequency,start,finish,disjoint;
    HR(d->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ,&frequency));
    HR(d->CreateQuery(D3DQUERYTYPE_TIMESTAMP,&start));HR(d->CreateQuery(D3DQUERYTYPE_TIMESTAMP,&finish));
    HR(d->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT,&disjoint));
    for(unsigned mode=0;mode<2;++mode) {
        HR(d->SetPixelShader(ps[mode].Get()));HR(disjoint->Issue(D3DISSUE_BEGIN));
        HR(frequency->Issue(D3DISSUE_END));HR(start->Issue(D3DISSUE_END));
        for(unsigned frame=0;frame<24;++frame){HR(d->BeginScene());HR(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(V)));HR(d->EndScene());}
        HR(finish->Issue(D3DISSUE_END));HR(disjoint->Issue(D3DISSUE_END));
        UINT64 hz=0,a=0,b=0;BOOL invalid=TRUE;const auto deadline=GetTickCount64()+10000;
        while(finish->GetData(&b,sizeof(b),D3DGETDATA_FLUSH)==S_FALSE){CHECK(GetTickCount64()<deadline);Sleep(1);}
        HR(frequency->GetData(&hz,sizeof(hz),0));HR(start->GetData(&a,sizeof(a),0));HR(disjoint->GetData(&invalid,sizeof(invalid),0));
        CHECK(!invalid && hz && b>=a);printf("GPU 3345x3410 mode %u %.4f ms/pass (24 draws, fixture only)\n",mode,1000.*double(b-a)/double(hz)/24.);
    }
    DestroyWindow(win);puts("PASS exact 379-word admission, all single-word mutations rejected, driver, flat field and moving native outline coverage");return 0;
}

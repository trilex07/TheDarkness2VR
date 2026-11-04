#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iterator>
#include <algorithm>
#include "../src/specular_footprint.h"
using Microsoft::WRL::ComPtr;
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define HR(x) do{const HRESULT checkResult=(x);if(FAILED(checkResult)){printf("FAIL line %d hr=%08lx: %s\n",__LINE__,checkResult,#x);exit(1);}}while(0)
constexpr unsigned W=128,N=512,F=32;
static ComPtr<ID3DBlob> Compile(const char* s,const char* target){
    ComPtr<ID3DBlob>b,e;HRESULT h=D3DCompile(s,strlen(s),nullptr,nullptr,nullptr,"main",target,0,0,&b,&e);
    if(e)printf("%s\n",static_cast<const char*>(e->GetBufferPointer()));HR(h);return b;
}
static ComPtr<IDirect3DTexture9> Texture(IDirect3DDevice9Ex* d,unsigned kind,bool flat=false){
    ComPtr<IDirect3DTexture9> t,stage;HR(d->CreateTexture(N,N,0,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&t,nullptr));
    HR(d->CreateTexture(N,N,0,0,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&stage,nullptr));
    for(unsigned mip=0;mip<stage->GetLevelCount();++mip){
        const unsigned n=std::max(1u,N>>mip);D3DLOCKED_RECT r{};HR(stage->LockRect(mip,&r,nullptr,0));
        for(unsigned y=0;y<n;++y)for(unsigned x=0;x<n;++x){
            DWORD c=kind==0?0xffffffff:0xff000000;
            if(kind==2){
                // Average native XY encoding over each mip footprint. Native
                // reconstruction discards the resulting normal-length variance.
                double nx=0,ny=0;unsigned samples=0,step=std::max(1u,(1u<<mip)/16);
                for(unsigned yy=0;yy<(1u<<mip);yy+=step)for(unsigned xx=0;xx<(1u<<mip);xx+=step){
                    nx+=flat?0:.22*sin(6.283185307179586*(double(x*(1u<<mip)+xx)+.5)/7.0);
                    ny+=flat?0:.17*cos(6.283185307179586*(double(y*(1u<<mip)+yy)+.5)/9.0);++samples;
                }
                auto byte=[](double v){return unsigned(std::lround(std::max(0.,std::min(255.,127.5*(v+1)))));};
                c=(byte(nx/samples)<<24)|0x00ff0000|(byte(ny/samples)<<8);
            }
            if(kind==3)c=((x/16+y/16)&1)?0xffe0e0e0:0xff202020;
            reinterpret_cast<DWORD*>(static_cast<char*>(r.pBits)+y*r.Pitch)[x]=c;
        }
        HR(stage->UnlockRect(mip));
    }
    HR(d->UpdateTexture(stage.Get(),t.Get()));return t;
}
int main(int argc,char**argv){
    CHECK(argc==2);std::ifstream in(argv[1],std::ios::binary);CHECK(in.good());
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)),{});CHECK(bytes.size()==425*4);
    std::vector<DWORD> source(425);memcpy(source.data(),bytes.data(),bytes.size());std::vector<DWORD> patched;
    CHECK(d2vr_specular_footprint::Build(source.data(),source.size(),patched));CHECK(patched.size()==678);
    for(size_t i=0;i<source.size();++i){auto changed=source;changed[i]^=1;std::vector<DWORD> rejected={123};
        CHECK(!d2vr_specular_footprint::Build(changed.data(),changed.size(),rejected)&&rejected.empty());}
    std::vector<DWORD> rejected;CHECK(!d2vr_specular_footprint::Build<DWORD>(nullptr,425,rejected));
    CHECK(!d2vr_specular_footprint::Build(source.data(),424,rejected));
    ComPtr<ID3DBlob> asmText;HR(D3DDisassemble(patched.data(),patched.size()*4,0,nullptr,&asmText));
    char system[MAX_PATH];CHECK(GetSystemDirectoryA(system,MAX_PATH));strcat_s(system,"\\d3d9.dll");
    HMODULE lib=LoadLibraryA(system);CHECK(lib);auto create=reinterpret_cast<HRESULT(WINAPI*)(UINT,IDirect3D9Ex**)>(GetProcAddress(lib,"Direct3DCreate9Ex"));CHECK(create);
    ComPtr<IDirect3D9Ex> api;HR(create(D3D_SDK_VERSION,&api));
    HWND win=CreateWindowA("STATIC","Specular footprint GPU fixture",WS_OVERLAPPEDWINDOW,0,0,W,W,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);CHECK(win);
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=win;
    pp.BackBufferWidth=pp.BackBufferHeight=W;pp.BackBufferFormat=D3DFMT_A8R8G8B8;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    ComPtr<IDirect3DDevice9Ex>d;HR(api->CreateDeviceEx(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,win,D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&pp,nullptr,&d));
    D3DADAPTER_IDENTIFIER9 adapter{};HR(api->GetAdapterIdentifier(0,0,&adapter));printf("GPU %s\n",adapter.Description);
    ComPtr<IDirect3DPixelShader9> ps[2];HR(d->CreatePixelShader(source.data(),&ps[0]));HR(d->CreatePixelShader(patched.data(),&ps[1]));
    const char* vs="float4 shift:register(c0); struct O{float4 p:POSITION;float4 c:COLOR0;float2 uv:TEXCOORD0;float3 view:TEXCOORD1;float4 sh:TEXCOORD2;float3 light:TEXCOORD3;};O main(float4 p:POSITION,float2 uv:TEXCOORD0){O o;o.p=p;o.c=1;o.uv=uv+shift.xy;o.view=float3(0,0,1);o.sh=float4(uv,0,1);o.light=float3(0,0,.5);return o;}";
    auto vsCode=Compile(vs,"vs_3_0");ComPtr<IDirect3DVertexShader9> vertex;HR(d->CreateVertexShader(static_cast<DWORD*>(vsCode->GetBufferPointer()),&vertex));HR(d->SetVertexShader(vertex.Get()));
    D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},{0,16,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
    ComPtr<IDirect3DVertexDeclaration9> decl;HR(d->CreateVertexDeclaration(elements,&decl));HR(d->SetVertexDeclaration(decl.Get()));
    struct V{float x,y,z,w,u,v;};const float halfPixel=1.f/W;
    V quad[]={{-1-halfPixel,1+halfPixel,0,1,0,0},{1-halfPixel,1+halfPixel,0,1,1,0},{-1-halfPixel,-1+halfPixel,0,1,0,1},{1-halfPixel,-1+halfPixel,0,1,1,1}};
    HR(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));HR(d->SetRenderState(D3DRS_ZENABLE,FALSE));HR(d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE));
    ComPtr<IDirect3DSurface9> rt,read;HR(d->GetRenderTarget(0,&rt));HR(d->CreateOffscreenPlainSurface(W,W,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&read,nullptr));
    auto white=Texture(d.Get(),0),black=Texture(d.Get(),1),normal=Texture(d.Get(),2),flat=Texture(d.Get(),2,true),checker=Texture(d.Get(),3);
    IDirect3DTexture9* textures[]={white.Get(),black.Get(),normal.Get(),white.Get(),black.Get(),white.Get(),black.Get()};
    for(unsigned s=0;s<7;++s){HR(d->SetTexture(s,textures[s]));HR(d->SetSamplerState(s,D3DSAMP_MINFILTER,D3DTEXF_ANISOTROPIC));HR(d->SetSamplerState(s,D3DSAMP_MAGFILTER,D3DTEXF_ANISOTROPIC));HR(d->SetSamplerState(s,D3DSAMP_MAXANISOTROPY,16));HR(d->SetSamplerState(s,D3DSAMP_MIPFILTER,D3DTEXF_POINT));}
    float c[8][4]{};c[1][0]=c[1][1]=c[1][2]=1;c[4][0]=128;c[5][0]=1;HR(d->SetPixelShaderConstantF(0,c[0],8));
    auto render=[&](unsigned shader,float phase){
        float shift[]={phase/N,phase*.37f/N,0,0};HR(d->SetVertexShaderConstantF(0,shift,1));HR(d->SetPixelShader(ps[shader].Get()));
        HR(d->BeginScene());HR(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(V)));HR(d->EndScene());
        HR(d->GetRenderTargetData(rt.Get(),read.Get()));D3DLOCKED_RECT lock{};HR(read->LockRect(&lock,nullptr,D3DLOCK_READONLY));
        std::vector<unsigned char> pixels(W*W);for(unsigned y=0;y<W;++y)for(unsigned x=0;x<W;++x)
            pixels[y*W+x]=static_cast<unsigned char>((reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+y*lock.Pitch)[x]>>16)&255);
        HR(read->UnlockRect());return pixels;
    };
    HR(d->SetTexture(2,flat.Get()));for(float gloss:{1.f,16.f,128.f,512.f}){
        c[4][0]=gloss;HR(d->SetPixelShaderConstantF(4,c[4],1));const auto a=render(0,0),b=render(1,0);
        unsigned error=0;for(unsigned i=0;i<a.size();++i)error=std::max(error,unsigned(abs(int(a[i])-b[i])));
        printf("flat gloss %.0f max byte error %u\n",gloss,error);CHECK(error<=1);
    }
    // Diffuse edges must not be filtered, even with rapidly varying normals.
    HR(d->SetTexture(2,normal.Get()));HR(d->SetTexture(1,checker.Get()));HR(d->SetTexture(4,white.Get()));
    c[5][0]=0;HR(d->SetPixelShaderConstantF(5,c[5],1));const auto da=render(0,0),db=render(1,0);CHECK(da==db);
    HR(d->SetTexture(1,black.Get()));HR(d->SetTexture(4,black.Get()));c[4][0]=128;c[5][0]=1;HR(d->SetPixelShaderConstantF(4,c[4],2));
    double means[2]{},variances[2]{};
    for(unsigned shader=0;shader<2;++shader){std::vector<double> sum(W*W),square(W*W);
        for(unsigned f=0;f<F;++f){const auto p=render(shader,float(f)*.25f);for(unsigned i=0;i<p.size();++i){double v=p[i]/255.;sum[i]+=v;square[i]+=v*v;}}
        for(unsigned i=0;i<sum.size();++i){const double m=sum[i]/F;means[shader]+=m;variances[shader]+=std::max(0.,square[i]/F-m*m);}
        means[shader]/=W*W;variances[shader]/=W*W;
    }
    const double cv0=sqrt(variances[0])/means[0],cv1=sqrt(variances[1])/means[1];
    printf("Temporal material fixture: original mean=%.6f std=%.6f CV=%.6f; filtered mean=%.6f std=%.6f CV=%.6f\n",means[0],sqrt(variances[0]),cv0,means[1],sqrt(variances[1]),cv1);
    CHECK(means[0]>.001 && means[1]>means[0]*.15 && cv1<cv0*.8);
    // Hardware cost gate at the user's unchanged eye dimensions. Deliberate
    // full-screen material overdraw is a stress fixture, not measured gameplay.
    constexpr UINT BW=2676,BH=2728;
    ComPtr<IDirect3DSurface9> large;HR(d->CreateRenderTarget(BW,BH,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&large,nullptr));
    HR(d->SetRenderTarget(0,large.Get()));D3DVIEWPORT9 vp={0,0,BW,BH,0,1};HR(d->SetViewport(&vp));
    ComPtr<IDirect3DQuery9> begin,end,frequency,disjoint;
    HR(d->CreateQuery(D3DQUERYTYPE_TIMESTAMP,&begin));HR(d->CreateQuery(D3DQUERYTYPE_TIMESTAMP,&end));
    HR(d->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT,&disjoint));HR(disjoint->Issue(D3DISSUE_BEGIN));
    HR(d->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ,&frequency));HR(frequency->Issue(D3DISSUE_END));
    auto wait=[&](IDirect3DQuery9* query,void* data,DWORD bytesCount){
        const ULONGLONG start=GetTickCount64();HRESULT result=query->GetData(data,bytesCount,D3DGETDATA_FLUSH);
        while(result==S_FALSE && GetTickCount64()-start<3000){SwitchToThread();result=query->GetData(data,bytesCount,0);}
        HR(result);CHECK(result==S_OK);
    };
    UINT64 hz=0;wait(frequency.Get(),&hz,sizeof(hz));CHECK(hz>0);std::vector<double> gpu[2];
    for(unsigned f=0;f<32;++f)for(unsigned pass=0;pass<2;++pass){
        unsigned shader=(f+pass)&1;HR(d->SetPixelShader(ps[shader].Get()));
        HR(d->BeginScene());HR(begin->Issue(D3DISSUE_END));
        HR(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(V)));HR(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(V)));
        HR(end->Issue(D3DISSUE_END));HR(d->EndScene());UINT64 b=0,e=0;wait(end.Get(),&e,sizeof(e));wait(begin.Get(),&b,sizeof(b));CHECK(e>=b);
        if(f>=4)gpu[shader].push_back(double(e-b)*1000/double(hz));
    }
    HR(disjoint->Issue(D3DISSUE_END));BOOL timingInvalid=TRUE;wait(disjoint.Get(),&timingInvalid,sizeof(timingInvalid));CHECK(!timingInvalid);
    for(unsigned shader=0;shader<2;++shader){std::sort(gpu[shader].begin(),gpu[shader].end());
        printf("GPU full-screen skin stress 2676x2728 twice shader=%u median=%.4fms p95=%.4fms (not FPS)\n",shader,gpu[shader][gpu[shader].size()/2],gpu[shader][gpu[shader].size()*95/100]);}
    printf("PASS actual native PS: digest/mutations, D3D9Ex creation, flat material, unchanged diffuse edges, 64 motion images; NOT a game/HMD shimmer or FPS result\n");
    d->SetTexture(0,nullptr);DestroyWindow(win);return 0;
}

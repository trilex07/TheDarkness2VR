#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstdlib>
#include "../src/submission_status_shader.h"
#define CHECK(x) do{if(!(x)){printf("FAIL line %d\n",__LINE__);exit(1);}}while(0)
ID3DBlob* Compile(const char* text,const char* target){
 ID3DBlob* code=nullptr,*error=nullptr;
 HRESULT hr=D3DCompile(text,strlen(text),nullptr,nullptr,nullptr,"main",target,0,0,&code,&error);
 if(error){printf("%s",(char*)error->GetBufferPointer());error->Release();}
 CHECK(SUCCEEDED(hr));return code;
}
int main(){
 static_assert(sizeof(void*)==4,"x86");
 // Extracted exact production pixel shader, including the label helper.
 #include "actual_submission_ps.inl"
 const char* vs="struct V{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};V main(uint id:SV_VertexID){V o;o.uv=float2((id<<1)&2,id&2);o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}";
 auto* pixel=Compile(psSrc,"ps_4_0");auto* vertex=Compile(vs,"vs_4_0");
 ID3D11Device* dev=nullptr;ID3D11DeviceContext* ctx=nullptr;
 CHECK(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&dev,nullptr,&ctx)));
 ID3D11PixelShader* ps=nullptr;ID3D11VertexShader* shader=nullptr;
 CHECK(SUCCEEDED(dev->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&ps)));
 CHECK(SUCCEEDED(dev->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&shader)));
 pixel->Release();vertex->Release();
 D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=512;td.MipLevels=td.ArraySize=1;
 td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_RENDER_TARGET;
 ID3D11Texture2D* rt=nullptr,*read=nullptr;
 CHECK(SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&rt)));
 ID3D11RenderTargetView* view=nullptr;CHECK(SUCCEEDED(dev->CreateRenderTargetView(rt,nullptr,&view)));
 td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 CHECK(SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&read)));
 D3D11_BUFFER_DESC bd{};bd.ByteWidth=112;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
 ID3D11Buffer* cb=nullptr;CHECK(SUCCEEDED(dev->CreateBuffer(&bd,nullptr,&cb)));
 D3D11_VIEWPORT vp{0,0,512,512,0,1};ctx->RSSetViewports(1,&vp);
 ctx->OMSetRenderTargets(1,&view,nullptr);ctx->VSSetShader(shader,nullptr,0);
 ctx->PSSetShader(ps,nullptr,0);ctx->PSSetConstantBuffers(0,1,&cb);
 ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 const char* digits[8][5]={
  {"00100","01100","00100","00100","01110"},
  {"11110","00001","00110","01000","11111"},
  {"11110","00001","00110","00001","11110"},
  {"10010","10010","11111","00010","00010"},
  {"11111","10000","11110","00001","11110"},
  {"01111","10000","11110","10001","01110"},
  {"11111","00001","00010","00100","00100"},
  {"01110","10001","01110","10001","01110"}};
 for(unsigned mode=0;mode<3;++mode)for(unsigned cull=0;cull<3;++cull)
 for(unsigned reason=1;reason<=(mode==2 && cull==1?8u:1u);++reason){
  float constants[28]{};constants[24]=float(mode);constants[25]=float(cull);constants[26]=float(reason);
  ctx->UpdateSubresource(cb,0,nullptr,constants,0,0);
  float black[4]{};ctx->ClearRenderTargetView(view,black);ctx->Draw(3,0);
  ctx->CopyResource(read,rt);D3D11_MAPPED_SUBRESOURCE map{};
  CHECK(SUCCEEDED(ctx->Map(read,0,D3D11_MAP_READ,0,&map)));
  unsigned green=0,orange=0,cullGreen=0,cullOrange=0;
  for(unsigned y=0;y<512;++y)for(unsigned x=0;x<512;++x){
   auto* p=(unsigned char*)map.pData+y*map.RowPitch+x*4;
   if(p[1]>220 && p[0]<100){if(y>=150)++cullGreen;else ++green;}
   if(p[0]>220 && p[1]>100 && p[1]<180){if(y>=150)++cullOrange;else ++orange;}
  }
  if(mode==2 && cull==1)for(unsigned row=0;row<5;++row)for(unsigned col=0;col<5;++col) {
   const unsigned x=unsigned((.37f+(54+col+.5f)*.006f)*512);
   const unsigned y=unsigned((.23f+(8+row+.5f)*.008f)*512);
   auto* p=(unsigned char*)map.pData+y*map.RowPitch+x*4;
   CHECK((p[0]>220 && p[1]>100 && p[1]<180)==(digits[reason-1][row][col]=='1'));
  }
  ctx->Unmap(read,0);
  CHECK(mode==0?(green==0 && orange==0):mode==1?(orange>100 && green==0):(green>100 && orange==0));
  CHECK(mode==0 || cull==0 ? (cullGreen==0 && cullOrange==0) : cull==1 ? (cullOrange>100 && cullGreen==0) : (cullGreen>100 && cullOrange==0));
  printf("PASS actual PS WARP status CPU=%u CULL=%u green=%u orange=%u cullGreen=%u cullOrange=%u\n",mode,cull,green,orange,cullGreen,cullOrange);
 }
 ctx->ClearState();cb->Release();view->Release();rt->Release();read->Release();
 ps->Release();shader->Release();ctx->Release();dev->Release();
 puts("No gameplay/FPS/HMD claim.");
}

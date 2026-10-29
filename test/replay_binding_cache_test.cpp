#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "replay_state_cache.h"
#include <array>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#define CHECK(x) do{if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
@ENUM@
struct Rec {unsigned op=0,n=0,a[6]{};};
using Binding=d2vr_replay_state::Bindings;
struct Value {uintptr_t a=0,b=0,c=0;bool operator==(const Value& o)const{return a==o.a && b==o.b && c==o.c;}};
using State=std::array<Value,57>;
static State state;
static std::vector<State> observed;
static unsigned calls=0,metadata=0,resolves=0,failAt=0;
static bool bufferValid=true;
static HRESULT Set(int key,uintptr_t a=0,uintptr_t b=0,uintptr_t c=0){
 ++calls;if(calls==failAt)return D3DERR_INVALIDCALL;
 if(key>=0 && key<57)state[key]={a,b,c};return S_OK;
}
struct Original {
 HRESULT SetTexture(IDirect3DDevice9*,UINT slot,IDirect3DBaseTexture9* p){return Set(Binding::Texture(slot),uintptr_t(p));}
 HRESULT SetVertexDeclaration(IDirect3DDevice9*,IDirect3DVertexDeclaration9* p){return Set(Binding::Declaration,uintptr_t(p));}
 HRESULT SetFVF(IDirect3DDevice9*,DWORD f){return Set(Binding::Declaration,0xf000+f);}
 HRESULT SetVertexShader(IDirect3DDevice9*,IDirect3DVertexShader9* p){return Set(Binding::VertexShader,uintptr_t(p));}
 HRESULT SetPixelShader(IDirect3DDevice9*,IDirect3DPixelShader9* p){return Set(Binding::PixelShader,uintptr_t(p));}
 HRESULT SetStreamSource(IDirect3DDevice9*,UINT slot,IDirect3DVertexBuffer9* p,UINT off,UINT stride){return Set(Binding::Stream(slot),uintptr_t(p),off,stride);}
 HRESULT SetStreamSourceFreq(IDirect3DDevice9*,UINT slot,UINT freq){return Set(Binding::Frequency(slot),freq);}
 HRESULT SetIndices(IDirect3DDevice9*,IDirect3DIndexBuffer9* p){return Set(Binding::Indices,uintptr_t(p));}
 HRESULT SetRenderState(IDirect3DDevice9*,D3DRENDERSTATETYPE,DWORD){return Set(-1);}
} g_orig;
static bool IsConstantRenderStateBinding(unsigned,unsigned){return true;}
static void SetActiveVertexShaderMetadata(IDirect3DVertexShader9*){++metadata;}
static void SetActivePixelShaderMetadata(const void*){++metadata;}
static const void* g_replayVs=nullptr;static const void* g_replayPs=nullptr;
static IUnknown* ReplayBuffer(IUnknown* p,bool& complete){++resolves;if(!bufferValid){complete=false;return nullptr;}return p;}
static bool ReplayBufferWrite(const Rec&,const uint8_t*){bufferValid=false;return false;}
static bool ReplayInheritedBuffers(IDirect3DDevice9*){return true;}
static bool SyncActiveShader(IDirect3DDevice9*){metadata+=2;return true;}
static void InvalidateShadowAtlasPass(){}
struct History {void ReplayStateBlock(){}};
static History g_vsConstantHistory,g_psEyeHistory;
static HRESULT Apply(IDirect3DStateBlock9*){++calls;for(auto& v:state)v={0xeeee,0,0};return S_OK;}
static auto g_origSBApply=&Apply;
#define HOOK_MARK(x) ((void)0)
struct UiClipState{};
static UiClipState BeginScaledUiClip(IDirect3DDevice9*){return {};}
static void EndScaledUiClip(IDirect3DDevice9*,UiClipState){}
static HRESULT DrawPrimitiveUPWithDiffuse(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,const void*,UINT){state[Binding::Stream(0)]={};return S_OK;}
static HRESULT DrawIndexedPrimitiveUPWithDiffuse(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT,UINT,const void*,D3DFORMAT,const void*,UINT){state[Binding::Stream(0)]={};state[Binding::Indices]={};return S_OK;}
struct HarnessBindings {
 Binding cache;bool enabled;
 void Clear(){cache.Clear();}void Invalidate(int key){cache.Invalidate(key);}
 template<class F>long Forward(int key,uintptr_t a,uintptr_t b,uintptr_t c,F&& f){return enabled?cache.Forward(key,a,b,c,f):f();}
};
static bool Actual(const std::vector<Rec>& records,bool enabled){
 state={};observed.clear();calls=metadata=resolves=0;bufferValid=true;
 d2vr_replay_state::Cache stateCache;HarnessBindings bindings{{},enabled};
 IDirect3DDevice9* dev=nullptr;const void* replayTex[16]{};
 const uint8_t arena[512]{};const uint8_t* dataBase=arena;bool replayComplete=true;
 for(const auto& r:records)switch((RecOp)r.op){
@CASES@
 case OP_DrawPrimitive:observed.push_back(state);break;
 default:CHECK(false);
 }
 return replayComplete;
}
static Rec R(unsigned op,unsigned a=0,unsigned b=0,unsigned c=0,unsigned d=0){Rec r;r.op=op;r.a[0]=a;r.a[1]=b;r.a[2]=c;r.a[3]=d;return r;}
int main(){
 std::vector<Rec> records;
 for(unsigned i=0;i<4000;++i){
  if(i%37==0)records.push_back(R(OP_ApplyStateBlock,1));
  if(i%19==0)records.push_back(R(OP_SetFVF,0x112));
  if(i%23==0)records.push_back(R(OP_DrawPrimitiveUP));
  if(i%29==0)records.push_back(R(OP_DrawIndexedPrimitiveUP));
  records.push_back(R(OP_SetTexture,0,100+i%2));
  for(unsigned slot=1;slot<8;++slot)records.push_back(R(OP_SetTexture,slot,100+slot));
  records.push_back(R(OP_SetTexture,257,300));
  records.push_back(R(OP_SetVertexShader,200));records.push_back(R(OP_SetPixelShader,201));
  records.push_back(R(OP_SetVertexDeclaration,202));
  records.push_back(R(OP_SetStreamSource,0,203,(i/11)%4*64,32));
  records.push_back(R(OP_SetStreamSource,1,204,0,16));
  records.push_back(R(OP_SetStreamSourceFreq,1,0x80000001));records.push_back(R(OP_SetIndices,205));
  records.push_back(R(OP_DrawPrimitive));
 }
 CHECK(Actual(records,false));const auto expected=observed;const auto before=calls,beforeMetadata=metadata;
 CHECK(Actual(records,true));CHECK(observed==expected);const auto after=calls;
 CHECK(after<before/4 && metadata<beforeMetadata/4);
 printf("PASS actual replay: 4000 draw states identical, %u -> %u binding calls, %u -> %u shader metadata lookups; FVF, UP/DIPUP, SB, instancing, offsets, vertex textures\n",before,after,beforeMetadata,metadata);
 // Failure remains sticky; the same binding is retried after a rejected write.
 const auto repeat=std::vector<Rec>{R(OP_SetVertexShader,1),R(OP_SetVertexShader,1)};
 failAt=1;CHECK(!Actual(repeat,true) && calls==2 && metadata==1);failAt=0;
 CHECK(Actual(repeat,true) && calls==1);CHECK(Actual(repeat,true) && calls==1); // replay reset
 CHECK(Actual({R(OP_SetTexture,999,1),R(OP_SetTexture,999,1)},true) && calls==2);
 CHECK(Actual({R(OP_SetIndices,0),R(OP_SetIndices,0)},true) && calls==1);
 CHECK(!Actual({R(OP_SetIndices,1),R(OP_BufferWrite),R(OP_SetIndices,1)},true) && resolves==2);
 // Independent stream offsets/strides/frequencies must never alias.
 CHECK(Actual({R(OP_SetStreamSource,0,1,0,16),R(OP_SetStreamSource,0,1,4,16),R(OP_SetStreamSource,0,1,4,32),R(OP_SetStreamSource,1,1,4,32)},true) && calls==4);
 for(bool enabled:{false,true}){
  const auto begin=std::chrono::steady_clock::now();unsigned total=0;
  for(unsigned i=0;i<80;++i){CHECK(Actual(records,enabled));total+=calls;}
  printf("BENCH CPU mock replay %s %.3f ms/frame; %u calls/frame; real driver/GPU excluded\n",enabled?"filtered":"baseline",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/80,total/80);
 }
 puts("PASS actual binding failures, null/unsupported slots, repeated replay reset and buffer validation before filter");
}

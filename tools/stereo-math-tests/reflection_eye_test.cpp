#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <initializer_list>
#include <algorithm>
#include "../../src/material_eye_allowlist.h"
#include "../../src/reflection_eye.h"
#include "../../src/shader_register_history.h"
using HRESULT=long;
#define FAILED(x) ((x)<0)
#define SUCCEEDED(x) ((x)>=0)
static void Check(bool ok,const char* name){if(!ok){std::fprintf(stderr,"FAIL %s\n",name);std::exit(1);}}
struct IDirect3DDevice9 {
    float values[36]{};int gets=0,sets=0;bool failGet=false,failFirstSet=false;
    HRESULT GetVertexShaderConstantF(unsigned start,float* out,unsigned count){
        ++gets;if(failGet)return -1;std::memcpy(out,values+4*start,16*count);return 0;
    }
};
static HRESULT Set(IDirect3DDevice9* dev,unsigned start,const float* data,unsigned count){
    ++dev->sets;std::memcpy(dev->values+4*start,data,16*count);
    return dev->failFirstSet && dev->sets==1?-1:0;
}
static struct {decltype(&Set) SetVertexShaderConstantF=Set;} g_orig;
static bool g_reflectionEyeExperiment=false;static int g_submitMode=4,g_stereoMode=1,g_replayEye=-1;
struct Vs{bool reflectionEye=true,worldSpecularEye=false,skinnedSpecularEye=false,auditedMaterialEye=false;unsigned hash=0x12CCC8DB;};static Vs vi;static Vs* g_activeVs=&vi;
static const void* g_boundPs=&vi;
static struct {bool allowed=true,world=false,skinned=false,audited=false;unsigned hash=0xBA3D8E61;struct Value{bool reflectionEye,worldSpecularEye,skinnedSpecularEye,auditedMaterialEye;unsigned hash;};Value Lookup(const void*){return {allowed,world,skinned,audited,hash};}} g_pixelShaders;
static d2vr_shader_registers::History g_vsConstantHistory;
struct Params{bool headGeometry=true;};static Params MakeEyeParams(int){return {};}
static bool IsShadowAtlasPass(IDirect3DDevice9*){return false;}
static void Log(const char*,...){}
namespace d2vr_submission {static int invalidations=0;static void Invalidate(){++invalidations;}}
#include "reflection_scope_actual.inl"
static const float projection[16]={1,0,0,0,0,1,0,0,0,0,1.01f,1,0,0,-.1f,0};
static void Setup(IDirect3DDevice9& device,int eye) {
    device={};g_vsConstantHistory.Clear();g_replayEye=eye;g_reflectionEyeExperiment=true;
    vi.auditedMaterialEye=false;g_pixelShaders.audited=false;vi.hash=0x12CCC8DB;g_pixelShaders.hash=0xBA3D8E61;
    vi.reflectionEye=true;vi.worldSpecularEye=false;vi.skinnedSpecularEye=false;g_pixelShaders.skinned=false;g_pixelShaders.allowed=true;g_pixelShaders.world=false;
    float raw[36]{};std::memcpy(raw,projection,sizeof(projection));raw[35]=123.25f;
    g_vsConstantHistory.live.Write(0,9,raw,sizeof(raw));
    g_vsConstantHistory.replay=g_vsConstantHistory.live;
    std::memcpy(device.values,raw,sizeof(raw));device.values[12]=eye==1?-.0325f:.0325f;
}
int main(){
    IDirect3DDevice9 device;
    for(int eye:{-1,1}){
        Setup(device,eye);float before[36];std::memcpy(before,device.values,sizeof(before));
        {ReflectionEyeScope scope(&device);
         Check(device.sets==1,"one temporary setter");
         Check(std::fabs(device.values[32]-(eye==1?.0325f:-.0325f))<1e-5f,"actual per-eye center");
         Check(device.values[35]==123.25f,"animation time preserved");
         Check(std::memcmp(before,device.values,32*sizeof(float))==0,"other registers unchanged");}
        Check(device.sets==2 && std::memcmp(before,device.values,sizeof(before))==0,"exact draw-scope restoration");
    }
    Setup(device,-1);g_reflectionEyeExperiment=false;{ReflectionEyeScope s(&device);}
    Check(!device.gets && !device.sets,"default off has no driver queries");
    for(int eye:{-1,1}) {
        Setup(device,eye);g_reflectionEyeExperiment=false;
        vi.reflectionEye=false;vi.worldSpecularEye=true;g_pixelShaders.allowed=false;g_pixelShaders.world=true;
        float old[36];std::memcpy(old,device.values,sizeof(old));
        {ReflectionEyeScope s(&device);Check(device.sets==1,"verified light-map pair admitted independently");
         Check(std::fabs(device.values[32]-(eye==1?.0325f:-.0325f))<1e-5f,"light-map eye follows actual projection");}
        Check(device.sets==2 && !std::memcmp(old,device.values,sizeof(old)),"light-map restores every register");
        device.gets=0;device.sets=0;g_pixelShaders.world=false;
        {ReflectionEyeScope s(&device);}Check(!device.gets && !device.sets,"light-map VS with foreign PS rejected");
        g_pixelShaders.world=true;vi.worldSpecularEye=false;
        {ReflectionEyeScope s(&device);}Check(!device.gets && !device.sets,"light-map PS with foreign VS rejected");
    }
    for(int eye:{-1,1}) {
        Setup(device,eye);g_reflectionEyeExperiment=false;
        vi.reflectionEye=false;vi.skinnedSpecularEye=true;g_pixelShaders.allowed=false;g_pixelShaders.skinned=true;
        float old[36];std::memcpy(old,device.values,sizeof(old));
        {ReflectionEyeScope s(&device);Check(device.sets==1,"crane pair corrected with cube experiment off");}
        Check(device.sets==2 && !std::memcmp(old,device.values,sizeof(old)),"crane restores every register");
        device.gets=0;device.sets=0;g_pixelShaders.skinned=false;
        {ReflectionEyeScope s(&device);}Check(!device.gets && !device.sets,"crane VS with foreign PS rejected");
        g_pixelShaders.skinned=true;vi.skinnedSpecularEye=false;
        {ReflectionEyeScope s(&device);}Check(!device.gets && !device.sets,"crane PS with foreign VS rejected");
    }
    for(int eye:{-1,1}) {
        Setup(device,eye);g_reflectionEyeExperiment=false;
        vi.reflectionEye=false;g_pixelShaders.allowed=false;
        vi.auditedMaterialEye=true;vi.hash=0x8EB68C19;
        g_pixelShaders.audited=true;g_pixelShaders.hash=0x865FF0EB;
        float old[36];std::memcpy(old,device.values,sizeof(old));
        {ReflectionEyeScope scope(&device);Check(device.sets==1,"audited material admitted");
         Check(std::fabs(device.values[32]-(eye==1?.0325f:-.0325f))<1e-5f,"audited per-eye center");}
        Check(device.sets==2 && !std::memcmp(old,device.values,sizeof(old)),"audited exact restore");
        device.gets=device.sets=0;g_pixelShaders.hash=0x75DE5534;
        Check(!d2vr_material_eyes::Pair(vi.hash,g_pixelShaders.hash),"unobserved cross pair fixture");
        {ReflectionEyeScope scope(&device);}Check(!device.gets && !device.sets,"cross pair rejected");
    }
    unsigned checks=0;
    const auto& pin=d2vr_material_eyes::kVertices[0];
    Check(d2vr_material_eyes::Admit(d2vr_material_eyes::kVertices,pin.hash,pin.bytes,[&](const char* sha){++checks;return !std::strcmp(sha,pin.sha);}),"pinned identity admitted");
    Check(!d2vr_material_eyes::Admit(d2vr_material_eyes::kVertices,pin.hash,pin.bytes+4,[&](const char*){++checks;return true;}) && checks==1,"wrong size rejected before digest");
    Check(!d2vr_material_eyes::Admit(d2vr_material_eyes::kVertices,pin.hash,pin.bytes,[](const char*){return false;}),"wrong SHA rejected");
    for(auto pair:d2vr_material_eyes::kPairs)Check(d2vr_material_eyes::Pair(unsigned(pair>>32),unsigned(pair)),"all pinned pairs discoverable");
    Setup(device,-1);vi.reflectionEye=false;{ReflectionEyeScope s(&device);}
    Check(!device.gets,"foreign VS rejected before driver query");
    Setup(device,-1);g_pixelShaders.allowed=false;{ReflectionEyeScope s(&device);}
    Check(!device.gets,"foreign PS rejected before driver query");
    Setup(device,-1);g_vsConstantHistory.live.Invalidate(8,1);{ReflectionEyeScope s(&device);}
    Check(!device.gets,"unknown raw register rejected");
    Setup(device,-1);device.values[32]=1;{ReflectionEyeScope s(&device);}
    Check(!device.sets,"device raw disagreement rejected");
    Setup(device,-1);device.failGet=true;{ReflectionEyeScope s(&device);}
    Check(!device.sets,"failed driver read does not mutate");
    Setup(device,-1);device.failFirstSet=true;float before[36];std::memcpy(before,device.values,sizeof(before));
    {ReflectionEyeScope s(&device);}
    Check(device.sets==2 && std::memcmp(before,device.values,sizeof(before))==0,"failed setter also restores");
    float out[4]={9,8,7,6},bad[16]{};float eye[4]={0,0,0,123};
    Check(!d2vr_reflection_eye::Correct(bad,projection,eye,out) && out[0]==9,"singular rejection without output mutation");
    eye[0]=1;Check(!d2vr_reflection_eye::Correct(projection,projection,eye,out),"wrong native eye rejected");
    float rotated[16];std::memcpy(rotated,projection,sizeof(rotated));
    // Object rotation/nonuniform scale and translation: camera center must
    // still map back to the same point after multiplication by its WVP.
    const float model[16]={0,0,2,0,0,3,0,0,-4,0,0,0,5,6,7,1};
    d2vr_math::Multiply(model,projection,rotated);float center[3];
    Check(d2vr_reflection_eye::Center(rotated,center),"scaled rotated object center");
    for(int c:{0,1,3}){
        const float value=center[0]*rotated[c]+center[1]*rotated[4+c]+center[2]*rotated[8+c]+rotated[12+c];
        Check(std::fabs(value)<1e-5f,"camera center projects to zero xyw");
    }
    std::puts("PASS actual reflection scope: both eyes, ownership, failures, exact restore and transformed geometry");
}

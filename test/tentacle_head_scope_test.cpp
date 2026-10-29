// Exercises the extracted production scope, including resource and shader gates.
#define NOMINMAX
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include "../src/stereo_math.h"
static unsigned checks=0;
#define CHECK(x) do {++checks;if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}} while(0)
struct D3DVERTEXBUFFER_DESC {UINT Size=302920;};
struct D3DINDEXBUFFER_DESC {UINT Size=213588;};
struct IDirect3DVertexBuffer9 {
    D3DVERTEXBUFFER_DESC desc; unsigned refs=0,queries=0; bool fail=false;
    HRESULT GetDesc(D3DVERTEXBUFFER_DESC* out){++queries;*out=desc;return fail?E_FAIL:S_OK;}
    void Release(){CHECK(refs>0);--refs;}
};
struct IDirect3DIndexBuffer9 {
    D3DINDEXBUFFER_DESC desc; unsigned refs=0,queries=0; bool fail=false;
    HRESULT GetDesc(D3DINDEXBUFFER_DESC* out){++queries;*out=desc;return fail?E_FAIL:S_OK;}
    void Release(){CHECK(refs>0);--refs;}
};
struct IDirect3DDevice9 {
    IDirect3DVertexBuffer9 vb;IDirect3DIndexBuffer9 ib;
    float matrix[16]{};UINT offset=0,stride=40;
    bool noVB=false,noIB=false,failGet=false,failSet=false;
    unsigned sets=0,indexQueries=0;
    HRESULT GetStreamSource(UINT stream,IDirect3DVertexBuffer9** out,UINT* off,UINT* st){
        CHECK(stream==0);*out=noVB?nullptr:&vb;*off=offset;*st=stride;
        if(*out)++vb.refs;return noVB?E_FAIL:S_OK;
    }
    HRESULT GetIndices(IDirect3DIndexBuffer9** out){++indexQueries;*out=noIB?nullptr:&ib;if(*out)++ib.refs;return noIB?E_FAIL:S_OK;}
    HRESULT GetVertexShaderConstantF(UINT start,float* out,UINT count){CHECK(start==0 && count==4);memcpy(out,matrix,64);return failGet?E_FAIL:S_OK;}
};
struct StereoParams {bool headGeometry=true;float inverseHeadClip[16]{};} params;
static bool g_headAttachedTentacles=true,shadow=false;
static bool headAttachmentAllowed=true;
static bool NativeTentacleHeadAttachmentAllowed(){return headAttachmentAllowed;}
static int g_submitMode=4,g_stereoMode=1,g_replayEye=-1;
struct Shader {unsigned hash=0x1CB7CEE2;} shader;
static Shader* g_activeVs=&shader;
static StereoParams MakeEyeParams(int eye){CHECK(eye>=0 && eye<2);return params;}
static bool IsShadowAtlasPass(IDirect3DDevice9*){return shadow;}
static void Log(const char*,...){}
static struct {
    HRESULT SetVertexShaderConstantF(IDirect3DDevice9* dev,UINT start,const float* data,UINT count){
        CHECK(start==0 && count==4);++dev->sets;
        if(dev->failSet)return E_FAIL;memcpy(dev->matrix,data,64);return S_OK;
    }
}g_orig;
#include "actual_tentacle_head_scope.inl"
static bool Equal(const float* a,const float* b){for(int i=0;i<16;++i)if(std::fabs(a[i]-b[i])>2e-5f)return false;return true;}
int main(){
    // A valid camera-local WVP and an actual tracked yaw/pitch/translation.
    const float attached[16]={1.1f,0,0,0,0,1.7f,0,0,0,0,1.0001f,1,0,0,-.10001f,0};
    float inverseProjection[16];CHECK(d2vr_math::Invert(attached,inverseProjection));
    for(unsigned pose=0;pose<30;++pose){
        float delta[16],headClip[16];const float q[4]={0,std::sin(pose*.01f),0,std::cos(pose*.01f)};
        const float position[3]={pose*.002f,pose*-.001f,pose*.003f};
        CHECK(d2vr_math::TrackingPose(q,position,delta));
        CHECK(d2vr_math::HeadClipTransform(inverseProjection,delta,headClip,params.inverseHeadClip));
        float original[16];d2vr_math::Multiply(attached,headClip,original);
        for(unsigned hash : {0x1CB7CEE2u,0x34D4EEE8u,0xE3259C61u})
        for(unsigned offset : {0u,2640u,120400u,158040u,160920u,267560u,285240u})
        for(int eye=-1;eye<2;++eye){
            IDirect3DDevice9 dev;dev.offset=offset;shader.hash=hash;g_replayEye=eye;memcpy(dev.matrix,original,64);
            {TentacleHeadScope scope(&dev);CHECK(scope.applied && Equal(dev.matrix,attached));CHECK(dev.sets==1);}
            CHECK(Equal(dev.matrix,original) && dev.sets==2 && !dev.vb.refs && !dev.ib.refs);
        }
    }
    IDirect3DDevice9 dev;memcpy(dev.matrix,attached,64);
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};memcpy(params.inverseHeadClip,identity,64);
    auto rejected=[&]{const unsigned sets=dev.sets;float before[16];memcpy(before,dev.matrix,64);
        {TentacleHeadScope scope(&dev);CHECK(!scope.applied);}
        CHECK(dev.sets==sets && Equal(before,dev.matrix) && !dev.vb.refs && !dev.ib.refs);};
    // Both eyes retain the original world transform throughout a hold. This
    // matches the unmodified held-object draw instead of head-attaching one end.
    headAttachmentAllowed=false;
    for(int eye=-1;eye<2;++eye){g_replayEye=eye;rejected();}
    CHECK(!dev.vb.queries && !dev.ib.queries && !dev.indexQueries);
    headAttachmentAllowed=true;
    shader.hash=0x12345678;rejected();shader.hash=0x1CB7CEE2;
    // Shared skinned shader alone never establishes player mesh ownership.
    const unsigned indicesBefore=dev.indexQueries;
    dev.vb.desc.Size++;rejected();dev.vb.desc.Size--;CHECK(dev.indexQueries==indicesBefore);
    dev.ib.desc.Size++;rejected();dev.ib.desc.Size--;
    const unsigned vbBefore=dev.vb.queries,ibBefore=dev.ib.queries,indicesBeforeStream=dev.indexQueries;
    dev.offset=1;rejected();dev.offset=0;
    dev.stride=32;rejected();dev.stride=40;
    CHECK(dev.vb.queries==vbBefore && dev.ib.queries==ibBefore && dev.indexQueries==indicesBeforeStream);
    shadow=true;rejected();shadow=false;
    g_headAttachedTentacles=false;rejected();g_headAttachedTentacles=true;
    params.headGeometry=false;rejected();params.headGeometry=true;
    g_submitMode=0;rejected();g_submitMode=4;g_stereoMode=0;rejected();g_stereoMode=1;
    g_activeVs=nullptr;rejected();g_activeVs=&shader;
    dev.noVB=true;rejected();dev.noVB=false;dev.noIB=true;rejected();dev.noIB=false;
    dev.vb.fail=true;rejected();dev.vb.fail=false;dev.ib.fail=true;rejected();dev.ib.fail=false;
    dev.failGet=true;rejected();dev.failGet=false;
    dev.matrix[12]=.2f;rejected();dev.matrix[12]=0;
    dev.matrix[14]=-.2f;rejected();dev.matrix[14]=attached[14];
    dev.matrix[15]=.1f;rejected();dev.matrix[15]=0;
    dev.matrix[13]=NAN;{TentacleHeadScope scope(&dev);CHECK(!scope.applied);}dev.matrix[13]=0;
    dev.failSet=true;{TentacleHeadScope scope(&dev);CHECK(!scope.applied && Equal(dev.matrix,attached));}
    CHECK(!dev.vb.refs && !dev.ib.refs);
    std::printf("PASS actual tentacle head scope: %u checks, 1890 mesh/shader/eye/pose scopes; restoration and foreign mesh/failure rejection. No HMD claim.\n",checks);
}

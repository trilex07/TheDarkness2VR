// Actual-source ownership regression with reference-counting API doubles.
// This verifies collection/expiration, not GPU readback or headset rendering.
#include <windows.h>
#include <unordered_map>
#include <cassert>
#include <cstdio>
#include <cstdint>
struct IDirect3DTexture9 {
    unsigned refs=1;
    void AddRef(){++refs;}
    void Release(){assert(refs);--refs;}
};
struct IDirect3DSurface9 {
    unsigned refs=1,calls=0;
    IDirect3DTexture9* texture=nullptr;
    bool fail=false;
    void AddRef(){++refs;}
    void Release(){assert(refs);--refs;}
    HRESULT GetContainer(const GUID&,void** out) {
        ++calls;*out=nullptr;
        if(fail || !texture)return E_NOINTERFACE;
        texture->AddRef();*out=texture;return S_OK;
    }
};
struct IDirect3DDevice9 {};
static std::unordered_map<const void*,const void*> g_surfToTex;
static std::unordered_map<const void*,uint32_t> g_lightEndHash;
static int g_lightRbFrames=-1,g_lightRbBudget=0,configured=0,reads=0,hashes=0;
static UINT FakeProfileInt(LPCSTR,LPCSTR,INT,LPCSTR){++reads;return configured;}
#define GetPrivateProfileIntA FakeProfileInt
static void Log(const char*,...){}
static uint32_t ReadbackTextureHash(IDirect3DDevice9*,IDirect3DTexture9*){++hashes;return 7;}
#include "actual_readback.inl"
int main() {
    IDirect3DTexture9 textures[401]; IDirect3DSurface9 surfaces[401];
    for(int i=0;i<401;++i)surfaces[i].texture=&textures[i];
    // Normal rendering does no container query, no extra reference and no map entry.
    for(int frame=0;frame<1000;++frame)CollectLightReadbackTarget(&surfaces[0]);
    assert(reads==1 && surfaces[0].calls==0 && g_surfToTex.empty());
    assert(surfaces[0].refs==1 && textures[0].refs==1);
    configured=2;g_lightRbFrames=-1;
    CollectLightReadbackTarget(&surfaces[0]);CollectLightReadbackTarget(&surfaces[0]);
    assert(surfaces[0].calls==1 && surfaces[0].refs==2 && textures[0].refs==2);
    surfaces[1].fail=true;CollectLightReadbackTarget(&surfaces[1]);
    assert(surfaces[1].refs==1 && textures[1].refs==1);
    surfaces[1].fail=false;
    for(int i=1;i<401;++i)CollectLightReadbackTarget(&surfaces[i]);
    assert(g_surfToTex.size()==400 && surfaces[400].calls==0);
    IDirect3DDevice9 device;
    LightEndOfWindowSnapshot(&device);assert(g_lightRbFrames==1 && hashes==400);
    LightEndOfWindowSnapshot(&device);assert(g_lightRbFrames==0 && hashes==800);
    // The final replay still has its owned inputs. The following boundary expires them.
    assert(g_lightRbBudget==2 && g_surfToTex.size()==400);
    LightEndOfWindowSnapshot(&device);
    assert(g_surfToTex.empty() && g_lightEndHash.empty() && g_lightRbBudget==0);
    for(int i=0;i<401;++i)assert(textures[i].refs==1 && surfaces[i].refs==1);
    // Reset releases promptly, preserves remaining diagnostic budget, and allows recollection.
    g_lightRbFrames=3;CollectLightReadbackTarget(&surfaces[0]);
    ReleaseLightReadbackTargets();ReleaseLightReadbackTargets();
    assert(g_lightRbFrames==3 && surfaces[0].refs==1 && textures[0].refs==1);
    CollectLightReadbackTarget(&surfaces[0]);assert(g_surfToTex.size()==1);
    ReleaseLightReadbackTargets();
    puts("PASS actual-source: OFF1000, duplicate/failure/cap400, expiration, Reset/recollection, balanced surface+texture ownership");
}

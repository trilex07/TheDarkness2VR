#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include "../src/stateblock_recording.h"
#include "../src/recorder_sync.h"
static unsigned checks=0,begins=0,ends=0,patches=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %u line%d %s\n",checks,__LINE__,#x);exit(1);}}while(0)
static HRESULT beginResult=S_OK,endResult=S_OK;
static IDirect3DDevice9* const device=reinterpret_cast<IDirect3DDevice9*>(0x1000);
static IDirect3DDevice9* const other=reinterpret_cast<IDirect3DDevice9*>(0x2000);
static IDirect3DStateBlock9* const block=reinterpret_cast<IDirect3DStateBlock9*>(0x3000);
static HRESULT STDMETHODCALLTYPE Begin(IDirect3DDevice9* d){CHECK(d==device);++begins;return beginResult;}
static HRESULT STDMETHODCALLTYPE End(IDirect3DDevice9* d,IDirect3DStateBlock9** out){CHECK(d==device);++ends;if(SUCCEEDED(endResult)&&out)*out=block;return endResult;}
static auto g_origBeginStateBlock=&Begin;
static auto g_origEndStateBlock=&End;
static void PatchStateBlock(IDirect3DStateBlock9* b){CHECK(b==block);++patches;}
#include "actual_stateblock_recording.inl"
int main(){
    for(unsigned frame=0;frame<1000;++frame){
        beginResult=E_OUTOFMEMORY;
        CHECK(Hook_BeginStateBlock(device)==beginResult && !d2vr_stateblock::Active(device));
        beginResult=S_OK;CHECK(Hook_BeginStateBlock(device)==S_OK && d2vr_stateblock::Active(device));
        CHECK(!d2vr_stateblock::Active(other));
        beginResult=D3DERR_INVALIDCALL;
        CHECK(Hook_BeginStateBlock(device)==beginResult && d2vr_stateblock::Active(device));
        endResult=D3DERR_INVALIDCALL;
        CHECK(Hook_EndStateBlock(device,nullptr)==endResult && d2vr_stateblock::Active(device));
        IDirect3DStateBlock9* out=nullptr;endResult=S_OK;
        CHECK(Hook_EndStateBlock(device,&out)==S_OK && out==block && !d2vr_stateblock::Active(device));
    }
    CHECK(begins==3000 && ends==2000 && patches==1000);
    d2vr_stateblock::Began(device,S_OK);d2vr_stateblock::Ended(other,S_OK);
    CHECK(d2vr_stateblock::Active(device));
    d2vr_stateblock::Ended(device,D3DERR_DEVICELOST);CHECK(d2vr_stateblock::Active(device));
    d2vr_stateblock::Ended(device,S_OK);CHECK(!d2vr_stateblock::Active(device));
    printf("PASS actual Begin/EndStateBlock: %u checks, 1000 cycles, failed begin/end/nested begin, device identity, exact HRESULT/original-once, reset transition; CPU fixture\n",checks);
}

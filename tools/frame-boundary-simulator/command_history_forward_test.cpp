#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <atomic>
#include <intrin.h>
#include <cstdio>
#include <cassert>
#include <thread>
#include "../../src/command_history.h"
static volatile LONG g_frameId=1,g_captureResetGeneration=0,g_captureResetting=0;
static bool building=false;
namespace d2vr_stateblock {static bool Active(IDirect3DDevice9*){return building;}}
static void Log(const char*,...){}
static void* PatchVTable(void*,int,void*){return nullptr;}
namespace fixture {
#include "actual_history_orig.inl"
static OrigFns g_orig;
static int g_replayEye=-1;
using PFN_SBApply=HRESULT(STDMETHODCALLTYPE*)(IDirect3DStateBlock9*);
#include "../../src/command_history_win.inl"
}
using namespace fixture::d2vr_command_history;
static HRESULT nextResult=S_OK;
static std::atomic<unsigned> forwarded{0};
static unsigned getterCalls=0;
static HRESULT STDMETHODCALLTYPE BlockApply(IDirect3DStateBlock9*){++forwarded;return nextResult;}
static HRESULT STDMETHODCALLTYPE SetVS(IDirect3DDevice9*,IDirect3DVertexShader9*){++forwarded;return nextResult;}
static HRESULT STDMETHODCALLTYPE SetPS(IDirect3DDevice9*,IDirect3DPixelShader9*){++forwarded;return nextResult;}
static HRESULT STDMETHODCALLTYPE SetRS(IDirect3DDevice9*,D3DRENDERSTATETYPE,DWORD){++forwarded;return nextResult;}
static HRESULT STDMETHODCALLTYPE ShaderGet(IDirect3DDevice9*,void** out){++getterCalls;*out=nullptr;return S_OK;}
static HRESULT STDMETHODCALLTYPE SurfaceGet(IDirect3DDevice9*,DWORD,void** out){++getterCalls;*out=nullptr;return D3DERR_NOTFOUND;}
static HRESULT STDMETHODCALLTYPE DepthGet(IDirect3DDevice9*,void** out){++getterCalls;*out=nullptr;return D3DERR_NOTFOUND;}
static HRESULT STDMETHODCALLTYPE TextureGet(IDirect3DDevice9*,DWORD,void** out){++getterCalls;*out=nullptr;return S_OK;}
static HRESULT STDMETHODCALLTYPE FVFGet(IDirect3DDevice9*,DWORD* out){++getterCalls;*out=42;return S_OK;}
static HRESULT STDMETHODCALLTYPE RSGet(IDirect3DDevice9*,DWORD,DWORD* out){++getterCalls;*out=7;return S_OK;}
static HRESULT STDMETHODCALLTYPE SamplerGet(IDirect3DDevice9*,DWORD,DWORD,DWORD* out){++getterCalls;*out=9;return S_OK;}
int main(){
    void* vt[120]{};void** object=vt;
    auto* dev=reinterpret_cast<IDirect3DDevice9*>(&object);
    vt[93]=vt[108]=vt[88]=reinterpret_cast<void*>(ShaderGet);
    vt[38]=reinterpret_cast<void*>(SurfaceGet);vt[40]=reinterpret_cast<void*>(DepthGet);
    vt[64]=reinterpret_cast<void*>(TextureGet);vt[90]=reinterpret_cast<void*>(FVFGet);
    vt[58]=reinterpret_cast<void*>(RSGet);vt[68]=reinterpret_cast<void*>(SamplerGet);
    configured=true;ownerTid=GetCurrentThreadId();ownerDevice=dev;status=1;
    frequency=10000000;threshold=~uint64_t(0);
    originals.SetVertexShader=SetVS;originals.SetPixelShader=SetPS;originals.SetRenderState=SetRS;
    auto* vs=reinterpret_cast<IDirect3DVertexShader9*>(1234);
    auto* ps=reinterpret_cast<IDirect3DPixelShader9*>(5678);
    assert(VS(dev,vs)==S_OK && pipeline.vs==1234 && forwarded==1);
    nextResult=E_FAIL;assert(VS(dev,nullptr)==E_FAIL && pipeline.vs==1234 && forwarded==2);
    nextResult=S_OK;building=true;
    assert(PS(dev,ps)==S_OK && pipeline.ps==0); // block construction is not effective state
    building=false;assert(PS(dev,ps)==S_OK && pipeline.ps==5678);
    assert(Render(dev,D3DRS_ALPHATESTENABLE,1)==S_OK && pipeline.render[3]==1);
    assert((pipeline.renderKnown&(1<<3))!=0);
    nextResult=E_FAIL;assert(Render(dev,D3DRS_ALPHATESTENABLE,0)==E_FAIL && pipeline.render[3]==1);
    nextResult=S_OK;
    // Actual snapshot code balances all successful null getters and records known masks.
    Snapshot(dev);assert(!dirty && pipeline.known==131071 && pipeline.renderKnown==65535 && pipeline.samplerKnown==63);
    assert(pipeline.fvf==42 && pipeline.render[0]==7 && pipeline.sampler[0]==9);
    const unsigned calls=getterCalls;
    for(unsigned i=0;i<8;++i)Snapshot(dev);
    assert(getterCalls==calls*4 && pipeline.known==0 && pipeline.renderKnown==0); // bounded getter budget
    ++g_frameId;Snapshot(dev);assert(pipeline.known==131071);
    originalApply=BlockApply;assert(Apply(nullptr)==S_OK && dirty && !pipeline.known);
    nextResult=E_FAIL;pipeline.known=3;assert(Apply(nullptr)==E_FAIL && !pipeline.known);
    nextResult=S_OK;
    Invalidate();assert(dirty && !pipeline.known);
    observedGeneration=0;pipeline.known=1;++g_captureResetGeneration;
    assert(Owns(dev) && dirty && pipeline.known==0 && observedGeneration==1);
    g_captureResetting=1;const auto beforeReset=history.written;
    Scope resetCall(dev,::d2vr_command_history::Draw,-1);
    assert(!resetCall.ticket && history.written==beforeReset && !pipeline.known);
    g_captureResetting=0;
    const unsigned oldEpoch=foreignEpoch.load();
    std::thread other([&]{assert(PS(dev,ps)==S_OK);});other.join();
    assert(foreignEpoch==oldEpoch+1 && pipeline.ps==0);
    assert(Owns(dev) && observedEpoch==foreignEpoch);
    ++g_frameId;Scope command(dev,::d2vr_command_history::Draw,-1,0,3,1,2);
    assert(command.ticket && history.At(0).pipeline.known==131071);
    foreignEpoch.fetch_add(1);assert(command.Finish(E_FAIL)==E_FAIL);
    assert(history.At(0).uncertain && history.At(0).result==uint32_t(E_FAIL));
    // OFF native StateBlock hook: getters replace an otherwise stale mirror.
    originalApply=nullptr;pipeline.vs=1234;pipeline.ps=5678;dirty=false;++g_frameId;
    Scope inherited(dev,::d2vr_command_history::Draw,-1);
    const auto& inheritedEvent=history.entries[(inherited.ticket-1)%32768];
    assert(inheritedEvent.pipeline.vs==0 && inheritedEvent.pipeline.ps==0);
    assert((inheritedEvent.pipeline.known&3)==3);inherited.Finish(S_OK);
    pipeline.vs=0;pipeline.ps=0;pipeline.renderKnown=65535;
    Scope samePair(dev,::d2vr_command_history::Draw,-1);
    assert(history.entries[(samePair.ticket-1)%32768].pipeline.known==3);
    assert(!history.entries[(samePair.ticket-1)%32768].pipeline.renderKnown);samePair.Finish(S_OK);
    const auto count=history.written;exportRequested=true;
    Scope stopped(dev,::d2vr_command_history::Draw,-1);
    assert(!stopped.ticket && history.written==count && status==2 && history.frozen);
    status=1;exportRequested=false;history.frozen=false;history.trigger=0;dirty=false;
    LARGE_INTEGER begin{},end{},freq{};QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&begin);
    for(unsigned i=0;i<300000;++i){Scope sample(dev,::d2vr_command_history::Draw,-1);sample.Finish(S_OK);}
    QueryPerformanceCounter(&end);
    printf("BENCH diagnostic bookkeeping %.4f ms/5000 commands; no driver/GPU/ETW; not runtime overhead proof\n",(end.QuadPart-begin.QuadPart)*1000.0/freq.QuadPart/60.0);
    // Run the actual export worker. Export is impossible until the external
    // signal, and the worker reads only after owner publication of a frozen ring.
    char cwd[MAX_PATH]{},root[MAX_PATH]{},folder[MAX_PATH]{},file[MAX_PATH]{};
    GetCurrentDirectoryA(MAX_PATH,cwd);sprintf_s(root,"%s\\worker-export",cwd);CreateDirectoryA(root,nullptr);
    sprintf_s(folder,"%s\\Darkness2VR",root);CreateDirectoryA(folder,nullptr);
    sprintf_s(file,"%s\\command-history-%lu.bin",folder,GetCurrentProcessId());
    SetEnvironmentVariableA("LOCALAPPDATA",root);
    armEvent=CreateEventA(nullptr,FALSE,FALSE,nullptr);exportEvent=CreateEventA(nullptr,FALSE,FALSE,nullptr);
    status=0;exportRequested=false;history.frozen=false;
    history.written=history.overwritten=history.missingEnds=history.trigger=0;
    HANDLE worker=CreateThread(nullptr,0,ExportWorker,nullptr,0,nullptr);assert(worker);
    SetEvent(armEvent);
    for(unsigned i=0;i<1000 && status.load()!=1;++i)Sleep(1);
    assert(status==1);
    Scope beforeExport(dev,::d2vr_command_history::Draw,-1);beforeExport.Finish(S_OK);
    assert(GetFileAttributesA(file)==INVALID_FILE_ATTRIBUTES); // no per-draw/early disk output
    SetEvent(exportEvent);
    for(unsigned i=0;i<1000 && status.load()==1;++i){Scope finish(dev,::d2vr_command_history::Draw,-1);finish.Finish(S_OK);Sleep(1);}
    assert(WaitForSingleObject(worker,3000)==WAIT_OBJECT_0 && status==3);
    assert(GetFileAttributesA(file)!=INVALID_FILE_ATTRIBUTES);
    FILE* receipt=nullptr;fopen_s(&receipt,"worker-result-path.txt","w");assert(receipt);fprintf(receipt,"%s",file);fclose(receipt);
    CloseHandle(worker);CloseHandle(armEvent);CloseHandle(exportEvent);
    printf("PASS actual worker: no early disk export, release/acquire publication, completed binary\n");
    printf("PASS actual Windows forwarding, failed HRESULT, StateBlock construction, snapshot budget, Reset, foreign mutation, export publication\n");
}

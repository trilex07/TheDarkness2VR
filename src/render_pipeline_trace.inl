#pragma once
// Fixed-capacity, non-waiting render-thread trace. Only the worker touches files.
// Process-lifetime proxy: worker pins this module; shutdown never joins under
// the loader lock. Abrupt process exit may lose the last flush interval.
#include <cstdio>
#include <share.h>
namespace d2vr_trace {
inline int g_enabled=0, g_traceScenes=0, g_traceAllScenes=0, g_maxEvents=500000;
inline volatile LONG g_count=0, g_drawsSinceEvent=0, g_dropped=0, g_stopping=0;
inline uint64_t g_t0Ms=0;
struct Row {
    uint64_t ms; uint32_t tid, records, aux, flags;
    int depth; LONG draws;
    void *device, *obj0, *obj1, *ret;
    char event[32], note[128];
};
inline Row g_ring[8192];
inline unsigned g_read=0, g_write=0;
inline SRWLOCK g_lock=SRWLOCK_INIT;
inline char g_path[MAX_PATH]{};
inline DWORD WINAPI Writer(void*) {
    HMODULE pinned=nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCSTR>(&Writer), &pinned)) return 1;
    FILE* f=_fsopen(g_path,"w",_SH_DENYWR);
    if (!f) return 2;
    fprintf(f,"ms,thread,event,sceneDepth,draws,records,device,obj0,obj1,auxU32,retaddr,flags,note\n");
    uint64_t lastFlush=GetTickCount64();
    for (;;) {
        Row batch[64]; unsigned count=0;
        AcquireSRWLockExclusive(&g_lock);
        while (g_read!=g_write && count<64) {
            batch[count++]=g_ring[g_read]; g_read=(g_read+1)%8192;
        }
        ReleaseSRWLockExclusive(&g_lock);
        for (unsigned i=0;i<count;++i) {
            const auto& r=batch[i];
            fprintf(f,"%llu,%u,%s,%d,%ld,%u,%p,%p,%p,%u,%p,%u,%s\n",
                r.ms,r.tid,r.event,r.depth,r.draws,r.records,r.device,r.obj0,r.obj1,r.aux,r.ret,r.flags,r.note);
        }
        if (GetTickCount64()-lastFlush>=500) {
            fflush(f); lastFlush=GetTickCount64();
        }
        if (!count) {
            if (InterlockedCompareExchange(&g_stopping,0,0)) break;
            Sleep(10);
        }
    }
    fprintf(f,"%llu,0,TraceStop,0,0,0,0,0,0,%ld,0,0,dropped-events\n",
        GetTickCount64()-g_t0Ms, InterlockedCompareExchange(&g_dropped,0,0));
    fclose(f); return 0;
}
inline void Init(const char* dir) {
    if (!g_enabled) return;
    g_t0Ms=GetTickCount64();
    sprintf_s(g_path,"%s\\render_pipeline_trace.csv",dir);
    HANDLE thread=CreateThread(nullptr,0,Writer,nullptr,0,nullptr);
    if (thread) CloseHandle(thread); else g_enabled=0;
}
inline void Shutdown() { InterlockedExchange(&g_stopping,1); }
inline void NoteDraw() { InterlockedIncrement(&g_drawsSinceEvent); }
inline void Emit(uint64_t ms,uint32_t tid,const char* event,int depth,uint32_t records,
    void* device,void* obj0,void* obj1,uint32_t aux,void* ret,uint32_t flags,const char* note="") {
    if (!g_enabled || InterlockedCompareExchange(&g_stopping,0,0)) return;
    if (InterlockedIncrement(&g_count)>g_maxEvents) { InterlockedIncrement(&g_dropped); return; }
    if (!TryAcquireSRWLockExclusive(&g_lock)) { InterlockedIncrement(&g_dropped); return; }
    unsigned next=(g_write+1)%8192;
    if (next==g_read) { ReleaseSRWLockExclusive(&g_lock); InterlockedIncrement(&g_dropped); return; }
    Row& r=g_ring[g_write];
    r.ms=ms-g_t0Ms; r.tid=tid; r.depth=depth; r.records=records;
    r.draws=InterlockedExchange(&g_drawsSinceEvent,0); r.device=device; r.obj0=obj0; r.obj1=obj1;
    r.aux=aux; r.ret=ret; r.flags=flags;
    strncpy_s(r.event,event?event:"",_TRUNCATE); strncpy_s(r.note,note?note:"",_TRUNCATE);
    g_write=next; ReleaseSRWLockExclusive(&g_lock);
}
inline void EmitSimple(uint64_t ms,uint32_t tid,const char* ev,int depth,uint32_t records,
    void* device,const char* note="") {
    Emit(ms,tid,ev,depth,records,device,nullptr,nullptr,0,nullptr,0,note);
}
}

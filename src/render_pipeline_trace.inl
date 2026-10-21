#pragma once
// Bounded D3D9 lifecycle trace (no OpenXR blocking). [Render] RenderPipelineTrace=1

#include <cstdio>
#include <share.h>
#include <unordered_map>

namespace d2vr_trace {

inline int g_enabled = 0;
inline int g_traceScenes = 0; // 0=no Begin/End rows (stable); 1=emit scene rows (see g_traceAllScenes)
inline int g_traceAllScenes = 0; // when g_traceScenes: 0=outer pairs only; 1=every nested pair
inline int g_maxEvents = 500000;
inline FILE* g_file = nullptr;
inline FILE* g_retFile = nullptr;
inline volatile LONG g_count = 0;
inline volatile LONG g_drawsSinceEvent = 0;
inline uint64_t g_t0Ms = 0;
inline std::unordered_map<uintptr_t, uint64_t> g_retCounts;

inline void Init(const char* logDir) {
    if (!g_enabled) return;
    g_t0Ms = GetTickCount64();
    char path[MAX_PATH];
    sprintf_s(path, "%s\\render_pipeline_trace.csv", logDir);
    g_file = _fsopen(path, "w", _SH_DENYWR);
    if (!g_file) return;
    fprintf(g_file,
            "ms,thread,event,sceneDepth,draws,records,device,obj0,obj1,auxU32,retaddr,flags,note\n");
    fflush(g_file);
    sprintf_s(path, "%s\\render_pipeline_retaddr.csv", logDir);
    g_retFile = _fsopen(path, "w", _SH_DENYWR);
    if (g_retFile) {
        fprintf(g_retFile, "retaddr,count,event\n");
        fflush(g_retFile);
    }
}

inline void NoteRet(uintptr_t ret, const char* ev) {
    if (!ret) return;
    ++g_retCounts[ret];
    (void)ev;
}

inline void Shutdown() {
    if (g_retFile) {
        for (const auto& kv : g_retCounts)
            fprintf(g_retFile, "%p,%llu,\n", (void*)kv.first, (unsigned long long)kv.second);
        fclose(g_retFile);
        g_retFile = nullptr;
    }
    if (g_file) {
        fclose(g_file);
        g_file = nullptr;
    }
}

inline void NoteDraw() { InterlockedIncrement(&g_drawsSinceEvent); }

inline void Emit(uint64_t ms, uint32_t tid, const char* event, int sceneDepth, uint32_t records,
                 void* device, void* obj0, void* obj1, uint32_t auxU32, void* retaddr, uint32_t flags,
                 const char* note = "") {
    if (!g_enabled || !g_file) return;
    if (InterlockedIncrement(&g_count) > g_maxEvents) return;
    const LONG draws = InterlockedExchange(&g_drawsSinceEvent, 0);
    if (retaddr) NoteRet((uintptr_t)retaddr, event);
    const uint64_t rel = g_t0Ms ? (ms - g_t0Ms) : ms;
    fprintf(g_file, "%llu,%u,%s,%d,%ld,%u,%p,%p,%p,%u,%p,%u,%s\n", rel, tid, event, sceneDepth, draws,
            records, device, obj0, obj1, auxU32, retaddr, flags, note);
    if ((g_count % 256) == 0 || (event && strcmp(event, "SwapChainPresent") == 0)) fflush(g_file);
}

inline void EmitSimple(uint64_t ms, uint32_t tid, const char* event, int sceneDepth, uint32_t records,
                       void* device, const char* note = "") {
    Emit(ms, tid, event, sceneDepth, records, device, nullptr, nullptr, 0, nullptr, 0, note);
}

} // namespace d2vr_trace

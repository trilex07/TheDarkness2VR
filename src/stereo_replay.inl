// stereo_replay.inl - Darkness2VR true same-frame stereo core
//
// Architecture (evidence-based, see HANDOFF.md v0.5):
//   Evolution Engine (DarknessII.exe) uploads a per-object row-major
//   WorldViewProjection at c0..c3 (LitInstance struct, count=12/17) and a
//   ScreenTextureProjection matrix (c8..c11 or c20..c23, shader dependent).
//   Position transform (verified from HelixMod HLSL dumps):
//       mul r0, c1, v0.y / mad r0, v0.x, c0, r0 / mad r0, v0.z, c2, r0
//       add o0, r0, c3        =>  clip = x*c0 + y*c1 + z*c2 + c3
//   i.e. clip.x constant term lives in c3.x (flat element 12 of the block).
//
// Stereo: per-eye shear  clip.x += sep*(clip.w - conv)  folded into the matrix:
//       c_i.x += sep * c_i.w   (i = 0..3)
//       c3.x  -= sep * conv
//   Depth (clip.z, clip.w) untouched => deferred lighting reconstruction,
//   decals and shadows stay coherent (unlike the old row-3 camera translation).
//
// Same-frame dual eye: the whole device call stream of the frame is recorded
// (state + draws) and re-executed once per eye. Eye 0 = live pass, eye 1 =
// replay pass. No AFR, no DIBR (both rejected by user with evidence).
//
#pragma once

namespace d2vr_replay {

// ------------------------------------------------------------- VS bytecode analysis
enum : uint32_t {
    SI_NOP = 0, SI_MOV = 1, SI_ADD = 2, SI_SUB = 3, SI_MAD = 4, SI_MUL = 5,
    SI_RCP = 6, SI_RSQ = 7, SI_DP3 = 8, SI_DP4 = 9, SI_MIN = 10, SI_MAX = 11,
    SI_SLT = 12, SI_SGE = 13, SI_EXP = 14, SI_LOG = 15, SI_LIT = 16, SI_DST = 17,
    SI_LRP = 18, SI_FRC = 19, SI_M4x4 = 20, SI_M4x3 = 21, SI_M3x4 = 22,
    SI_M3x3 = 23, SI_M3x2 = 24, SI_CALL = 25, SI_CALLNZ = 26, SI_LOOP = 27,
    SI_RET = 28, SI_ENDLOOP = 29, SI_LABEL = 30, SI_DCL = 31, SI_POW = 32,
    SI_CRS = 33, SI_SGN = 34, SI_ABS = 35, SI_NRM = 36, SI_SINCOS = 37,
    SI_REP = 38, SI_ENDREP = 39, SI_IF = 40, SI_IFC = 41, SI_ELSE = 42,
    SI_ENDIF = 43, SI_BREAK = 44, SI_BREAKC = 45, SI_MOVA = 46, SI_DEFB = 47,
    SI_DEFI = 48, SI_TEXCOORD = 64, SI_TEXKILL = 65, SI_TEX = 66, SI_DEF = 81,
    SI_CMP = 88, SI_SETP = 94, SI_TEXLDL = 95,
    SI_COMMENT = 0xFFFE, SI_END = 0xFFFF
};

struct VsInfo {
    uint32_t hash = 0;
    int wvpBase = -1;      // const base of the 4x4 that writes o0 (clip pos)
    int stpBase = -1;      // const base of a 4x4 that writes another output (screen texture proj)
    bool depthOnly = false;
    bool hasBones = false;
    bool analyzed = false;
};

static inline uint32_t RegKey(uint32_t tok) {
    uint32_t type = ((tok & 0x70000000u) >> 28) | ((tok & 0x1800u) >> 8);
    return (type << 11) | (tok & 0x7FFu);
}
static inline int RegNum(uint32_t tok) { return (int)(tok & 0x7FFu); }
static inline int RegType(uint32_t tok) {
    return (int)(((tok & 0x70000000u) >> 28) | ((tok & 0x1800u) >> 8));
}
// output register classes: RASTOUT(4)=oPos/oFog/oPts, ATTROUT(5)=oD#, OUTPUT/TEXCRDOUT(6)=o#
static inline bool IsClipPosOut(uint32_t tok) {
    int t = RegType(tok), n = RegNum(tok);
    return (t == 4 && n == 0) || (t == 6 && n == 0);
}
static inline bool IsAnyOutput(uint32_t tok) {
    int t = RegType(tok);
    return t == 4 || t == 5 || t == 6 || t == 8 || t == 9;
}

static uint32_t HashBytes(const void* p, size_t n) {
    const uint8_t* b = (const uint8_t*)p;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 16777619u; }
    return h;
}

// Detects row-vector 4x4 matrix chains that write output registers:
//   mul rA, cN+1, v?.y ; mad rA, v?.x, cN, rA ; mad rA, v?.z, cN+2, rA ; add dst, rA, cN+3
//   m4x4 dst, src, cN
//   dp4 dst.x, src, cN ; dp4 dst.y, src, cN+1 ; ...
static void AnalyzeVsTokenStreamInner(const DWORD* fn, VsInfo& out) {
    if (!fn) return;
    // count tokens
    size_t nwords = 1; // token zero is the shader version, not an instruction
    while (nwords < 8192) {
        uint32_t tok = fn[nwords];
        if ((tok & 0xFFFF) == SI_END) { nwords++; break; }
        if ((tok & 0xFFFF) == SI_COMMENT) { nwords += 1 + ((tok >> 16) & 0x7FFF); continue; }
        nwords += 1 + ((tok >> 24) & 0xF);
    }

    struct Tag { int base; int rows; };
    struct RegTag { uint32_t key; Tag tag; };
    RegTag tags[64];
    int ntags = 0;
    auto setTag = [&](uint32_t key, Tag t) {
        for (int i = 0; i < ntags; ++i) if (tags[i].key == key) { tags[i].tag = t; return; }
        if (ntags < 64) tags[ntags++] = { key, t };
    };
    auto getTag = [&](uint32_t key, Tag* t) -> bool {
        for (int i = 0; i < ntags; ++i) if (tags[i].key == key) { *t = tags[i].tag; return true; }
        return false;
    };

    // per-op scratch: remember last mad/mul chain base per temp register
    struct Chain { uint32_t key; int base; int stage; };
    Chain chains[64];
    int nchains = 0;
    auto setChain = [&](uint32_t key, int base, int stage) {
        for (int i = 0; i < nchains; ++i)
            if (chains[i].key == key) { chains[i].base = base; chains[i].stage = stage; return; }
        if (nchains < 64) chains[nchains++] = { key, base, stage };
    };
    auto getChain = [&](uint32_t key, int* base, int* stage) -> bool {
        for (int i = 0; i < nchains; ++i)
            if (chains[i].key == key) { *base = chains[i].base; *stage = chains[i].stage; return true; }
        return false;
    };

    size_t i = 1;
    while (i < nwords) {
        uint32_t tok = fn[i];
        uint32_t op = tok & 0xFFFF;
        if (op == SI_END) break;
        if (op == SI_COMMENT) { i += 1 + ((tok >> 16) & 0x7FFF); continue; }
        uint32_t len = (tok >> 24) & 0xF;
        const DWORD* a = fn + i + 1;

        if (op == SI_M4x4 && len >= 3) {
            Tag t{ RegNum(a[2]), 4 };
            setTag(RegKey(a[0]), t);
        } else if (op == SI_M4x3 && len >= 3) {
            setTag(RegKey(a[0]), { RegNum(a[2]), 3 });
        } else if (op == SI_M3x4 && len >= 3) {
            setTag(RegKey(a[0]), { RegNum(a[2]), 3 });
        } else if (op == SI_MUL && len >= 3) {
            // mul rA, cN+1, v?.y  -> chain stage 1
            if (RegType(a[1]) == 2 && RegType(a[2]) == 1) {
                setChain(RegKey(a[0]), RegNum(a[1]) - 1, 1);
            }
        } else if (op == SI_MAD && len >= 4) {
            // mad rA, v?.x, cN, rA -> chain stage 2 ;  mad rA, v?.z, cN+2, rA -> stage 3
            if (RegType(a[1]) == 1 && RegType(a[2]) == 2) {
                int base = 0, stage = 0;
                if (getChain(RegKey(a[3]), &base, &stage)) {
                    if (RegNum(a[2]) == base && stage == 1) setChain(RegKey(a[0]), base, 2);
                    else if (RegNum(a[2]) == base + 2 && stage == 2) setChain(RegKey(a[0]), base, 3);
                }
            }
        } else if (op == SI_ADD && len >= 3) {
            // add dst, rChain, cN+3  -> matrix complete (rows=4)
            int base = 0, stage = 0;
            if (RegType(a[2]) == 2 && getChain(RegKey(a[1]), &base, &stage) &&
                stage == 3 && RegNum(a[2]) == base + 3) {
                setTag(RegKey(a[0]), { base, 4 });
            }
        } else if (op == SI_MOV && len >= 2) {
            Tag t;
            if (getTag(RegKey(a[1]), &t)) setTag(RegKey(a[0]), t);
        }

        // output write?
        if (len >= 1 && IsAnyOutput(a[0]) && op != SI_DCL && op != SI_DEF) {
            Tag t;
            if (getTag(RegKey(a[0]), &t)) {
                if (IsClipPosOut(a[0]) && out.wvpBase < 0) out.wvpBase = t.base;
                else if (!IsClipPosOut(a[0]) && out.stpBase < 0) out.stpBase = t.base;
            }
            if (IsClipPosOut(a[0]) && op == SI_DP4 && len >= 3 && RegType(a[2]) == 2) {                // dp4 o0.x, v0, cN  => scan forward for the y/z/w companions
                int base = RegNum(a[2]);
                size_t j = i + 1 + len;
                int comps = 1;
                while (j < nwords && comps < 4) {
                    uint32_t t2 = fn[j];
                    uint32_t op2 = t2 & 0xFFFF;
                    if (op2 == SI_COMMENT) { j += 1 + ((t2 >> 16) & 0x7FFF); continue; }
                    uint32_t l2 = (t2 >> 24) & 0xF;
                    const DWORD* a2 = fn + j + 1;
                    if (op2 == SI_DP4 && l2 >= 3 && RegKey(a2[0]) == RegKey(a[0]) &&
                        RegType(a2[2]) == 2 && RegNum(a2[2]) == base + comps) comps++;
                    else break;
                    j += 1 + l2;
                }
                if (comps == 4 && out.wvpBase < 0) out.wvpBase = base;
            }
            if (!IsClipPosOut(a[0])) out.depthOnly = false;   // color/aux output -> not a depth-only pass
        }
        if (op == SI_M3x4 || op == SI_M3x3 || op == SI_M4x3) {
            // skinning palette likely
            if (RegNum(a[2]) >= 32) out.hasBones = true;
        }
        i += 1 + len;
    }
    out.analyzed = true;
}

// SEH-guarded entry: shader bytecode must never crash the game
static void AnalyzeVsTokenStream(const DWORD* fn, VsInfo& out) {
    if (!fn) return;
    __try {
        AnalyzeVsTokenStreamInner(fn, out);
    } __except (1 /* EXCEPTION_EXECUTE_HANDLER */) {
        out.wvpBase = -1;
        out.stpBase = -1;
        out.analyzed = true;
    }
}

static size_t SafeShaderTokenCount(const DWORD* fn) {
    size_t n = 1;
    __try {
        if (!fn || (fn[0] >> 16) != 0xFFFE || ((fn[0] >> 8) & 0xFF) < 2) return 0;
        while (n < 8192) {
            const DWORD token = fn[n];
            if ((token & 0xFFFF) == SI_END) return n;
            const size_t length = (token & 0xFFFF) == SI_COMMENT
                ? ((token >> 16) & 0x7FFF) : ((token >> 24) & 0xF);
            if (length >= 8192 - n) return 0;
            n += 1 + length;
        }
    } __except (1) {
        return 0;
    }
    return 0;
}

// ------------------------------------------------------------- stereo constant injection
struct StereoParams {
    float separation = 0.0f;   // shear coefficient (screen-space, per eye sign applied)
    float convergence = 2.0f;  // in clip-w units (world-ish distance of zero disparity)
    int mode = 0;              // 0 = translation (clip.x += Px*dx, VR-correct), 1 = shear sep*(w-conv)
    float transX = 0.0f;       // translation mode: eye offset along view X in world units (IPD/2)
    float transY = 0.0f;
    float fovScaleX = 1.0f;    // <1 widens the rendered horizontal FOV (clip.x *= fovScaleX)
    float fovScaleY = 1.0f;
};

// Runtime projection estimates (Px=1/tan(halfFovX)) live in the enclosing namespace
// (g_estPx/g_estPy/g_estValid declared before this include).

// Folds the per-eye transform into a row-major 4x4 matrix block (4 registers).
// Row-vector convention (verified from engine HLSL): clip = x*c0 + y*c1 + z*c2 + c3
//   clip.x constant term lives in c3.x = p[12]; clip.w = sum(v_i * c_i.w).
static void ApplyEyeShiftToMatrix(float* p, int n, const StereoParams& sp) {
    if (n < 4) return;
    const float l0 = std::sqrt(p[0] * p[0] + p[4] * p[4] + p[8] * p[8]);
    const float l1 = std::sqrt(p[1] * p[1] + p[5] * p[5] + p[9] * p[9]);
    const float lw = std::sqrt(p[3] * p[3] + p[7] * p[7] + p[11] * p[11]);
    const float px = (lw > 1e-5f) ? (l0 / lw) : 1.0f;
    const float py = (lw > 1e-5f) ? (l1 / lw) : 1.0f;
    if (px > 0.2f && px < 8.0f && py > 0.2f && py < 16.0f) {
        g_estPx = px;
        g_estPy = py;
        InterlockedExchange(&g_estValid, 1);
    }

    // FOV widen: scaling rows x/y shrinks NDC for the same view angle = wider frustum
    if (sp.fovScaleX != 1.0f) {
        for (int i = 0; i < 4; ++i) p[i * 4 + 0] *= sp.fovScaleX;
    }
    if (sp.fovScaleY != 1.0f) {
        for (int i = 0; i < 4; ++i) p[i * 4 + 1] *= sp.fovScaleY;
    }

    if (sp.mode == 0) {
        // VR-correct camera translation: clip.x += Px*dx (screen shift = k/w -> 0 at infinity,
        // absolute depth scale stays natural). Row-vector constant row = p[12..15].
        p[12] += px * sp.transX;
        p[13] += py * sp.transY;
    } else {
        // HelixMod shear: clip.x += sep*(clip.w - conv)
        for (int i = 0; i < 4; ++i) {
            p[i * 4 + 0] += sp.separation * p[i * 4 + 3];
        }
        p[12] -= sp.separation * sp.convergence;
    }
}

// ------------------------------------------------------------- frame recording
enum RecOp : uint16_t {
    OP_END = 0,
    OP_SetRenderTarget, OP_SetDepthStencilSurface, OP_BeginScene, OP_EndScene,
    OP_Clear, OP_SetTransform, OP_SetViewport, OP_SetRenderState, OP_SetTexture,
    OP_SetTextureStageState, OP_SetSamplerState, OP_SetScissorRect,
    OP_DrawPrimitive, OP_DrawIndexedPrimitive, OP_DrawPrimitiveUP, OP_DrawIndexedPrimitiveUP,
    OP_SetVertexDeclaration, OP_SetFVF, OP_SetVertexShader,
    OP_SetVSConstF, OP_SetVSConstI, OP_SetVSConstB,
    OP_SetStreamSource, OP_SetStreamSourceFreq, OP_SetIndices,
    OP_SetPixelShader, OP_SetPSConstF, OP_SetPSConstI, OP_SetPSConstB,
    OP_StretchRect, OP_ColorFill, OP_UpdateSurface, OP_UpdateTexture,
    OP_SetClipStatus, OP_PresentMarker, OP_ApplyStateBlock
};

struct Rec {
    uint32_t op;
    uint32_t n;      // generic arg count / data length
    uint32_t a[6];
};

// State blocks can bind multiple states at once. A zero VS histogram was also
// caused by Hook_SetVertexShader failing to emit its record, not proof of their use.
typedef HRESULT(STDMETHODCALLTYPE* PFN_SBApply)(IDirect3DStateBlock9*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SBCapture)(IDirect3DStateBlock9*);
static PFN_SBApply g_origSBApply = nullptr;
static PFN_SBCapture g_origSBCapture = nullptr;
static int g_sbHooked = 0;

static std::vector<Rec> g_rec;
static std::vector<uint8_t> g_recData;
static bool g_recActive = false;
static bool g_recOverflow = false;
static uint64_t g_recCounts[40] = { 0 };
static uint64_t g_unhookedStateBlock = 0;
#define HOOK_MARK(n) (g_lastHookId = (n))

// The engine can issue device calls from more than one thread - guard the record stream.
static volatile LONG g_recLock = 0;
struct RecLockGuard {
    RecLockGuard() {
        while (InterlockedCompareExchange(&g_recLock, 1, 0) != 0) { Sleep(0); }
    }
    ~RecLockGuard() { InterlockedExchange(&g_recLock, 0); }
};

// D3D state blocks exclude render targets and depth surfaces. Keep those explicitly.
struct DeviceSnapshot {
    IDirect3DStateBlock9* state = nullptr;
    IDirect3DSurface9* targets[4] = {};
    IDirect3DSurface9* depth = nullptr;
    DWORD targetCount = 0;
    ~DeviceSnapshot() { Release(); }
    void Release() {
        if (state) { state->Release(); state = nullptr; }
        for (auto& t : targets) if (t) { t->Release(); t = nullptr; }
        if (depth) { depth->Release(); depth = nullptr; }
    }
    bool Capture(IDirect3DDevice9* dev) {
        Release();
        D3DCAPS9 caps{};
        if (FAILED(dev->GetDeviceCaps(&caps))) return false;
        targetCount = std::min<DWORD>(4, caps.NumSimultaneousRTs);
        if (FAILED(dev->CreateStateBlock(D3DSBT_ALL, &state))) return false;
        for (DWORD i = 0; i < targetCount; ++i) dev->GetRenderTarget(i, &targets[i]);
        dev->GetDepthStencilSurface(&depth);
        return targets[0] != nullptr;
    }
    void Apply(IDirect3DDevice9* dev) {
        if (!state) return;
        for (DWORD i = 0; i < targetCount; ++i) dev->SetRenderTarget(i, targets[i]);
        dev->SetDepthStencilSurface(depth);
        // Apply state AFTER RTs because SetRenderTarget resets the viewport.
        if (g_origSBApply) g_origSBApply(state); else state->Apply();
    }
};
static DeviceSnapshot g_frameStart;
static std::vector<IUnknown*> g_recordRefs;
static void ReleaseRecording() {
    g_recActive = false;
    g_rec.clear();
    g_recData.clear();
    for (auto* ref : g_recordRefs) ref->Release();
    g_recordRefs.clear();
    g_frameStart.Release();
}

static void RecBeginFrame(IDirect3DDevice9* dev) {
    RecLockGuard lock;
    ReleaseRecording();
    if (!g_frameStart.Capture(dev)) return;
    if (g_rec.capacity() < 200000) g_rec.reserve(200000);
    if (g_recData.capacity() < (4u << 20)) g_recData.reserve(4u << 20);
    g_recOverflow = false;
    g_recActive = true;
}
// Crash-safe data capture: never let diagnostic recording kill the game.
static inline bool RecOnRenderThread() {
    // The engine records from a worker thread (texture uploads). Those calls are texture
    // *content* updates which the replay deliberately skips, so dropping them entirely
    // removes the last cross-thread access to the record stream.
    return GetCurrentThreadId() == g_renderThreadId;
}

static size_t ClampReadableBytes(const void* p, size_t bytes) {
    const uint8_t* b = (const uint8_t*)p;
    size_t done = 0;
    while (done < bytes) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(b + done, &mbi, sizeof(mbi))) break;
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) break;
        const uint8_t* regionEnd = (const uint8_t*)mbi.BaseAddress + mbi.RegionSize;
        size_t chunk = (size_t)(regionEnd - (b + done));
        if (chunk == 0) break;
        done += chunk;
    }
    return done;
}
static void SafeMemcpy(void* dst, const void* src, size_t bytes) {
    __try {
        memcpy(dst, src, bytes);
    } __except (1) {
        memset(dst, 0, bytes);
    }
}

static inline uint32_t RecPush(const void* data, size_t bytes) {
    RecLockGuard lock;
    if (bytes > (size_t)8 * 1024 * 1024) bytes = 0;   // sanity clamp
    uint32_t off = (uint32_t)g_recData.size();
    g_recData.resize(off + bytes);
    if (bytes) {
        const size_t readable = ClampReadableBytes(data, bytes);
        if (readable < bytes) {
            // game passed a smaller buffer than the draw parameters imply - copy what exists
            memset(g_recData.data() + off, 0, bytes);
            if (readable) SafeMemcpy(g_recData.data() + off, data, readable);
        } else {
            SafeMemcpy(g_recData.data() + off, data, bytes);
        }
    }
    return off;
}

static inline void RecEmit(RecOp op, uint32_t n, uint32_t a0 = 0, uint32_t a1 = 0,
                           uint32_t a2 = 0, uint32_t a3 = 0, uint32_t a4 = 0, uint32_t a5 = 0) {
    if (!g_recActive || g_recOverflow) return;
    RecLockGuard lock;
    if (g_rec.size() >= 400000) { g_recOverflow = true; return; }
    Rec r;
    r.op = op; r.n = n;
    r.a[0] = a0; r.a[1] = a1; r.a[2] = a2; r.a[3] = a3; r.a[4] = a4; r.a[5] = a5;
    // Keep COM objects alive until replay finishes. This preserves object lifetime,
    // not the contents of dynamic buffers overwritten later in the same frame.
    int pointerArg = -1;
    switch (op) {
    case OP_SetRenderTarget: case OP_SetTexture: case OP_SetStreamSource:
        pointerArg = 1; break;
    case OP_SetDepthStencilSurface: case OP_SetVertexDeclaration:
    case OP_SetVertexShader: case OP_SetIndices: case OP_SetPixelShader:
    case OP_ColorFill: case OP_ApplyStateBlock:
        pointerArg = 0; break;
    default: break;
    }
    if (pointerArg >= 0 && r.a[pointerArg]) {
        auto* ref = reinterpret_cast<IUnknown*>((uintptr_t)r.a[pointerArg]);
        ref->AddRef(); g_recordRefs.push_back(ref);
    }
    if (op == OP_StretchRect) {
        for (int i = 0; i < 2; ++i) if (r.a[i]) {
            auto* ref = reinterpret_cast<IUnknown*>((uintptr_t)r.a[i]);
            ref->AddRef(); g_recordRefs.push_back(ref);
        }
    }
    g_rec.push_back(r);
    if (op < 40) g_recCounts[op]++;
}
static inline void RecEmitData(RecOp op, const void* data, size_t bytes,
                               uint32_t a0 = 0, uint32_t a1 = 0, uint32_t a2 = 0, uint32_t a3 = 0) {
    if (!g_recActive || g_recOverflow) return;
    uint32_t off = RecPush(data, bytes);
    RecEmit(op, (uint32_t)(bytes / 4), a0, a1, a2, a3, off);
}

// replay context: original device functions
struct OrigFns {
    HRESULT(STDMETHODCALLTYPE* SetRenderTarget)(IDirect3DDevice9*, DWORD, IDirect3DSurface9*) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetDepthStencilSurface)(IDirect3DDevice9*, IDirect3DSurface9*) = nullptr;
    HRESULT(STDMETHODCALLTYPE* BeginScene)(IDirect3DDevice9*) = nullptr;
    HRESULT(STDMETHODCALLTYPE* EndScene)(IDirect3DDevice9*) = nullptr;
    HRESULT(STDMETHODCALLTYPE* Clear)(IDirect3DDevice9*, DWORD, const D3DRECT*, DWORD, D3DCOLOR, float, DWORD) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetTransform)(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, const D3DMATRIX*) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetViewport)(IDirect3DDevice9*, const D3DVIEWPORT9*) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetRenderState)(IDirect3DDevice9*, D3DRENDERSTATETYPE, DWORD) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetTexture)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetTextureStageState)(IDirect3DDevice9*, DWORD, D3DTEXTURESTAGESTATETYPE, DWORD) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetSamplerState)(IDirect3DDevice9*, DWORD, D3DSAMPLERSTATETYPE, DWORD) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetScissorRect)(IDirect3DDevice9*, const RECT*) = nullptr;
    HRESULT(STDMETHODCALLTYPE* DrawPrimitive)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT) = nullptr;
    HRESULT(STDMETHODCALLTYPE* DrawIndexedPrimitive)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT) = nullptr;
    HRESULT(STDMETHODCALLTYPE* DrawPrimitiveUP)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, const void*, UINT) = nullptr;
    HRESULT(STDMETHODCALLTYPE* DrawIndexedPrimitiveUP)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT, UINT, const void*, D3DFORMAT, const void*, UINT) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetVertexDeclaration)(IDirect3DDevice9*, IDirect3DVertexDeclaration9*) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetFVF)(IDirect3DDevice9*, DWORD) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetVertexShader)(IDirect3DDevice9*, IDirect3DVertexShader9*) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetVertexShaderConstantF)(IDirect3DDevice9*, UINT, const float*, UINT) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetVertexShaderConstantI)(IDirect3DDevice9*, UINT, const int*, UINT) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetVertexShaderConstantB)(IDirect3DDevice9*, UINT, const BOOL*, UINT) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetStreamSource)(IDirect3DDevice9*, UINT, IDirect3DVertexBuffer9*, UINT, UINT) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetStreamSourceFreq)(IDirect3DDevice9*, UINT, UINT) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetIndices)(IDirect3DDevice9*, IDirect3DIndexBuffer9*) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetPixelShader)(IDirect3DDevice9*, IDirect3DPixelShader9*) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetPixelShaderConstantF)(IDirect3DDevice9*, UINT, const float*, UINT) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetPixelShaderConstantI)(IDirect3DDevice9*, UINT, const int*, UINT) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetPixelShaderConstantB)(IDirect3DDevice9*, UINT, const BOOL*, UINT) = nullptr;
    HRESULT(STDMETHODCALLTYPE* StretchRect)(IDirect3DDevice9*, IDirect3DSurface9*, const RECT*, IDirect3DSurface9*, const RECT*, D3DTEXTUREFILTERTYPE) = nullptr;
    HRESULT(STDMETHODCALLTYPE* ColorFill)(IDirect3DDevice9*, IDirect3DSurface9*, const RECT*, D3DCOLOR) = nullptr;
    HRESULT(STDMETHODCALLTYPE* UpdateSurface)(IDirect3DDevice9*, IDirect3DSurface9*, const RECT*, IDirect3DSurface9*, const POINT*) = nullptr;
    HRESULT(STDMETHODCALLTYPE* UpdateTexture)(IDirect3DDevice9*, IDirect3DBaseTexture9*, IDirect3DBaseTexture9*) = nullptr;
    HRESULT(STDMETHODCALLTYPE* SetClipStatus)(IDirect3DDevice9*, const D3DCLIPSTATUS9*) = nullptr;
    HRESULT(STDMETHODCALLTYPE* CreateVertexShader)(IDirect3DDevice9*, const DWORD*, IDirect3DVertexShader9**) = nullptr;
};
static OrigFns g_orig;

// ------------------------------------------------------------- state
static StereoParams g_stereo;          // current eye params (live pass = eye 0)
static int g_replayEye = -1;           // -1 = live/mono, 0/1 = eye during replay
static int g_stereoMode = 1;           // 0 = mono, 1 = same-frame dual eye, 2 = validation replay (shift 0)
static std::unordered_map<IDirect3DVertexShader9*, VsInfo> g_vsInfo;
static VsInfo* g_activeVs = nullptr;
static bool g_shaderAnalysisLog = false;
static volatile LONG g_shiftCount = 0;      // diagnostics: matrix uploads shifted per frame

// ------------------------------------------------------------- VS create/set hooks
static HRESULT STDMETHODCALLTYPE Hook_CreateVertexShader(IDirect3DDevice9* dev, const DWORD* fn,
                                                         IDirect3DVertexShader9** out) {
    HRESULT hr = g_orig.CreateVertexShader(dev, fn, out);
    if (SUCCEEDED(hr) && out && *out && fn) {
        size_t n = SafeShaderTokenCount(fn);
        if (n == 0) return hr;
        uint32_t h = HashBytes(fn, (n + 1) * 4);
        VsInfo vi;
        vi.hash = h;
        AnalyzeVsTokenStream(fn, vi);
        bool isNew = false;
        {
            RecLockGuard lock;   // shader map is shared - the engine creates shaders off-thread
            isNew = (g_vsInfo.find(*out) == g_vsInfo.end());
            if (isNew) g_vsInfo[*out] = vi;
        }
        // Diagnostic: dump raw shader bytecode for offline pattern analysis
        {
            static int s_dumpedShaders = 0;
            if (s_dumpedShaders < 8 && n > 0) {
                char spath[MAX_PATH];
                sprintf_s(spath, "test-output\\shaders\\vs_%04d_%08X.bin", s_dumpedShaders, h);
                CreateDirectoryA("test-output", nullptr);
                CreateDirectoryA("test-output\\shaders", nullptr);
                FILE* sf = nullptr;
                if (fopen_s(&sf, spath, "wb") == 0 && sf) {
                    fwrite(fn, 4, n + 1, sf);
                    fclose(sf);
                    Log("[ShaderDump] %s (%zu tokens)", spath, n + 1);
                }
                ++s_dumpedShaders;
            }
        }
        static int s_logged = 0;
        if (isNew && (s_logged < 15 || ((vi.wvpBase >= 0 || vi.stpBase >= 0) && s_logged < 40))) {
            ++s_logged;
            Log("[VS %08X] wvpBase=c%d stpBase=c%d depthOnly=%d bones=%d",
                h, vi.wvpBase, vi.stpBase, (int)vi.depthOnly, (int)vi.hasBones);
        }
    }
    return hr;
}

static HRESULT STDMETHODCALLTYPE Hook_SetVertexShader(IDirect3DDevice9* dev, IDirect3DVertexShader9* sh) {
    if (g_recActive) RecEmit(OP_SetVertexShader, 0, (uint32_t)(uintptr_t)sh);
    if (sh) {
        RecLockGuard lock;
        auto it = g_vsInfo.find(sh);
        g_activeVs = (it != g_vsInfo.end()) ? &it->second : nullptr;
    } else {
        g_activeVs = nullptr;
    }
    return g_orig.SetVertexShader(dev, sh);
}

// ------------------------------------------------------------- constant injection
// Returns true if the upload was modified (copies into caller buffer).
// Two strategies:
//   1. Shader analysis: shift exactly the matrix blocks that feed o0 (WorldViewProjection)
//      and screen-space outputs (ScreenTextureProjection).
//   2. Fallback (engine register semantics, table at 0x00712800): every upload at c0 with
//      count >= 4 starts with the per-object WorldViewProjection (c0..c3); LitInstance also
//      carries ScreenTextureProjection at c8..c11. c4..c7 (World) must NOT be touched.
static bool ApplyShiftToUpload(float* dst, const float* src, UINT start, UINT count,
                               const VsInfo* vi, const StereoParams& sp) {
    bool modified = false;
    int bases[4] = { -1, -1, -1, -1 };
    int nbases = 0;

    const bool haveAnalysis = vi && vi->analyzed && vi->wvpBase >= 0;
    if (haveAnalysis) {
        bases[nbases++] = vi->wvpBase;
        if (vi->stpBase >= 0) bases[nbases++] = vi->stpBase;
    } else if (start == 0 && count >= 4) {
        // Engine semantics: c0..c3 = WorldViewProjection. Skip 2D/ortho blocks
        // (w-column near zero) so HUD/menus don't get disparity.
        const float* m = src;   // block at c0
        const float wlen = std::sqrt(m[3] * m[3] + m[7] * m[7] + m[11] * m[11]);
        if (wlen > 0.02f) {
            bases[nbases++] = 0;
            if (count >= 12) bases[nbases++] = 8;   // LitInstance: ScreenTextureProjection
        }
    }

    for (int k = 0; k < nbases; ++k) {
        int b = bases[k];
        if (b < 0) continue;
        if ((int)start > b || (int)(start + count) < b + 4) continue;
        if (!modified) {
            memcpy(dst, src, sizeof(float) * 4 * count);
            modified = true;
        }
        {
            static int s_shiftLog = 0;
            if (s_shiftLog < 6) {
                ++s_shiftLog;
                float* q = dst + (b - (int)start) * 4;
                const float* q0 = src + (b - (int)start) * 4;
                const float l0 = std::sqrt(q0[0] * q0[0] + q0[4] * q0[4] + q0[8] * q0[8]);
                const float lw = std::sqrt(q0[3] * q0[3] + q0[7] * q0[7] + q0[11] * q0[11]);
                Log("[ShiftDbg] c%d start=%u count=%u | l0=%.3f lw=%.3f px=%.3f | c3.x %.4f -> will add %.4f | sp mode=%d transX=%.3f",
                    b, start, count, l0, lw, (lw > 1e-5f) ? l0 / lw : 0.0f,
                    q0[12], ((lw > 1e-5f) ? l0 / lw : 1.0f) * sp.transX, sp.mode, sp.transX);
                (void)q;
            }
        }
        ApplyEyeShiftToMatrix(dst + (b - (int)start) * 4, 4, sp);
        ++g_shiftCount;
    }
    return modified;
}

// ------------------------------------------------------------- recording hooks
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetRenderTarget)(IDirect3DDevice9*, DWORD, IDirect3DSurface9*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_Clear)(IDirect3DDevice9*, DWORD, const D3DRECT*, DWORD, D3DCOLOR, float, DWORD);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetTransform)(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, const D3DMATRIX*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetViewport)(IDirect3DDevice9*, const D3DVIEWPORT9*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetRenderState)(IDirect3DDevice9*, D3DRENDERSTATETYPE, DWORD);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetTextureStageState)(IDirect3DDevice9*, DWORD, D3DTEXTURESTAGESTATETYPE, DWORD);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetSamplerState)(IDirect3DDevice9*, DWORD, D3DSAMPLERSTATETYPE, DWORD);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetScissorRect)(IDirect3DDevice9*, const RECT*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_DrawPrimitive)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* PFN_DrawIndexedPrimitive)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* PFN_DrawPrimitiveUP)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, const void*, UINT);
typedef HRESULT(STDMETHODCALLTYPE* PFN_DrawIndexedPrimitiveUP)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT, UINT, const void*, D3DFORMAT, const void*, UINT);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetVertexDeclaration)(IDirect3DDevice9*, IDirect3DVertexDeclaration9*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetFVF)(IDirect3DDevice9*, DWORD);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetVSConstI)(IDirect3DDevice9*, UINT, const int*, UINT);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetVSConstB)(IDirect3DDevice9*, UINT, const BOOL*, UINT);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetStreamSource)(IDirect3DDevice9*, UINT, IDirect3DVertexBuffer9*, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetStreamSourceFreq)(IDirect3DDevice9*, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetIndices)(IDirect3DDevice9*, IDirect3DIndexBuffer9*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetPixelShader)(IDirect3DDevice9*, IDirect3DPixelShader9*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetPSConstF)(IDirect3DDevice9*, UINT, const float*, UINT);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetPSConstI)(IDirect3DDevice9*, UINT, const int*, UINT);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetPSConstB)(IDirect3DDevice9*, UINT, const BOOL*, UINT);
typedef HRESULT(STDMETHODCALLTYPE* PFN_StretchRect)(IDirect3DDevice9*, IDirect3DSurface9*, const RECT*, IDirect3DSurface9*, const RECT*, D3DTEXTUREFILTERTYPE);
typedef HRESULT(STDMETHODCALLTYPE* PFN_ColorFill)(IDirect3DDevice9*, IDirect3DSurface9*, const RECT*, D3DCOLOR);
typedef HRESULT(STDMETHODCALLTYPE* PFN_UpdateSurface)(IDirect3DDevice9*, IDirect3DSurface9*, const RECT*, IDirect3DSurface9*, const POINT*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_UpdateTexture)(IDirect3DDevice9*, IDirect3DBaseTexture9*, IDirect3DBaseTexture9*);

static HRESULT STDMETHODCALLTYPE Hook_SetRenderTarget(IDirect3DDevice9* dev, DWORD idx, IDirect3DSurface9* s) {
    if (g_recActive) RecEmit(OP_SetRenderTarget, 0, idx, (uint32_t)(uintptr_t)s);
    return g_orig.SetRenderTarget(dev, idx, s);
}
static HRESULT STDMETHODCALLTYPE Hook_Clear(IDirect3DDevice9* dev, DWORD count, const D3DRECT* rects,
                                            DWORD flags, D3DCOLOR color, float z, DWORD st) {
    if (g_recActive && rects && count) {
        uint32_t off = RecPush(rects, sizeof(D3DRECT) * count);
        RecEmit(OP_Clear, count, flags, color, (uint32_t&)z, st, off);
    } else if (g_recActive) {
        RecEmit(OP_Clear, 0, flags, color, (uint32_t&)z, st);
    }
    return g_orig.Clear(dev, count, rects, flags, color, z, st);
}
static HRESULT STDMETHODCALLTYPE Hook_SetTransform(IDirect3DDevice9* dev, D3DTRANSFORMSTATETYPE st, const D3DMATRIX* m) {
    if (g_recActive) { uint32_t off = RecPush(m, sizeof(D3DMATRIX)); RecEmit(OP_SetTransform, 0, st, off); }
    return g_orig.SetTransform(dev, st, m);
}
static HRESULT STDMETHODCALLTYPE Hook_SetViewport(IDirect3DDevice9* dev, const D3DVIEWPORT9* vp) {
    if (g_recActive) { uint32_t off = RecPush(vp, sizeof(D3DVIEWPORT9)); RecEmit(OP_SetViewport, 0, off); }
    return g_orig.SetViewport(dev, vp);
}
static HRESULT STDMETHODCALLTYPE Hook_SetRenderState(IDirect3DDevice9* dev, D3DRENDERSTATETYPE st, DWORD v) {
    if (g_recActive) RecEmit(OP_SetRenderState, 0, st, v);
    return g_orig.SetRenderState(dev, st, v);
}
static HRESULT STDMETHODCALLTYPE Hook_SetTextureStageState(IDirect3DDevice9* dev, DWORD stage, D3DTEXTURESTAGESTATETYPE t, DWORD v) {
    if (g_recActive) RecEmit(OP_SetTextureStageState, 0, stage, t, v);
    return g_orig.SetTextureStageState(dev, stage, t, v);
}
static HRESULT STDMETHODCALLTYPE Hook_SetSamplerState(IDirect3DDevice9* dev, DWORD smp, D3DSAMPLERSTATETYPE t, DWORD v) {
    if (g_recActive) RecEmit(OP_SetSamplerState, 0, smp, t, v);
    return g_orig.SetSamplerState(dev, smp, t, v);
}
static HRESULT STDMETHODCALLTYPE Hook_SetScissorRect(IDirect3DDevice9* dev, const RECT* r) {
    if (g_recActive) {
        if (r) { uint32_t off = RecPush(r, sizeof(RECT)); RecEmit(OP_SetScissorRect, 1, off); }
        else RecEmit(OP_SetScissorRect, 0);
    }
    return g_orig.SetScissorRect(dev, r);
}
static HRESULT STDMETHODCALLTYPE Hook_DrawPrimitive(IDirect3DDevice9* dev, D3DPRIMITIVETYPE t, UINT sv, UINT pc) {
    if (g_recActive) RecEmit(OP_DrawPrimitive, 0, t, sv, pc);
    return g_orig.DrawPrimitive(dev, t, sv, pc);
}
static HRESULT STDMETHODCALLTYPE Hook_DrawIndexedPrimitive(IDirect3DDevice9* dev, D3DPRIMITIVETYPE t,
    INT bvi, UINT mvi, UINT nv, UINT si, UINT pc) {
    if (g_recActive) RecEmit(OP_DrawIndexedPrimitive, 0, t, (uint32_t)bvi, mvi, nv, si, pc);
    return g_orig.DrawIndexedPrimitive(dev, t, bvi, mvi, nv, si, pc);
}
static HRESULT STDMETHODCALLTYPE Hook_DrawPrimitiveUP(IDirect3DDevice9* dev, D3DPRIMITIVETYPE t,
    UINT pc, const void* vb, UINT stride) {
    HOOK_MARK(83);
    if (g_recActive) {
        UINT verts = (t == D3DPT_TRIANGLESTRIP || t == D3DPT_TRIANGLEFAN) ? pc + 2 :
                     (t == D3DPT_TRIANGLELIST) ? pc * 3 :
                     (t == D3DPT_LINESTRIP) ? pc + 1 : (t == D3DPT_LINELIST) ? pc * 2 : pc + 1;
        if (verts < (1u << 20) && vb) {
            uint32_t off = RecPush(vb, (size_t)stride * verts);
            RecEmit(OP_DrawPrimitiveUP, 0, t, pc, stride, off);
        } else {
            RecEmit(OP_DrawPrimitiveUP, 1, t, pc, stride, (uint32_t)(uintptr_t)vb);   // data skipped (too large / null)
        }
    }
    return g_orig.DrawPrimitiveUP(dev, t, pc, vb, stride);
}
static HRESULT STDMETHODCALLTYPE Hook_DrawIndexedPrimitiveUP(IDirect3DDevice9* dev, D3DPRIMITIVETYPE t,
    UINT mvi, UINT nv, UINT pc, const void* ib, D3DFORMAT fmt, const void* vb, UINT stride) {
    HOOK_MARK(84);
    if (g_recActive) {
        UINT verts = nv;
        UINT idxSize = (fmt == D3DFMT_INDEX16) ? 2 : 4;
        UINT idxCount = (t == D3DPT_TRIANGLELIST) ? pc * 3 :
                        (t == D3DPT_TRIANGLESTRIP || t == D3DPT_TRIANGLEFAN) ? pc + 2 :
                        (t == D3DPT_LINELIST) ? pc * 2 : pc + 1;
        // Single atomic record: aux fields live in the data arena (no adjacent-record pairing,
        // which breaks when the engine records from a second thread between two RecEmit calls).
        struct DipAux { uint32_t vbOff, stride, ibOff, idxCount, idxSize; };
        DipAux aux{};
        aux.stride = stride;
        aux.idxCount = idxCount;
        aux.idxSize = idxSize;
        if (verts < (1u << 20) && vb && ib) {
            aux.vbOff = RecPush(vb, (size_t)stride * verts);
            aux.ibOff = RecPush(ib, (size_t)idxSize * idxCount);
        } else {
            aux.vbOff = (uint32_t)(uintptr_t)vb;
            aux.ibOff = (uint32_t)(uintptr_t)ib;
            aux.stride |= 0x80000000u;   // mark: pointers are live, not arena offsets
        }
        uint32_t auxOff = RecPush(&aux, sizeof(aux));
        RecEmit(OP_DrawIndexedPrimitiveUP, 0, t, mvi, nv, pc, (uint32_t)fmt, auxOff);
    }
    return g_orig.DrawIndexedPrimitiveUP(dev, t, mvi, nv, pc, ib, fmt, vb, stride);
}
static HRESULT STDMETHODCALLTYPE Hook_SetVertexDeclaration(IDirect3DDevice9* dev, IDirect3DVertexDeclaration9* d) {
    if (g_recActive) RecEmit(OP_SetVertexDeclaration, 0, (uint32_t)(uintptr_t)d);
    return g_orig.SetVertexDeclaration(dev, d);
}
static HRESULT STDMETHODCALLTYPE Hook_SetFVF(IDirect3DDevice9* dev, DWORD fvf) {
    if (g_recActive) RecEmit(OP_SetFVF, 0, fvf);
    return g_orig.SetFVF(dev, fvf);
}
static HRESULT STDMETHODCALLTYPE Hook_SetVSConstI(IDirect3DDevice9* dev, UINT s, const int* d, UINT n) {
    if (g_recActive) RecEmitData(OP_SetVSConstI, d, sizeof(int) * 4 * n, s, n);
    return g_orig.SetVertexShaderConstantI(dev, s, d, n);
}
static HRESULT STDMETHODCALLTYPE Hook_SetVSConstB(IDirect3DDevice9* dev, UINT s, const BOOL* d, UINT n) {
    if (g_recActive) RecEmitData(OP_SetVSConstB, d, sizeof(BOOL) * n, s, n);
    return g_orig.SetVertexShaderConstantB(dev, s, d, n);
}
static HRESULT STDMETHODCALLTYPE Hook_SetStreamSource(IDirect3DDevice9* dev, UINT num, IDirect3DVertexBuffer9* vb, UINT off, UINT stride) {
    if (g_recActive) RecEmit(OP_SetStreamSource, 0, num, (uint32_t)(uintptr_t)vb, off, stride);
    return g_orig.SetStreamSource(dev, num, vb, off, stride);
}
static HRESULT STDMETHODCALLTYPE Hook_SetStreamSourceFreq(IDirect3DDevice9* dev, UINT num, UINT freq) {
    if (g_recActive) RecEmit(OP_SetStreamSourceFreq, 0, num, freq);
    return g_orig.SetStreamSourceFreq(dev, num, freq);
}
static HRESULT STDMETHODCALLTYPE Hook_SetIndices(IDirect3DDevice9* dev, IDirect3DIndexBuffer9* ib) {
    if (g_recActive) RecEmit(OP_SetIndices, 0, (uint32_t)(uintptr_t)ib);
    return g_orig.SetIndices(dev, ib);
}
static HRESULT STDMETHODCALLTYPE Hook_SetPixelShader(IDirect3DDevice9* dev, IDirect3DPixelShader9* ps) {
    if (g_recActive) RecEmit(OP_SetPixelShader, 0, (uint32_t)(uintptr_t)ps);
    return g_orig.SetPixelShader(dev, ps);
}
static HRESULT STDMETHODCALLTYPE Hook_SetPSConstF(IDirect3DDevice9* dev, UINT s, const float* d, UINT n) {
    if (g_recActive) RecEmitData(OP_SetPSConstF, d, sizeof(float) * 4 * n, s, n);
    return g_orig.SetPixelShaderConstantF(dev, s, d, n);
}
static HRESULT STDMETHODCALLTYPE Hook_SetPSConstI(IDirect3DDevice9* dev, UINT s, const int* d, UINT n) {
    if (g_recActive) RecEmitData(OP_SetPSConstI, d, sizeof(int) * 4 * n, s, n);
    return g_orig.SetPixelShaderConstantI(dev, s, d, n);
}
static HRESULT STDMETHODCALLTYPE Hook_SetPSConstB(IDirect3DDevice9* dev, UINT s, const BOOL* d, UINT n) {
    if (g_recActive) RecEmitData(OP_SetPSConstB, d, sizeof(BOOL) * n, s, n);
    return g_orig.SetPixelShaderConstantB(dev, s, d, n);
}
static HRESULT STDMETHODCALLTYPE Hook_StretchRect(IDirect3DDevice9* dev, IDirect3DSurface9* a, const RECT* ra,
    IDirect3DSurface9* b, const RECT* rb, D3DTEXTUREFILTERTYPE f) {
    if (g_recActive) {
        uint32_t oa = 0, ob = 0;
        if (ra) oa = RecPush(ra, sizeof(RECT));
        if (rb) ob = RecPush(rb, sizeof(RECT));
        RecEmit(OP_StretchRect, (ra ? 1 : 0) | (rb ? 2 : 0), (uint32_t)(uintptr_t)a, (uint32_t)(uintptr_t)b, f, oa, ob);
    }
    return g_orig.StretchRect(dev, a, ra, b, rb, f);
}
static HRESULT STDMETHODCALLTYPE Hook_ColorFill(IDirect3DDevice9* dev, IDirect3DSurface9* s, const RECT* r, D3DCOLOR c) {
    if (g_recActive) {
        uint32_t off = 0;
        if (r) off = RecPush(r, sizeof(RECT));
        RecEmit(OP_ColorFill, r ? 1 : 0, (uint32_t)(uintptr_t)s, c, off);
    }
    return g_orig.ColorFill(dev, s, r, c);
}
static HRESULT STDMETHODCALLTYPE Hook_UpdateSurface(IDirect3DDevice9* dev, IDirect3DSurface9* a, const RECT* ra,
    IDirect3DSurface9* b, const POINT* p) {
    if (g_recActive) {
        uint32_t oa = 0, op = 0;
        if (ra) oa = RecPush(ra, sizeof(RECT));
        if (p) op = RecPush(p, sizeof(POINT));
        RecEmit(OP_UpdateSurface, (ra ? 1 : 0) | (p ? 2 : 0), (uint32_t)(uintptr_t)a, (uint32_t)(uintptr_t)b, oa, op);
    }
    return g_orig.UpdateSurface(dev, a, ra, b, p);
}
static HRESULT STDMETHODCALLTYPE Hook_UpdateTexture(IDirect3DDevice9* dev, IDirect3DBaseTexture9* a, IDirect3DBaseTexture9* b) {
    if (g_recActive) RecEmit(OP_UpdateTexture, 0, (uint32_t)(uintptr_t)a, (uint32_t)(uintptr_t)b);
    return g_orig.UpdateTexture(dev, a, b);
}

// ------------------------------------------------------------- replay executor
static void ReplayStream(IDirect3DDevice9* dev, int eye, const StereoParams& sp) {
    g_replayEye = eye;
    static int s_dumpFrames = 0;
    const bool dump = (s_dumpFrames < 3);
    if (dump) ++s_dumpFrames;
    // Hold the record lock for the whole replay: the engine records from a second thread
    // (texture uploads) and a concurrent push_back would invalidate g_rec references here.
    while (InterlockedCompareExchange(&g_recLock, 1, 0) != 0) { Sleep(0); }
    const uint8_t* dataBase = g_recData.data();   // captured AFTER the lock (arena stable)
    if (g_replayTestClear) {
        // diagnostic: flood the current render target magenta before replaying; if the eye1
        // capture does not turn magenta, the replay never reaches the captured surface.
        g_orig.Clear(dev, 0, nullptr, D3DCLEAR_TARGET, 0x00FF00FF, 1.0f, 0);
    }
    for (size_t i = 0; i < g_rec.size(); ++i) {
        const Rec& r = g_rec[i];
        if (dump) {
            Log("  rec[%zu] op=%u n=%u a=[%u %u %u %u %u %u]", i, r.op, r.n,
                r.a[0], r.a[1], r.a[2], r.a[3], r.a[4], r.a[5]);
        }
        HOOK_MARK(10000 + (LONG)r.op);
        __try {
        switch ((RecOp)r.op) {
        case OP_SetRenderTarget:
            g_orig.SetRenderTarget(dev, r.a[0], (IDirect3DSurface9*)(uintptr_t)r.a[1]);
            break;
        case OP_SetDepthStencilSurface:
            g_orig.SetDepthStencilSurface(dev, (IDirect3DSurface9*)(uintptr_t)r.a[0]);
            break;
        case OP_BeginScene:
            g_orig.BeginScene(dev);
            break;
        case OP_EndScene:
            g_orig.EndScene(dev);
            break;
        case OP_Clear: {
            const D3DRECT* rects = r.n ? (const D3DRECT*)(dataBase + r.a[4]) : nullptr;
            float z; *(uint32_t*)&z = r.a[2];
            g_orig.Clear(dev, r.n, rects, r.a[0], (D3DCOLOR)r.a[1], z, r.a[3]);
            break;
        }
        case OP_SetTransform:
            g_orig.SetTransform(dev, (D3DTRANSFORMSTATETYPE)r.a[0], (const D3DMATRIX*)(dataBase + r.a[1]));
            break;
        case OP_SetViewport:
            g_orig.SetViewport(dev, (const D3DVIEWPORT9*)(dataBase + r.a[0]));
            break;
        case OP_SetRenderState:
            g_orig.SetRenderState(dev, (D3DRENDERSTATETYPE)r.a[0], r.a[1]);
            break;
        case OP_SetTexture:
            g_orig.SetTexture(dev, r.a[0], (IDirect3DBaseTexture9*)(uintptr_t)r.a[1]);
            break;
        case OP_SetTextureStageState:
            g_orig.SetTextureStageState(dev, r.a[0], (D3DTEXTURESTAGESTATETYPE)r.a[1], r.a[2]);
            break;
        case OP_SetSamplerState:
            g_orig.SetSamplerState(dev, r.a[0], (D3DSAMPLERSTATETYPE)r.a[1], r.a[2]);
            break;
        case OP_SetScissorRect:
            g_orig.SetScissorRect(dev, r.n ? (const RECT*)(dataBase + r.a[0]) : nullptr);
            break;
        case OP_DrawPrimitive:
            g_orig.DrawPrimitive(dev, (D3DPRIMITIVETYPE)r.a[0], r.a[1], r.a[2]);
            break;
        case OP_DrawIndexedPrimitive:
            g_orig.DrawIndexedPrimitive(dev, (D3DPRIMITIVETYPE)r.a[0], (INT)r.a[1], r.a[2], r.a[3], r.a[4], r.a[5]);
            break;
        case OP_DrawPrimitiveUP: {
            HOOK_MARK(900 + r.op);
            const void* data = (r.n == 1) ? (const void*)(uintptr_t)r.a[3]
                                          : (const void*)(dataBase + r.a[3]);
            g_orig.DrawPrimitiveUP(dev, (D3DPRIMITIVETYPE)r.a[0], r.a[1], data, r.a[2]);
            HOOK_MARK(1900 + r.op);
            break;
        }
        case OP_DrawIndexedPrimitiveUP: {
            HOOK_MARK(900 + r.op);
            struct DipAux { uint32_t vbOff, stride, ibOff, idxCount, idxSize; };
            const DipAux* aux = (const DipAux*)(dataBase + r.a[5]);
            const bool livePtrs = (aux->stride & 0x80000000u) != 0;
            const UINT stride = aux->stride & 0x7FFFFFFFu;
            const void* vb = livePtrs ? (const void*)(uintptr_t)aux->vbOff : (const void*)(dataBase + aux->vbOff);
            const void* ib = livePtrs ? (const void*)(uintptr_t)aux->ibOff : (const void*)(dataBase + aux->ibOff);
            g_orig.DrawIndexedPrimitiveUP(dev, (D3DPRIMITIVETYPE)r.a[0], r.a[1], r.a[2], r.a[3],
                                         ib, (D3DFORMAT)r.a[4], vb, stride);
            HOOK_MARK(1900 + r.op);
            break;
        }
        case OP_SetVertexDeclaration:
            g_orig.SetVertexDeclaration(dev, (IDirect3DVertexDeclaration9*)(uintptr_t)r.a[0]);
            break;
        case OP_SetFVF:
            g_orig.SetFVF(dev, r.a[0]);
            break;
        case OP_SetVertexShader: {
            IDirect3DVertexShader9* sh = (IDirect3DVertexShader9*)(uintptr_t)r.a[0];
            auto it = g_vsInfo.find(sh);
            g_activeVs = (it != g_vsInfo.end()) ? &it->second : nullptr;
            g_orig.SetVertexShader(dev, sh);
            break;
        }
        case OP_SetVSConstF: {
            const float* src = (const float*)(dataBase + r.a[4]);
            // MUST hold Vector4fCount vectors of 4 floats each (bone palettes upload up to
            // 68 vectors = 272 floats). The old size (17*4) overflowed the stack -> fail-fast.
            float tmp[68 * 4];
            if (eye >= 0 && r.a[1] <= 68) {
                if (ApplyShiftToUpload(tmp, src, r.a[0], r.a[1], g_activeVs, sp)) src = tmp;
            }
            g_orig.SetVertexShaderConstantF(dev, r.a[0], src, r.a[1]);
            break;
        }
        case OP_SetVSConstI:
            g_orig.SetVertexShaderConstantI(dev, r.a[0], (const int*)(dataBase + r.a[4]), r.a[1]);
            break;
        case OP_SetVSConstB:
            g_orig.SetVertexShaderConstantB(dev, r.a[0], (const BOOL*)(dataBase + r.a[4]), r.a[1]);
            break;
        case OP_SetStreamSource:
            g_orig.SetStreamSource(dev, r.a[0], (IDirect3DVertexBuffer9*)(uintptr_t)r.a[1], r.a[2], r.a[3]);
            break;
        case OP_SetStreamSourceFreq:
            g_orig.SetStreamSourceFreq(dev, r.a[0], r.a[1]);
            break;
        case OP_SetIndices:
            g_orig.SetIndices(dev, (IDirect3DIndexBuffer9*)(uintptr_t)r.a[0]);
            break;
        case OP_SetPixelShader:
            g_orig.SetPixelShader(dev, (IDirect3DPixelShader9*)(uintptr_t)r.a[0]);
            break;
        case OP_SetPSConstF:
            g_orig.SetPixelShaderConstantF(dev, r.a[0], (const float*)(dataBase + r.a[4]), r.a[1]);
            break;
        case OP_SetPSConstI:
            g_orig.SetPixelShaderConstantI(dev, r.a[0], (const int*)(dataBase + r.a[4]), r.a[1]);
            break;
        case OP_SetPSConstB:
            g_orig.SetPixelShaderConstantB(dev, r.a[0], (const BOOL*)(dataBase + r.a[4]), r.a[1]);
            break;
        case OP_StretchRect:
            g_orig.StretchRect(dev, (IDirect3DSurface9*)(uintptr_t)r.a[0],
                              (r.n & 1) ? (const RECT*)(dataBase + r.a[3]) : nullptr,
                              (IDirect3DSurface9*)(uintptr_t)r.a[1],
                              (r.n & 2) ? (const RECT*)(dataBase + r.a[4]) : nullptr,
                              (D3DTEXTUREFILTERTYPE)r.a[2]);
            break;
        case OP_ColorFill:
            g_orig.ColorFill(dev, (IDirect3DSurface9*)(uintptr_t)r.a[0],
                            r.n ? (const RECT*)(dataBase + r.a[2]) : nullptr, (D3DCOLOR)r.a[1]);
            break;
        case OP_UpdateSurface:
            // Texture *content* updates are shared between both eyes and the replay runs
            // after the live pass - the target already holds the data. Replaying them is
            // redundant AND dangerous (the game releases temp textures within the frame).
            break;
        case OP_UpdateTexture:
            break;
        case OP_ApplyStateBlock: {
            HOOK_MARK(900 + r.op);
            IDirect3DStateBlock9* sb = (IDirect3DStateBlock9*)(uintptr_t)r.a[0];
            // call the ORIGINAL Apply: the vtable points at our hook which would try to
            // record while the record lock is held here -> deadlock
            if (sb && g_origSBApply) g_origSBApply(sb);
            HOOK_MARK(1900 + r.op);
            break;
        }
        default:
            break;
        }
        } __except (1) {
            // The NVIDIA/D3D9 runtime can raise on stale resource pointers recorded
            // earlier in the frame (temp textures). Skip the call - never kill the game.
            static int s_replayErrs = 0;
            if (s_replayErrs++ < 20) {
                Log("[TrueStereo] replay: exception in record %zu op=%u a=[%u %u] - skipped",
                    i, r.op, r.a[0], r.a[1]);
            }
        }
    }
    InterlockedExchange(&g_recLock, 0);
    g_replayEye = -1;
}

// ------------------------------------------------------------- state block hooks
static HRESULT STDMETHODCALLTYPE Hook_SBApply(IDirect3DStateBlock9* sb) {
    if (g_recActive) RecEmit(OP_ApplyStateBlock, 0, (uint32_t)(uintptr_t)sb);
    return g_origSBApply ? g_origSBApply(sb) : D3D_OK;
}

static void PatchStateBlock(IDirect3DStateBlock9* sb) {
    if (!sb || g_sbHooked || !g_stateBlockHookEnabled) return;
    void** vt = *reinterpret_cast<void***>(sb);
    // The runtime object layout can differ from the SDK header (we were burned twice).
    // Classify the tiny methods by their epilogue: "ret" (0xC3) = 0 stack args (Capture/Apply),
    // "ret 4" (0xC2 0x04 0x00) = 1 stack arg (GetDevice). Only hook when slots 4 and 5 are
    // BOTH 0-arg methods, which matches the documented Capture(4)/Apply(5) order.
    auto argcOf = [](void* fn) -> int {
        if (!fn) return -1;
        const uint8_t* p = (const uint8_t*)fn;
        __try {
            for (int i = 0; i < 96; ++i) {
                if (p[i] == 0xC2) return (p[i + 1] == 0x04 && p[i + 2] == 0x00) ? 1 : -2;
                if (p[i] == 0xC3) return 0;
            }
        } __except (1) {
            return -1;
        }
        return -1;
    };
    const int a4 = argcOf(vt[4]);
    const int a5 = argcOf(vt[5]);
    if (a4 != 0 || a5 != 0) {
        Log("[StateBlock] vtable layout unexpected (slot4 argc=%d slot5 argc=%d) - hook skipped", a4, a5);
        g_sbHooked = -1;   // do not retry
        return;
    }
    DWORD old = 0;
    if (VirtualProtect(&vt[5], sizeof(void*), PAGE_READWRITE, &old)) {
        g_origSBApply = reinterpret_cast<PFN_SBApply>(vt[5]);
        vt[5] = reinterpret_cast<void*>(&Hook_SBApply);
        VirtualProtect(&vt[5], sizeof(void*), old, &old);
        g_sbHooked = 1;
        Log("[StateBlock] Apply@5 hooked (orig=%p) - shader/state binds now recorded", g_origSBApply);
    }
}

typedef HRESULT(STDMETHODCALLTYPE* PFN_CreateStateBlock)(IDirect3DDevice9*, D3DSTATEBLOCKTYPE, IDirect3DStateBlock9**);
typedef HRESULT(STDMETHODCALLTYPE* PFN_EndStateBlock)(IDirect3DDevice9*, IDirect3DStateBlock9**);
static PFN_CreateStateBlock g_origCreateStateBlock = nullptr;
static PFN_EndStateBlock g_origEndStateBlock = nullptr;

static HRESULT STDMETHODCALLTYPE Hook_CreateStateBlock(IDirect3DDevice9* dev, D3DSTATEBLOCKTYPE t,
                                                       IDirect3DStateBlock9** out) {
    HRESULT hr = g_origCreateStateBlock(dev, t, out);
    if (SUCCEEDED(hr) && out && *out) PatchStateBlock(*out);
    return hr;
}
static HRESULT STDMETHODCALLTYPE Hook_EndStateBlock(IDirect3DDevice9* dev, IDirect3DStateBlock9** out) {
    HRESULT hr = g_origEndStateBlock(dev, out);
    if (SUCCEEDED(hr) && out && *out) PatchStateBlock(*out);
    return hr;
}

// ------------------------------------------------------------- hook installation
static bool InstallReplayHooks(void* dev) {
    struct Slot { int idx; void** dst; void* hook; const char* name; };
#define ORIG(field) ((void**)&g_orig.field)
    Slot slots[] = {
        { 30, ORIG(UpdateSurface), (void*)&Hook_UpdateSurface, "UpdateSurface@30" },
        { 31, ORIG(UpdateTexture), (void*)&Hook_UpdateTexture, "UpdateTexture@31" },
        { 34, ORIG(StretchRect), (void*)&Hook_StretchRect, "StretchRect@34" },
        { 35, ORIG(ColorFill), (void*)&Hook_ColorFill, "ColorFill@35" },
        { 37, ORIG(SetRenderTarget), (void*)&Hook_SetRenderTarget, "SetRT@37" },
        { 43, ORIG(Clear), (void*)&Hook_Clear, "Clear@43" },
        { 44, ORIG(SetTransform), (void*)&Hook_SetTransform, "SetTransform@44" },
        { 47, ORIG(SetViewport), (void*)&Hook_SetViewport, "SetViewport@47" },
        { 57, ORIG(SetRenderState), (void*)&Hook_SetRenderState, "SetRS@57" },
        { 59, (void**)&g_origCreateStateBlock, (void*)&Hook_CreateStateBlock, "CreateStateBlock@59" },
        { 61, (void**)&g_origEndStateBlock, (void*)&Hook_EndStateBlock, "EndStateBlock@61" },
        { 67, ORIG(SetTextureStageState), (void*)&Hook_SetTextureStageState, "SetTSS@67" },
        { 69, ORIG(SetSamplerState), (void*)&Hook_SetSamplerState, "SetSamp@69" },
        { 75, ORIG(SetScissorRect), (void*)&Hook_SetScissorRect, "SetScissor@75" },
        { 81, ORIG(DrawPrimitive), (void*)&Hook_DrawPrimitive, "DP@81" },
        { 82, ORIG(DrawIndexedPrimitive), (void*)&Hook_DrawIndexedPrimitive, "DIP@82" },
        { 83, ORIG(DrawPrimitiveUP), (void*)&Hook_DrawPrimitiveUP, "DPUP@83" },
        { 84, ORIG(DrawIndexedPrimitiveUP), (void*)&Hook_DrawIndexedPrimitiveUP, "DIPUP@84" },
        { 87, ORIG(SetVertexDeclaration), (void*)&Hook_SetVertexDeclaration, "SetVDecl@87" },
        { 89, ORIG(SetFVF), (void*)&Hook_SetFVF, "SetFVF@89" },
        { 91, ORIG(CreateVertexShader), (void*)&Hook_CreateVertexShader, "CreateVS@91" },
        { 92, ORIG(SetVertexShader), (void*)&Hook_SetVertexShader, "SetVS@92" },
        { 96, ORIG(SetVertexShaderConstantI), (void*)&Hook_SetVSConstI, "SetVSCI@96" },
        { 98, ORIG(SetVertexShaderConstantB), (void*)&Hook_SetVSConstB, "SetVSCB@98" },
        { 100, ORIG(SetStreamSource), (void*)&Hook_SetStreamSource, "SetStream@100" },
        { 102, ORIG(SetStreamSourceFreq), (void*)&Hook_SetStreamSourceFreq, "SetStreamFreq@102" },
        { 104, ORIG(SetIndices), (void*)&Hook_SetIndices, "SetIndices@104" },
        { 107, ORIG(SetPixelShader), (void*)&Hook_SetPixelShader, "SetPS@107" },
        { 109, ORIG(SetPixelShaderConstantF), (void*)&Hook_SetPSConstF, "SetPSCF@109" },
        { 111, ORIG(SetPixelShaderConstantI), (void*)&Hook_SetPSConstI, "SetPSCI@111" },
        { 113, ORIG(SetPixelShaderConstantB), (void*)&Hook_SetPSConstB, "SetPSCB@113" },
    };
#undef ORIG
    int ok = 0;
    int installed = 0;
    for (Slot& s : slots) {
        const bool wantRecordHooks = (g_hookMask & 1) != 0;
        const bool wantVsHooks = (s.idx == 91 || s.idx == 92) ? ((g_hookMask & 4) != 0) : wantRecordHooks;
        if (!wantVsHooks) continue;
        void* orig = PatchVTable(dev, s.idx, s.hook);
        if (orig) { *s.dst = orig; ++ok; }
        ++installed;
    }
    Log("Replay hooks installed: %d/%d (mask=0x%X)", ok, installed, g_hookMask);
    return ok > 20;
}

} // namespace d2vr_replay

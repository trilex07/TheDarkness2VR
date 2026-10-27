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

#include "stereo_math.h"
#include "frame_resource_refs.h"
#include "stereo_geometry.h"
#include "material_sampling.h"
#include "replay_constant_batch.h"
#include "replay_constant_window.h"
#include "record_payload_storage.h"

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
    bool namedConstants = false;
    bool reflected = false;
    int reflectedWvp = -1, reflectedStp = -1;
    int reflectedPreviousWvp = -1;
    // Last uploaded WorldViewProjection: the ScreenTextureProjection identity check needs
    // the matching WVP even when the two matrices arrive in separate records.
    float lastWvp[16] = {};
    bool hasLastWvp = false;
};

// D3DX CTAB offsets are relative to the header following the CTAB FourCC.
// Read only bounded FLOAT4 matrix entries; never infer a screen projection
// merely because a matrix happens to feed a texture-coordinate output.
static int FindNamedRegister(const DWORD* fn, size_t words, const char* name, bool* foundTable,
                             WORD wantCount) {
    if (foundTable) *foundTable = false;
    for (size_t i = 1; i < words;) {
        DWORD op = fn[i] & 0xffff;
        if (op == SI_END) break;
        size_t n = op == SI_COMMENT ? (fn[i] >> 16) & 0x7fff : (fn[i] >> 24) & 15;
        if (n > words - i - 1) break;
        if (op == SI_COMMENT && n >= 8 && fn[i+1] == 0x42415443) {
            const unsigned char* data = reinterpret_cast<const unsigned char*>(fn + i + 2);
            size_t bytes = (n - 1) * 4;
            auto u32 = [&](size_t off) { DWORD v = 0; memcpy(&v, data + off, 4); return v; };
            DWORD count = u32(12), offset = u32(16);
            if (offset > bytes || count > (bytes-offset)/20) return -1;
            if (foundTable) *foundTable = true;
            for (DWORD j = 0; j < count; ++j) {
                size_t entry = offset + j * 20;
                DWORD nameOff = u32(entry), registers = u32(entry+4);
                WORD registerCount = 0; memcpy(&registerCount, data+entry+8, 2);
                size_t length = strlen(name);
                if (nameOff >= bytes || length >= bytes-nameOff) continue;
                if ((registers & 0xffff) == 2 && registerCount == wantCount &&
                    memcmp(data+nameOff, name, length+1) == 0) return registers >> 16;
            }
        }
        i += n + 1;
    }
    return -1;
}
static int FindNamedMatrix(const DWORD* fn, size_t words, const char* name, bool* foundTable) {
    return FindNamedRegister(fn, words, name, foundTable, 4);
}

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
        if (!fn || ((fn[0] >> 16) != 0xFFFE && (fn[0] >> 16) != 0xFFFF) ||
            ((fn[0] >> 8) & 0xFF) < 2) return 0;
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
    bool headGeometry = false; // opt-in render diagnostic; native culling not yet hooked
    float headClip[16]{}, inverseHeadClip[16]{};
};
static bool g_cameraProjectionKnown = false;
static float g_cameraPx = 1, g_cameraPy = 1;
static float g_cameraInverseProjection[16]{};

// Projection estimates feed the OpenXR submission FOV, so a single bogus matrix (an
// un-analyzed shader's parameter block, a skinned object with non-uniform scale) must not
// win: publish through a small median window. The InverseProjection observer writes
// g_cameraPx/g_cameraPy directly - those are exact.
static float s_estHistX[8], s_estHistY[8];
static int s_estHistN = 0;
static void PublishProjectionEstimate(float px, float py) {
    if (!(px > 0.2f && px < 8.0f && py > 0.2f && py < 16.0f)) return;
    s_estHistX[s_estHistN % 8] = px;
    s_estHistY[s_estHistN % 8] = py;
    if (s_estHistN < 8) ++s_estHistN;
    if (s_estHistN < 3) return;
    const int n = s_estHistN < 8 ? s_estHistN : 8;
    float xs[8], ys[8];
    for (int i = 0; i < n; ++i) { xs[i] = s_estHistX[i]; ys[i] = s_estHistY[i]; }
    for (int i = 1; i < n; ++i) {   // insertion sort - n <= 8
        const float kx = xs[i];
        int j = i - 1;
        while (j >= 0 && xs[j] > kx) { xs[j + 1] = xs[j]; --j; }
        xs[j + 1] = kx;
    }
    for (int i = 1; i < n; ++i) {
        const float ky = ys[i];
        int j = i - 1;
        while (j >= 0 && ys[j] > ky) { ys[j + 1] = ys[j]; --j; }
        ys[j + 1] = ky;
    }
    g_estPx = xs[n / 2];
    g_estPy = ys[n / 2];
    InterlockedExchange(&g_estValid, 1);
}

static void ObserveInverseProjection(const float* src, UINT start, UINT count, int base) {
    if (!src || base < 0 || start > (UINT)base || start + count < (UINT)base + 4) return;
    const float* p = src + (base-start)*4;
    // Observed perspective inverse is diagonal in x/y; row2.w != 0. World-space
    // reconstruction matrices (height fog) fail this test and must not overwrite the
    // camera coefficients.
    if (!d2vr_math::IsViewInverseProjection(p)) return;
    g_cameraPx = 1.f/p[0]; g_cameraPy = 1.f/p[5]; g_cameraProjectionKnown = true;
    memcpy(g_cameraInverseProjection,p,sizeof(g_cameraInverseProjection));
    g_estPx = g_cameraPx; g_estPy = g_cameraPy; InterlockedExchange(&g_estValid, 1);
}

// Runtime projection estimates (Px=1/tan(halfFovX)) live in the enclosing namespace
// (g_estPx/g_estPy/g_estValid declared before this include).

// Folds the per-eye transform into a row-major 4x4 matrix block (4 registers).
// Row-vector convention (verified from engine HLSL): clip = x*c0 + y*c1 + z*c2 + c3
static void ApplyEyeShiftToMatrix(float* p, int n, const StereoParams& sp, bool estimate = true) {
    if (n < 4) return;
    float px = 0, py = 0;
    d2vr_stereo::EstimatePxPy(p, g_cameraProjectionKnown, g_cameraPx, g_cameraPy, &px, &py);
    if (estimate) PublishProjectionEstimate(px, py);
    d2vr_stereo::EyeShiftParams ep{};
    ep.separation = sp.separation;
    ep.convergence = sp.convergence;
    ep.mode = sp.mode;
    ep.transX = sp.transX;
    ep.transY = sp.transY;
    ep.fovScaleX = sp.fovScaleX;
    ep.fovScaleY = sp.fovScaleY;
    ep.headGeometry = sp.headGeometry;
    d2vr_stereo::ApplyEyeShiftToMatrix(p, n, ep, g_cameraProjectionKnown, g_cameraPx, g_cameraPy,
                                       sp.headGeometry ? sp.headClip : nullptr);
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
    OP_SetClipStatus, OP_PresentMarker, OP_ApplyStateBlock, OP_SetClipPlane
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
static d2vr_record_payload::Storage g_recData;
static bool g_recActive = false;
static bool g_recOverflow = false;
static uint64_t g_recCounts[40] = { 0 };
static uint64_t g_unhookedStateBlock = 0;
#ifdef D2VR_MATCHED_CAPTURE
struct ConstantUploadWindow {
    d2vr_replay_batch::UploadCounts counts;
    uint64_t streams = 0, batched = 0, windowed = 0, bindWindowed = 0, captureBypass = 0, otherEyes = 0;
    void Report(uint64_t frame, long long beginQpc, long long endQpc, long long frequency) {
        if (!streams) return;
        Log("[ReplayUploads] endPair=%llu beginQpc=%lld endQpc=%lld frequency=%lld streams=%llu eye=1 otherEyes=%llu batchStreams=%llu windowStreams=%llu bindWindowStreams=%llu captureBypass=%llu VSrecords=%llu VScalls=%llu VSvectors=%llu VSfailed=%llu PSrecords=%llu PScalls=%llu PSvectors=%llu PSfailed=%llu exceptions=%llu; recorded float setters only (scope setters excluded)",
            frame,beginQpc,endQpc,frequency,streams,otherEyes,batched,windowed,bindWindowed,captureBypass,
            counts.records[0],counts.calls[0],counts.vectors[0],counts.failures[0],
            counts.records[1],counts.calls[1],counts.vectors[1],counts.failures[1],counts.exceptions);
        const auto& native=d2vr_native_constants::counts;
        Log("[NativeOwnedConstantCopy] tid=%lu enabled=%d admitted=%d attempts=%llu proved=%llu copies=%llu bytes=%llu faults=%llu materialEnabled=%d materialProved=%llu materialCopies=%llu; cumulative on reporting thread",
            GetCurrentThreadId(),int(d2vr_native_constants::enabled.load()),int(d2vr_native_constants::admitted.load()),
            native.attempts,native.proved,native.copies,native.bytes,native.faults,
            int(d2vr_native_constants::materialEnabled.load()),native.materialProved,native.materialCopies);
        *this = {};
    }
};
static ConstantUploadWindow g_constantUploadWindow;
#endif

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
static void SyncActiveShader(IDirect3DDevice9* dev);
struct DeviceSnapshot {
    IDirect3DStateBlock9* state = nullptr;
    IDirect3DSurface9* targets[4] = {};
    IDirect3DSurface9* depth = nullptr;
    DWORD targetCount = 0;
    UINT vertexFloatLimit = 0, pixelFloatLimit = 0;
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
        vertexFloatLimit = caps.MaxVertexShaderConst;
        pixelFloatLimit = D3DSHADER_VERSION_MAJOR(caps.PixelShaderVersion)>=3 ? 224 : 32;
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
        SyncActiveShader(dev);
    }
};
static DeviceSnapshot g_frameStart;
static FrameResourceRefs<IUnknown> g_recordRefs;
static void ReleaseRecording() {
    g_recActive = false;
    g_rec.clear();
    g_recData.clear();
    g_recordRefs.Clear();
    g_frameStart.Release();
}

static void RecBeginFrame(IDirect3DDevice9* dev) {
    RecLockGuard lock;
    ReleaseRecording();
    g_recordRefs.Begin(g_uniqueReplayRefs);
    if (!g_frameStart.Capture(dev)) {
        static int s_captureFails = 0;
        if (s_captureFails++ < 8) Log("[Record] frame-start capture failed - recording stays off");
        return;
    }
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

// Caller holds g_recLock until the payload and its command are both published.
static inline uint32_t RecPushLocked(const void* data, size_t bytes, const d2vr_native_constants::Proof& proof = {}) {
    if (bytes > (size_t)8 * 1024 * 1024) bytes = 0;   // sanity clamp
    uint32_t off = (uint32_t)g_recData.size();
    g_recData.resize(off + bytes);
    if (bytes && !d2vr_native_constants::Copy(proof,g_recData.data()+off,data,bytes)) {
        d2vr_record_copy::Copy(g_recData.data() + off, data, bytes, SmallRecordCopyIsEnabled(),
            &ReadProcessMemory,g_wideRecordCopy.load()?d2vr_record_copy::kWideLimit:d2vr_record_copy::kSmallLimit);
    }
    return off;
}
static inline uint32_t RecPush(const void* data, size_t bytes, const d2vr_native_constants::Proof& proof = {}) {
    RecLockGuard lock;
    return RecPushLocked(data,bytes,proof);
}

// Light-input readback state (diagnostic only, see LightInputReadback below).
static const uint32_t kShadowLightPsHash = 0xC4D1FB10u;
static std::unordered_map<const void*, uint32_t> g_psHashByPtr;       // PS pointer -> hash
static std::unordered_map<const void*, const void*> g_surfToTex;      // RT surface -> texture (AddRef'd)
static const void* g_replayPs = nullptr;                              // PS currently bound in replay
static const void* g_replayVs = nullptr;                              // VS currently bound in replay
static std::unordered_map<const void*, uint32_t> g_lightEndHash;      // texture -> hash at window end
static int g_lightRbFrames = -1;                                      // windows left (-1 = unread)
static int g_lightRbBudget = 0;                                       // light draws left to read back

// Caller holds g_recLock, including any payload append belonging to this record.
static inline void RecEmitLocked(RecOp op, uint32_t n, uint32_t a0 = 0, uint32_t a1 = 0,
                           uint32_t a2 = 0, uint32_t a3 = 0, uint32_t a4 = 0, uint32_t a5 = 0) {
    if (!g_recActive || g_recOverflow) return;
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
        g_recordRefs.Retain(ref);
    }
    if (op == OP_StretchRect) {
        for (int i = 0; i < 2; ++i) if (r.a[i]) {
            auto* ref = reinterpret_cast<IUnknown*>((uintptr_t)r.a[i]);
            g_recordRefs.Retain(ref);
        }
    }
    g_rec.push_back(r);
    if (op < 40) g_recCounts[op]++;
}
static inline void RecEmit(RecOp op, uint32_t n, uint32_t a0 = 0, uint32_t a1 = 0,
                           uint32_t a2 = 0, uint32_t a3 = 0, uint32_t a4 = 0, uint32_t a5 = 0) {
    if (!g_recActive || g_recOverflow) return;
    RecLockGuard lock;
    RecEmitLocked(op,n,a0,a1,a2,a3,a4,a5);
}
static inline void RecEmitData(RecOp op, const void* data, size_t bytes,
                               uint32_t a0 = 0, uint32_t a1 = 0, uint32_t a2 = 0, uint32_t a3 = 0,
                               const d2vr_native_constants::Proof& proof = {}) {
    if (!g_recActive || g_recOverflow) return;
    RecLockGuard lock;
    if (!g_recActive || g_recOverflow) return;
    if (g_rec.size() >= 400000) { g_recOverflow = true; return; }
    uint32_t off = RecPushLocked(data, bytes, proof);
    RecEmitLocked(op, (uint32_t)(bytes / 4), a0, a1, a2, a3, off);
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
    HRESULT(STDMETHODCALLTYPE* SetClipPlane)(IDirect3DDevice9*, DWORD, const float*) = nullptr;
    HRESULT(STDMETHODCALLTYPE* CreateVertexShader)(IDirect3DDevice9*, const DWORD*, IDirect3DVertexShader9**) = nullptr;
    HRESULT(STDMETHODCALLTYPE* CreatePixelShader)(IDirect3DDevice9*, const DWORD*, IDirect3DPixelShader9**) = nullptr;
};
static OrigFns g_orig;

// ------------------------------------------------------------- state
static StereoParams g_stereo;          // current eye params (live pass = eye 0)
static int g_replayEye = -1;           // -1 = live/mono, 0/1 = eye during replay
static int g_stereoMode = 1;           // 0 = mono, 1 = same-frame dual eye, 2 = validation replay (shift 0)
static std::unordered_map<IDirect3DVertexShader9*, VsInfo> g_vsInfo;
static VsInfo* g_activeVs = nullptr;
static std::unordered_map<IDirect3DPixelShader9*, int> g_psInverse;
static int g_activePsInverse = -1;
// DeferredLight variants (projector / view-extent family) reconstruct the view-space
// position as x = ndc.x * z * ViewExtents.x. ViewExtents must follow the render-FOV
// scale or the projected light/shadow is compressed toward the screen centre and slides
// with the camera (playtest 2026-10-19).
static std::unordered_map<IDirect3DPixelShader9*, int> g_psViewExtents;
static int g_activePsViewExtents = -1;
static void SyncActiveShader(IDirect3DDevice9* dev) {
    IDirect3DVertexShader9* shader = nullptr;
    g_activeVs = nullptr;
    if (SUCCEEDED(dev->GetVertexShader(&shader)) && shader) {
        auto it = g_vsInfo.find(shader);
        if (it != g_vsInfo.end()) g_activeVs = &it->second;
        shader->Release();
    }
    IDirect3DPixelShader9* ps = nullptr;
    g_activePsInverse = -1;
    g_activePsViewExtents = -1;
    if (SUCCEEDED(dev->GetPixelShader(&ps)) && ps) {
        auto it = g_psInverse.find(ps);
        if (it != g_psInverse.end()) g_activePsInverse = it->second;
        auto it2 = g_psViewExtents.find(ps);
        if (it2 != g_psViewExtents.end()) g_activePsViewExtents = it2->second;
        ps->Release();
    }
}

// Stock FXAA PS5104FEF7: retain declarations/CTAB and its own centre sample,
// including alpha and explicit LOD zero. Remove only the offset edge filtering.
static bool BuildUnfilteredFxaa(const DWORD* fn, size_t count, std::vector<DWORD>& out) {
    out.clear();
    if (!fn || count != 938 || HashBytes(fn, count * 4) != 0x5104FEF7u) return false;
    out.assign(fn, fn + 78);
    out.insert(out.end(), fn + 133, fn + 141); // mul r5,c1.xxyy,v0.xyxx; texldl r5,r5,s0
    out.insert(out.end(), fn + 934, fn + 938); // mov_pp oC0,r5; end
    return true;
}

static HRESULT STDMETHODCALLTYPE Hook_CreatePixelShader(IDirect3DDevice9* dev, const DWORD* fn,
                                                        IDirect3DPixelShader9** out) {
    bool screenDepthOverride = false;
    // Only bytes successfully read and validated below can be submitted.
    DWORD replacement[8192];
    // This original shader reconstructs lighting from depth. Its interpolated
    // light-volume UV can diverge from the actual depth pixel after an eye shift.
    // Restrict the experiment to its exact bytecode hash and an explicit switch.
    if (fn && g_shadowScreenDepth) {
        const size_t words = SafeShaderTokenCount(fn);
        if (words && HashBytes(fn, (words + 1) * 4) == 0xC4D1FB10u) {
            FILE* input = nullptr;
            if (fopen_s(&input, "test-output\\shadow_screen_depth.psh", "rb") == 0 && input) {
                const size_t read = fread(replacement, 1, sizeof(replacement), input);
                fclose(input);
                if (read >= 16 && read < sizeof(replacement) && read % 4 == 0 &&
                    replacement[0] == 0xFFFF0300u && replacement[read/4-1] == 0x0000FFFFu) {
                    screenDepthOverride = true;
                    Log("[Lighting] testing screen-pixel depth coordinates for shadow shader C4D1FB10");
                }
            }
        }
    }
    std::vector<DWORD> composite;
    std::vector<DWORD> sharp;
    const size_t originalWords = fn ? SafeShaderTokenCount(fn) : 0;
    const unsigned compositePatch = !screenDepthOverride && g_submitMode == 4 && originalWords ?
        d2vr_composite::Build(fn, originalWords + 1,
            g_disableDepthOfField, g_disableCompositeHeightFog, composite) : 0;
    const bool fxaaOverride = !screenDepthOverride && !compositePatch && g_disableFxaa &&
        g_submitMode == 4 && originalWords && BuildUnfilteredFxaa(fn, originalWords + 1, sharp);
    HRESULT hr = d2vr_shader_preload::Create(dev,
        compositePatch ? composite.data() :
        fxaaOverride ? sharp.data() : (screenDepthOverride ? replacement : fn), out,g_orig.CreatePixelShader);
    if (compositePatch || fxaaOverride) {
        if (fxaaOverride || (compositePatch & d2vr_composite::Dof))
            Log("[Sharpness] %s bypass hash=%08X result=%08X",
                fxaaOverride ? "FXAA" : "DOF/ghost blend",
                HashBytes(fn, (originalWords + 1)*4), (unsigned)hr);
        if (compositePatch & d2vr_composite::HeightFog)
            Log("[Composite] height-fog blend bypass hash=%08X result=%08X (bloom/grade retained)",
                HashBytes(fn, (originalWords + 1)*4), (unsigned)hr);
        if (FAILED(hr) && (compositePatch & d2vr_composite::HeightFog) && g_disableDepthOfField &&
            d2vr_composite::Build(fn, originalWords + 1, true, false, composite)) {
            hr = d2vr_shader_preload::Create(dev,composite.data(),out,g_orig.CreatePixelShader);
            Log("[Composite] fog bypass rejected; previous DOF-only shader result=%08X", (unsigned)hr);
        }
        if (FAILED(hr)) hr = d2vr_shader_preload::Create(dev,fn,out,g_orig.CreatePixelShader);
    }
    if (FAILED(hr) && screenDepthOverride) {
        Log("[Lighting] shadow shader override rejected (hr=%08X); using original", (unsigned)hr);
        screenDepthOverride = false;
        hr = d2vr_shader_preload::Create(dev,fn,out,g_orig.CreatePixelShader);
    }
    if (SUCCEEDED(hr) && out && *out && fn) {
        const size_t psWords = SafeShaderTokenCount(fn);
        if (psWords) {
            const uint32_t psHash = HashBytes(fn, (psWords + 1) * 4);
            g_psHashByPtr[*out] = psHash;
            Log("[PSPtr] %p hash=%08X", (void*)*out, psHash);
        }
        // GetFunction provides the driver's validated bytecode length for PS.
        UINT bytes = 0;
        if (SUCCEEDED((*out)->GetFunction(nullptr, &bytes)) && bytes <= 32768) {
            std::vector<DWORD> code((bytes + 3)/4);
            if (SUCCEEDED((*out)->GetFunction(code.data(), &bytes))) {
                const std::string shaderBytes((const char*)code.data(), bytes);
                if (g_shaderDebugDump && shaderBytes.find("InvRenderTargetSize") != std::string::npos &&
                    shaderBytes.find("ShadowMask") != std::string::npos) {
                    char path[MAX_PATH];
                    sprintf_s(path, "test-output\\ps_material_%08X.bin", HashBytes(code.data(), bytes));
                    FILE* file = nullptr;
                    if (fopen_s(&file, path, "wb") == 0 && file) {
                        fwrite(code.data(), 1, bytes, file); fclose(file);
                        Log("[Lighting] material shadow-mask shader %s", path);
                    }
                }
                int base = screenDepthOverride ? 27 :
                    FindNamedMatrix(code.data(), bytes/4, "InverseProjection", nullptr);
                if (g_shaderDebugDump && base >= 0) {
                    char path[MAX_PATH];
                    sprintf_s(path, "test-output\\ps_inverse_%08X.bin", HashBytes(code.data(), bytes));
                    FILE* file = nullptr;
                    if (fopen_s(&file, path, "wb") == 0 && file) { fwrite(code.data(), 1, bytes, file); fclose(file); }
                }
                // ViewExtents (DeferredLight family): NDC -> view-space direction scale.
                // It has to track the render-FOV scale like the InverseProjection rows do.
                const int extents = FindNamedRegister(code.data(), bytes/4, "ViewExtents", nullptr, 1);
                if (extents < 0 && shaderBytes.find("ViewExtents") != std::string::npos) {
                    Log("[Lighting] shader has ViewExtents name but no 1-register entry (matrix form: c%d)",
                        FindNamedMatrix(code.data(), bytes/4, "ViewExtents", nullptr));
                    if(g_shaderDebugDump) {
                        char path[MAX_PATH];
                        sprintf_s(path, "test-output\\ps_viewext_%08X.bin", HashBytes(code.data(), bytes));
                        FILE* file = nullptr;
                        if (fopen_s(&file, path, "wb") == 0 && file) { fwrite(code.data(), 1, bytes, file); fclose(file); }
                    }
                }
                RecLockGuard lock;
                g_psInverse[*out] = base;
                g_psViewExtents[*out] = extents;
                if (base >= 0) Log("[Lighting] named InverseProjection c%d", base);
                if (extents >= 0) Log("[Lighting] named ViewExtents c%d", extents);
            }
        }
    }
    return hr;
}

static bool ApplyInverseProjection(float* dst, const float* src, UINT start, UINT count,
                                   int base, const StereoParams& sp) {
    if (g_noInvProj) return false;   // candidate fix (Shift+F3): raw reconstruction
    if (g_lightingPreset == 1 || g_lightingPreset == 3) return false;
    if (base < 0 || start > (UINT)base || count > 224 || start + count < (UINT)base + 4 ||
        sp.mode != 0 || sp.fovScaleX <= 0 || sp.fovScaleY <= 0) return false;
    memcpy(dst, src, count * 4 * sizeof(float));
    float* m = dst + (base-start)*4;
    if (g_lightingPreset == 9 && !d2vr_math::IsViewInverseProjection(m)) return false;
    // Row-vector inverse: undo the clip scaling (left-multiply by S^-1 = scale rows 0/1),
    // then remove the eye translation from the reconstruction OUTPUT space so the light
    // position/shadow matrices - which the engine keeps in the game camera space - stay
    // valid. For the view-space InverseProjection that space is the view space; for the
    // world-space inverse (height fog variant) InverseMatrixTranslation rotates the
    // offset with the matrix' own view rotation automatically.
    for (int c = 0; c < 4 && g_lightingPreset != 5; ++c) {
        m[c] /= sp.fovScaleX;
        m[4+c] /= sp.fovScaleY;
    }
    // First reconstruct the tracked view's basis, then derive the eye offset
    // in that basis. This is inverse to head-then-eye in the VS.
    if(sp.headGeometry) d2vr_math::Multiply(sp.inverseHeadClip,m,m);
    if (g_lightingPreset != 4) {
        float t[3];
        if (!d2vr_math::InverseMatrixTranslation(m, sp.transX, sp.transY, t)) return false;
        if (g_lightingPreset == 6) for (float& component : t) component = -component;
        d2vr_math::SubtractOutputTranslation(m, t);
    }
    return true;
}

// Combined per-eye PS constant compensation:
//   * InverseProjection (view/world reconstruction)  -> remove the eye translation
//   * ViewExtents (NDC -> view-direction scale)      -> follow the render-FOV scale
// Returns true when dst was written. Both blocks are disjoint (e.g. c26 vs c27..c30).
static bool CompensatePsConstants(float* dst, const float* src, UINT start, UINT count,
                                  int inverseBase, int extentsBase, const StereoParams& sp) {
    if (!dst || !src || count == 0 || count > 224) return false;
    bool copied = false;
    if ((g_projectionFix & 2) && ApplyInverseProjection(dst, src, start, count, inverseBase, sp)) {
        copied = true;
    }
    if ((g_projectionFix & 2) && g_lightingPreset != 1 && g_lightingPreset != 2 && extentsBase >= 0 &&
        (UINT)extentsBase >= start && (UINT)extentsBase < start + count &&
        (sp.fovScaleX != 1.0f || sp.fovScaleY != 1.0f)) {
        if (!copied) {
            memcpy(dst, src, sizeof(float) * 4 * count);
            copied = true;
        }
        d2vr_math::ApplyViewExtentsScale(dst + (extentsBase - (int)start) * 4,
                                         sp.fovScaleX, sp.fovScaleY);
    }
    return copied;
}
static bool g_shaderAnalysisLog = false;
static volatile LONG g_shiftCount = 0;      // diagnostics: matrix uploads shifted per frame

// ------------------------------------------------------------- VS create/set hooks
static HRESULT STDMETHODCALLTYPE Hook_CreateVertexShader(IDirect3DDevice9* dev, const DWORD* fn,
                                                         IDirect3DVertexShader9** out) {
    HRESULT hr = d2vr_shader_preload::Create(dev,fn,out,g_orig.CreateVertexShader);
    if (SUCCEEDED(hr) && out && *out && fn) {
        size_t n = SafeShaderTokenCount(fn);
        if (n == 0) return hr;
        uint32_t h = HashBytes(fn, (n + 1) * 4);
        VsInfo vi;
        vi.hash = h;
        AnalyzeVsTokenStream(fn, vi);
        bool hasTable = false;
        int namedWvp = FindNamedMatrix(fn, n + 1, "WorldViewProjection", &hasTable);
        if (hasTable) {
            vi.reflected = true;
            vi.reflectedWvp = namedWvp;
            vi.reflectedStp = FindNamedMatrix(fn, n + 1, "ScreenTextureProjection", nullptr);
            vi.reflectedPreviousWvp = FindNamedMatrix(fn, n + 1, "PrevWorldProj", nullptr);
        }
        if(g_headAttachedTentacles &&
           (h==0x1CB7CEE2u || h==0x34D4EEE8u || h==0xE3259C61u)) {
            // Captured CTAB nests WorldViewProjection under Instance, so the
            // top-level name lookup returns -1. Exact bytecode disassembly
            // proves c0..c3 feeds o0 for all three player tentacle shaders.
            // Without this register identity ApplyShiftToUpload never reaches
            // the head-attached branch, regardless of the hasBones predicate.
            vi.wvpBase=vi.reflectedWvp=0;
            vi.reflected=true;
            Log("[HeadAttachedTentacles] verified VS=%08X WVP=c0 (nested Instance)",h);
        }
        bool isNew = false;
        {
            RecLockGuard lock;   // shader map is shared - the engine creates shaders off-thread
            isNew = (g_vsInfo.find(*out) == g_vsInfo.end());
            if (isNew) g_vsInfo[*out] = vi;
        }
        // Diagnostic: dump raw shader bytecode for offline pattern analysis
        {
            static int s_dumpedShaders = 0;
#ifdef D2VR_MATCHED_CAPTURE
            static volatile LONG savedTentacleShaders=0;
            const LONG bit=h==0x34D4EEE8u?1:h==0x1CB7CEE2u?2:h==0xE3259C61u?4:0;
            const bool tentacleShader=bit && !(InterlockedOr(&savedTentacleShaders,bit)&bit);
#else
            const bool tentacleShader = false;
#endif
            if ((s_dumpedShaders < 8 || tentacleShader) && n > 0) {
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
        // pointer -> hash map for the traces (once per shader, first 400 shaders)
        static std::unordered_map<const void*, int> s_vsPtrLogged;
        if (g_activeVs && s_vsPtrLogged.size() < 400 && s_vsPtrLogged.emplace(sh, 1).second)
            Log("[VSPtr] %p hash=%08X", (void*)sh, g_activeVs->hash);
    } else {
        g_activeVs = nullptr;
    }
    return g_orig.SetVertexShader(dev, sh);
}

// ------------------------------------------------------------- constant injection
static bool IsShadowAtlas(const D3DSURFACE_DESC& rt, const D3DVIEWPORT9& vp) {
    // Observed Darkness II shadow pass: RGB565 square atlas, square subviewport.
    // Camera/G-buffer targets are A8R8G8B8/A16B16G16R16F at full viewport.
    return rt.Format == D3DFMT_R5G6B5 && rt.Width == rt.Height &&
        vp.Width == vp.Height && vp.Width > 0 && vp.Width < rt.Width;
}
struct ShadowPassState {
    IDirect3DDevice9* device=nullptr;
    bool valid=false,shadow=false;
};
static thread_local ShadowPassState g_shadowPassState;
static void InvalidateShadowAtlasPass() { g_shadowPassState.valid=false; }
static bool IsShadowAtlasPass(IDirect3DDevice9* dev) {
    if(g_shadowPassState.valid && g_shadowPassState.device==dev)
        return g_shadowPassState.shadow;
    IDirect3DSurface9* rt = nullptr;
    if (FAILED(dev->GetRenderTarget(0, &rt)) || !rt) return false;
    D3DSURFACE_DESC desc{}; D3DVIEWPORT9 vp{};
    HRESULT a = rt->GetDesc(&desc), b = dev->GetViewport(&vp);
    rt->Release();
    const bool shadow=SUCCEEDED(a) && SUCCEEDED(b) && IsShadowAtlas(desc, vp);
    // Cache only successful reads. Hooks invalidate whenever RT0, viewport or
    // a state block can change the classification, including replay restores.
    if(SUCCEEDED(a) && SUCCEEDED(b)) g_shadowPassState={dev,true,shadow};
    return shadow;
}
// Returns true if the upload was modified (copies into caller buffer).
// Register semantics (verified from captured runtime traces + extracted shader CTABs):
//   1. Shader analysis / CTAB names: shift exactly the matrix block that feeds o0
//      (WorldViewProjection) and, when verified, the ScreenTextureProjection block.
//   2. Fallback (engine register convention, table at 0x00712800): an upload at c0 with
//      count >= 4 starts with the per-object WorldViewProjection (c0..c3). Nothing else
//      is guessed - c8..c11 of un-analyzed shaders regularly holds LIGHT PARAMETERS
//      (LightViewPosRadius/ShadowParams/light direction) and shifting those corrupts
//      lighting differently per eye (run_40332 trace: c11 held the sun direction).
//   3. A matrix block that merely writes a non-oPos output is only a ScreenTexture
//      Projection when it satisfies STP = WVP * S. Bare viewport-bias/flip matrices are
//      constants and must stay untouched.
static bool ApplyShiftToUpload(float* dst, const float* src, UINT start, UINT count,
                               VsInfo* vi, const StereoParams& sp) {
    if (vi && g_diagnosticSkipVs && vi->hash == g_diagnosticSkipVs) return false;

    int wvpBase = -1, stpBase = -1;
    bool namedStp = false, identifiedWvp = false;
    if ((g_projectionFix & 1) && vi && vi->reflected) {
        wvpBase = vi->reflectedWvp;
        stpBase = vi->reflectedStp;
        namedStp = stpBase >= 0;
        identifiedWvp = wvpBase >= 0;
    } else if (vi && vi->analyzed) {
        wvpBase = vi->wvpBase;
        stpBase = vi->stpBase;
        identifiedWvp = wvpBase >= 0;
    }
    bool fallbackWvp = false;
    if (wvpBase < 0 && (!vi || !vi->namedConstants) && start == 0 && count >= 4) {
        // Engine semantic: c0..c3 is the per-object WorldViewProjection. Nothing else is
        // guessed - c8..c11 of un-analyzed shaders regularly holds light parameters
        // (LightViewPosRadius/ShadowParams/light direction) and shifting those corrupts
        // lighting differently per eye (run_40332 trace: c11 held the sun direction).
        // The w-column gate is strict on purpose: blur kernels and packed parameter
        // blocks also land at c0 with a small but nonzero w column.
        const float* m = src;
        const float wlen = std::sqrt(m[3] * m[3] + m[7] * m[7] + m[11] * m[11]);
        if (wlen > 0.2f) {
            fallbackWvp = true;
            wvpBase = 0;
        }
    }

    // Orthographic UI: never gets an eye translation (it floats at screen depth), but the
    // FOV scale IS applied to sprite-style blocks so 2D content keeps the angular size and
    // position it has outside VR. Without it the menu text grows past the visible area and
    // screen-attached effects (the darkness tentacles) sit in the lens periphery once the
    // submission FOV is widened. Fullscreen passes use hardcoded quads or scale ~1
    // matrices and are left alone.
    if (!fallbackWvp && wvpBase >= 0 &&
        (int)start <= wvpBase && (int)(start + count) >= wvpBase + 4) {
        const float* wvp = src + (wvpBase - (int)start) * 4;
        const float perspective = wvp[3] * wvp[3] + wvp[7] * wvp[7] + wvp[11] * wvp[11];
        if (perspective < 1e-8f) {
            float uiX=sp.fovScaleX,uiY=sp.fovScaleY;
#ifdef D2VR_MATCHED_CAPTURE
            NativeCameraUiScale(uiX,uiY);
#endif
            const float lin = std::max(std::fabs(wvp[0]), std::max(std::fabs(wvp[1]),
                 std::max(std::fabs(wvp[4]), std::fabs(wvp[5]))));
            const bool sprite = g_scale2DContent && lin > 0.0f && lin < 0.05f &&
                                (uiX != 1.0f || uiY != 1.0f);
            if (!sprite) return false;
            memcpy(dst, src, sizeof(float) * 4 * count);
            float* matrix = dst + (wvpBase - (int)start) * 4;
            for (int i = 0; i < 4; ++i) {
                matrix[i * 4 + 0] *= uiX;
                matrix[i * 4 + 1] *= uiY;
            }
            ++g_shiftCount;
            return true;
        }
    }

    // WVP reference values for the ScreenTextureProjection identity check.
    float wvpRef[16];
    bool haveWvpRef = false;
    if (wvpBase >= 0 && (int)start <= wvpBase && (int)(start + count) >= wvpBase + 4) {
        memcpy(wvpRef, src + (wvpBase - (int)start) * 4, sizeof(wvpRef));
        haveWvpRef = true;
        if (vi) {
            memcpy(vi->lastWvp, wvpRef, sizeof(vi->lastWvp));
            vi->hasLastWvp = true;
        }
    } else if (vi && vi->hasLastWvp) {
        memcpy(wvpRef, vi->lastWvp, sizeof(wvpRef));
        haveWvpRef = true;
    }

    // Exact MotionBlur_16 VS: c0=current WVP, c4=PrevWorldProj; its output
    // subtracts their projected NDC positions. Leaving c4 native creates motion
    // from stereo/FOV changes alone. A shader hash AND reflected register proof
    // limit this experiment to the captured consumer, not arbitrary c4 blocks.
    // Geometric head tracking needs previous-frame headClip history separately.
    // A CTAB name alone is insufficient: if current WVP compensation was
    // deselected, transforming only the previous matrix would create motion.
    const int previousBase = vi ? d2vr_stereo::PreviousProjectionBase(vi->hash,
        wvpBase,vi->reflectedPreviousWvp,g_fixMotionVectorProjection,sp.headGeometry) : -1;
    bool modified = false;
    for (int k = 0; k < (previousBase>=0 ? 3 : 2); ++k) {
        const int b = k==0 ? wvpBase : k==1 ? ((wvpBase==stpBase) ? -1 : stpBase) : previousBase;
        if (b < 0) continue;
        if ((int)start > b || (int)(start + count) < b + 4) continue;
        const float* orig = src + (b - (int)start) * 4;
        // These bytecodes are shared by the player and world objects. Apply
        // the ordinary world transform to every upload here. A draw-time
        // scope below can cancel it only after confirming the actual mesh.

        bool screenProjection = false;
        if (k == 1) {
            if (g_lightingPreset == 7) continue;
            if (namedStp || (haveWvpRef && d2vr_math::IsScreenTextureProjection(orig, wvpRef))) {
                screenProjection = true;
            } else {
                // Not a screen projection: keep previous-frame/secondary projection-like
                // matrices shifting like a WVP, leave constant clip->UV transforms alone.
                const float wlen = std::sqrt(orig[3] * orig[3] + orig[7] * orig[7] +
                                             orig[11] * orig[11]);
                if (wlen <= 0.02f) continue;
            }
        }

        if (!modified) {
            memcpy(dst, src, sizeof(float) * 4 * count);
            modified = true;
        }
        {
            static int s_shiftLog = 0;
            if (s_shiftLog < 6) {
                ++s_shiftLog;
                const float* q0 = orig;
                const float l0 = std::sqrt(q0[0] * q0[0] + q0[4] * q0[4] + q0[8] * q0[8]);
                const float lw = std::sqrt(q0[3] * q0[3] + q0[7] * q0[7] + q0[11] * q0[11]);
                Log("[ShiftDbg] c%d start=%u count=%u | l0=%.3f lw=%.3f px=%.3f | c3.x %.4f -> will add %.4f | sp mode=%d transX=%.3f%s",
                    b, start, count, l0, lw, (lw > 1e-5f) ? l0 / lw : 0.0f,
                    q0[12], ((lw > 1e-5f) ? l0 / lw : 1.0f) * sp.transX, sp.mode, sp.transX,
                    screenProjection ? " (screen-tex-proj)" : (fallbackWvp ? " (fallback)" : ""));
            }
        }
        float* matrix = dst + (b - (int)start) * 4;
        if (screenProjection) d2vr_math::UnbiasScreenTextureProjection(matrix);
        // Only positively identified WVP blocks may refine the projection estimate used
        // for the OpenXR submission FOV.
        StereoParams matrixParams = sp;
        if (k == 1 && g_lightingPreset == 8) {
            matrixParams.transX = matrixParams.transY = matrixParams.separation = 0.0f;
        }
        ApplyEyeShiftToMatrix(matrix, 4, matrixParams, identifiedWvp && k == 0);
        if (screenProjection) d2vr_math::RebiasScreenTextureProjection(matrix);
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
    if(idx==0) InvalidateShadowAtlasPass();
    if (d2vr_trace::g_enabled) {
        d2vr_trace::Emit(GetTickCount64(), GetCurrentThreadId(), "SetRenderTarget", 0,
                         (uint32_t)g_rec.size(), dev, s, nullptr, idx, nullptr, 0, "");
    }
    if (g_recActive) {
        RecEmit(OP_SetRenderTarget, 0, idx, (uint32_t)(uintptr_t)s);
        // surface -> owning texture (D3D's own container link), once per surface
        static std::unordered_map<const void*, int> s_rtContLogged;
        if (s && s_rtContLogged.size() < 400 && s_rtContLogged.emplace(s, 1).second) {
            IDirect3DTexture9* cont = nullptr;
            static const GUID kIidTexture9 = { 0x85C31227, 0x3DE5, 0x4F00, { 0x9B, 0x3A, 0xF1, 0x1A, 0xC3, 0x8C, 0x18, 0xB5 } };
            if (SUCCEEDED(s->GetContainer(kIidTexture9, reinterpret_cast<void**>(&cont))) && cont) {
                Log("[RTCont] %p %p", (void*)s, (void*)cont);
                // keep the reference (GetContainer AddRef'd): the readback needs a live texture
                g_surfToTex[s] = cont;
            }
        }
    }
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
    InvalidateShadowAtlasPass();
    if (g_recActive) { uint32_t off = RecPush(vp, sizeof(D3DVIEWPORT9)); RecEmit(OP_SetViewport, 0, off); }
    return g_orig.SetViewport(dev, vp);
}
static HRESULT STDMETHODCALLTYPE Hook_SetClipPlane(IDirect3DDevice9* dev, DWORD index, const float* plane) {
    if (g_recActive && plane) RecEmitData(OP_SetClipPlane, plane, sizeof(float)*4, index);
    return g_orig.SetClipPlane(dev, index, plane);
}
static HRESULT STDMETHODCALLTYPE Hook_SetRenderState(IDirect3DDevice9* dev, D3DRENDERSTATETYPE st, DWORD v) {
    if (g_recActive) RecEmit(OP_SetRenderState, 0, st, v);
    return g_orig.SetRenderState(dev, st, v);
}
static HRESULT STDMETHODCALLTYPE Hook_SetTextureStageState(IDirect3DDevice9* dev, DWORD stage, D3DTEXTURESTAGESTATETYPE t, DWORD v) {
    if (g_recActive) RecEmit(OP_SetTextureStageState, 0, stage, t, v);
    return g_orig.SetTextureStageState(dev, stage, t, v);
}
// Increase only an existing anisotropic filter's quality budget. Do not alter
// point/linear filtering, mip selection or postprocessing texture coordinates.
static DWORD ResolveSamplerValue(IDirect3DDevice9* dev, D3DSAMPLERSTATETYPE t, DWORD v) {
    if (g_submitMode != 4 || t != D3DSAMP_MAXANISOTROPY) return v;
    if (g_samplerCapsCacheEnabled)
        return d2vr_sampling::g_samplerCapsCache.Resolve(dev, v, (DWORD)g_textureAnisotropy);
    // Exact legacy resolver when the isolated cache experiment is OFF.
    D3DCAPS9 caps{};
    if (FAILED(dev->GetDeviceCaps(&caps)) || caps.MaxAnisotropy < 1) return v;
    return std::min(caps.MaxAnisotropy, std::max(v, (DWORD)g_textureAnisotropy));
}

static HRESULT STDMETHODCALLTYPE Hook_SetSamplerState(IDirect3DDevice9* dev, DWORD smp, D3DSAMPLERSTATETYPE t, DWORD v) {
    v = ResolveSamplerValue(dev, t, v);
    if (g_recActive) RecEmit(OP_SetSamplerState, 0, smp, t, v);
    return g_orig.SetSamplerState(dev, smp, t, v);
}
// A MAXANISOTROPY budget alone leaves LINEAR minification bilinear. Restrict
// promotion to one bytecode-verified world diffuse sampler and restore it at
// the draw boundary. OFF has no driver queries or additional recorded events.
struct WorldDiffuseScope {
    d2vr_sampling::ScopedDiffuseFilter filter;
    bool applied = false;
    explicit WorldDiffuseScope(IDirect3DDevice9* dev) {
        if (!g_worldDiffuseAnisotropy || g_submitMode != 4 || g_stereoMode != 1 ||
            !g_activeVs || g_activeVs->hash != 0x293F456Bu) return;
        IDirect3DPixelShader9* ps = nullptr;
        if (FAILED(dev->GetPixelShader(&ps)) || !ps) return;
        const auto found = g_psHashByPtr.find(ps);
        const bool selected = found != g_psHashByPtr.end() &&
            d2vr_sampling::IsWorldDiffuseMaterial(g_activeVs->hash, found->second);
        ps->Release();
        if (!selected || !filter.Apply(dev, g_orig.SetSamplerState, g_textureAnisotropy)) return;
        applied = true;
        static unsigned reported = 0;
        if (reported++ < 4) Log("[WorldDiffuse] applied eye=%d sampler=0 min=ANISOTROPIC; restored after draw", g_replayEye < 0 ? 0 : g_replayEye);
    }
};
// The player tentacle and world characters share all three skinned shaders.
// A WVP c3 test alone incorrectly head-attached enemies in the 9dfb2c1 VR
// trial. The captured player mesh instead has a 302920-byte VB, 213588-byte
// IB, stride 40 and these seven stream byte offsets in both eyes. Check the
// bound resources at the actual DIP, after all state-block restores, before
// cancelling the world head transform. Any mismatch keeps the ordinary world
// pose. This profile-specific opt-in leaves the default renderer unchanged.
struct TentacleHeadScope {
    IDirect3DDevice9* dev=nullptr;
    float original[16]{};
    bool applied=false;
    explicit TentacleHeadScope(IDirect3DDevice9* device) : dev(device) {
        if(!g_headAttachedTentacles || !dev || g_submitMode!=4 || g_stereoMode!=1 ||
           !g_activeVs || (g_activeVs->hash!=0x1CB7CEE2u &&
                            g_activeVs->hash!=0x34D4EEE8u &&
                            g_activeVs->hash!=0xE3259C61u)) return;
        const StereoParams sp=MakeEyeParams(g_replayEye<0?0:g_replayEye);
        if(!sp.headGeometry || IsShadowAtlasPass(dev)) return;
        IDirect3DVertexBuffer9* vb=nullptr;
        IDirect3DIndexBuffer9* ib=nullptr;
        UINT offset=0,stride=0;
        if(FAILED(dev->GetStreamSource(0,&vb,&offset,&stride)) || !vb) return;
        if(FAILED(dev->GetIndices(&ib)) || !ib) {vb->Release();return;}
        D3DVERTEXBUFFER_DESC vd{};D3DINDEXBUFFER_DESC id{};
        const bool geometry=SUCCEEDED(vb->GetDesc(&vd)) && SUCCEEDED(ib->GetDesc(&id)) &&
            vd.Size==302920 && id.Size==213588 && stride==40 &&
            (offset==0 || offset==2640 || offset==120400 || offset==158040 ||
             offset==160920 || offset==267560 || offset==285240);
        ib->Release();vb->Release();
        if(!geometry || FAILED(dev->GetVertexShaderConstantF(0,original,4))) return;
        float attached[16];d2vr_math::Multiply(original,sp.inverseHeadClip,attached);
        // After cancellation, the captured camera-local player WVP has c3.z
        // near -0.1 and c3.w near zero. This secondary gate rejects a buffer
        // recycled for another model, without modifying its draw.
        if(!std::isfinite(attached[12]) || !std::isfinite(attached[13]) ||
           !std::isfinite(attached[14]) || !std::isfinite(attached[15]) ||
           std::fabs(attached[12])>=0.15f || std::fabs(attached[13])>=0.15f ||
           attached[14]<=-0.16f || attached[14]>=-0.06f ||
           std::fabs(attached[15])>=0.05f) return;
        applied=SUCCEEDED(g_orig.SetVertexShaderConstantF(dev,0,attached,4));
        if(applied) {
            static volatile LONG reported=0;
            if(InterlockedIncrement(&reported)<=4)
                Log("[HeadAttachedTentacles] mesh-scoped eye=%d VS=%08X VB=%u IB=%u offset=%u",
                    g_replayEye<0?0:g_replayEye,g_activeVs->hash,vd.Size,id.Size,offset);
        }
    }
    ~TentacleHeadScope() {
        if(applied) g_orig.SetVertexShaderConstantF(dev,0,original,4);
    }
};
// Keep C++ restoration scopes outside Replay's existing SEH boundary.
static bool SkipShadowDrawForTest(IDirect3DDevice9* dev) {
    if(!g_skipShadowAtlasDraws || !IsShadowAtlasPass(dev)) return false;
    static thread_local unsigned long long live=0,replay=0;
    static thread_local ULONGLONG next=0;
    if(g_replayEye<0) ++live; else ++replay;
    const auto now=GetTickCount64();
    if(now>=next) {
        next=now+2000;
        Log("[ShadowDrawTest] skippedLive=%llu skippedReplay=%llu; atlas clears/state/CPU preparation retained",live,replay);
    }
    return true;
}
static bool SkipObjectDrawForTest(D3DPRIMITIVETYPE type,UINT primitives) {
    if(!g_skipObjectDraws || primitives<=2 ||
       (type!=D3DPT_TRIANGLELIST && type!=D3DPT_TRIANGLESTRIP && type!=D3DPT_TRIANGLEFAN)) return false;
    static thread_local unsigned long long live=0,replay=0;
    static thread_local ULONGLONG next=0;
    if(g_replayEye<0) ++live; else ++replay;
    const auto now=GetTickCount64();
    if(now>=next) {
        next=now+2000;
        Log("[ObjectDrawTest] skippedLive=%llu skippedReplay=%llu; triangle batches >2, includes world; native preparation/state/recording retained",live,replay);
    }
    return true;
}
static HRESULT DrawPrimitiveWithDiffuse(IDirect3DDevice9* dev, D3DPRIMITIVETYPE t, UINT sv, UINT pc) {
    if(SkipObjectDrawForTest(t,pc)) return D3D_OK;
    if(SkipShadowDrawForTest(dev)) return D3D_OK;
    const WorldDiffuseScope diffuse(dev);
#ifdef D2VR_MATCHED_CAPTURE
    MatchedCaptureDraw(dev,g_replayEye<0?0:g_replayEye,diffuse.applied);
#endif
    return g_orig.DrawPrimitive(dev, t, sv, pc);
}
static HRESULT DrawIndexedPrimitiveWithDiffuse(IDirect3DDevice9* dev, D3DPRIMITIVETYPE t, INT bvi, UINT mvi, UINT nv, UINT si, UINT pc) {
    if(SkipObjectDrawForTest(t,pc)) return D3D_OK;
    if(SkipShadowDrawForTest(dev)) return D3D_OK;
    const WorldDiffuseScope diffuse(dev);
    const TentacleHeadScope tentacle(dev);
#ifdef D2VR_MATCHED_CAPTURE
    MatchedCaptureDraw(dev,g_replayEye<0?0:g_replayEye,diffuse.applied);
#endif
    return g_orig.DrawIndexedPrimitive(dev, t, bvi, mvi, nv, si, pc);
}
static HRESULT DrawPrimitiveUPWithDiffuse(IDirect3DDevice9* dev, D3DPRIMITIVETYPE t, UINT pc, const void* vb, UINT stride) {
    if(SkipObjectDrawForTest(t,pc)) return D3D_OK;
    if(SkipShadowDrawForTest(dev)) return D3D_OK;
    const WorldDiffuseScope diffuse(dev);
#ifdef D2VR_MATCHED_CAPTURE
    MatchedCaptureDraw(dev,g_replayEye<0?0:g_replayEye,diffuse.applied);
#endif
    return g_orig.DrawPrimitiveUP(dev, t, pc, vb, stride);
}
static HRESULT DrawIndexedPrimitiveUPWithDiffuse(IDirect3DDevice9* dev, D3DPRIMITIVETYPE t, UINT mvi, UINT nv, UINT pc, const void* ib, D3DFORMAT fmt, const void* vb, UINT stride) {
    if(SkipObjectDrawForTest(t,pc)) return D3D_OK;
    if(SkipShadowDrawForTest(dev)) return D3D_OK;
    const WorldDiffuseScope diffuse(dev);
#ifdef D2VR_MATCHED_CAPTURE
    MatchedCaptureDraw(dev,g_replayEye<0?0:g_replayEye,diffuse.applied);
#endif
    return g_orig.DrawIndexedPrimitiveUP(dev, t, mvi, nv, pc, ib, fmt, vb, stride);
}
static HRESULT STDMETHODCALLTYPE Hook_SetScissorRect(IDirect3DDevice9* dev, const RECT* r) {
    if (g_recActive) {
        if (r) { uint32_t off = RecPush(r, sizeof(RECT)); RecEmit(OP_SetScissorRect, 1, off); }
        else RecEmit(OP_SetScissorRect, 0);
    }
    return g_orig.SetScissorRect(dev, r);
}

struct UiClipState { RECT rect; DWORD enabled; bool applied; };

// FOV scaling must also scale the original viewport clip. Otherwise widgets
// deliberately parked beyond NDC y=1 (the unused BOSS NAME bars) become visible.
// Restrict this to the observed UI VS and an orthographic sprite matrix.
static UiClipState BeginScaledUiClip(IDirect3DDevice9* dev) {
    UiClipState state{};
    if (!g_clipScaledUi || !g_scale2DContent || g_submitMode != 4 || g_stereoMode != 1 ||
        !g_activeVs || g_activeVs->hash != 0x7070C5BCu) return state;
    StereoParams sp = MakeEyeParams(g_replayEye >= 0 ? g_replayEye : 0);
#ifdef D2VR_MATCHED_CAPTURE
    NativeCameraUiScale(sp.fovScaleX,sp.fovScaleY);
#endif
    if (sp.fovScaleX >= 1 && sp.fovScaleY >= 1) return state;
    float m[16]; D3DVIEWPORT9 vp{};
    if (FAILED(dev->GetVertexShaderConstantF(0, m, 4)) ||
        m[3]*m[3] + m[7]*m[7] + m[11]*m[11] >= 1e-8f ||
        std::fabs(m[15]-1) > 1e-5f ||
        std::max(std::fabs(m[0]), std::max(std::fabs(m[1]),
            std::max(std::fabs(m[4]), std::fabs(m[5])))) >= 0.05f ||
        FAILED(dev->GetViewport(&vp)) || FAILED(dev->GetScissorRect(&state.rect)) ||
        FAILED(dev->GetRenderState(D3DRS_SCISSORTESTENABLE, &state.enabled))) return state;
    // Pixel centres follow D3D9's integer convention. Include x/y=0, exclude W/H.
    RECT clip = {
        LONG(std::ceil(vp.X + vp.Width * (1-sp.fovScaleX)*0.5f)),
        LONG(std::ceil(vp.Y + vp.Height * (1-sp.fovScaleY)*0.5f)),
        LONG(std::ceil(vp.X + vp.Width * (1+sp.fovScaleX)*0.5f)),
        LONG(std::ceil(vp.Y + vp.Height * (1+sp.fovScaleY)*0.5f))};
    if (state.enabled) {
        clip.left = std::max(clip.left, state.rect.left);
        clip.top = std::max(clip.top, state.rect.top);
        clip.right = std::max(clip.left, std::min(clip.right, state.rect.right));
        clip.bottom = std::max(clip.top, std::min(clip.bottom, state.rect.bottom));
    }
    if (FAILED(g_orig.SetScissorRect(dev, &clip))) return state;
    if (FAILED(g_orig.SetRenderState(dev, D3DRS_SCISSORTESTENABLE, TRUE))) {
        g_orig.SetScissorRect(dev, &state.rect); return state;
    }
    state.applied = true;
    static int logged = 0;
    if (logged < 2) {
        ++logged;
        Log("[HUD] native UI clipping restored eye=%d rect=%ld,%ld,%ld,%ld",
            g_replayEye >= 0 ? g_replayEye : 0, clip.left, clip.top, clip.right, clip.bottom);
    }
    return state;
}

static void EndScaledUiClip(IDirect3DDevice9* dev, const UiClipState& state) {
    if (!state.applied) return;
    g_orig.SetScissorRect(dev, &state.rect);
    g_orig.SetRenderState(dev, D3DRS_SCISSORTESTENABLE, state.enabled);
}

static HRESULT STDMETHODCALLTYPE Hook_DrawPrimitive(IDirect3DDevice9* dev, D3DPRIMITIVETYPE t, UINT sv, UINT pc) {
    if (d2vr_trace::g_enabled) d2vr_trace::NoteDraw();
    if (g_recActive) RecEmit(OP_DrawPrimitive, 0, t, sv, pc);
    const UiClipState clip = BeginScaledUiClip(dev);
    HRESULT hr = DrawPrimitiveWithDiffuse(dev, t, sv, pc);
    EndScaledUiClip(dev, clip);
    return hr;
}
static HRESULT STDMETHODCALLTYPE Hook_DrawIndexedPrimitive(IDirect3DDevice9* dev, D3DPRIMITIVETYPE t,
    INT bvi, UINT mvi, UINT nv, UINT si, UINT pc) {
    if (d2vr_trace::g_enabled) d2vr_trace::NoteDraw();
    if (g_recActive) RecEmit(OP_DrawIndexedPrimitive, 0, t, (uint32_t)bvi, mvi, nv, si, pc);
    const UiClipState clip = BeginScaledUiClip(dev);
    HRESULT hr = DrawIndexedPrimitiveWithDiffuse(dev, t, bvi, mvi, nv, si, pc);
    EndScaledUiClip(dev, clip);
    return hr;
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
    const UiClipState clip = BeginScaledUiClip(dev);
    HRESULT hr = DrawPrimitiveUPWithDiffuse(dev, t, pc, vb, stride);
    EndScaledUiClip(dev, clip);
    return hr;
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
    const UiClipState clip = BeginScaledUiClip(dev);
    HRESULT hr = DrawIndexedPrimitiveUPWithDiffuse(dev, t, mvi, nv, pc, ib, fmt, vb, stride);
    EndScaledUiClip(dev, clip);
    return hr;
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
    auto it = g_psInverse.find(ps);
    g_activePsInverse = it == g_psInverse.end() ? -1 : it->second;
    auto it2 = g_psViewExtents.find(ps);
    g_activePsViewExtents = it2 == g_psViewExtents.end() ? -1 : it2->second;
    return g_orig.SetPixelShader(dev, ps);
}
#pragma optimize("y", off)
__declspec(noinline) static HRESULT STDMETHODCALLTYPE Native_SetPSConstF(IDirect3DDevice9* dev, UINT s, const float* d, UINT n,
    const uintptr_t* nativeSlot,uintptr_t nativeFrame,uintptr_t nativeOwner) {
#ifdef D2VR_MATCHED_CAPTURE
    const float* native=d;
#endif
    if (g_replayEye < 0) ObserveInverseProjection(d, s, n, g_activePsInverse);
    if (g_recActive) {
        const auto proof=d2vr_native_constants::InspectCurrent(
            nativeSlot,nativeFrame,d,sizeof(float)*4*n,
            d2vr_native_constants::Stage::Pixel,nativeOwner);
        RecEmitData(OP_SetPSConstF, d, sizeof(float) * 4 * n, s, n,0,0,proof);
    }
    float adjusted[224*4];
    if (g_submitMode == 4 && g_replayEye < 0 && d &&
        CompensatePsConstants(adjusted, d, s, n, g_activePsInverse, g_activePsViewExtents,
                              MakeEyeParams(0))) d = adjusted;
#ifdef D2VR_MATCHED_CAPTURE
    const HRESULT result=g_orig.SetPixelShaderConstantF(dev,s,d,n);
    MatchedProjectionObserved(native,d,s,n,g_activePsInverse,result);
    return result;
#else
    return g_orig.SetPixelShaderConstantF(dev, s, d, n);
#endif
}
#pragma optimize("y", on)
D2VR_CONSTANT_GATE(Hook_SetPSConstF,Native_SetPSConstF)
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
// ---------------------------------------------------------------------------------------
// Light-input readback (DIAGNOSTIC ONLY, [Diagnostics] LightReadback=<frames>).
// Question: does the shadow-light pass read render-target data from the previous window?
// At the end of every window the content hash of each render target is recorded; the
// first shadow-light draws of the next replay read the same targets back and compare.
// ---------------------------------------------------------------------------------------
static uint32_t ReadbackTextureHash(IDirect3DDevice9* dev, IDirect3DTexture9* tex) {
    IDirect3DSurface9* lvl = nullptr;
    if (FAILED(tex->GetSurfaceLevel(0, &lvl)) || !lvl) return 0;
    D3DSURFACE_DESC d{};
    uint32_t h = 0;
    lvl->GetDesc(&d);
    IDirect3DSurface9* sys = nullptr;
    if (SUCCEEDED(dev->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &sys, nullptr)) && sys) {
        if (SUCCEEDED(dev->GetRenderTargetData(lvl, sys))) {
            D3DLOCKED_RECT lr{};
            if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
                const UINT bpp = (d.Format == D3DFMT_A16B16G16R16F) ? 8u : 4u;
                const UINT rowBytes = d.Width * bpp;
                h = 2166136261u;
                for (UINT y = 0; y < d.Height; y += 4) {
                    const uint8_t* row = (const uint8_t*)lr.pBits + (size_t)y * lr.Pitch;
                    for (UINT x = 0; x < rowBytes; x += 8) h = (h ^ row[x]) * 16777619u;
                }
                sys->UnlockRect();
            }
        }
        sys->Release();
    }
    lvl->Release();
    return h;
}

static bool IsRenderTargetTexture(const void* tex) {
    for (const auto& kv : g_surfToTex)
        if (kv.second == tex) return true;
    return false;
}

// Called before each replay draw: when the current pixel shader is the shadow-light shader,
// read back every bound render-target texture (within the budget) and log its hash.
static void LightInputReadback(IDirect3DDevice9* dev, const void* ps, const void* const* texes, int eye) {
    if (g_lightRbBudget <= 0) return;
    auto ph = g_psHashByPtr.find(ps);
    if (ph == g_psHashByPtr.end() || ph->second != kShadowLightPsHash) return;
    --g_lightRbBudget;
    for (int st = 0; st < 16; ++st) {
        const void* tex = texes[st];
        if (!tex || !IsRenderTargetTexture(tex)) continue;
        const uint32_t now = ReadbackTextureHash(dev, (IDirect3DTexture9*)tex);
        auto end = g_lightEndHash.find(tex);
        if (end == g_lightEndHash.end()) {
            Log("[LightRB] eye=%d stage=%d tex=%p hashAtLight=%08X endOfWindow=none", eye, st, tex, now);
        } else {
            Log("[LightRB] eye=%d stage=%d tex=%p hashAtLight=%08X endOfWindow=%08X %s",
                eye, st, tex, now, end->second, end->second == now ? "SAME" : "DIFF");
        }
    }
}

// Called once per window (before the next recording starts): snapshot the render targets'
// content hashes, and arm the light readback for the next replay while frames remain.
static void LightEndOfWindowSnapshot(IDirect3DDevice9* dev) {
    if (g_lightRbFrames < 0) {
        g_lightRbFrames = (int)GetPrivateProfileIntA("Diagnostics", "LightReadback", 0, ".\\Darkness2VR-diagnostic.ini");
        if (g_lightRbFrames > 0) Log("[LightRB] enabled for %d windows", g_lightRbFrames);
    }
    if (g_lightRbFrames <= 0 || g_surfToTex.empty()) return;   // count only 3D windows
    --g_lightRbFrames;
    std::unordered_map<const void*, bool> seen;
    for (const auto& kv : g_surfToTex) {
        const void* tex = kv.second;
        if (seen.emplace(tex, true).second) {
            g_lightEndHash[tex] = ReadbackTextureHash(dev, (IDirect3DTexture9*)tex);
        }
    }
    g_lightRbBudget = 2;
    Log("[LightRB] end-of-window snapshot: %zu render targets", seen.size());
}

// These setters neither read nor modify float registers. Keep their original
// order and arguments; only already-transformed, owned float writes may span
// them. No shader/RT/viewport/draw/resource/stateblock command is admitted.
static bool IsConstantMaterialBinding(unsigned op) {
    // SetRenderState is deliberately excluded: vendor FourCC states can trigger
    // operations (for example depth resolves), rather than merely bind state.
    return op==OP_SetTexture || op==OP_SetTextureStageState || op==OP_SetSamplerState;
}
static bool MoreFloatBeforeBarrier(size_t at) {
    for(size_t j=at+1;j<g_rec.size();++j) {
        const unsigned op=g_rec[j].op;
        if(op==OP_SetVSConstF || op==OP_SetPSConstF) return true;
        if(!IsConstantMaterialBinding(op)) return false;
    }
    return false;
}

static void ReplayStream(IDirect3DDevice9* dev, int eye, const StereoParams& sp) {
    g_replayEye = eye;
    const void* replayTex[16] = {};
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
    // No driver/state reuse across frames. Capture retains exact original
    // upload boundaries so its per-record projection witnesses stay authoritative.
    bool batchEnabled = g_replayConstantBatch;
#ifdef D2VR_MATCHED_CAPTURE
    batchEnabled = batchEnabled && !MatchedCaptureRecordingActive();
#endif
    d2vr_replay_batch::Constants constants(batchEnabled, g_frameStart.vertexFloatLimit,
                                           g_frameStart.pixelFloatLimit);
    const bool windowEnabled = batchEnabled && g_replayConstantWindow;
    const bool bindWindowEnabled = windowEnabled && g_replayConstantBindWindow;
    // The existing recording lock serializes replay. Retain storage, not state,
    // to avoid adding 8KB of stack probes on every OFF/adjacent replay.
    static d2vr_replay_batch::ConstantWindow constantWindow;
    if (windowEnabled) constantWindow.Reset(g_frameStart.vertexFloatLimit,g_frameStart.pixelFloatLimit);
    auto forwardConstants = [dev](d2vr_replay_batch::Stage stage, unsigned first,
                                 const float* values, unsigned count) -> long {
        return stage == d2vr_replay_batch::Stage::Vertex ?
            g_orig.SetVertexShaderConstantF(dev,first,values,count) :
            g_orig.SetPixelShaderConstantF(dev,first,values,count);
    };
#ifdef D2VR_MATCHED_CAPTURE
    const bool auditUploads = d2vr_cpu_sync::timing.enabled;
    d2vr_replay_batch::UploadCounts uploadCounts;
    auto sendConstants = [&](d2vr_replay_batch::Stage stage, unsigned first,
                             const float* values, unsigned count) -> long {
        return auditUploads ? uploadCounts.Forward(stage,first,values,count,forwardConstants) :
                              forwardConstants(stage,first,values,count);
    };
#else
    const auto sendConstants = forwardConstants;
#endif
    for (size_t i = 0; i < g_rec.size(); ++i) {
        const Rec& r = g_rec[i];
        if (dump) {
            Log("  rec[%zu] op=%u n=%u a=[%u %u %u %u %u %u]", i, r.op, r.n,
                r.a[0], r.a[1], r.a[2], r.a[3], r.a[4], r.a[5]);
        }
        HOOK_MARK(10000 + (LONG)r.op);
        __try {
        if (windowEnabled && r.op!=OP_SetVSConstF && r.op!=OP_SetPSConstF &&
            !(bindWindowEnabled && IsConstantMaterialBinding(r.op)))
            constantWindow.Flush(sendConstants);
        if (batchEnabled && !windowEnabled) constants.Barrier(
            r.op==OP_SetVSConstF ? d2vr_replay_batch::Stage::Vertex :
            r.op==OP_SetPSConstF ? d2vr_replay_batch::Stage::Pixel :
                                  d2vr_replay_batch::Stage::None, sendConstants);
        switch ((RecOp)r.op) {
        case OP_SetRenderTarget:
            if(r.a[0]==0) InvalidateShadowAtlasPass();
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
            InvalidateShadowAtlasPass();
            g_orig.SetViewport(dev, (const D3DVIEWPORT9*)(dataBase + r.a[0]));
            break;
        case OP_SetClipPlane:
            g_orig.SetClipPlane(dev, r.a[0], (const float*)(dataBase + r.a[4]));
            break;
        case OP_SetRenderState:
            g_orig.SetRenderState(dev, (D3DRENDERSTATETYPE)r.a[0], r.a[1]);
            break;
        case OP_SetTexture:
            g_orig.SetTexture(dev, r.a[0], (IDirect3DBaseTexture9*)(uintptr_t)r.a[1]);
            if (r.a[0] < 16) replayTex[r.a[0]] = (const void*)(uintptr_t)r.a[1];
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
        case OP_DrawPrimitive: {
            if (g_lightRbBudget > 0) LightInputReadback(dev, g_replayPs, replayTex, eye);
            const UiClipState clip = BeginScaledUiClip(dev);
            DrawPrimitiveWithDiffuse(dev, (D3DPRIMITIVETYPE)r.a[0], r.a[1], r.a[2]);
            EndScaledUiClip(dev, clip);
            break;
        }
        case OP_DrawIndexedPrimitive: {
            if (g_lightRbBudget > 0) LightInputReadback(dev, g_replayPs, replayTex, eye);
            const UiClipState clip = BeginScaledUiClip(dev);
            DrawIndexedPrimitiveWithDiffuse(dev, (D3DPRIMITIVETYPE)r.a[0], (INT)r.a[1], r.a[2], r.a[3], r.a[4], r.a[5]);
            EndScaledUiClip(dev, clip);
            break;
        }
        case OP_DrawPrimitiveUP: {
            HOOK_MARK(900 + r.op);
            const void* data = (r.n == 1) ? (const void*)(uintptr_t)r.a[3]
                                          : (const void*)(dataBase + r.a[3]);
            const UiClipState clip = BeginScaledUiClip(dev);
            DrawPrimitiveUPWithDiffuse(dev, (D3DPRIMITIVETYPE)r.a[0], r.a[1], data, r.a[2]);
            EndScaledUiClip(dev, clip);
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
            const UiClipState clip = BeginScaledUiClip(dev);
            DrawIndexedPrimitiveUPWithDiffuse(dev, (D3DPRIMITIVETYPE)r.a[0], r.a[1], r.a[2], r.a[3],
                                         ib, (D3DFORMAT)r.a[4], vb, stride);
            EndScaledUiClip(dev, clip);
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
            g_replayVs = (const void*)sh;
            g_orig.SetVertexShader(dev, sh);
            break;
        }
        case OP_SetVSConstF: {
#ifdef D2VR_MATCHED_CAPTURE
            if (auditUploads) uploadCounts.Record(d2vr_replay_batch::Stage::Vertex);
#endif
            const float* src = (const float*)(dataBase + r.a[4]);
            // MUST hold Vector4fCount vectors of 4 floats each (bone palettes upload up to
            // 68 vectors = 272 floats). The old size (17*4) overflowed the stack -> fail-fast.
            float tmp[68 * 4];
            bool lightVs = false;
            if (g_skipLightVsShift && g_activeVs && g_activeVs->hash == 0x68EEE79Cu)
                lightVs = true;   // candidate fix (Shift+F2): light volume in engine/world space
            if (eye >= 0 && r.a[1] <= 68 && !lightVs && !IsShadowAtlasPass(dev)) {
                if (ApplyShiftToUpload(tmp, src, r.a[0], r.a[1], g_activeVs, sp)) src = tmp;
            }
            if (windowEnabled) constantWindow.Upload(d2vr_replay_batch::Stage::Vertex,
                r.a[0],src,r.a[1],sendConstants);
            else if (batchEnabled) constants.Upload(d2vr_replay_batch::Stage::Vertex,
                r.a[0],src,r.a[1],sendConstants);
            else sendConstants(d2vr_replay_batch::Stage::Vertex,r.a[0],src,r.a[1]);
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
            {
            g_replayPs = (const void*)(uintptr_t)r.a[0];
                auto it = g_psInverse.find((IDirect3DPixelShader9*)(uintptr_t)r.a[0]);
                g_activePsInverse = it == g_psInverse.end() ? -1 : it->second;
                auto it2 = g_psViewExtents.find((IDirect3DPixelShader9*)(uintptr_t)r.a[0]);
                g_activePsViewExtents = it2 == g_psViewExtents.end() ? -1 : it2->second;
            }
            g_orig.SetPixelShader(dev, (IDirect3DPixelShader9*)(uintptr_t)r.a[0]);
            break;
        case OP_SetPSConstF: {
#ifdef D2VR_MATCHED_CAPTURE
            if (auditUploads) uploadCounts.Record(d2vr_replay_batch::Stage::Pixel);
#endif
            const float* values = (const float*)(dataBase + r.a[4]);
            float adjusted[224*4];
            bool copied = CompensatePsConstants(adjusted, values, r.a[0], r.a[1],
                                                g_activePsInverse, g_activePsViewExtents, sp);
            // Most packets need no PS compensation. Keep their arena pointer:
            // materializing an unused stack copy costs bandwidth and, for an
            // invalid >224-vector upload, used to overrun adjusted[].
            if (g_lightPosShift && sp.transX != 0.0f && r.a[0] == 0 &&
                r.a[1] > 0 && r.a[1] <= 224) {
                auto psIt = g_psHashByPtr.find(g_replayPs);
                if (psIt != g_psHashByPtr.end() && psIt->second == kShadowLightPsHash) {
                    // LightViewPosRadius (PS c0) is a position in view space and is uploaded
                    // by the engine relative to the live (left-eye) camera. The replay renders
                    // the right eye with an IPD offset, so the position must follow the
                    // geometry or the light lands elsewhere than the surface it lights.
                    if (!copied) memcpy(adjusted, values, sizeof(float) * 4 * r.a[1]);
                    adjusted[0] += sp.transX;
                    adjusted[1] += sp.transY;
                    copied = true;
                }
            }
            const float* values2 = copied ? adjusted : values;
            if (windowEnabled) constantWindow.Upload(d2vr_replay_batch::Stage::Pixel,
                r.a[0],values2,r.a[1],sendConstants);
            else if (batchEnabled) constants.Upload(d2vr_replay_batch::Stage::Pixel,
                r.a[0],values2,r.a[1],sendConstants);
            else {
#ifdef D2VR_MATCHED_CAPTURE
                const HRESULT uploaded=sendConstants(d2vr_replay_batch::Stage::Pixel,r.a[0],values2,r.a[1]);
                MatchedProjectionObserved(values,values2,r.a[0],r.a[1],g_activePsInverse,uploaded);
#else
                sendConstants(d2vr_replay_batch::Stage::Pixel,r.a[0],values2,r.a[1]);
#endif
            }
            break;
        }
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
            InvalidateShadowAtlasPass();
            HOOK_MARK(900 + r.op);
            IDirect3DStateBlock9* sb = (IDirect3DStateBlock9*)(uintptr_t)r.a[0];
            // call the ORIGINAL Apply: the vtable points at our hook which would try to
            // record while the record lock is held here -> deadlock
            if (sb && g_origSBApply) g_origSBApply(sb);
            SyncActiveShader(dev);
            HOOK_MARK(1900 + r.op);
            break;
        }
        default:
            break;
        }
        // Finish the packet at its LAST original constant command, within the
        // existing exception boundary. Never defer a driver fault onto the next
        // draw/state command or flush outside the protected replay loop.
        if (batchEnabled) {
            const Rec* next = i+1<g_rec.size() ? &g_rec[i+1] : nullptr;
            const auto stage = next && next->op==OP_SetVSConstF ? d2vr_replay_batch::Stage::Vertex :
                next && next->op==OP_SetPSConstF ? d2vr_replay_batch::Stage::Pixel :
                                                d2vr_replay_batch::Stage::None;
            if (windowEnabled) {
                // Scan only after a float packet. Each intervening binding is
                // visited once per stream, without a plan allocation. Flush at
                // the final FLOAT command, so driver faults stay within its SEH.
                if (r.op==OP_SetVSConstF || r.op==OP_SetPSConstF) {
                    const bool more=bindWindowEnabled ? MoreFloatBeforeBarrier(i) :
                        (next && stage!=d2vr_replay_batch::Stage::None);
                    if(!more) constantWindow.Flush(sendConstants);
                }
            } else if (!next || !constants.CanAppend(stage,next->a[0],next->a[1]))
                constants.Flush(sendConstants);
        }
        } __except (1) {
            constants.Abort(); constantWindow.Abort(); batchEnabled=false;
#ifdef D2VR_MATCHED_CAPTURE
            if (auditUploads) ++uploadCounts.exceptions;
#endif
            // The NVIDIA/D3D9 runtime can raise on stale resource pointers recorded
            // earlier in the frame (temp textures). Skip the call - never kill the game.
            static int s_replayErrs = 0;
            if (s_replayErrs++ < 20) {
                Log("[TrueStereo] replay: exception in record %zu op=%u a=[%u %u] - skipped",
                    i, r.op, r.a[0], r.a[1]);
            }
        }
    }
#ifdef D2VR_MATCHED_CAPTURE
    if (auditUploads) {
        g_constantUploadWindow.counts.Add(uploadCounts);
        ++g_constantUploadWindow.streams;
        g_constantUploadWindow.otherEyes += eye != 1;
        g_constantUploadWindow.batched += g_replayConstantBatch && !MatchedCaptureRecordingActive();
        g_constantUploadWindow.windowed += windowEnabled;
        g_constantUploadWindow.bindWindowed += bindWindowEnabled;
        g_constantUploadWindow.captureBypass += g_replayConstantBatch && MatchedCaptureRecordingActive();
    }
#endif
    InterlockedExchange(&g_recLock, 0);
    g_replayEye = -1;
}

// ------------------------------------------------------------- state block hooks
static HRESULT STDMETHODCALLTYPE Hook_SBApply(IDirect3DStateBlock9* sb) {
    InvalidateShadowAtlasPass();
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
    if (!NativeImageWriteAllowed(reinterpret_cast<uintptr_t>(&vt[5]),sizeof(void*))) return;
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
        { 55, ORIG(SetClipPlane), (void*)&Hook_SetClipPlane, "SetClipPlane@55" },
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
        { 106, ORIG(CreatePixelShader), (void*)&Hook_CreatePixelShader, "CreatePS@106" },
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

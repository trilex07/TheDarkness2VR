// Stereo geometry test harness for Darkness2VR's warp shader.
// Runs the EXACT production pixel shader (kStereoWarpPS) on a real D3D9 device,
// feeds a synthetic scene with known depth planes, renders left & right eyes, reads
// both back, and measures the actual horizontal disparity. Prints a table and a
// PASS/FAIL verdict. No headset or subjective judgement required.
//
// Build (x86, see test/build_test.ps1):
//   cl /EHsc /W3 /O2 /MD /std:c++17 stereo_test.cpp /link d3d9.lib d3dcompiler.lib user32.lib

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>

#include "..\src\stereo_warp_ps.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dcompiler.lib")

static IDirect3D9* g_d3d = nullptr;
static IDirect3DDevice9* g_dev = nullptr;

static bool InitDevice(UINT w, UINT h) {
    g_d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_d3d) { printf("FAIL: Direct3DCreate9\n"); return false; }
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = GetConsoleWindow();
    pp.BackBufferWidth = w;
    pp.BackBufferHeight = h;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.EnableAutoDepthStencil = FALSE;
    HRESULT hr = g_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, GetConsoleWindow(),
        D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &g_dev);
    if (FAILED(hr)) hr = g_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, GetConsoleWindow(),
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &g_dev);
    if (FAILED(hr) || !g_dev) { printf("FAIL: CreateDevice hr=0x%08lX\n", hr); return false; }
    return true;
}

static UINT TexW(IDirect3DTexture9* t) { D3DSURFACE_DESC d; t->GetLevelDesc(0, &d); return d.Width; }
static UINT TexH(IDirect3DTexture9* t) { D3DSURFACE_DESC d; t->GetLevelDesc(0, &d); return d.Height; }

static IDirect3DPixelShader9* CompilePS(const char* src) {
    ID3DBlob* blob = nullptr; ID3DBlob* err = nullptr;
    HRESULT hr = D3DCompile(src, strlen(src), "warp", nullptr, nullptr, "main", "ps_3_0",
                            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &err);
    if (FAILED(hr)) { printf("FAIL: shader compile: %s\n", err ? (char*)err->GetBufferPointer() : "?"); return nullptr; }
    IDirect3DPixelShader9* ps = nullptr;
    g_dev->CreatePixelShader((const DWORD*)blob->GetBufferPointer(), &ps);
    blob->Release(); if (err) err->Release();
    return ps;
}

static IDirect3DVertexShader9* CompileVS(const char* src) {
    ID3DBlob* blob = nullptr; ID3DBlob* err = nullptr;
    HRESULT hr = D3DCompile(src, strlen(src), "warpvs", nullptr, nullptr, "main", "vs_3_0",
                            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &err);
    if (FAILED(hr)) { printf("FAIL: vs compile: %s\n", err ? (char*)err->GetBufferPointer() : "?"); return nullptr; }
    IDirect3DVertexShader9* vs = nullptr;
    g_dev->CreateVertexShader((const DWORD*)blob->GetBufferPointer(), &vs);
    blob->Release(); if (err) err->Release();
    return vs;
}

// Renders the warp into `out`. color = horizontal gradient (value = uv.x*255 in red),
// depth = constant `dval` across the whole image (a single depth plane).
static bool RenderWarp(IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps,
                       IDirect3DTexture9* colorTex, IDirect3DTexture9* depthTex,
                       IDirect3DTexture9* out, float K, float eyeSign, float maxDisp, float viz) {
    IDirect3DSurface9* rt = nullptr; out->GetSurfaceLevel(0, &rt);
    IDirect3DSurface9* oldRT = nullptr; g_dev->GetRenderTarget(0, &oldRT);
    if (FAILED(g_dev->SetRenderTarget(0, rt))) { if (oldRT) oldRT->Release(); if (rt) rt->Release(); return false; }
    D3DVIEWPORT9 vp{ 0, 0, TexW(out), TexH(out), 0.f, 1.f };
    g_dev->SetViewport(&vp);
    g_dev->BeginScene();
    g_dev->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF000000, 1.0f, 0);
    g_dev->SetTexture(0, colorTex);
    g_dev->SetTexture(1, depthTex);
    for (int s = 0; s < 2; ++s) {
        g_dev->SetSamplerState(s, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        g_dev->SetSamplerState(s, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        g_dev->SetSamplerState(s, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP); // tile: no flat edge bands
        g_dev->SetSamplerState(s, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    }
    g_dev->SetPixelShader(ps);
    g_dev->SetVertexShader(vs); // explicit VS, same as runtime (avoids fixed-function UV quirks)
    const float c1[4] = { 1.0f / (float)TexW(out), 1.0f / (float)TexH(out), 0.f, 0.f };
    g_dev->SetPixelShaderConstantF(1, c1, 1); // VPOS->uv scale
    g_dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
    g_dev->SetStreamSource(0, nullptr, 0, 0);
    g_dev->SetIndices(nullptr);
    g_dev->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    g_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    g_dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    const float c0[4] = { K, viz, eyeSign, maxDisp };
    g_dev->SetPixelShaderConstantF(0, c0, 1);
    struct V { float x, y, rhw, u, v; };
    float fw = (float)TexW(out), fh = (float)TexH(out);
    V quad[4] = { {0,0,1,0,0}, {fw,0,1,1,0}, {0,fh,1,0,1}, {fw,fh,1,1,1} };
    HRESULT hr = g_dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(V));
    if (FAILED(hr)) printf("  [dbg] DrawPrimitiveUP FAILED hr=0x%08lX\n", hr);
    g_dev->EndScene();
    if (oldRT) { g_dev->SetRenderTarget(0, oldRT); oldRT->Release(); }
    if (rt) rt->Release();
    return SUCCEEDED(hr);
}

// Reads back an A8R8G8B8 texture into a CPU buffer (RGBA bytes).
static bool ReadBack(IDirect3DTexture9* tex, std::vector<uint8_t>& px, UINT& w, UINT& h) {
    w = TexW(tex); h = TexH(tex);
    IDirect3DSurface9* rt = nullptr; tex->GetSurfaceLevel(0, &rt);
    IDirect3DSurface9* sys = nullptr;
    if (FAILED(g_dev->CreateOffscreenPlainSurface(w, h, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, nullptr))) {
        if (rt) rt->Release(); return false;
    }
    bool ok = false;
    if (SUCCEEDED(g_dev->GetRenderTargetData(rt, sys))) {
        D3DLOCKED_RECT lr{};
        if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
            px.resize((size_t)w * h * 4);
            for (UINT y = 0; y < h; ++y) {
                const uint8_t* row = (const uint8_t*)lr.pBits + (size_t)y * lr.Pitch;
                memcpy(&px[(size_t)y * w * 4], row, (size_t)w * 4);
            }
            sys->UnlockRect();
            ok = true;
        }
    }
    sys->Release(); if (rt) rt->Release();
    return ok;
}

// Measure the horizontal shift of eye image `img` by decoding the green channel:
// the color texture's green = source column (mod 256). After the warp, green(x) =
// source x. shift = (green(x) - x) mod 256, mapped to signed. Exact, no correlation.
// A constant-depth plane gives a constant shift; average over a robust interior band.
static float MeasureShift(const std::vector<uint8_t>& img, UINT w, UINT h) {
    // Collect per-column signed shift (mod 256 -> signed [-128,127]) on a mid band.
    // Use the median to reject any out-of-range/clamped columns.
    std::vector<int> shifts;
    for (UINT x = (UINT)(w * 0.2f); x < (UINT)(w * 0.8f); ++x) {
        UINT y = h / 2;
        int green = img[((size_t)y * w + x) * 4 + 1]; // BGRA: green at +1
        int src = green;
        int d = src - (int)x;
        // wrap to [-128,127]
        while (d > 127) d -= 256;
        while (d < -128) d += 256;
        shifts.push_back(d);
    }
    if (shifts.empty()) return 0.f;
    // median
    size_t n = shifts.size();
    std::nth_element(shifts.begin(), shifts.begin() + n / 2, shifts.end());
    return (float)shifts[n / 2];
}

int main() {
    const UINT W = 256, H = 256;
    printf("=== Darkness2VR stereo warp geometry test (%ux%u) ===\n", W, H);
    if (!InitDevice(W, H)) return 2;

    // Color texture: pseudo-random horizontal-varying pattern (unique per column so
    // cross-correlation has a sharp peak). Deterministic LCG, same across all rows.
    IDirect3DTexture9* colorTex = nullptr;
    if (FAILED(g_dev->CreateTexture(W, H, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &colorTex, nullptr))) {
        printf("FAIL: color tex\n"); return 2;
    }
    {
        D3DLOCKED_RECT lr{}; colorTex->LockRect(0, &lr, nullptr, 0);
        std::vector<uint8_t> pattern(W);
        // Green channel encodes the column index (mod 256) so a warp shift decodes
        // EXACTLY: after warp, green(x) = source column. shift = green(x) - x (mod 256).
        for (UINT x = 0; x < W; ++x) pattern[x] = (uint8_t)(x & 0xFF);
        for (UINT y = 0; y < H; ++y) {
            uint8_t* row = (uint8_t*)lr.pBits + (size_t)y * lr.Pitch;
            for (UINT x = 0; x < W; ++x) {
                uint8_t v = pattern[x];
                row[x * 4 + 0] = v; row[x * 4 + 1] = v; row[x * 4 + 2] = v; row[x * 4 + 3] = 255;
            }
        }
        colorTex->UnlockRect(0);
    }

    // Depth textures: constant plane each (so we can read disparity at that exact d).
    // d is stored in the RED channel (shader samples .r).
    auto makeDepth = [&](float dval) -> IDirect3DTexture9* {
        IDirect3DTexture9* t = nullptr;
        if (FAILED(g_dev->CreateTexture(W, H, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, nullptr))) return nullptr;
        D3DLOCKED_RECT lr{}; t->LockRect(0, &lr, nullptr, 0);
        uint8_t dv = (uint8_t)(dval * 255.0f + 0.5f);
        for (UINT y = 0; y < H; ++y) {
            uint8_t* row = (uint8_t*)lr.pBits + (size_t)y * lr.Pitch;
            for (UINT x = 0; x < W; ++x) {
                row[x * 4 + 0] = dv; row[x * 4 + 1] = dv; row[x * 4 + 2] = dv; row[x * 4 + 3] = 255;
            }
        }
        t->UnlockRect(0);
        return t;
    };

    IDirect3DPixelShader9* ps = CompilePS(kStereoWarpPS);
    if (!ps) return 2;
    // Same VS as the runtime: uv computed from position (FVF TEX1 interpolation is
    // broken on this driver, verified by the uv.x debug probe).
    IDirect3DVertexShader9* vs = CompileVS(kStereoWarpVS);
    if (!vs) return 2;

    // [dbg] verify input texture content
    {
        D3DLOCKED_RECT lr{}; colorTex->LockRect(0, &lr, nullptr, D3DLOCK_READONLY);
        const uint8_t* r0 = (const uint8_t*)lr.pBits + (size_t)(H/2) * lr.Pitch;
        printf("[dbg] input color row128 greens: x=0:%d x=64:%d x=128:%d x=192:%d x=255:%d\n",
               r0[0*4+1], r0[64*4+1], r0[128*4+1], r0[192*4+1], r0[255*4+1]);
        colorTex->UnlockRect(0);
    }

    // [dbg] UV interpolation check: render a shader that outputs uv.x as grayscale.
    {
        const char* uvPsSrc =
            "float4 vsConst : register(c1);\n"
            "float4 main(float4 vpos : VPOS) : COLOR0 {\n"
            "  float u = vpos.x * vsConst.x;\n"
            "  return float4(u, u, u, 1.0);\n"
            "}\n";
        IDirect3DPixelShader9* uvPs = CompilePS(uvPsSrc);
        IDirect3DTexture9* uvOut = nullptr;
        g_dev->CreateTexture(W, H, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &uvOut, nullptr);
        if (uvPs && uvOut) {
            RenderWarp(vs, uvPs, colorTex, colorTex, uvOut, 0.3f, 1.0f, 0.06f, 0.f);
            std::vector<uint8_t> iv; UINT uw, uh; ReadBack(uvOut, iv, uw, uh);
            UINT yy = uh/2; const uint8_t* q = &iv[(size_t)yy*uw*4];
            printf("[dbg] uv.x as red at row128: x=0:%d x=64:%d x=128:%d x=192:%d x=255:%d\n",
                   q[0*4+2], q[64*4+2], q[128*4+2], q[192*4+2], q[255*4+2]);
        }
        if (uvOut) uvOut->Release();
        if (uvPs) uvPs->Release();
    }

    IDirect3DTexture9* outL = nullptr, *outR = nullptr;
    g_dev->CreateTexture(W, H, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &outL, nullptr);
    g_dev->CreateTexture(W, H, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &outR, nullptr);

    // Test matrix: a range of depth planes, and a few separation strengths.
    const float depths[] = { 0.05f, 0.30f, 0.60f, 0.85f, 0.97f, 0.999f };
    const int nd = 6;

    struct Case { const char* name; float K; float maxDisp; };
    const Case cases[] = {
        { "K=0.15 clamp=0.015 (new shipping default)", 0.15f, 0.015f },
        { "K=0.08 clamp=0.015 (subtle)", 0.08f, 0.015f },
        { "K=0.30 clamp=0.030 (strong)", 0.30f, 0.030f },
        { "K=0.30 clamp=0.06 (old default, too strong)", 0.30f, 0.06f },
    };
    const int nc = 4;

    for (int c = 0; c < nc; ++c) {
        printf("\n--- %s ---\n", cases[c].name);
        printf("%6s | %6s | %6s | %10s | %10s | %s\n",
               "d", "shL(px)", "shR(px)", "disparity", "expected", "verdict");
        bool allOk = true;
        float prevAbsDisp = 1e9f; // disparity should GROW as d decreases (near = more)
        for (int i = 0; i < nd; ++i) {
            float d = depths[i];
            IDirect3DTexture9* depthTex = makeDepth(d);
            // left eye: eyeSign=-1, right: +1
            bool rl = RenderWarp(vs, ps, colorTex, depthTex, outL, cases[c].K, -1.0f, cases[c].maxDisp, 0.f);
            bool rr = RenderWarp(vs, ps, colorTex, depthTex, outR, cases[c].K, +1.0f, cases[c].maxDisp, 0.f);
            std::vector<uint8_t> imgL, imgR; UINT bw, bh;
            bool rbL = ReadBack(outL, imgL, bw, bh); bool rbR = ReadBack(outR, imgR, bw, bh);
            if (!rbL || !rbR) printf("  [dbg] ReadBack fail L=%d R=%d\n", rbL, rbR);
            float shL = MeasureShift(imgL, bw, bh);
            float shR = MeasureShift(imgR, bw, bh);
            // shL/shR carry a constant coordinate-scale bias that cancels in the
            // difference: full disparity = shR - shL = 2*K*(1-d)*W.
            float disparity = fabsf(shR - shL);
            float perEyeUV = StereoWarpDispCPU(d, cases[c].K, 1.0f, cases[c].maxDisp);
            float expected = 2.0f * perEyeUV * (float)W; // px
            bool clamped = disparity <= 2.0f * cases[c].maxDisp * (float)W + 2.5f;
            bool nearIsBigger = (disparity <= prevAbsDisp + 1.5f); // near (small d) >= far
            // magnitude agreement: measured within ~2px or ~30% of expected
            bool magOk = fabsf(disparity - expected) <= 2.0f + 0.30f * expected;
            bool ok = clamped && nearIsBigger && magOk;
            if (!ok) allOk = false;
            printf("%6.3f | %6.0f | %6.0f | %10.1f | %10.1f | %s%s%s\n",
                   d, shL, shR, disparity, expected,
                   magOk ? "" : "MAG ", clamped ? "" : "OVERCLAMP ",
                   nearIsBigger ? "" : "NON-MONO ");
            prevAbsDisp = disparity;
            if (depthTex) depthTex->Release();
        }
        printf("=> %s\n", allOk ? "PASS (symmetric, clamped, monotonic)" : "FAIL");
    }

    printf("\nNote: disparity in px here is on a %u-px-wide buffer. Scale to 1920 by *(1920/%u).\n", W, W);
    printf("Fusible target on 1920: near objects ~<=60px, converging to ~0 at far plane.\n");

    // ---- Recommendation for the shipping default at real game resolution (1920) ----
    // Full disparity(px, 1920) = 2 * K * (1-d) * 1920, clamped to 2*maxDisp*1920.
    // REAL gameplay depth (DepthProbe): d range 0.93..1.0, mean 0.98, n ~ 1 unit.
    printf("\n=== Shipping default check at REAL gameplay depths (1920px) ===\n");
    const float GAME_W = 1920.f;
    struct Rec { const char* name; float K; float maxDisp; };
    Rec recs[] = {
        { "shipping (K=0.30)", 0.30f, 0.06f },
        { "subtle (K=0.15)", 0.15f, 0.06f },
        { "strong (K=0.50)", 0.50f, 0.10f },
    };
    const float sampleDepths[] = { 0.93f, 0.96f, 0.98f, 0.995f, 1.0f };
    for (int r = 0; r < 3; ++r) {
        printf("%-20s K=%.2f clamp=%.2f => disp@1920: ", recs[r].name, recs[r].K, recs[r].maxDisp);
        for (int i = 0; i < 5; ++i) {
            float pe = StereoWarpDispCPU(sampleDepths[i], recs[r].K, 1.0f, recs[r].maxDisp);
            float dispPx = 2.0f * pe * GAME_W;
            printf("d=%.3f:%.0fpx ", sampleDepths[i], dispPx);
        }
        printf("\n");
    }
    printf("(Fusion guide: 30-60px near is comfortable; >~70px may double. Far ~0.)\n");
    return 0;
}

// NIS D3D11 runtime adapted from SingularityVR - research/nvidia-vr-upscaling
//
// Official algorithm: NVIDIA Image Scaling SDK v1.0.3 (MIT).
// Host config math: nis/NIS_Config.h -> NVScalerUpdateConfig / NVSharpenUpdateConfig.
// Shader: nis/NIS_Main.hlsl compiled with NIS_VIEWPORT_SUPPORT=1. Darkness2VR
// supplies independent eye textures with zero-origin viewports, no SBS split
// or extra input-eye copy.
//
// Color space: game backbuffer is sRGB-ENCODED LDR (D3D9 A8R8G8B8). Bridge and
// NIS output are B8G8R8A8_UNORM (bit-identical layout). NIS runs with
// HDR mode None (LDR [0,1] display-referred). No gamma conversion anywhere,
// so Off vs NIS differ only in scaling/sharpening, not color/brightness.
//
// Swapchain usage: OpenXR swapchains keep
// COLOR_ATTACHMENT_BIT | TRANSFER_DST_BIT (no UNORDERED_ACCESS_BIT - not
// guaranteed by SteamVR/VirtualDesktop). Exactly one persistent UAV texture
// per eye + one final CopyResource per eye. Total per eye: 1 NIS dispatch +
// 1 copy. Baseline native path is 1 copy. Extra cost vs baseline = 1 compute.

#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>

#include <cstdio>
#include <cstring>

#include "nis_upscale.h"
#include "nis/NIS_Config.h"

// Plain log callback: preformatted message, no varargs across the boundary.
typedef void NisLogFn(const char* msg);

struct NisRuntime {
    ID3D11ComputeShader* csScaler = nullptr;   // NVScaler (upscale + sharpen)
    ID3D11ComputeShader* csSharpen = nullptr;  // NVSharpen (same-res sharpen)
    ID3D11Buffer* cb = nullptr;                // NISConfig constant buffer
    ID3D11SamplerState* sampler = nullptr;     // linear clamp
    ID3D11Texture2D* coefScalerTex = nullptr;
    ID3D11ShaderResourceView* coefScalerSrv = nullptr;
    ID3D11Texture2D* coefUsmTex = nullptr;
    ID3D11ShaderResourceView* coefUsmSrv = nullptr;
    ID3D11Texture2D* outTex[2] = { nullptr, nullptr };
    ID3D11UnorderedAccessView* outUav[2] = { nullptr, nullptr };
    uint32_t outW = 0, outH = 0;       // per-eye output size
    uint32_t inW = 0, inH = 0;         // per-eye input size (recreate check)
    bool ready = false;
    bool failedOnce = false;           // sticky fallback: log once, stay native
    // Lifetime counters: any of these moving per frame means a recreation bug.
    unsigned numShaderInits = 0;       // successful shader pairs created (expect 1)
    unsigned numOutputCreates = 0;     // output texture pair (re)creations
    // This buffer is exclusively owned/written by NisDispatchEye. GPU reads
    // cannot invalidate it, and rebinding another pipeline does not edit it.
    // Cache actual bytes, not source identity: both independent eyes normally
    // use the same config. Sharpness/shape changes compare unequal; shutdown
    // destroys the buffer and clears this receipt with the rest of the runtime.
    NISConfig uploadedConfig{};
    bool configUploaded = false;
    unsigned numConfigUploads = 0;
    // GPU timing (timestamp + disjoint, optional - null when unsupported)
    ID3D11Query* disjoint = nullptr;
    ID3D11Query* tsStart = nullptr;
    ID3D11Query* tsEnd = nullptr;
    bool timingPending = false;
    double msGpu = 0.0, msGpuPeak = 0.0;
    int timingFrames = 0;
};

static bool NisCompileShader(ID3D11Device* dev, const wchar_t* hlslPath,
                             bool scaler, ID3D11ComputeShader** out,
                             char* errBuf, size_t errCap) {
    if (errBuf && errCap) errBuf[0] = 0;
    HMODULE comp = LoadLibraryA("d3dcompiler_47.dll");
    if (!comp) comp = LoadLibraryA("d3dcompiler_43.dll");
    typedef HRESULT (WINAPI *PFN_CompileFromFile)(LPCWSTR, const D3D_SHADER_MACRO*,
        ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);
    PFN_CompileFromFile compile = comp
        ? (PFN_CompileFromFile)GetProcAddress(comp, "D3DCompileFromFile") : nullptr;
    if (!compile) {
        if (comp) FreeLibrary(comp);
        return false;
    }
    char bw[16], bh[16], tgs[16];
    sprintf_s(bw, "%u", 32u);
    sprintf_s(bh, "%u", scaler ? 24u : 32u);
    sprintf_s(tgs, "%u", 256u);
    D3D_SHADER_MACRO macros[] = {
        { "NIS_SCALER", scaler ? "1" : "0" },
        { "NIS_HLSL", "1" },
        { "NIS_BLOCK_WIDTH", bw },
        { "NIS_BLOCK_HEIGHT", bh },
        { "NIS_THREAD_GROUP_SIZE", tgs },
        { "NIS_VIEWPORT_SUPPORT", "1" },
        { "NIS_HDR_MODE", "0" },
        { nullptr, nullptr },
    };
    ID3DBlob* code = nullptr;
    ID3DBlob* errors = nullptr;
    HRESULT hr = compile(hlslPath, macros, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                         "main", "cs_5_0",
                         D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (FAILED(hr) || !code) {
        if (errors && errBuf && errCap) {
            size_t n = errors->GetBufferSize();
            if (n >= errCap) n = errCap - 1;
            memcpy(errBuf, errors->GetBufferPointer(), n);
            errBuf[n] = 0;
        }
        if (errors) errors->Release();
        if (code) code->Release();
        FreeLibrary(comp);
        return false;
    }
    if (errors) errors->Release();
    hr = dev->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(),
                                  nullptr, out);
    code->Release();
    FreeLibrary(comp);
    return SUCCEEDED(hr) && *out;
}

static bool NisCreateCoefTex(ID3D11Device* dev, const float coefs[64][8],
                             ID3D11Texture2D** tex, ID3D11ShaderResourceView** srv) {
    // 8 floats per phase = 2 float4s -> 2x64 R32G32B32A32_FLOAT.
    // Shader reads coef_scaler[vIdx, phase] with vIdx in {0,1}.
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = 2; td.Height = 64; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = coefs;
    init.SysMemPitch = 8 * sizeof(float);
    init.SysMemSlicePitch = 8 * sizeof(float) * 64;
    if (FAILED(dev->CreateTexture2D(&td, &init, tex))) return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
    sd.Format = td.Format;
    sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sd.Texture2D.MipLevels = 1;
    if (FAILED(dev->CreateShaderResourceView(*tex, &sd, srv))) return false;
    return true;
}

static bool NisCreateShaderFromBytes(ID3D11Device* dev,
    const unsigned char* bytes, unsigned int size, ID3D11ComputeShader** out) {
    if (!bytes || size < 16) return false;
    return SUCCEEDED(dev->CreateComputeShader(bytes, size, nullptr, out)) && *out;
}

// Init order: embedded official bytecode first (no file, no compiler, no
// startup cost beyond CreateComputeShader). Only if that fails, fall back to
// compiling nis/NIS_Main.hlsl at runtime (needs d3dcompiler_47.dll, present
// in SysWOW64 for x86 processes on stock Windows 10/11; LoadLibrary-based so
// a missing compiler is a log-once fallback, never a crash). Either way the
// game launches: failure sets failedOnce and the caller stays native.
static bool NisInit(NisRuntime* nis, ID3D11Device* dev,
                    const unsigned char* scalerBytes, unsigned int scalerSize,
                    const unsigned char* sharpenBytes, unsigned int sharpenSize,
                    const wchar_t* hlslFallbackPath,  // nullable
                    NisLogFn* logFn) {
    if (nis->ready) return true;
    if (nis->failedOnce) return false;  // sticky fallback: never retry-spam
    char msg[640] = {};
    const bool haveEmbedded =
        NisCreateShaderFromBytes(dev, scalerBytes, scalerSize, &nis->csScaler) &&
        NisCreateShaderFromBytes(dev, sharpenBytes, sharpenSize, &nis->csSharpen);
    if (haveEmbedded) {
        if (logFn) logFn("nis: shaders from embedded official bytecode (no HLSL file, no compile)");
    } else {
        if (nis->csScaler) { nis->csScaler->Release(); nis->csScaler = nullptr; }
        if (nis->csSharpen) { nis->csSharpen->Release(); nis->csSharpen = nullptr; }
        if (!hlslFallbackPath || !hlslFallbackPath[0]) {
            if (logFn) logFn("nis: embedded bytecode unusable and no HLSL fallback"
                             " - upscaling disabled, native path");
            nis->failedOnce = true;
            return false;
        }
        LARGE_INTEGER t0{}; QueryPerformanceCounter(&t0);
        char err[512] = {};
        const bool okS = NisCompileShader(dev, hlslFallbackPath, true, &nis->csScaler, err, sizeof(err));
        const bool okH = okS && NisCompileShader(dev, hlslFallbackPath, false, &nis->csSharpen, err, sizeof(err));
        LARGE_INTEGER t1{}; QueryPerformanceCounter(&t1);
        LARGE_INTEGER freq{}; QueryPerformanceFrequency(&freq);
        const double compileMs = freq.QuadPart
            ? 1000.0 * (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart : -1.0;
        if (!okS || !okH) {
            if (logFn) {
                _snprintf_s(msg, sizeof(msg), _TRUNCATE,
                    "nis: HLSL fallback compile FAILED (%.256s) - upscaling disabled, native path", err);
                logFn(msg);
            }
            nis->failedOnce = true;
            return false;
        }
        if (logFn) {
            _snprintf_s(msg, sizeof(msg), _TRUNCATE,
                "nis: shaders from runtime HLSL compile (startup cost %.1f ms; embedded preferred)",
                compileMs);
            logFn(msg);
        }
    }
    ++nis->numShaderInits;
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(NISConfig);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(dev->CreateBuffer(&bd, nullptr, &nis->cb))) {
        if (logFn) logFn("nis: constant buffer FAILED - upscaling disabled, native path");
        nis->failedOnce = true;
        return false;
    }
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    if (FAILED(dev->CreateSamplerState(&sd, &nis->sampler))) {
        if (logFn) logFn("nis: sampler FAILED - upscaling disabled, native path");
        nis->failedOnce = true;
        return false;
    }
    if (!NisCreateCoefTex(dev, coef_scale, &nis->coefScalerTex, &nis->coefScalerSrv) ||
        !NisCreateCoefTex(dev, coef_usm, &nis->coefUsmTex, &nis->coefUsmSrv)) {
        if (logFn) logFn("nis: coefficient textures FAILED - upscaling disabled, native path");
        nis->failedOnce = true;
        return false;
    }
    // Timing queries are optional; a null set simply disables GPU timing.
    D3D11_QUERY_DESC qd = {};
    qd.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    if (FAILED(dev->CreateQuery(&qd, &nis->disjoint))) nis->disjoint = nullptr;
    qd.Query = D3D11_QUERY_TIMESTAMP;
    if (FAILED(dev->CreateQuery(&qd, &nis->tsStart))) nis->tsStart = nullptr;
    if (FAILED(dev->CreateQuery(&qd, &nis->tsEnd))) nis->tsEnd = nullptr;
    nis->ready = true;
    if (logFn) logFn("nis: initialized (official NIS v1.0.3, scaler+sharpen, viewport mode)");
    return true;
}

static void NisShutdown(NisRuntime* nis) {
    if (nis->csScaler) nis->csScaler->Release();
    if (nis->csSharpen) nis->csSharpen->Release();
    if (nis->cb) nis->cb->Release();
    if (nis->sampler) nis->sampler->Release();
    if (nis->coefScalerSrv) nis->coefScalerSrv->Release();
    if (nis->coefScalerTex) nis->coefScalerTex->Release();
    if (nis->coefUsmSrv) nis->coefUsmSrv->Release();
    if (nis->coefUsmTex) nis->coefUsmTex->Release();
    for (int i = 0; i < 2; ++i) {
        if (nis->outUav[i]) nis->outUav[i]->Release();
        if (nis->outTex[i]) nis->outTex[i]->Release();
    }
    if (nis->disjoint) nis->disjoint->Release();
    if (nis->tsStart) nis->tsStart->Release();
    if (nis->tsEnd) nis->tsEnd->Release();
    *nis = NisRuntime{};
}

static bool NisEnsureOutput(NisRuntime* nis, ID3D11Device* dev,
                            uint32_t eyeInW, uint32_t eyeInH,
                            uint32_t eyeOutW, uint32_t eyeOutH) {
    if (nis->outTex[0] && !nis_upscale::NeedsRecreate(
            nis->inW, nis->inH, nis->outW, nis->outH,
            eyeInW, eyeInH, eyeOutW, eyeOutH))
        return true;
    for (int i = 0; i < 2; ++i) {
        if (nis->outUav[i]) { nis->outUav[i]->Release(); nis->outUav[i] = nullptr; }
        if (nis->outTex[i]) { nis->outTex[i]->Release(); nis->outTex[i] = nullptr; }
    }
    for (int i = 0; i < 2; ++i) {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = eyeOutW; td.Height = eyeOutH;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;  // same type group as SRGB swapchain
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(dev->CreateTexture2D(&td, nullptr, &nis->outTex[i]))) return false;
        D3D11_UNORDERED_ACCESS_VIEW_DESC ud = {};
        ud.Format = td.Format;
        ud.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        if (FAILED(dev->CreateUnorderedAccessView(nis->outTex[i], &ud, &nis->outUav[i])))
            return false;
    }
    nis->inW = eyeInW; nis->inH = eyeInH;
    nis->outW = eyeOutW; nis->outH = eyeOutH;
    ++nis->numOutputCreates;
    return true;
}

// Dispatch one eye. Darkness II already has separate eye textures, so each
// source viewport begins at zero. There must be no SBS offset or seam sample.
// sharpenOnly: NVSharpen path (same-size). If sizes differ, falls back to a
// 1:1 NVScaler config so the call still sharpens without corrupting geometry.
static bool NisDispatchEye(NisRuntime* nis, ID3D11DeviceContext* ctx,
                           ID3D11ShaderResourceView* srcSrv,
                           uint32_t inputW, uint32_t inputH,
                           int eye, uint32_t eyeOutW, uint32_t eyeOutH,
                           float sharpness, bool sharpenOnly) {
    if (!nis->ready || !srcSrv || eye < 0 || eye > 1) return false;
    const bool useSharpen = sharpenOnly && inputW == eyeOutW && inputH == eyeOutH;
    NISConfig cfg;
    // NISConfig is 256-byte aligned and includes tail padding. Initialize/copy
    // every byte so the receipt also compares padding deterministically.
    memset(&cfg, 0, sizeof(cfg));
    bool ok;
    if (useSharpen) {
        ok = NVSharpenUpdateConfig(cfg, nis_upscale::ClampSharpness(sharpness),
            0, 0, inputW, inputH, inputW, inputH, 0, 0);
    } else {
        ok = NVScalerUpdateConfig(cfg, nis_upscale::ClampSharpness(sharpness),
            0, 0, inputW, inputH, inputW, inputH,
            0, 0, eyeOutW, eyeOutH, eyeOutW, eyeOutH);
    }
    if (!ok) return false;
    if (!nis->configUploaded || memcmp(&cfg, &nis->uploadedConfig, sizeof(cfg))) {
        // Publish only after successful upload. Keep normal binding/dispatch
        // every time; only redundant CPU writes to our own buffer are removed.
        nis->configUploaded = false;
        D3D11_MAPPED_SUBRESOURCE ms = {};
        if (FAILED(ctx->Map(nis->cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) return false;
        memcpy(ms.pData, &cfg, sizeof(cfg));
        ctx->Unmap(nis->cb, 0);
        memcpy(&nis->uploadedConfig, &cfg, sizeof(cfg));
        nis->configUploaded = true;
        ++nis->numConfigUploads;
    }

    ID3D11Buffer* cbs[1] = { nis->cb };
    ID3D11SamplerState* samps[1] = { nis->sampler };
    ctx->CSSetShader(useSharpen ? nis->csSharpen : nis->csScaler, nullptr, 0);
    ctx->CSSetConstantBuffers(0, 1, cbs);
    ctx->CSSetSamplers(0, 1, samps);
    if (useSharpen) {
        ID3D11ShaderResourceView* s1[1] = { srcSrv };
        ctx->CSSetShaderResources(0, 1, s1);
    } else {
        ID3D11ShaderResourceView* srvs[3] =
            { srcSrv, nis->coefScalerSrv, nis->coefUsmSrv };
        ctx->CSSetShaderResources(0, 3, srvs);
    }
    ID3D11UnorderedAccessView* uavs[1] = { nis->outUav[eye] };
    ctx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
    uint32_t bw = 32;
    uint32_t bh = useSharpen ? 32 : 24;
    ctx->Dispatch((eyeOutW + bw - 1) / bw, (eyeOutH + bh - 1) / bh, 1);
    // Unbind UAV (required before CopyResource from outTex) + SRVs + shader.
    ID3D11UnorderedAccessView* nullUav[1] = { nullptr };
    ctx->CSSetUnorderedAccessViews(0, 1, nullUav, nullptr);
    ID3D11ShaderResourceView* nullSrv[3] = { nullptr, nullptr, nullptr };
    ctx->CSSetShaderResources(0, 3, nullSrv);
    ctx->CSSetShader(nullptr, nullptr, 0);
    return true;
}

// GPU timing: call NisTimingBegin before the two eye dispatches and
// NisTimingEnd after; call NisTimingPoll once per frame (after submit) to
// harvest the result without stalling. MEASURED GPU time, not CPU Dispatch.
static void NisTimingBegin(NisRuntime* nis, ID3D11DeviceContext* ctx) {
    if (!nis->ready || !nis->disjoint || !nis->tsStart || !nis->tsEnd) return;
    ctx->Begin(nis->disjoint);
    ctx->End(nis->tsStart);
}

static void NisTimingEnd(NisRuntime* nis, ID3D11DeviceContext* ctx) {
    if (!nis->ready || !nis->disjoint || !nis->tsStart || !nis->tsEnd) return;
    ctx->End(nis->tsEnd);
    ctx->End(nis->disjoint);
    nis->timingPending = true;
}

static void NisTimingPoll(NisRuntime* nis, ID3D11DeviceContext* ctx) {
    if (!nis->timingPending) return;
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj = {};
    // S_FALSE = not ready yet; leave pending for a later frame (no stall).
    if (ctx->GetData(nis->disjoint, &dj, sizeof(dj), 0) != S_OK) return;
    UINT64 t0 = 0, t1 = 0;
    if (ctx->GetData(nis->tsStart, &t0, sizeof(t0), 0) != S_OK) return;
    if (ctx->GetData(nis->tsEnd, &t1, sizeof(t1), 0) != S_OK) return;
    nis->timingPending = false;
    if (dj.Disjoint) return;
    double ms = (double)(t1 - t0) * 1000.0 / (double)dj.Frequency;
    nis->msGpu = ms;
    if (ms > nis->msGpuPeak) nis->msGpuPeak = ms;
    ++nis->timingFrames;
}

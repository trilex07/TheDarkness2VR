// Optional DLSS 4.5 Super Resolution / DLAA (and AMD FidelityFX FSR fallback)
// for the completed per-eye D3D11 stereo caches in SubmitMode=4.
//
// Runs the 64-bit helper (d2vr_dlss_host64.exe) over NT-shared textures and D3D11/D3D12
// fences. Helper startup and feature creation run on a background worker thread so the
// render thread never stalls; while warming up in Super Resolution modes (DlssMode >= 2),
// NisEyeProcess upscales the smaller eye caches to OpenXR swapchain size so zero frames
// are dropped.
// [Render] INI settings:
//   DlssMode:         0=Off, 1=DLAA (1.0x), 2=Quality (1.5x), 3=Balanced (1.72x),
//                     4=Performance (2.0x), 5=UltraPerformance (3.0x), 6=UltraQuality (1.3x)
//   DlssPreset:       0/11=K, 10=J, 12=L (DLSS 4.5), 13=M (DLSS 4.5), 16=Per-mode (K/M/L), 5=E, 6=F
//   DlssBackend:      0=DLSS (NGX), 1=FSR (FidelityFX API)
//   DlssObjectMotion: 1=enable 8x8 tile + 3x3 patch block-matching motion pass (default 1)
//   DlssBiasMask:     1=enable current-color disagreement bias mask (default 0)
static int g_dlssMode = 0;
static int g_dlssPreset = 16;
static int g_dlssBackend = 0;
static bool g_dlssObjectMotion = true;
static bool g_dlssBiasMask = false;

namespace d2vr_dlss_eye {

enum class State : int { Off = 0, Working = 1, Ready = 2, Failed = 3 };

static d2vr::dlss::Client g_client;
static d2vr::dlss::GuideGpu g_guides;
static bool g_guidesReady = false;
static bool g_guidesFailed = false;
static std::atomic<int> g_state{(int)State::Off};
static std::thread g_worker;
static char g_failReason[256] = "";

static uint32_t g_builtInW = 0, g_builtInH = 0;
static uint32_t g_builtOutW = 0, g_builtOutH = 0;
static int g_builtPreset = -1;
static int g_builtBackend = -1;

// Per-eye SRVs for the input eye caches (color)
static ID3D11ShaderResourceView* g_inSrv[2] = { nullptr, nullptr };
static ID3D11Texture2D* g_inOwner[2] = { nullptr, nullptr };

// Per-eye BGRA8 output textures (matching OpenXR swapchain & NIS format family)
static ID3D11Texture2D* g_outTex[2] = { nullptr, nullptr };
static ID3D11RenderTargetView* g_outRtv[2] = { nullptr, nullptr };
static uint32_t g_outW = 0, g_outH = 0;
static ID3D11VertexShader* g_copyVs = nullptr;
static ID3D11PixelShader* g_copyPs = nullptr;
static ID3D11RasterizerState* g_copyRs = nullptr;

// Per-eye temporal pose/FOV history for GuideParams
struct EyeHistory {
    bool valid = false;
    XrPosef pose{};
    float tanH = 1.0f;
    float tanV = 1.0f;
    ULONGLONG tickMs = 0;
};
static EyeHistory g_hist[2];

static void LogCallback(int level, const char* line) {
    Log("[DLSS%s] %s", level == 2 ? ":ERR" : level == 1 ? ":WARN" : "", line);
}

static const char* ModeName(int mode) {
    switch (mode) {
    case 1: return "DLAA (1.00x)";
    case 2: return "Quality (1.50x)";
    case 3: return "Balanced (1.72x)";
    case 4: return "Performance (2.00x)";
    case 5: return "UltraPerformance (3.00x)";
    case 6: return "UltraQuality (1.30x)";
    default: return "Off";
    }
}

static float ModeRatio(int mode) {
    switch (mode) {
    case 1: return 1.00f;
    case 2: return 1.50f;
    case 3: return 1.72f;
    case 4: return 2.00f;
    case 5: return 3.00f;
    case 6: return 1.30f;
    default: return 1.00f;
    }
}

static int ParseDlssMode(const char* s) {
    if (!s || !s[0]) return 0;
    if (!_stricmp(s, "OFF") || !strcmp(s, "0")) return 0;
    if (!_stricmp(s, "DLAA") || !_stricmp(s, "NATIVE") || !strcmp(s, "1")) return 1;
    if (!_stricmp(s, "QUALITY") || !strcmp(s, "2")) return 2;
    if (!_stricmp(s, "BALANCED") || !strcmp(s, "3")) return 3;
    if (!_stricmp(s, "PERFORMANCE") || !strcmp(s, "4")) return 4;
    if (!_stricmp(s, "ULTRAPERFORMANCE") || !_stricmp(s, "ULTRA_PERFORMANCE") || !strcmp(s, "5")) return 5;
    if (!_stricmp(s, "ULTRAQUALITY") || !_stricmp(s, "ULTRA_QUALITY") || !strcmp(s, "6")) return 6;
    const int v = atoi(s);
    return (v >= 0 && v <= 6) ? v : 0;
}

static int ParseDlssPreset(const char* s) {
    if (!s || !s[0]) return 16;
    if (!_stricmp(s, "DEFAULT") || !_stricmp(s, "PERMODE") || !_stricmp(s, "AUTO") || !strcmp(s, "16")) return 16;
    if (!_stricmp(s, "K") || !strcmp(s, "11") || !strcmp(s, "0")) return 11;
    if (!_stricmp(s, "J") || !strcmp(s, "10")) return 10;
    if (!_stricmp(s, "L") || !strcmp(s, "12")) return 12;
    if (!_stricmp(s, "M") || !strcmp(s, "13")) return 13;
    if (!_stricmp(s, "E") || !strcmp(s, "5")) return 5;
    if (!_stricmp(s, "F") || !strcmp(s, "6")) return 6;
    const int v = atoi(s);
    return (v >= 0 && v <= 16) ? v : 16;
}

static bool LocateHelperExe(wchar_t outPath[MAX_PATH], wchar_t outDataDir[MAX_PATH]) {
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&LocateHelperExe), &self);
    wchar_t modDir[MAX_PATH] = {};
    GetModuleFileNameW(self, modDir, MAX_PATH);
    if (wchar_t* sl = wcsrchr(modDir, L'\\')) *(sl + 1) = 0;

    const wchar_t* relCandidates[] = {
        L"d2vr_dlss\\d2vr_dlss_host64.exe",
        L"d2vr_dlss_host64.exe",
        L"build\\dlss_host\\d2vr_dlss_host64.exe"
    };
    outPath[0] = 0;
    for (const wchar_t* rel : relCandidates) {
        wchar_t cand[MAX_PATH] = {};
        _snwprintf_s(cand, _TRUNCATE, L"%s%s", modDir, rel);
        if (GetFileAttributesW(cand) != INVALID_FILE_ATTRIBUTES) {
            wcscpy_s(outPath, MAX_PATH, cand);
            break;
        }
    }
    wchar_t localApp[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, localApp))) {
        _snwprintf_s(outDataDir, MAX_PATH, _TRUNCATE, L"%s\\Darkness2VR\\dlss", localApp);
        CreateDirectoryW(outDataDir, nullptr);
    } else {
        _snwprintf_s(outDataDir, MAX_PATH, _TRUNCATE, L"%sd2vr_dlss", modDir);
    }
    return outPath[0] != 0;
}

static void ReleaseLocalViews() {
    for (int e = 0; e < 2; ++e) {
        if (g_inSrv[e]) { g_inSrv[e]->Release(); g_inSrv[e] = nullptr; }
        if (g_inOwner[e]) { g_inOwner[e]->Release(); g_inOwner[e] = nullptr; }
        if (g_outRtv[e]) { g_outRtv[e]->Release(); g_outRtv[e] = nullptr; }
        if (g_outTex[e]) { g_outTex[e]->Release(); g_outTex[e] = nullptr; }
        g_hist[e] = EyeHistory{};
    }
    g_outW = g_outH = 0;
}

static bool EnsureOutTextures(ID3D11Device* dev, uint32_t w, uint32_t h) {
    if (g_outTex[0] && g_outTex[1] && g_outRtv[0] && g_outRtv[1] && g_outW == w && g_outH == h)
        return true;
    for (int e = 0; e < 2; ++e) {
        if (g_outRtv[e]) { g_outRtv[e]->Release(); g_outRtv[e] = nullptr; }
        if (g_outTex[e]) { g_outTex[e]->Release(); g_outTex[e] = nullptr; }
    }
    g_outW = g_outH = 0;
    for (int e = 0; e < 2; ++e) {
        D3D11_TEXTURE2D_DESC td{};
        td.Width = w;
        td.Height = h;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(dev->CreateTexture2D(&td, nullptr, &g_outTex[e])) ||
            FAILED(dev->CreateRenderTargetView(g_outTex[e], nullptr, &g_outRtv[e])))
            return false;
    }
    if (!g_copyVs || !g_copyPs || !g_copyRs) {
        const char* src =
            "Texture2D t0 : register(t0);\n"
            "float4 vs(uint id : SV_VertexID) : SV_Position {\n"
            "    float2 uv = float2((id << 1) & 2, id & 2);\n"
            "    return float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);\n"
            "}\n"
            "float4 ps(float4 pos : SV_Position) : SV_Target0 {\n"
            "    return t0.Load(int3(pos.xy, 0));\n"
            "}\n";
        ID3DBlob* vb = CompileShaderBlob(src, "vs", "vs_4_0");
        ID3DBlob* pb = CompileShaderBlob(src, "ps", "ps_4_0");
        if (vb) { dev->CreateVertexShader(vb->GetBufferPointer(), vb->GetBufferSize(), nullptr, &g_copyVs); vb->Release(); }
        if (pb) { dev->CreatePixelShader(pb->GetBufferPointer(), pb->GetBufferSize(), nullptr, &g_copyPs); pb->Release(); }
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        dev->CreateRasterizerState(&rd, &g_copyRs);
        if (!g_copyVs || !g_copyPs || !g_copyRs) return false;
    }
    g_outW = w;
    g_outH = h;
    return true;
}

static void CopyOutputToBgra(ID3D11DeviceContext* ctx, int eye, ID3D11ShaderResourceView* srv, uint32_t w, uint32_t h) {
    D3D11_VIEWPORT vp{ 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f };
    ctx->OMSetRenderTargets(1, &g_outRtv[eye], nullptr);
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(g_copyRs);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(g_copyVs, nullptr, 0);
    ctx->PSSetShader(g_copyPs, nullptr, 0);
    ctx->PSSetShaderResources(0, 1, &srv);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* nullSrv = nullptr;
    ID3D11RenderTargetView* nullRtv = nullptr;
    ctx->PSSetShaderResources(0, 1, &nullSrv);
    ctx->OMSetRenderTargets(1, &nullRtv, nullptr);
}

static void JoinWorker() {
    if (g_worker.joinable()) g_worker.join();
}

static void StartAsyncBuild(ID3D11Device* dev, uint32_t inW, uint32_t inH, uint32_t outW, uint32_t outH,
                            int preset, int backend) {
    JoinWorker();
    g_state.store((int)State::Working, std::memory_order_release);
    dev->AddRef();
    g_worker = std::thread([dev, inW, inH, outW, outH, preset, backend]() {
        char why[256] = "";
        bool ok = true;
        if (!g_client.running() || (int)g_client.backend() != backend) {
            wchar_t exe[MAX_PATH] = {}, dataDir[MAX_PATH] = {};
            if (!LocateHelperExe(exe, dataDir)) {
                strcpy_s(g_failReason, "d2vr_dlss_host64.exe not found in d2vr_dlss\\ or game root");
                Log("[DLSS:ERR] %s", g_failReason);
                dev->Release();
                g_state.store((int)State::Failed, std::memory_order_release);
                return;
            }
            d2vr::dlss::StartParams sp{};
            sp.hostExe = exe;
            sp.dataDir = dataDir;
            sp.log = LogCallback;
            sp.backend = (uint32_t)backend;
            ok = g_client.start(dev, sp, why, sizeof(why));
            if (!ok) {
                strcpy_s(g_failReason, why[0] ? why : "helper start failed");
                Log("[DLSS:ERR] helper start failed: %s", g_failReason);
                dev->Release();
                g_state.store((int)State::Failed, std::memory_order_release);
                return;
            }
        }
        if (ok) ok = g_client.build(0, inW, inH, outW, outH, DXGI_FORMAT_B8G8R8A8_UNORM, preset, why, sizeof(why));
        if (ok) ok = g_client.build(1, inW, inH, outW, outH, DXGI_FORMAT_B8G8R8A8_UNORM, preset, why, sizeof(why));
        dev->Release();
        if (!ok) {
            strcpy_s(g_failReason, why[0] ? why : "feature build failed");
            Log("[DLSS:ERR] build %ux%u -> %ux%u failed: %s", inW, inH, outW, outH, g_failReason);
            g_state.store((int)State::Failed, std::memory_order_release);
            return;
        }
        g_builtInW = inW; g_builtInH = inH;
        g_builtOutW = outW; g_builtOutH = outH;
        g_builtPreset = preset;
        g_builtBackend = backend;
        Log("[DLSS] pair ready: mode=%s (%d) %ux%u -> %ux%u preset=%d backend=%s objMotion=%d biasMask=%d",
            ModeName(g_dlssMode), g_dlssMode, inW, inH, outW, outH, preset,
            backend == 1 ? "FSR" : "DLSS", (int)g_dlssObjectMotion, (int)g_dlssBiasMask);
        g_state.store((int)State::Ready, std::memory_order_release);
    });
}

} // namespace d2vr_dlss_eye

static bool DlssEyeActive() {
    return g_dlssMode > 0 && g_submitMode == 4 &&
           d2vr_dlss_eye::g_state.load(std::memory_order_acquire) != (int)d2vr_dlss_eye::State::Failed;
}

static void DlssEyeCacheSize(UINT outW, UINT outH, UINT* cacheW, UINT* cacheH) {
    *cacheW = outW;
    *cacheH = outH;
    if (!DlssEyeActive() || g_dlssMode <= 1) return;
    const float r = d2vr_dlss_eye::ModeRatio(g_dlssMode);
    UINT w = (UINT)((float)outW / r + 0.5f) & ~1u;
    UINT h = (UINT)((float)outH / r + 0.5f) & ~1u;
    if (w < 64) w = 64;
    if (h < 64) h = 64;
    if (w > outW) w = outW;
    if (h > outH) h = outH;
    *cacheW = w;
    *cacheH = h;
}

static void DlssEyeReleaseSources() {
    d2vr_dlss_eye::ReleaseLocalViews();
    if (d2vr_dlss_eye::g_guidesReady) {
        d2vr_dlss_eye::g_guides.forget(0);
        d2vr_dlss_eye::g_guides.forget(1);
    }
}

static void DlssEyeShutdown() {
    d2vr_dlss_eye::JoinWorker();
    d2vr_dlss_eye::ReleaseLocalViews();
    d2vr_dlss_eye::g_guides.shutdown();
    d2vr_dlss_eye::g_guidesReady = false;
    d2vr_dlss_eye::g_guidesFailed = false;
    d2vr_dlss_eye::g_client.stop();
    if (d2vr_dlss_eye::g_copyVs) { d2vr_dlss_eye::g_copyVs->Release(); d2vr_dlss_eye::g_copyVs = nullptr; }
    if (d2vr_dlss_eye::g_copyPs) { d2vr_dlss_eye::g_copyPs->Release(); d2vr_dlss_eye::g_copyPs = nullptr; }
    if (d2vr_dlss_eye::g_copyRs) { d2vr_dlss_eye::g_copyRs->Release(); d2vr_dlss_eye::g_copyRs = nullptr; }
    d2vr_dlss_eye::g_builtInW = d2vr_dlss_eye::g_builtInH = 0;
    d2vr_dlss_eye::g_builtOutW = d2vr_dlss_eye::g_builtOutH = 0;
    d2vr_dlss_eye::g_state.store((int)d2vr_dlss_eye::State::Off, std::memory_order_release);
}

static bool DlssEyeProcess(ID3D11Texture2D* left, ID3D11Texture2D* right, ID3D11Texture2D* outPair[2]) {
    using namespace d2vr_dlss_eye;
    outPair[0] = outPair[1] = nullptr;
    if (g_dlssMode <= 0 || g_submitMode != 4 || !g_dev11 || !g_ctx11 || !left || !right)
        return false;

    const State st = (State)g_state.load(std::memory_order_acquire);
    if (st == State::Failed || g_guidesFailed) return false;
    if (st == State::Working) return false; // warming up on worker thread

    D3D11_TEXTURE2D_DESC d0{}, d1{};
    left->GetDesc(&d0);
    right->GetDesc(&d1);
    if (!d0.Width || !d0.Height || d0.Width != d1.Width || d0.Height != d1.Height ||
        d0.Format != DXGI_FORMAT_B8G8R8A8_UNORM || !g_scW || !g_scH)
        return false;

    const uint32_t inW = d0.Width, inH = d0.Height;
    const uint32_t outW = g_scW, outH = g_scH;
    if (st == State::Off || !g_client.running() ||
        g_builtInW != inW || g_builtInH != inH || g_builtOutW != outW || g_builtOutH != outH ||
        g_builtPreset != g_dlssPreset || g_builtBackend != g_dlssBackend) {
        StartAsyncBuild(g_dev11, inW, inH, outW, outH, g_dlssPreset, g_dlssBackend);
        return false;
    }

    if (!g_guidesReady) {
        char why[256] = "";
        if (!g_guides.init(g_dev11, why, sizeof(why))) {
            Log("[DLSS:ERR] guide shaders init failed: %s", why);
            g_guidesFailed = true;
            return false;
        }
        g_guidesReady = true;
    }
    if (!EnsureOutTextures(g_dev11, outW, outH)) {
        Log("[DLSS:ERR] output BGRA8 textures %ux%u allocation failed", outW, outH);
        g_guidesFailed = true;
        return false;
    }

    ID3D11Texture2D* inputs[2] = { left, right };
    for (int e = 0; e < 2; ++e) {
        if (g_inOwner[e] != inputs[e]) {
            if (g_inSrv[e]) { g_inSrv[e]->Release(); g_inSrv[e] = nullptr; }
            if (g_inOwner[e]) { g_inOwner[e]->Release(); g_inOwner[e] = nullptr; }
            if (FAILED(g_dev11->CreateShaderResourceView(inputs[e], nullptr, &g_inSrv[e])))
                return false;
            g_inOwner[e] = inputs[e];
            inputs[e]->AddRef();
        }
    }

    const ULONGLONG nowMs = GetTickCount64();
    char why[256] = "";
    for (int e = 0; e < 2; ++e) {
        const XrPosef curPose = g_pairSubmitMetaValid ? g_pairSubmitEyePose[e] : g_eyePose[e];
        const XrFovf curFov = g_pairSubmitMetaValid ? g_pairSubmitFov[e] : g_eyeFov[e];
        const float tanH = std::max(0.1f, 0.5f * (std::tan(curFov.angleRight) - std::tan(curFov.angleLeft)));
        const float tanV = std::max(0.1f, 0.5f * (std::tan(curFov.angleUp) - std::tan(curFov.angleDown)));

        d2vr::dlss::GuideParams gp{};
        gp.w = inW;
        gp.h = inH;
        gp.tanH = tanH;
        gp.tanV = tanV;
        gp.prevTanH = g_hist[e].valid ? g_hist[e].tanH : tanH;
        gp.prevTanV = g_hist[e].valid ? g_hist[e].tanV : tanV;
        gp.historyValid = g_hist[e].valid && (nowMs - g_hist[e].tickMs < 250);

        if (gp.historyValid) {
            const auto& qp = g_hist[e].pose.orientation;
            const auto& qc = curPose.orientation;
            const XrVector3f Fp = RotVec(qp, 0.f, 0.f, -1.f);
            const XrVector3f Rp = RotVec(qp, 1.f, 0.f,  0.f);
            const XrVector3f Up = RotVec(qp, 0.f, 1.f,  0.f);
            const XrVector3f Fc = RotVec(qc, 0.f, 0.f, -1.f);
            const XrVector3f Rc = RotVec(qc, 1.f, 0.f,  0.f);
            const XrVector3f Uc = RotVec(qc, 0.f, 1.f,  0.f);
            auto dot3 = [](const XrVector3f& a, const XrVector3f& b) {
                return a.x * b.x + a.y * b.y + a.z * b.z;
            };
            gp.prevFromCur.m[0][0] = dot3(Fp, Fc); gp.prevFromCur.m[0][1] = dot3(Fp, Rc); gp.prevFromCur.m[0][2] = dot3(Fp, Uc);
            gp.prevFromCur.m[1][0] = dot3(Rp, Fc); gp.prevFromCur.m[1][1] = dot3(Rp, Rc); gp.prevFromCur.m[1][2] = dot3(Rp, Uc);
            gp.prevFromCur.m[2][0] = dot3(Up, Fc); gp.prevFromCur.m[2][1] = dot3(Up, Rc); gp.prevFromCur.m[2][2] = dot3(Up, Uc);
        }

        if (!g_guides.run(g_dev11, g_ctx11, gp, why, sizeof(why))) {
            Log("[DLSS:ERR] guide run eye %d failed: %s", e, why);
            return false;
        }
        if (g_dlssObjectMotion && gp.historyValid) {
            d2vr::dlss::GuideGpu::ObjParams op{};
            g_guides.objmotion(g_dev11, g_ctx11, e, g_inSrv[e], 0.0f, 0.0f, op, why, sizeof(why));
        }
        if (g_dlssBiasMask) {
            g_guides.mask(g_dev11, g_ctx11, e, g_inSrv[e], gp.historyValid, 0.03f, 0.12f, why, sizeof(why));
        }
        if (g_dlssObjectMotion || g_dlssBiasMask) {
            g_guides.keep(g_dev11, g_ctx11, e, inputs[e]);
        }

        d2vr::dlss::EyeInputs in{};
        in.color = inputs[e];
        in.depth = g_guides.depth();
        in.motion = g_guides.motion();
        in.bias = g_dlssBiasMask ? g_guides.bias() : nullptr;
        in.reset = !gp.historyValid;
        in.mvSign = g_dlssBackend == 1 ? d2vr::dlss::jitter::kFsrMvSign : 1.0f;
        in.frameTimeMs = gp.historyValid ? (float)std::max<ULONGLONG>(1, nowMs - g_hist[e].tickMs) : 11.1f;
        in.fovY = 2.0f * std::atan(tanV);
        in.metersPerUnit = 1.0f / std::max(0.01f, g_gameUnitsPerMeter);

        if (!g_client.evaluate(g_ctx11, e, in, why, sizeof(why))) {
            Log("[DLSS:ERR] evaluate eye %d failed: %s", e, why);
            if (!g_client.running()) g_state.store((int)State::Failed, std::memory_order_release);
            return false;
        }
        CopyOutputToBgra(g_ctx11, e, g_client.output(e), outW, outH);

        g_hist[e].valid = true;
        g_hist[e].pose = curPose;
        g_hist[e].tanH = tanH;
        g_hist[e].tanV = tanV;
        g_hist[e].tickMs = nowMs;
    }

    if (g_client.stats.frames[0] == 1 || (g_client.stats.frames[0] % 300) == 0) {
        const double gpuL = g_client.stats.gpuN[0] ? g_client.stats.gpuMsSum[0] / g_client.stats.gpuN[0] : -1.0;
        const double gpuR = g_client.stats.gpuN[1] ? g_client.stats.gpuMsSum[1] / g_client.stats.gpuN[1] : -1.0;
        const double cpuL = g_client.stats.frames[0] ? g_client.stats.cpuMsSum[0] / g_client.stats.frames[0] : 0.0;
        const double cpuR = g_client.stats.frames[1] ? g_client.stats.cpuMsSum[1] / g_client.stats.frames[1] : 0.0;
        Log("[DLSS] frames=%llu/%llu refused=%llu %ux%u->%ux%u GPU avg L=%.2fms R=%.2fms CPU avg L=%.2fms R=%.2fms objTiles=%llu/%llu",
            (unsigned long long)g_client.stats.frames[0], (unsigned long long)g_client.stats.frames[1],
            (unsigned long long)(g_client.stats.refused[0] + g_client.stats.refused[1]),
            inW, inH, outW, outH, gpuL, gpuR, cpuL, cpuR,
            (unsigned long long)g_guides.objStats.overridden, (unsigned long long)g_guides.objStats.searched);
    }

    outPair[0] = g_outTex[0];
    outPair[1] = g_outTex[1];
    return true;
}

static void LoadDlssConfig(const char* chosenIni) {
    char dlssVal[32]{};
    GetPrivateProfileStringA("Render", "DlssMode", "0", dlssVal, sizeof(dlssVal), chosenIni);
    g_dlssMode = d2vr_dlss_eye::ParseDlssMode(dlssVal);
    GetPrivateProfileStringA("Render", "DlssPreset", "16", dlssVal, sizeof(dlssVal), chosenIni);
    g_dlssPreset = d2vr_dlss_eye::ParseDlssPreset(dlssVal);
    GetPrivateProfileStringA("Render", "DlssBackend", "0", dlssVal, sizeof(dlssVal), chosenIni);
    g_dlssBackend = (!_stricmp(dlssVal, "FSR") || !strcmp(dlssVal, "1")) ? 1 : 0;
    g_dlssObjectMotion = GetPrivateProfileIntA("Render", "DlssObjectMotion", 1, chosenIni) != 0;
    g_dlssBiasMask = GetPrivateProfileIntA("Render", "DlssBiasMask", 0, chosenIni) != 0;
    Log("[DLSS] mode=%d (%s) preset=%d backend=%s objectMotion=%d biasMask=%d",
        g_dlssMode, d2vr_dlss_eye::ModeName(g_dlssMode), g_dlssPreset,
        g_dlssBackend == 1 ? "FSR" : "DLSS", (int)g_dlssObjectMotion, (int)g_dlssBiasMask);
}

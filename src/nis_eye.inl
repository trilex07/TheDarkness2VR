// Optional NIS sharpening for the completed per-eye D3D11 images. The game
// renders both eye caches first; this filter never changes their projections.
#include "nis_runtime.h"
#include "nis/NIS_Scaler_CS.h"
#include "nis/NIS_Sharpen_CS.h"

static NisRuntime g_nisEye;
static ID3D11ShaderResourceView* g_nisEyeSource[2]{};
static ID3D11Texture2D* g_nisEyeSourceOwner[2]{};
static bool g_nisScalerFailed=false;
static bool g_nisEyeFailed=false;
static bool g_nisEyeReported=false;

static void NisEyeLog(const char* message) { Log("[NIS] %s",message); }

static void NisEyeReleaseSources() {
    for(int eye=0;eye<2;++eye) {
        if(g_nisEyeSource[eye]) g_nisEyeSource[eye]->Release();
        if(g_nisEyeSourceOwner[eye]) g_nisEyeSourceOwner[eye]->Release();
        g_nisEyeSource[eye]=nullptr;
        g_nisEyeSourceOwner[eye]=nullptr;
    }
}

static void NisEyeShutdown() {
    NisEyeReleaseSources();
    NisShutdown(&g_nisEye);
    g_nisScalerFailed=false;
    g_nisEyeFailed=false;
    g_nisEyeReported=false;
}

static bool NisEyeFailure(const char* operation) {
    g_nisEyeFailed=true;
    Log("[NIS] %s failed; filter disabled until device Reset",operation);
    return false;
}

static bool NisEyeProcess(ID3D11Texture2D* left,ID3D11Texture2D* right,
                          ID3D11Texture2D* filtered[2]) {
    filtered[0]=filtered[1]=nullptr;
    if(g_nisEyeFailed || (g_nisMode!=2 && g_nisMode!=3) || !g_dev11 || !g_ctx11 || !left || !right) return false;
    ID3D11Texture2D* originals[2]={left,right};
    D3D11_TEXTURE2D_DESC desc[2]{};
    originals[0]->GetDesc(&desc[0]); originals[1]->GetDesc(&desc[1]);
    if(!desc[0].Width || !desc[0].Height ||
       desc[0].Width!=desc[1].Width || desc[0].Height!=desc[1].Height ||
       desc[0].Format!=DXGI_FORMAT_B8G8R8A8_UNORM ||
       desc[1].Format!=desc[0].Format) return false;
    if(!NisInit(&g_nisEye,g_dev11,kNisScalerCS,sizeof(kNisScalerCS),
                kNisSharpenCS,sizeof(kNisSharpenCS),nullptr,NisEyeLog)) return NisEyeFailure("shader init");
    for(int eye=0;eye<2;++eye) if(g_nisEyeSourceOwner[eye]!=originals[eye]) {
        if(g_nisEyeSource[eye]) g_nisEyeSource[eye]->Release();
        if(g_nisEyeSourceOwner[eye]) g_nisEyeSourceOwner[eye]->Release();
        g_nisEyeSource[eye]=nullptr;
        g_nisEyeSourceOwner[eye]=nullptr;
        if(FAILED(g_dev11->CreateShaderResourceView(originals[eye],nullptr,&g_nisEyeSource[eye]))) {
            NisEyeReleaseSources();return NisEyeFailure("eye SRV");
        }
        g_nisEyeSourceOwner[eye]=originals[eye]; originals[eye]->AddRef();
    }
    const UINT outW=g_nisMode==3 ? g_scW : desc[0].Width;
    const UINT outH=g_nisMode==3 ? g_scH : desc[0].Height;
    if(!NisEnsureOutput(&g_nisEye,g_dev11,desc[0].Width,desc[0].Height,outW,outH)) return NisEyeFailure("output allocation");
    const int previousSamples=g_nisEye.timingFrames;
    NisTimingPoll(&g_nisEye,g_ctx11);
    if(g_nisEye.timingFrames!=previousSamples &&
       (g_nisEye.timingFrames==1 || g_nisEye.timingFrames%120==0))
        Log("[NIS] GPU pair dispatch sample=%.3fms peak=%.3fms samples=%d; excludes final copies/XR, asynchronous DONOTFLUSH",
            g_nisEye.msGpu,g_nisEye.msGpuPeak,g_nisEye.timingFrames);
    NisTimingBegin(&g_nisEye,g_ctx11);
    for(int eye=0;eye<2;++eye)
        if(!NisDispatchEye(&g_nisEye,g_ctx11,g_nisEyeSource[eye],
                           desc[eye].Width,desc[eye].Height,eye,
                           outW,outH,g_nisSharpness,g_nisMode==2)) {
            NisTimingEnd(&g_nisEye,g_ctx11);
            return NisEyeFailure("eye dispatch");
        }
    NisTimingEnd(&g_nisEye,g_ctx11);
    if(!g_nisEyeReported) {
        Log("[NIS] pair dispatched mode=%d strength=%.2f source=%ux%u output=%ux%u; independent eyes, native projections retained",
            g_nisMode,g_nisSharpness,desc[0].Width,desc[0].Height,outW,outH);
        g_nisEyeReported=true;
    }
    if(g_nisMode==3) {
        static UINT loggedInW=0,loggedInH=0,loggedOutW=0,loggedOutH=0;
        if(loggedInW!=desc[0].Width || loggedInH!=desc[0].Height ||
           loggedOutW!=outW || loggedOutH!=outH) {
            Log("[NIS] eye pair %ux%u -> %ux%u; mode=3",desc[0].Width,desc[0].Height,outW,outH);
            loggedInW=desc[0].Width;loggedInH=desc[0].Height;loggedOutW=outW;loggedOutH=outH;
        }
    }
    filtered[0]=g_nisEye.outTex[0];filtered[1]=g_nisEye.outTex[1];
    return true;
}

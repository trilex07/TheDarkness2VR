// Optional NIS sharpening for the completed per-eye D3D11 images. The game
// renders both eye caches first; this filter never changes their projections.
#include "nis_runtime.h"
#include "nis/NIS_Scaler_CS.h"
#include "nis/NIS_Sharpen_CS.h"
#include "gaze_nis_copy_base.inl"

static NisRuntime g_nisEye;
static ID3D11ShaderResourceView* g_nisEyeSource[2]{};
static ID3D11Texture2D* g_nisEyeSourceOwner[2]{};
static bool g_nisScalerFailed=false;
static bool g_nisEyeFailed=false;
static bool g_nisEyeReported=false;
static unsigned long long g_nisCopyBaseEyes=0;

static void NisEyeLog(const char* message) { Log("[NIS] %s",message); }
// Foveated sharpening ([Render] FoveatedSharpen): gaze pixel of this eye's
// image, defined in eye_gaze.inl (needs eye poses/FOV declared later).
static bool FoveaGazePixel(int eye,uint32_t width,uint32_t height,float& px,float& py);

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
        Log("[NIS] GPU pair dispatch sample=%.3fms peak=%.3fms samples=%d copyBaseEyes=%llu; excludes final copies/XR, asynchronous DONOTFLUSH",
            g_nisEye.msGpu,g_nisEye.msGpuPeak,g_nisEye.timingFrames,g_nisCopyBaseEyes);
    NisTimingBegin(&g_nisEye,g_ctx11);
    // Foveated: whole image at the periphery level, then a transition square
    // and the fovea square around the gaze, each overwriting its rectangle.
    // Same-size sharpen mode only; any missing gaze -> uniform NisSharpness.
    const bool foveaMode=g_foveatedSharpen && g_nisMode==2 && outW==desc[0].Width && outH==desc[0].Height;
    float gx[2]{}, gy[2]{}; bool gaze[2]{};
    if(foveaMode) for(int eye=0;eye<2;++eye) gaze[eye]=FoveaGazePixel(eye,outW,outH,gx[eye],gy[eye]);
    const auto levels=d2vr_fovea::Grade(g_foveaPeriphery,g_foveaSharpness);
    for(int eye=0;eye<2;++eye) {
        const bool copyBase=gaze[eye] && g_foveatedCopyBase &&
            desc[eye].SampleDesc.Count==1 && desc[eye].MipLevels==1 && desc[eye].ArraySize==1;
        if(copyBase) {
            const auto outer=d2vr_fovea::Around(gx[eye],gy[eye],outW,outH,g_foveaSize*1.7f);
            const auto inner=d2vr_fovea::Around(gx[eye],gy[eye],outW,outH,g_foveaSize);
            if(!NisCopyBaseAndSharpenGaze(&g_nisEye,g_ctx11,originals[eye],g_nisEyeSource[eye],
                                         outW,outH,eye,outer,inner,levels)) {
                NisTimingEnd(&g_nisEye,g_ctx11);
                return NisEyeFailure("gaze copy-base dispatch");
            }
            ++g_nisCopyBaseEyes;
            continue;
        }
        const float base=gaze[eye] ? levels.periphery : g_nisSharpness;
        if(!NisDispatchEye(&g_nisEye,g_ctx11,g_nisEyeSource[eye],
                           desc[eye].Width,desc[eye].Height,eye,
                           outW,outH,base,g_nisMode==2)) {
            NisTimingEnd(&g_nisEye,g_ctx11);
            return NisEyeFailure("eye dispatch");
        }
        if(gaze[eye]) {
            const auto outer=d2vr_fovea::Around(gx[eye],gy[eye],outW,outH,g_foveaSize*1.7f);
            const auto inner=d2vr_fovea::Around(gx[eye],gy[eye],outW,outH,g_foveaSize);
            if(!NisDispatchEyeRect(&g_nisEye,g_ctx11,g_nisEyeSource[eye],outW,outH,eye,
                                   outer.x,outer.y,outer.w,outer.h,levels.transition) ||
               !NisDispatchEyeRect(&g_nisEye,g_ctx11,g_nisEyeSource[eye],outW,outH,eye,
                                   inner.x,inner.y,inner.w,inner.h,levels.fovea)) {
                NisTimingEnd(&g_nisEye,g_ctx11);
                return NisEyeFailure("foveated dispatch");
            }
        }
    }
    NisTimingEnd(&g_nisEye,g_ctx11);
    // Log a state only after it held for 1 s (blinks drop the gaze briefly).
    static int lastFovea=-1, pendingFovea=-1; static ULONGLONG pendingSince=0;
    const int foveaState=int(gaze[0])+int(gaze[1]);
    const ULONGLONG foveaNow=GetTickCount64();
    if(foveaState!=pendingFovea) { pendingFovea=foveaState; pendingSince=foveaNow; }
    if(foveaMode && foveaState!=lastFovea && foveaNow-pendingSince>=1000) {
        lastFovea=foveaState;
        Log("[NIS] foveated sharpening %s (periphery=%.2f transition=%.2f fovea=%.2f size=%.2f); gaze eyes=%d",
            foveaState ? "active" : "waiting for gaze (uniform NisSharpness)",levels.periphery,levels.transition,levels.fovea,g_foveaSize,foveaState);
    }
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

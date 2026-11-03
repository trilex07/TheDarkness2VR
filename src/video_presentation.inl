// Presentation identity only: no GPU readback and no device-state mutations.
// Same-frame cinema evidence: distinguish decoder-to-texture draws from the
// final submitted target. A decoded Bink handle alone cannot establish this.
static IDirect3DSurface9* g_videoProbeTargets[16]{};
static unsigned g_videoProbeTargetCount=0;
static uint64_t g_videoProbeFrame=0;
static uint64_t g_presentedVideoFrame=0;
static bool PresentedVideoThisFrame() {
    return g_trackedFrame.open && g_trackedFrame.id && g_presentedVideoFrame==g_trackedFrame.id;
}
static void ClearVideoProbeTargets() {
    for(unsigned i=0;i<g_videoProbeTargetCount;++i) {
        g_videoProbeTargets[i]->Release();g_videoProbeTargets[i]=nullptr;
    }
    g_videoProbeTargetCount=0;
}
static void ObserveVideoPresentation(IDirect3DSurface9* finalTarget) {
    if(!g_videoProbeTargetCount)return;
    if(g_videoProbeFrame!=g_trackedFrame.id){ClearVideoProbeTargets();return;}
    bool matched=false;
    for(unsigned i=0;i<g_videoProbeTargetCount;++i) {
        matched=matched || g_videoProbeTargets[i]==finalTarget;
        g_videoProbeTargets[i]->Release();g_videoProbeTargets[i]=nullptr;
    }
    if(matched)g_presentedVideoFrame=g_trackedFrame.id;
    static unsigned logged=0;
    if(logged++<32)Log("[VideoPresentationProbe] finalTarget=%p videoTargets=%u matched=%d; same-frame cinema evidence",finalTarget,g_videoProbeTargetCount,int(matched));
    g_videoProbeTargetCount=0;
}
static void ObserveVideoDraw(IDirect3DDevice9* dev) {
    static const bool enabled=GetPrivateProfileIntA("Diagnostics","VideoDrawProbe",0,".\\Darkness2VR-diagnostic.ini")!=0;
    if((!enabled && !g_cinematicTheater) || d2vr_replay::g_replayEye>=0 ||
       GetCurrentThreadId()!=g_renderThreadId || !g_trackedFrame.open)return;
    const auto ps=d2vr_replay::g_activePsHash;
    if(ps!=0xC1DC2D91u && ps!=0xD59D80ACu)return;
    IDirect3DSurface9 *rt=nullptr,*back=nullptr;IDirect3DVertexShader9* vs=nullptr;IDirect3DPixelShader9* pixel=nullptr;
    D3DSURFACE_DESC desc{};D3DVIEWPORT9 vp{};DWORD depth=0,fvf=0;
    const bool valid=SUCCEEDED(dev->GetRenderTarget(0,&rt)) && rt &&
        SUCCEEDED(rt->GetDesc(&desc)) && SUCCEEDED(dev->GetViewport(&vp)) &&
        SUCCEEDED(dev->GetPixelShader(&pixel)) && pixel &&
        SUCCEEDED(dev->GetVertexShader(&vs)) && SUCCEEDED(dev->GetRenderState(D3DRS_ZENABLE,&depth));
    const auto metadata=d2vr_replay::g_vsInfo.Lookup(vs);
    const auto vh=metadata?metadata->hash:0;
    if(valid && metadata && metadata->videoClip && d2vr_replay::g_pixelShaders.Lookup(pixel).videoYuv &&
       depth==0 && vp.X==0 && vp.Y==0 && vp.Width==desc.Width && vp.Height==desc.Height) {
        if(g_videoProbeFrame!=g_trackedFrame.id)ClearVideoProbeTargets();
        g_videoProbeFrame=g_trackedFrame.id;
        bool retained=false;
        for(unsigned i=0;i<g_videoProbeTargetCount;++i)retained=retained || g_videoProbeTargets[i]==rt;
        if(!retained && g_videoProbeTargetCount<16) {
            rt->AddRef();g_videoProbeTargets[g_videoProbeTargetCount++]=rt;
        }
    }
    if(enabled) {
    dev->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back);dev->GetFVF(&fvf);
    static uint64_t seen[32]{};static unsigned count=0;
    const uint64_t key=(uint64_t(ps)<<32)^vh^(uint64_t(rt==back)<<63)^(uint64_t(depth)<<62);
    bool found=false;for(unsigned i=0;i<count;++i)found=found||seen[i]==key;
    if(valid && !found && count<32) {
        seen[count++]=key;
        Log("[VideoDrawProbe] ps=%08X vs=%08X rt=%p back=%p directBackbuffer=%d depth=%lu fvf=%08lX size=%ux%u viewport=%u,%u,%u,%u wvp=%d; candidate video draw, final-target match required",
            ps,vh,rt,back,int(rt==back),depth,fvf,desc.Width,desc.Height,vp.X,vp.Y,vp.Width,vp.Height,metadata?metadata->wvpBase:-1);
    }
    }
    if(pixel)pixel->Release();if(vs)vs->Release();if(back)back->Release();if(rt)rt->Release();
}
static HRESULT ObserveVideoDrawResult(IDirect3DDevice9* dev,HRESULT result) {
#ifdef D2VR_MATCHED_CAPTURE
    MatchedCaptureDrawReturned(dev,result);
#endif
    if(SUCCEEDED(result))ObserveVideoDraw(dev);
    return result;
}

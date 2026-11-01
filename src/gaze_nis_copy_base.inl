// Same-size eye images only. Copy the already rendered image, then run NIS
// only over the two gaze rectangles. The native D3D9 scene is unchanged.
static bool NisCopyBaseAndSharpenGaze(NisRuntime* nis,ID3D11DeviceContext* ctx,
    ID3D11Texture2D* original,ID3D11ShaderResourceView* source,
    UINT width,UINT height,int eye,const d2vr_fovea::Rect& outer,
    const d2vr_fovea::Rect& inner,const d2vr_fovea::Levels& levels) {
    if(!nis || !nis->ready || !ctx || !original || !source ||
       eye<0 || eye>1 || !nis->outTex[eye] ||
       nis->outW!=width || nis->outH!=height || outer.Empty() || inner.Empty())return false;
    // Subresource 0 is the single-sample, single-mip BGRA eye. The caller
    // checks its descriptor before this copy; no read/write alias is allowed.
    if(original==nis->outTex[eye])return false;
    ctx->CopySubresourceRegion(nis->outTex[eye],0,0,0,0,original,0,nullptr);
    return NisDispatchEyeRect(nis,ctx,source,width,height,eye,
               outer.x,outer.y,outer.w,outer.h,levels.transition) &&
           NisDispatchEyeRect(nis,ctx,source,width,height,eye,
               inner.x,inner.y,inner.w,inner.h,levels.fovea);
}

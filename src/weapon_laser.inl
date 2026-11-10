// Independent swapchain reuses the tested settings acquire/wait/release path.
// Only one solid texel is sampled; the panel itself is never submitted here.
static void AppendWeaponLaserLayers(const XrCompositionLayerBaseHeader** layers,
    uint32_t& count,unsigned capacity,XrCompositionLayerQuad (&beams)[2],bool shouldRender) {
#ifdef D2VR_MATCHED_CAPTURE
    if(!g_vrSettings.Options().weaponLaser || !shouldRender || !count ||
       g_vrSettings.Visible() || g_wheelGate.open || MenuConfirmActive() ||
       g_xrSessionState!=XR_SESSION_STATE_FOCUSED || !ProtectedWeaponAvailable())return;
    NativeGameplaySample owner;
    if(!CopyNativeGameplay(owner))return;
    XrPosef poses[2]{};XrExtent2Df sizes[2]{};bool visible[2]{};
    const auto head=HeadCenterPose(g_eyePose[0],g_eyePose[1]);
    for(unsigned h=0;h<2;++h) {
        XrPosef aim{};
        // Same owned firearm, controller space and optional support-hand pose
        // as ResolveProtectedWeapon. Empty/unknown/stock-aim weapons get no ray.
        visible[h]=CopyProtectedWeaponHandPose(owner.camera,h,aim) &&
            (g_vrSettings.Options().weaponLaser==2 ?
                d2vr_gameplay::WeaponDot(aim,head,g_xrSpace,poses[h],sizes[h]):
                d2vr_gameplay::WeaponBeam(aim,head,g_xrSpace,poses[h],sizes[h]));
    }
    if(!visible[0]&&!visible[1])return;
    XrCompositionLayerQuad texture{XR_TYPE_COMPOSITION_LAYER_QUAD};
    if(!g_weaponLaserRenderer.Render(texture,true))return;
    const unsigned limit=std::min(capacity,g_weaponLaserRenderer.maxLayers);
    for(unsigned h=0;h<2 && count<limit;++h)if(visible[h]) {
        auto& beam=beams[h];beam=texture;beam.space=g_xrSpace;
        beam.pose=poses[h];beam.size=sizes[h];
        if(g_vrSettings.Options().weaponLaser==2) {
            beam.layerFlags|=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            beam.subImage.imageRect={{32,0},{16,16}};
        } else beam.subImage.imageRect={{0,0},{1,1}};
        layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&beam);
    }
#endif
}

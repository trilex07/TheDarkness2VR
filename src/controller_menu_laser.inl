static void RecordProjectedMenuSurface() {
#ifdef D2VR_MATCHED_CAPTURE
    if(!g_haveViews || !g_in3DGameplayMode || CinemaSceneThisFrame() ||
       !NativeMenuPresentationActive() || !g_nativeCameraCoverConfirmed ||
       !(g_nativeUiReferencePx>0) || !(g_nativeUiReferencePy>0))return;
    auto pose=HeadCenterPose(g_eyePose[0],g_eyePose[1]);
    double q[4];if(!d2vr_menu::detail::Pose(pose,q))return;
    const auto forward=d2vr_menu::detail::Rotate(q,{0,0,-1.5});
    pose.position.x+=float(forward.x);pose.position.y+=float(forward.y);pose.position.z+=float(forward.z);
    XrCompositionLayerQuad menu{XR_TYPE_COMPOSITION_LAYER_QUAD};
    menu.pose=pose;menu.space=g_xrSpace;
    menu.size={3.f*1.55f/g_nativeUiReferencePx,3.f*1.55f/g_nativeUiReferencePy};
    RecordMenuSurface(menu); // same NativeCameraUiScale plane as the settings entry
#endif
}
static void AppendControllerMenuLaser(const XrCompositionLayerBaseHeader** layers,
    uint32_t& count,unsigned capacity,XrCompositionLayerQuad (&quads)[2],bool shouldRender) {
#ifdef D2VR_MATCHED_CAPTURE
    if(!shouldRender || !count || !ControllerMenuEnabled())return;
    MenuSurface source;
    AcquireSRWLockShared(&g_menuSurfaceLock);source=g_menuSurface;ReleaseSRWLockShared(&g_menuSurfaceLock);
    if(!source.valid || source.frame!=g_trackedFrame.id || source.space!=g_xrSpace ||
       !g_eyeCacheW || !g_eyeCacheH)return;
    d2vr_menu::Surface surface;surface.identity=1;surface.frame=source.frame;surface.space=source.space;
    surface.pose=source.pose;surface.width=source.width;surface.height=source.height;
    surface.cursor={0,0,g_eyeCacheW,g_eyeCacheH};
    d2vr_menu::Aim aim;aim.frame=surface.frame;aim.space=surface.space;
    aim.pose=g_hands[1].aim;aim.tracked=aim.menu=true;
    d2vr_menu::Hit hit;if(!d2vr_menu::Project(surface,aim,hit) || !hit.inside)return;
    XrCompositionLayerQuad texture{XR_TYPE_COMPOSITION_LAYER_QUAD};
    if(!g_weaponLaserRenderer.Render(texture,true))return;
    const unsigned limit=std::min(capacity,g_weaponLaserRenderer.maxLayers);
    if(count>=limit)return;
    auto& dot=quads[0];dot=texture;dot.space=surface.space;dot.pose=surface.pose;
    dot.pose.position=hit.point;
    double q[4];if(!d2vr_menu::detail::Pose(surface.pose,q))return;
    const auto offset=d2vr_menu::detail::Rotate(q,{0,0,.006});
    dot.pose.position.x+=float(offset.x);dot.pose.position.y+=float(offset.y);dot.pose.position.z+=float(offset.z);
    dot.size={.008f,.008f};dot.subImage.imageRect={{0,0},{1,1}};
    layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&dot);
    XrPosef pose{};XrExtent2Df size{};
    if(count<limit && d2vr_settings::Laser(surface,aim,HeadCenterPose(g_eyePose[0],g_eyePose[1]),pose,size)) {
        auto& beam=quads[1];beam=dot;beam.pose=pose;beam.size=size;
        layers[count++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&beam);
    }
#endif
}

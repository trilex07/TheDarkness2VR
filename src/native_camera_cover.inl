// Included only in the opt-in diagnostic build. No game addresses are touched
// until the SHA/PE/site gate installs the single selected-view call site.
static bool g_nativeCameraCoverInstalled=false, g_nativeCameraCoverConfirmed=false;
static float g_nativeCoveredPx=0, g_nativeCoveredPy=0;
static float g_nativeUiReferencePx=0,g_nativeUiReferencePy=0;
static uint64_t g_nativeCoveredFrame=0, g_nativeCoverChanges=0;
static uintptr_t g_nativeCoveredCamera=0;
static float g_nativeCoveredProjection[16]{},g_nativeCoveredViewProjection[16]{};
static void NativeCameraCaptureChanged();
static void NativeCameraUiScale(float& x,float& y) {
    if(g_nativeCameraCoverInstalled && g_nativeCameraCoverConfirmed && g_nativeUiReferencePx>0 && g_nativeUiReferencePy>0) {
        // Pixel-space HUD is not rebuilt by the native camera setter. Preserve
        // its original angular scale and the BOSS NAME scissor fix independently.
        x*=g_nativeCoveredPx/g_nativeUiReferencePx;y*=g_nativeCoveredPy/g_nativeUiReferencePy;
    }
}

static bool NativeCameraEyeScale(int eye,float& x,float& y) {
    if(!g_nativeCameraCoverInstalled || !g_nativeCameraCoverConfirmed || !g_haveViews) return false;
    const auto& f=g_eyeFov[eye&1];
    const float angles[4]={f.angleLeft,f.angleRight,f.angleUp,f.angleDown};
    // Native frustum is the conservative union. Contract excess coverage in
    // one axis when necessary; the legacy widen-only clamp cannot do this.
    return d2vr_camera_cover::EyeScale(g_nativeCoveredPx,g_nativeCoveredPy,angles,g_autoFovCoverage,1+g_edgeCrop,x,y);
}

static void RefreshSelectedEyeParams(const float* inverse,bool changed) {
    // Shader inverse-projection probes can replace the shared estimate between
    // selected-camera callbacks. Restore this frame's complete native matrix,
    // even at constant FOV; its clip planes may have changed independently too.
    const auto previousLeft=g_frameEyeParams[0],previousRight=g_frameEyeParams[1];
    d2vr_replay::g_cameraPx=g_estPx=g_nativeCoveredPx;
    d2vr_replay::g_cameraPy=g_estPy=g_nativeCoveredPy;
    d2vr_replay::g_cameraProjectionKnown=true;
    std::memcpy(d2vr_replay::g_cameraInverseProjection,inverse,16*sizeof(float));
    InterlockedExchange(&g_estValid,1);
    g_frameEyeParamsValid=false;
    for(int e=0;e<2;++e) g_frameEyeParams[e]=BuildEyeParams(e);
    g_frameEyeParamsValid=true;
    for(int e=0;e<2;++e) g_perEyeCropX[e]=g_perEyeCropY[e]=1/(1+g_edgeCrop);
    // A capture header written before this callback must not describe different
    // eye transforms than the draws. Compare fields, never structure padding.
    const auto differs=[](const d2vr_replay::StereoParams& a,const d2vr_replay::StereoParams& b) {
        return a.headGeometry!=b.headGeometry || a.fovScaleX!=b.fovScaleX || a.fovScaleY!=b.fovScaleY ||
            std::memcmp(a.headClip,b.headClip,sizeof(a.headClip)) ||
            std::memcmp(a.inverseHeadClip,b.inverseHeadClip,sizeof(a.inverseHeadClip));
    };
    if(changed || differs(previousLeft,g_frameEyeParams[0]) || differs(previousRight,g_frameEyeParams[1]))
        NativeCameraCaptureChanged();
}

struct NativeCameraFields {
    float aspect,fov,nearZ,farZ,projection[16];
};
static bool ReadSelectedRenderCamera(void* camera,void* owner,NativeCameraFields& out) {
    __try {
        const auto c=static_cast<const unsigned char*>(camera);
        const auto o=static_cast<const unsigned char*>(owner);
        // EBX owns the selected-camera strong reference throughout 985D80.
        // Never scan arbitrary objects or retain this reference after return.
        if(!c || !o || *reinterpret_cast<const uint32_t*>(c)!=d2vr_camera_cover::cameraVtable ||
           **reinterpret_cast<void***>(const_cast<unsigned char*>(o)+0x3f4)!=camera || c[0x614]!=0) return false;
        std::memcpy(&out.aspect,c+0x5f4,4*sizeof(float));
        std::memcpy(out.projection,c+0x4c0,sizeof(out.projection));
        return d2vr_camera_cover::Projection(out.projection,out.fov,out.aspect,out.nearZ,out.farZ);
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}

static void __fastcall CoverSelectedRenderCamera(void* camera,void* owner) {
    if(!g_nativeCameraCoverInstalled || GetCurrentThreadId()!=g_renderThreadId ||
       !g_trackedFrame.open || !g_haveViews || !g_in3DGameplayMode ||
       g_submitMode!=4 || g_stereoMode!=1) return;
    // At most one selected render camera per XR frame. Reflections and shadow
    // cameras do not enter this call site; reject any unexpected second owner.
    if(g_nativeCoveredFrame==g_trackedFrame.id) {
        if(g_nativeCoveredCamera!=reinterpret_cast<uintptr_t>(camera)) {
            g_trackedFrame.renderTrackingValid=false;NativeCameraCaptureChanged();
            Log("[NativeCameraCover] REFUSED multiple selected cameras in frame=%llu",g_trackedFrame.id);
        }
        return;
    }
    if(g_had3DCameraThisFrame) return;
    NativeCameraFields before{};
    if(!ReadSelectedRenderCamera(camera,owner,before)) return;
    float angles[2][4];
    for(int e=0;e<2;++e) {
        const auto& f=g_eyeFov[e];
        angles[e][0]=f.angleLeft;angles[e][1]=f.angleRight;angles[e][2]=f.angleUp;angles[e][3]=f.angleDown;
    }
    d2vr_camera_cover::Plan plan;
    if(!d2vr_camera_cover::Cover(before.fov,before.aspect,angles,g_autoFovCoverage,1+g_edgeCrop,plan)) return;
    using Setter=void(__thiscall*)(void*,float);
    const auto setFov=reinterpret_cast<Setter>(0xC813B0);
    if(std::fabs(before.fov-plan.fov)>1e-4f) setFov(camera,plan.fov);
    NativeCameraFields after{};
    if(!ReadSelectedRenderCamera(camera,owner,after) || std::fabs(after.fov-plan.fov)>1e-3f ||
       after.aspect!=before.aspect || after.nearZ!=before.nearZ || after.farZ!=before.farZ) {
        setFov(camera,before.fov);
        Log("[NativeCameraCover] FATAL setter result mismatch; restoring original FOV then stopping candidate");
        RaiseFailFastException(nullptr,nullptr,0);return;
    }
    float inverse[16];
    if(!d2vr_math::Invert(after.projection,inverse)) {
        setFov(camera,before.fov);RaiseFailFastException(nullptr,nullptr,0);return;
    }
    // Freeze the actual native projection and both eye transforms BEFORE any
    // 3D command is recorded. No previous-frame widen may be applied to it.
    const bool changed=!g_nativeCameraCoverConfirmed ||
        std::memcmp(g_nativeCoveredProjection,after.projection,sizeof(after.projection))!=0;
    if(!g_nativeCameraCoverConfirmed) {
        g_nativeUiReferencePx=before.projection[0];g_nativeUiReferencePy=before.projection[5];
    }
    g_nativeCoveredPx=after.projection[0];g_nativeCoveredPy=after.projection[5];
    g_nativeCameraCoverConfirmed=true;g_nativeCoveredFrame=g_trackedFrame.id;
    g_nativeCoveredCamera=reinterpret_cast<uintptr_t>(camera);
    std::memcpy(g_nativeCoveredProjection,after.projection,sizeof(after.projection));
    std::memcpy(g_nativeCoveredViewProjection,static_cast<const unsigned char*>(camera)+0x500,64);
    RefreshSelectedEyeParams(inverse,changed);
    // First transition cannot masquerade as a matched capture whose header
    // was emitted before this camera was known. A later stable frame can pass.
    if(++g_nativeCoverChanges<=3 || g_nativeCoverChanges%300==0)
        Log("[NativeCameraCover] frame=%llu camera=%p owner=%p fov=%.6g->%.6g aspect=%.6g PxPy=%.7g,%.7g eyesScale=%.7g,%.7g/%.7g,%.7g projection+VP+frustum=native-rebuilt",
            g_trackedFrame.id,camera,owner,before.fov,after.fov,after.aspect,g_nativeCoveredPx,g_nativeCoveredPy,
            g_frameEyeParams[0].fovScaleX,g_frameEyeParams[0].fovScaleY,g_frameEyeParams[1].fovScaleX,g_frameEyeParams[1].fovScaleY);
}

static void* g_nativeNearGetter=reinterpret_cast<void*>(0xD3A320);
// The original leaf getter only returns ST(0). Preserve even volatile registers
// and the complete x87/SSE state; tail-call that getter for the exact near value.
D2VR_SELECTED_CAMERA_SHIM(SelectedRenderCameraNear,CoverSelectedRenderCamera,g_nativeNearGetter)

static d2vr_native_view::ProjectionWitness g_nativeViewWitness;
static d2vr_native_view::InstanceTable g_nativeRenderViewTable;
static bool PrepareNativeRenderViewTable();
static void NativeViewProjectionObserved(const float* inverse) {
    if(!g_nativeViewBoundary.Installed() || GetCurrentThreadId()!=g_renderThreadId)return;
    if(g_nativeViewWitness.Observe(g_trackedFrame.id,g_had3DCameraThisFrame,inverse)) {
        static unsigned reports=0;
        if(++reports<=3 || reports%600==0)
            Log("[NativeViewBoundary] projection corroborated frame=%llu view=%p renderer=%p camera=%p",
                g_trackedFrame.id,reinterpret_cast<void*>(g_nativeViewWitness.view),
                reinterpret_cast<void*>(g_nativeViewWitness.renderer),reinterpret_cast<void*>(g_nativeViewWitness.camera));
    }
}
struct NativeViewReadStatus {
    unsigned stage=0;
    uintptr_t viewTable=0,cameraTable=0;
};
// Native frustum plane/type vectors already contain six entries. Reusing their
// existing capacity mirrors 4BE0C0 without shallow-copying an owning structure.
static bool RebuildOwnedCullingFrustum(void* camera,const d2vr_culling_cover::Input& input) {
    __try {
        auto frustum=static_cast<unsigned char*>(camera)+0x620;
        const auto planes=*reinterpret_cast<float**>(frustum+0x30);
        const auto planeEnd=*reinterpret_cast<float**>(frustum+0x34);
        const auto planeCapacity=*reinterpret_cast<float**>(frustum+0x38);
        const auto types=*reinterpret_cast<unsigned**>(frustum+0x150);
        const auto typeEnd=*reinterpret_cast<unsigned**>(frustum+0x154);
        const auto typeCapacity=*reinterpret_cast<unsigned**>(frustum+0x158);
        if(!planes || !types || reinterpret_cast<uintptr_t>(planes)%16 ||
           reinterpret_cast<uintptr_t>(planeEnd)-reinterpret_cast<uintptr_t>(planes)!=96 ||
           reinterpret_cast<uintptr_t>(typeEnd)-reinterpret_cast<uintptr_t>(types)!=24 ||
           reinterpret_cast<uintptr_t>(planeCapacity)<reinterpret_cast<uintptr_t>(planeEnd) ||
           reinterpret_cast<uintptr_t>(typeCapacity)<reinterpret_cast<uintptr_t>(typeEnd))return false;
        for(int i=0;i<24;++i)if(!std::isfinite(planes[i]))return false;
        for(int i=0;i<6;++i)if(types[i]!=(i==0?1u:i==5?2u:0u))return false;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    // Both are the original thiscall operations issued by 4BE0C0, in order.
    // Neither owns a camera reference or edits camera FOV/projection/VP.
    using Build=void(__thiscall*)(void*,const float*);
    auto frustum=static_cast<unsigned char*>(camera)+0x620;
    reinterpret_cast<Build>(0x607CD0)(frustum,input.viewProjection);
    reinterpret_cast<Build>(0x685050)(frustum,input.inverseViewProjection);
    return true;
}
static bool PrepareOwnedCullingInput(void* camera,const NativeCameraFields& fields,d2vr_culling_cover::Input& input) {
    float view[16],vp[16];
    __try {
        std::memcpy(view,static_cast<unsigned char*>(camera)+0x480,sizeof(view));
        std::memcpy(vp,static_cast<unsigned char*>(camera)+0x500,sizeof(vp));
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    float eyes[2][4];
    for(int e=0;e<2;++e) {
        const auto& f=g_eyeFov[e];
        eyes[e][0]=f.angleLeft;eyes[e][1]=f.angleRight;eyes[e][2]=f.angleUp;eyes[e][3]=f.angleDown;
    }
    return d2vr_culling_cover::Prepare(view,fields.projection,vp,fields.fov,fields.aspect,
        fields.nearZ,fields.farZ,eyes,g_autoFovCoverage,1+g_edgeCrop,input);
}
static bool ReadOwnedViewCamera(void* view,void*& renderer,void*& camera,NativeCameraFields& fields,
                               NativeViewReadStatus& status) {
    __try {
        const auto root=static_cast<unsigned char*>(view);
        if(!root)return false;
        status.stage=1;
        const auto table=status.viewTable=*reinterpret_cast<uintptr_t*>(root);
        if(table!=0xFC8930 && table!=0xFC6800 && !g_nativeRenderViewTable.Owns(view))return false;
        status.stage=2;
        auto reference=*reinterpret_cast<void***>(root+0xa8);
        if(!reference || !(renderer=*reference))return false;
        status.stage=3;
        auto selected=*reinterpret_cast<void***>(static_cast<unsigned char*>(renderer)+0x3f4);
        if(!selected || !(camera=*selected))return false;
        status.stage=4;status.cameraTable=*reinterpret_cast<uintptr_t*>(camera);
        const bool valid=ReadSelectedRenderCamera(camera,renderer,fields);
        if(valid)status.stage=5;
        return valid;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static void ObserveNativeCameraBoundary(void* view,bool beforeRender) {
    if(GetCurrentThreadId()!=g_renderThreadId)return;
    static unsigned entries=0;
    const bool report=++entries<=3 || entries%600==0;
    if(report)
        Log("[NativeViewBoundary] ESI captured=%p thread=%lu frame=%llu open=%d world=%d views=%d had3D=%d worldDraw=%d",
            view,GetCurrentThreadId(),g_trackedFrame.id,int(g_trackedFrame.open),int(g_in3DGameplayMode),int(g_haveViews),int(g_had3DCameraThisFrame),int(g_hadWorldDrawThisFrame));
    if(GetCurrentThreadId()!=g_renderThreadId || !g_trackedFrame.open || !g_haveViews ||
       !g_in3DGameplayMode || g_submitMode!=4 || g_stereoMode!=1)return;
    void* renderer=nullptr;void* camera=nullptr;NativeCameraFields fields{};
    NativeViewReadStatus status;
    if(!ReadOwnedViewCamera(view,renderer,camera,fields,status)) {
        static unsigned rejected=0;
        if(++rejected<=3 || rejected%600==0)
            Log("[NativeViewBoundary] rejected stage=%u viewTable=%08X renderer=%p camera=%p cameraTable=%08X; 1=view table 2=renderer ref 3=camera ref 4=camera contract; observation only",
                status.stage,unsigned(status.viewTable),renderer,camera,unsigned(status.cameraTable));
        return;
    }
    const auto v=reinterpret_cast<uintptr_t>(view),r=reinterpret_cast<uintptr_t>(renderer),c=reinterpret_cast<uintptr_t>(camera);
    const bool ready=g_nativeViewWitness.CanCover(g_trackedFrame.id,v,r,c);
    d2vr_culling_cover::Input cullingInput;
    const bool cullingReady=PrepareOwnedCullingInput(camera,fields,cullingInput);
    bool cullingApplied=false;
    // Only the instance +A8 render entry owns this mutation phase. QPC is
    // discovery/observation only, even when no draw has yet been counted.
    if(beforeRender && g_nativeViewBoundaryMode==2 && ready && cullingReady) {
        cullingApplied=RebuildOwnedCullingFrustum(camera,cullingInput);
        static unsigned appliedReports=0;
        if(cullingApplied && (++appliedReports<=3 || appliedReports%600==0))
            Log("[NativeCullingCover] beforeNativeRender=1 frame=%llu camera=%p FOV=%.5g cullingFOV=%.5g planes+bounds=native-rebuilt renderProjection=unchanged",
                g_trackedFrame.id,camera,fields.fov,cullingInput.fov);
    }
    float inverse[16];if(!d2vr_math::Invert(fields.projection,inverse))return;
    g_nativeViewWitness.Begin(g_trackedFrame.id,v,r,c,inverse);
    static unsigned reports=0;
    if(++reports<=3 || reports%600==0)
        Log("[NativeViewBoundary] beforeRender=%d frame=%llu ESI=%p renderer=%p camera=%p FOV=%.5g corroborated=%d cullingInput=%d cullingFOV=%.5g cullingApplied=%d uploadFrame=%llu inverseXY=%.6g,%.6g uploadXY=%.6g,%.6g; EXE/IAT unchanged",
            int(beforeRender),g_trackedFrame.id,view,renderer,camera,fields.fov,ready,cullingReady,cullingInput.fov,cullingApplied,
            g_nativeViewWitness.uploadedFrame,inverse[0],inverse[5],g_nativeViewWitness.uploadedInverse[0],g_nativeViewWitness.uploadedInverse[5]);
}
static void __fastcall BeforeNativeViewRender(void* view,void*) {
    const DWORD error=GetLastError();
    ObserveNativeCameraBoundary(view,true);
    SetLastError(error);
}
static void* g_nativeViewRenderNext=reinterpret_cast<void*>(0x99F1B0);
D2VR_SELECTED_CAMERA_SHIM(NativeViewRenderEntry,BeforeNativeViewRender,g_nativeViewRenderNext)
static void ObserveNativeViewBoundary(void* view) {
    if(GetCurrentThreadId()!=g_renderThreadId)return;
    if(g_nativeViewBoundaryMode==2 && PrepareNativeRenderViewTable() &&
       g_nativeRenderViewTable.Publish(view,g_nativeImageGuard)) {
        static unsigned reports=0;
        if(++reports<=3)Log("[NativeRenderEntry] instance=%p private read-only table; +A8 -> original99F1B0; EXE/static tables unchanged",view);
    }
    ObserveNativeCameraBoundary(view,false);
}

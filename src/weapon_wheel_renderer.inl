// Compositor quads for the weapon wheel and the controller ammo counters. Same
// acquire/wait/release contract as vr_settings_renderer.inl; no native
// geometry, shader or device-state edits. Each atlas is one swapchain split
// into square cells; the CPU image is rebuilt only when its content key
// changes, never per frame.
struct QuadAtlas {
    const char* name; int cell, cells;
    XrSession session=XR_NULL_HANDLE;
    XrSwapchain chain=XR_NULL_HANDLE;
    std::vector<XrSwapchainImageD3D11KHR> images;
    std::vector<uint64_t> imageRevision;
    std::vector<uint32_t> pixels, scratch;
    uint32_t index=0, maxLayers=0;
    bool acquired=false, waited=false, publishable=false, failed=false;
    uint64_t revision=0;
    QuadAtlas(const char* n,int c,int count):name(n),cell(c),cells(count) {}
    int Width() const { return cell*cells; }
    void Reset() {
        if(chain && XR_FAILED(xrDestroySwapchain(chain))) return;
        chain=XR_NULL_HANDLE; session=XR_NULL_HANDLE; images.clear(); imageRevision.clear();
        acquired=waited=publishable=failed=false; maxLayers=0;
    }
    bool Ensure() {
        if(session!=g_xrSession) Reset();
        if(chain) return true;
        if(failed) return false;
        failed=true; // one attempt per session
        XrSystemProperties system{XR_TYPE_SYSTEM_PROPERTIES};
        if(xrGetSystemProperties(g_xrInstance,g_xrSystem,&system)!=XR_SUCCESS) return false;
        maxLayers=system.graphicsProperties.maxLayerCount;
        uint32_t count=0;
        if(xrEnumerateSwapchainFormats(g_xrSession,0,&count,nullptr)!=XR_SUCCESS || !count || count>1024) return false;
        std::vector<int64_t> formats(count);
        if(xrEnumerateSwapchainFormats(g_xrSession,count,&count,formats.data())!=XR_SUCCESS) return false;
        int64_t format=0;
        for(auto candidate:{DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,DXGI_FORMAT_B8G8R8A8_UNORM})
            if(!format && std::find(formats.begin(),formats.end(),int64_t(candidate))!=formats.end()) format=candidate;
        if(!format) return false;
        XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        ci.usageFlags=XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        ci.format=format; ci.width=Width(); ci.height=cell;
        ci.sampleCount=ci.faceCount=ci.arraySize=ci.mipCount=1;
        if(xrCreateSwapchain(g_xrSession,&ci,&chain)!=XR_SUCCESS) { chain=XR_NULL_HANDLE; return false; }
        session=g_xrSession;
        if(xrEnumerateSwapchainImages(chain,0,&count,nullptr)!=XR_SUCCESS || !count || count>64) { Reset(); failed=true; return false; }
        images.assign(count,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        if(xrEnumerateSwapchainImages(chain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()))!=XR_SUCCESS) {
            Reset(); failed=true; return false;
        }
        imageRevision.assign(images.size(),~0ull);
        pixels.assign(size_t(Width())*cell,0);
        scratch.assign(d2vr_wheel_raster::ScratchPixels(cell),0);
        failed=false; ++revision;
        Log("[%s] quad swapchain %dx%d format=%lld images=%u maxLayers=%u",name,Width(),cell,(long long)format,count,maxLayers);
        return true;
    }
    d2vr_wheel_raster::Target Cell(int i) { return {pixels.data(),Width(),cell,Width(),i*cell,0,cell}; }
    // Uploads the CPU image if needed and releases the image. False: nothing to submit.
    bool Publish() {
        if(!acquired) {
            XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            const auto result=xrAcquireSwapchainImage(chain,&ai,&index);
            if(XR_FAILED(result)) return false;
            acquired=true; waited=false; publishable=result==XR_SUCCESS;
        }
        if(!waited) {
            XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO}; wi.timeout=0;
            const auto result=xrWaitSwapchainImage(chain,&wi);
            if(result==XR_TIMEOUT_EXPIRED) return false; // keep the acquired image for the next frame
            if(XR_FAILED(result)) return false;
            waited=true; publishable=publishable && result==XR_SUCCESS;
        }
        const bool valid=publishable && index<images.size() && images[index].texture;
        if(valid && imageRevision[index]!=revision) {
            g_ctx11->UpdateSubresource(images[index].texture,0,nullptr,pixels.data(),Width()*4,0);
            imageRevision[index]=revision;
        }
        XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        const auto result=xrReleaseSwapchainImage(chain,&ri);
        if(XR_SUCCEEDED(result)) acquired=waited=publishable=false;
        return valid && result==XR_SUCCESS;
    }
    void Quad(XrCompositionLayerQuad& q,int i,XrSpace space,const XrPosef& pose,float size) const {
        q={XR_TYPE_COMPOSITION_LAYER_QUAD};
        // Image is premultiplied: blend with its alpha, no unpremultiply flag.
        q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        q.space=space; q.pose=pose; q.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
        q.size={size,size};
        q.subImage.swapchain=chain;
        q.subImage.imageRect={{i*cell,0},{cell,cell}};
    }
};
static QuadAtlas g_wheelAtlas("WeaponWheel",512,2);
static QuadAtlas g_ammoAtlas("AmmoHud",128,2);
static QuadAtlas g_gazeAtlas("GazeDot",64,1);
static bool g_gazeDotDrawn=false;
static uint64_t g_wheelDrawn=~0ull;
static int g_ammoDrawn[2]{-3,-3};
static int g_ammoReserveDrawn[2]{-3,-3};
static void ReleaseWeaponWheelRenderer() {
    g_wheelAtlas.Reset(); g_ammoAtlas.Reset(); g_gazeAtlas.Reset(); g_wheelDrawn=~0ull; g_ammoDrawn[0]=g_ammoDrawn[1]=-3;
    g_ammoReserveDrawn[0]=g_ammoReserveDrawn[1]=-3;
    g_gazeDotDrawn=false; // g_viewSpace belongs to the session code in d3d9.cpp
}

static void AppendWheelQuads(const XrCompositionLayerBaseHeader** layers,uint32_t& layerCount,uint32_t limit,
                             XrCompositionLayerQuad quads[2]) {
    WheelDisplay d;
    AcquireSRWLockShared(&g_wheelDisplayLock); d=g_wheelDisplay; ReleaseSRWLockShared(&g_wheelDisplayLock);
    if((!d.visible[0] && !d.visible[1]) || d.space!=g_xrSpace || !g_wheelAtlas.Ensure()) return;
    if(g_wheelDrawn!=d.revision) {
        for(int h=0;h<2;++h) {
            auto t=g_wheelAtlas.Cell(h);
            if(d.visible[h]) d2vr_wheel_raster::DrawWheel(t,d.wheel[h],h==0,g_wheelAtlas.scratch.data());
            else d2vr_wheel_raster::Clear(t);
        }
        g_wheelDrawn=d.revision; ++g_wheelAtlas.revision;
    }
    const bool ok=g_wheelAtlas.Publish();
    static bool lastOk=true;
    if(ok!=lastOk) { lastOk=ok; Log(ok ? "[WeaponWheel] quads submitted" : "[WeaponWheel] quads unavailable this frame (swapchain/runtime)"); }
    if(!ok) return;
    for(int h=0;h<2 && layerCount<limit;++h) if(d.visible[h]) {
        g_wheelAtlas.Quad(quads[h],h,d.space,d.pose[h],kWheelSizeM);
        layers[layerCount++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quads[h]);
    }
}

// Magazine/reserve counter on the back of each armed hand, attached to the grip space
// so the compositor moves it with the controller (no extra latency).
static void AppendAmmoQuads(const XrCompositionLayerBaseHeader** layers,uint32_t& layerCount,uint32_t limit,
                            XrCompositionLayerQuad quads[2]) {
    if(!g_ammoHudEnabled || !g_inputContext || g_wheelGate.open) return;
    WeaponLoadoutSample s;
    if(!CopyWeaponLoadout(s)) return;
    int ammo[2]{-3,-3},reserve[2]{-3,-3}; bool show[2]{}; // XR order: left, right
    for(int native=0;native<2;++native) {
        const int slot=s.loadout.hand[native];
        if(slot!=d2vr_wheel::kSlotTwoHanded && !d2vr_wheel::IsOneHanded(slot)) continue;
        const int xr=native==0 ? 1 : 0;
        show[xr]=g_handGripSpace[xr]!=XR_NULL_HANDLE; ammo[xr]=s.handAmmo[native];
        reserve[xr]=s.handReserve[native];
        // Low: a quarter of the largest magazine seen for this weapon type.
        const int max=s.maxAmmo[slot];
        if(ammo[xr]>=0 && max>=4 && ammo[xr]*4<=max) ammo[xr]+=10000; // low flag carried in the key
    }
    if(!show[0] && !show[1]) return;
    if(!g_ammoAtlas.Ensure()) return;
    if(ammo[0]!=g_ammoDrawn[0] || ammo[1]!=g_ammoDrawn[1] ||
       reserve[0]!=g_ammoReserveDrawn[0] || reserve[1]!=g_ammoReserveDrawn[1]) {
        for(int h=0;h<2;++h) {
            auto t=g_ammoAtlas.Cell(h);
            const bool low=ammo[h]>=10000; const int value=low ? ammo[h]-10000 : ammo[h];
            if(show[h]) d2vr_wheel_raster::DrawShape(t,g_ammoAtlas.scratch.data(),[&](auto&& emit){ d2vr_ammo_hud::Build(value,reserve[h],low,emit); });
            else d2vr_wheel_raster::Clear(t);
        }
        g_ammoDrawn[0]=ammo[0]; g_ammoDrawn[1]=ammo[1]; ++g_ammoAtlas.revision;
        g_ammoReserveDrawn[0]=reserve[0];g_ammoReserveDrawn[1]=reserve[1];
    }
    if(!g_ammoAtlas.Publish()) return;
    // Grip space: -Z forward along the barrel, +Y up. Above the back of the
    // hand, tilted ~35 degrees towards the eyes.
    XrPosef pose{}; pose.orientation={-.3007f,0,0,.9537f}; pose.position={0,.055f,.045f};
    for(int h=0;h<2 && layerCount<limit;++h) if(show[h]) {
        g_ammoAtlas.Quad(quads[h],h,g_handGripSpace[h],pose,.08f);
        layers[layerCount++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quads[h]);
    }
}

// Eye-tracking check: a small ring 1.5 m along the gaze in VIEW space
// ([EyeTracking] GazeDot). Looking at the four corners must move the ring to
// the same corners; otherwise fix [EyeTracking] GazeAxes.
static void AppendGazeDot(const XrCompositionLayerBaseHeader** layers,uint32_t& layerCount,uint32_t limit,
                          XrCompositionLayerQuad& quad) {
    if(!g_gazeDotEnabled || layerCount>=limit) return;
    XrVector3f d{};
    if(!LatestGaze(d)) return;
    if(!g_viewSpace) return; // VIEW space created with the session (d3d9.cpp)
    if(!g_gazeAtlas.Ensure()) return;
    if(!g_gazeDotDrawn) {
        auto t=g_gazeAtlas.Cell(0);
        d2vr_wheel_raster::DrawShape(t,g_gazeAtlas.scratch.data(),[&](auto&& emit) {
            const d2vr_weapon_wheel_graphics::Placement ring{0,0,1};
            d2vr_weapon_wheel_graphics::detail::Arc(emit,ring,.62f,.92f,0,2*d2vr_weapon_wheel_graphics::Pi,48,{.92f,.895f,.845f,.95f});
            d2vr_weapon_wheel_graphics::detail::Arc(emit,ring,0,.22f,0,2*d2vr_weapon_wheel_graphics::Pi,24,{.74f,.055f,.075f,1});
        });
        g_gazeDotDrawn=true; ++g_gazeAtlas.revision;
    }
    if(!g_gazeAtlas.Publish()) return;
    XrPosef pose{};
    pose.position={d.x*1.5f,d.y*1.5f,d.z*1.5f};
    // Face the eye: quad +Z points back along the gaze.
    XrVector3f z{-d.x,-d.y,-d.z}, up{0,1,0};
    XrVector3f x{up.y*z.z-up.z*z.y,up.z*z.x-up.x*z.z,up.x*z.y-up.y*z.x};
    const float n=std::sqrt(x.x*x.x+x.y*x.y+x.z*x.z);
    if(n<1e-3f) return;
    x={x.x/n,x.y/n,x.z/n};
    const XrVector3f y{z.y*x.z-z.z*x.y,z.z*x.x-z.x*x.z,z.x*x.y-z.y*x.x};
    pose.orientation=QuatFromBasis(x,y,z);
    g_gazeAtlas.Quad(quad,0,g_viewSpace,pose,.035f);
    layers[layerCount++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
}

// Called at frame end with the layers already chosen. Never displaces the
// world projection layer; skipped when the runtime has no room.
static void AppendWeaponWheelLayers(const XrCompositionLayerBaseHeader** layers,uint32_t& layerCount,
                                    uint32_t capacity,XrCompositionLayerQuad quads[5],bool shouldRender) {
    if(!layerCount || !shouldRender || g_xrSessionState!=XR_SESSION_STATE_FOCUSED || !g_ctx11) return;
    const uint32_t runtimeMax=std::max(g_wheelAtlas.maxLayers,g_ammoAtlas.maxLayers);
    const uint32_t limit=runtimeMax ? std::min(capacity,runtimeMax) : capacity;
    AppendWheelQuads(layers,layerCount,limit,quads);
    AppendAmmoQuads(layers,layerCount,limit,quads+2);
    AppendGazeDot(layers,layerCount,limit,quads[4]);
}

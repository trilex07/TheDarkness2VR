// Two compositor quads (one per hand) for the dual weapon wheels. Same
// acquire/wait/release contract as vr_settings_renderer.inl; no native
// geometry, shader or device-state edits. One swapchain, left half = left
// wheel, right half = right wheel. The CPU image is rebuilt only when the
// display revision changes (open / hover / loadout), never per frame.
struct WeaponWheelRenderer {
    static constexpr int kHalf=512, kWidth=kHalf*2, kHeight=kHalf;
    XrSession session=XR_NULL_HANDLE;
    XrSwapchain chain=XR_NULL_HANDLE;
    std::vector<XrSwapchainImageD3D11KHR> images;
    std::vector<uint64_t> imageRevision;
    std::vector<uint32_t> pixels, scratch;
    uint32_t index=0, maxLayers=0;
    bool acquired=false, waited=false, publishable=false, failed=false;
    uint64_t drawnRevision=~0ull;
    void Reset() {
        if(chain && XR_FAILED(xrDestroySwapchain(chain))) return;
        chain=XR_NULL_HANDLE; session=XR_NULL_HANDLE; images.clear(); imageRevision.clear();
        acquired=waited=publishable=failed=false; drawnRevision=~0ull; maxLayers=0;
    }
    bool Ensure() {
        if(session!=g_xrSession) Reset();
        if(chain) return true;
        if(failed) return false;
        failed=true; // one attempt per session; logged by the caller
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
        ci.format=format; ci.width=kWidth; ci.height=kHeight;
        ci.sampleCount=ci.faceCount=ci.arraySize=ci.mipCount=1;
        if(xrCreateSwapchain(g_xrSession,&ci,&chain)!=XR_SUCCESS) { chain=XR_NULL_HANDLE; return false; }
        session=g_xrSession;
        if(xrEnumerateSwapchainImages(chain,0,&count,nullptr)!=XR_SUCCESS || !count || count>64) { Reset(); failed=true; return false; }
        images.assign(count,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        if(xrEnumerateSwapchainImages(chain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()))!=XR_SUCCESS) {
            Reset(); failed=true; return false;
        }
        imageRevision.assign(images.size(),~0ull);
        pixels.assign(size_t(kWidth)*kHeight,0);
        scratch.assign(d2vr_wheel_raster::ScratchPixels(kHalf),0);
        failed=false;
        Log("[WeaponWheel] quad swapchain %dx%d format=%lld images=%u maxLayers=%u",kWidth,kHeight,(long long)format,count,maxLayers);
        return true;
    }
    void Draw(const WheelDisplay& d) {
        if(drawnRevision==d.revision) return;
        for(int h=0;h<2;++h) {
            d2vr_wheel_raster::Target t{pixels.data(),kWidth,kHeight,kWidth,h*kHalf,0,kHalf};
            if(d.visible[h]) d2vr_wheel_raster::DrawWheel(t,d.wheel[h],h==0,scratch.data());
            else d2vr_wheel_raster::Clear(t);
        }
        drawnRevision=d.revision;
    }
    // Appends up to two quads. Returns false if nothing could be submitted.
    bool Render(const WheelDisplay& d,XrCompositionLayerQuad quads[2],uint32_t& count) {
        count=0;
        if(!g_ctx11 || !Ensure()) return false;
        Draw(d);
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
        if(valid && imageRevision[index]!=drawnRevision) {
            g_ctx11->UpdateSubresource(images[index].texture,0,nullptr,pixels.data(),kWidth*4,0);
            imageRevision[index]=drawnRevision;
        }
        XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        const auto result=xrReleaseSwapchainImage(chain,&ri);
        if(XR_SUCCEEDED(result)) acquired=waited=publishable=false;
        if(!valid || result!=XR_SUCCESS) return false;
        for(int h=0;h<2;++h) {
            if(!d.visible[h]) continue;
            auto& q=quads[count++];
            q={XR_TYPE_COMPOSITION_LAYER_QUAD};
            // Image is premultiplied: blend with its alpha, no unpremultiply flag.
            q.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            q.space=d.space; q.pose=d.pose[h]; q.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
            q.size={kWheelSizeM,kWheelSizeM};
            q.subImage.swapchain=chain;
            q.subImage.imageRect={{h*kHalf,0},{kHalf,kHalf}};
        }
        return count>0;
    }
};
static WeaponWheelRenderer g_weaponWheelRenderer;
static void ReleaseWeaponWheelRenderer() { g_weaponWheelRenderer.Reset(); }

// Called at frame end with the layers already chosen. Never displaces the
// world projection layer; skipped when the runtime has no room.
static void AppendWeaponWheelLayers(const XrCompositionLayerBaseHeader** layers,uint32_t& layerCount,
                                    uint32_t capacity,XrCompositionLayerQuad quads[2],bool shouldRender) {
    WheelDisplay d;
    AcquireSRWLockShared(&g_wheelDisplayLock); d=g_wheelDisplay; ReleaseSRWLockShared(&g_wheelDisplayLock);
    if(!d.visible[0] && !d.visible[1]) return;
    if(!layerCount || !shouldRender || g_xrSessionState!=XR_SESSION_STATE_FOCUSED || d.space!=g_xrSpace) return;
    uint32_t count=0;
    const bool ok=g_weaponWheelRenderer.Render(d,quads,count);
    static bool lastOk=true;
    if(ok!=lastOk) { lastOk=ok; Log(ok ? "[WeaponWheel] quads submitted" : "[WeaponWheel] quads unavailable this frame (swapchain/runtime)"); }
    const uint32_t limit=g_weaponWheelRenderer.maxLayers ? std::min(capacity,g_weaponWheelRenderer.maxLayers) : capacity;
    for(uint32_t i=0;i<count && layerCount<limit;++i)
        layers[layerCount++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quads[i]);
}

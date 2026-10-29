// A separate compositor quad: no edits to native menu geometry, eye caches,
// shaders or device state. API ownership follows hello_xr's acquire/wait/release
// contract. The hit-test surface is exactly the pose/size/rect submitted below.
struct VrSettingsRenderer {
    XrSession session=XR_NULL_HANDLE;
    XrSwapchain chain=XR_NULL_HANDLE;
    std::vector<XrSwapchainImageD3D11KHR> images;
    uint32_t index=0;
    bool acquired=false,waited=false,publishable=false;
    bool pending=false;
    uint64_t revision=0;
    std::vector<uint64_t> imageRevision;
    int drawnMode=-1,drawnX=-1,drawnY=-1;
    int drawnToggleKey=-1;
    bool drawnPointer=false;
    HDC dc=nullptr;
    HBITMAP bitmap=nullptr;
    HFONT font=nullptr;
    HGDIOBJ oldBitmap=nullptr,oldFont=nullptr;
    void* pixels=nullptr;
    void Reset() {
        if(chain && XR_FAILED(xrDestroySwapchain(chain)))return;
        chain=XR_NULL_HANDLE;session=XR_NULL_HANDLE;images.clear();
        acquired=waited=publishable=pending=false;
        revision=0;imageRevision.clear();drawnMode=-1;
        if(dc && oldFont && oldFont!=HGDI_ERROR)SelectObject(dc,oldFont);
        if(dc && oldBitmap && oldBitmap!=HGDI_ERROR)SelectObject(dc,oldBitmap);
        if(font)DeleteObject(font);
        if(bitmap)DeleteObject(bitmap);
        if(dc)DeleteDC(dc);
        dc=nullptr;bitmap=nullptr;font=nullptr;oldBitmap=oldFont=nullptr;pixels=nullptr;
    }
    bool Ensure() {
        if(session!=g_xrSession)Reset();
        if(chain && session==g_xrSession && dc && pixels)return true;
        if(chain)return false;
        XrSystemProperties system{XR_TYPE_SYSTEM_PROPERTIES};
        if(xrGetSystemProperties(g_xrInstance,g_xrSystem,&system)!=XR_SUCCESS ||
           system.graphicsProperties.maxLayerCount<2)return false;
        uint32_t count=0;
        if(xrEnumerateSwapchainFormats(g_xrSession,0,&count,nullptr)!=XR_SUCCESS || !count || count>1024)return false;
        std::vector<int64_t> formats(count);
        if(xrEnumerateSwapchainFormats(g_xrSession,count,&count,formats.data())!=XR_SUCCESS)return false;
        int64_t format=0;
        for(auto candidate:{DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,DXGI_FORMAT_B8G8R8A8_UNORM})
            if(!format && std::find(formats.begin(),formats.end(),int64_t(candidate))!=formats.end())format=candidate;
        if(!format)return false;
        XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        ci.usageFlags=XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        ci.format=format;ci.width=d2vr_settings::Width;ci.height=d2vr_settings::Height;
        ci.sampleCount=ci.faceCount=ci.arraySize=ci.mipCount=1;
        if(xrCreateSwapchain(g_xrSession,&ci,&chain)!=XR_SUCCESS){chain=XR_NULL_HANDLE;return false;}
        session=g_xrSession;
        if(xrEnumerateSwapchainImages(chain,0,&count,nullptr)!=XR_SUCCESS || !count || count>64){Reset();return false;}
        images.assign(count,{XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        if(xrEnumerateSwapchainImages(chain,count,&count,
           reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()))!=XR_SUCCESS){Reset();return false;}
        imageRevision.assign(images.size(),0);
        dc=CreateCompatibleDC(nullptr);
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=d2vr_settings::Width;info.bmiHeader.biHeight=-d2vr_settings::Height;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        if(dc)bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
        font=CreateFontA(-23,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,"Segoe UI");
        if(!dc || !bitmap || !pixels || !font){Reset();return false;}
        oldBitmap=SelectObject(dc,bitmap);oldFont=SelectObject(dc,font);
        if(!oldBitmap || oldBitmap==HGDI_ERROR || !oldFont || oldFont==HGDI_ERROR){Reset();return false;}
        SetBkMode(dc,TRANSPARENT);return true;
    }
    bool Fill(d2vr_settings::Rect r,COLORREF color) {
        RECT box{r.x,r.y,r.x+r.w,r.y+r.h};auto brush=CreateSolidBrush(color);
        if(!brush)return false;
        const bool ok=FillRect(dc,&box,brush)!=0;DeleteObject(brush);return ok;
    }
    bool Text(int x,int y,const char* text) {
        SetTextColor(dc,RGB(240,245,250));return TextOutA(dc,x,y,text,int(strlen(text)))!=0;
    }
    bool Draw() {
        using namespace d2vr_settings;
        const auto mode=g_adaptiveFPSMode;
        const auto& pointer=g_vrSettings.Pointer();
        const int px=pointer.visible?std::clamp(pointer.hit.x,0,Width-1):-1;
        const int py=pointer.visible?std::clamp(pointer.hit.y,0,Height-1):-1;
        if(drawnMode==int(mode) && drawnPointer==pointer.visible && drawnX==px && drawnY==py &&
           drawnToggleKey==g_vrSettingsToggleKey)return true;
        if(!Fill({0,0,Width,Height},RGB(16,24,36)) || !Text(32,24,"DARKNESS II VR - SETTINGS") ||
           !Text(32,80,"Adaptive Reprojection"))return false;
        const char* names[]={"OFF","AUTO","HALF"};
        for(unsigned i=0;i<3;++i) {
            const auto r=ModeRect(i);const auto& p=g_vrSettings.Pointer();
            const bool hover=p.visible && r.Contains(p.hit.x,p.hit.y);
            if(!Fill(r,hover?RGB(40,110,145):unsigned(mode)==i?RGB(28,82,100):RGB(42,50,66)) ||
               !Text(r.x+32,r.y+14,names[i]))return false;
        }
        // This build has no documented runtime half-rate adapter. Selecting a
        // request must never imply that motion smoothing has actually started.
        if(!Text(32,220,mode==d2vr_adaptive::Mode::Off ? "OFF - runtime pacing unchanged" :
            "UNSUPPORTED - runtime pacing unchanged") ||
           !Text(32,286,g_vrSettingsToggleKey==VK_INSERT ? "INSERT: toggle   B: close" :
                g_vrSettingsToggleKey ? "Custom toggle key   B: close" : "B: close") ||
           !Fill(CloseRect,RGB(60,65,78)) || !Text(502,286,"CLOSE"))return false;
        const auto& p=g_vrSettings.Pointer();
        if(p.visible) {
            const int x=std::clamp(p.hit.x,0,Width-1),y=std::clamp(p.hit.y,0,Height-1);
            if(!Fill({std::max(0,x-3),std::max(0,y-3),std::min(7,Width-std::max(0,x-3)),
                std::min(7,Height-std::max(0,y-3))},RGB(80,240,255)))return false;
        }
        if(!GdiFlush())return false;
        drawnMode=int(mode);drawnPointer=pointer.visible;drawnX=px;drawnY=py;
        drawnToggleKey=g_vrSettingsToggleKey;
        ++revision;return true;
    }
    bool Render(XrCompositionLayerQuad& quad) {
        pending=false;
        if(!g_ctx11 || !Ensure() || !Draw())return false;
        if(!acquired) {
            XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            const auto result=xrAcquireSwapchainImage(chain,&ai,&index);
            if(XR_FAILED(result))return false;
            acquired=true;waited=false;publishable=result==XR_SUCCESS;
        }
        if(!waited) {
            XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wi.timeout=0;
            const auto result=xrWaitSwapchainImage(chain,&wi);
            if(result==XR_TIMEOUT_EXPIRED){pending=true;return false;}
            if(XR_FAILED(result))return false;
            waited=true;publishable=publishable && result==XR_SUCCESS;
        }
        const bool valid=publishable && index<images.size() && images[index].texture;
        if(valid && imageRevision[index]!=revision) {
            g_ctx11->UpdateSubresource(images[index].texture,0,nullptr,pixels,d2vr_settings::Width*4,0);
            imageRevision[index]=revision;
        }
        XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        const auto result=xrReleaseSwapchainImage(chain,&ri);
        if(XR_SUCCEEDED(result))acquired=waited=publishable=false;
        if(!valid || result!=XR_SUCCESS)return false;
        const auto& s=g_vrSettings.Surface();
        quad={XR_TYPE_COMPOSITION_LAYER_QUAD};quad.space=s.space;quad.pose=s.pose;
        quad.eyeVisibility=XR_EYE_VISIBILITY_BOTH;quad.size={s.width,s.height};
        quad.subImage.swapchain=chain;
        quad.subImage.imageRect={{0,0},{d2vr_settings::Width,d2vr_settings::Height}};
        return true;
    }
};
static VrSettingsRenderer g_vrSettingsRenderer;
static void ReleaseVrSettingsRenderer(){g_vrSettingsRenderer.Reset();}
enum class VrSettingsRender {Ready,Pending,Failed};
static VrSettingsRender RenderVrSettings(XrCompositionLayerQuad& quad) {
    const bool ok=g_vrSettingsRenderer.Render(quad);
    if(g_vrSettingsRenderer.pending)return VrSettingsRender::Pending;
    if(!ok)Log("[VRSettings] panel unavailable; surface/input capture cancelled; stereo layer retained");
    return ok?VrSettingsRender::Ready:VrSettingsRender::Failed;
}

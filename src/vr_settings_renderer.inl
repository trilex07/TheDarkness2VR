// A separate compositor quad: no edits to native menu geometry, eye caches,
// shaders or device state. API ownership follows hello_xr's acquire/wait/release
// contract. The hit-test surface is exactly the pose/size/rect submitted below.
struct VrSettingsRenderer {
    XrSession session=XR_NULL_HANDLE;
    XrSwapchain chain=XR_NULL_HANDLE;
    std::vector<XrSwapchainImageD3D11KHR> images;
    uint32_t index=0,maxLayers=0;
    bool acquired=false,waited=false,publishable=false;
    bool pending=false;
    uint64_t revision=0;
    std::vector<uint64_t> imageRevision;
    int drawnMode=-1,drawnX=-1,drawnY=-1;
    int drawnToggleKey=-1;
    uint64_t drawnPreferences=0;bool drawnEntry=false;
    bool drawnPointer=false;
    HDC dc=nullptr;
    HBITMAP bitmap=nullptr;
    HFONT font=nullptr;
    HGDIOBJ oldBitmap=nullptr,oldFont=nullptr;
    void* pixels=nullptr;
    void Reset() {
        if(chain && XR_FAILED(xrDestroySwapchain(chain)))return;
        chain=XR_NULL_HANDLE;session=XR_NULL_HANDLE;images.clear();maxLayers=0;
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
        maxLayers=system.graphicsProperties.maxLayerCount;
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
        font=CreateFontW(-23,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
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
    bool Text(int x,int y,const wchar_t* text) {
        SetTextColor(dc,RGB(240,245,250));return TextOutW(dc,x,y,text,int(wcslen(text)))!=0;
    }
    bool Draw() {
        using namespace d2vr_settings;
        const auto& options=g_vrSettings.Options();
        const auto& pointer=g_vrSettings.Pointer();
        const int px=pointer.visible?std::clamp(pointer.hit.x,0,Width-1):-1;
        const int py=pointer.visible?std::clamp(pointer.hit.y,0,Height-1):-1;
        const bool entry=g_vrSettings.Entry();
        if(revision && drawnPreferences==options.revision && drawnEntry==entry &&
           drawnPointer==pointer.visible && drawnX==px && drawnY==py)return true;
        if(!Fill({0,0,Width,Height},RGB(16,24,36)))return false;
        auto button=[&](Rect r,const wchar_t* name,bool selected=false) {
            const bool hover=pointer.visible && r.Contains(px,py);
            return Fill(r,hover?RGB(40,110,145):selected?RGB(28,82,100):RGB(42,50,66)) &&
                Text(r.x+12,r.y+8,name);
        };
        if(entry) {
            if(!Text(210,24,L"VR settings"))return false;
        } else {
            if(!Text(24,22,L"DARKNESS II VR settings"))return false;
            const wchar_t* tabs[]={L"Obraz",L"J\u0119zyk",L"Sterowanie"};
            for(unsigned i=0;i<3;++i)if(!button(TabRect(i),tabs[i],options.page==i))return false;
            wchar_t label[160]{};
            if(options.page==0) {
                if(!button(SettingRect(0),options.nis?L"NIS: W\u0141.":L"NIS: WY\u0141."))return false;
                swprintf_s(label,L"-   Ostro\u015b\u0107 NIS: %u%%                         +",options.nisPercent);
                if(!button(SettingRect(1),label))return false;
                swprintf_s(label,L"Supersampling: %u%%   (kliknij, aby zmieni\u0107)",options.supersample);
                if(!button(SettingRect(2),label))return false;
            } else if(options.page==1) {
                if(!button(SettingRect(0),options.polishVoice?L"Dubbing: polski":L"Dubbing: angielski") ||
                   !button(SettingRect(1),options.polishText?L"Tekst: polski":L"Tekst: angielski") ||
                   !Text(28,222,L"J\u0119zyk i supersampling: po restarcie gry."))return false;
            } else {
                if(!Text(28,120,L"Spust: wybierz    K\u00f3\u0142ko: powr\u00f3t") ||
                   !Text(28,151,L"2x tr\u00f3jk\u0105t: pauza    K\u00f3\u0142ko (1 s): cel misji") ||
                   !Text(28,182,L"L3+R3: centrowanie; przytrzymaj: kino / VR") ||
                   !Text(28,213,L"Ruch: lewa ga\u0142ka    Obr\u00f3t: prawa ga\u0142ka"))return false;
            }
            if(!Text(24,280,options.failed?L"B\u0142\u0105d zapisu ustawie\u0144":
                options.pending?L"Zapisano. Uruchom gr\u0119 ponownie.":L"INSERT: VR settings") ||
               !button(CloseRect,L"Zamknij"))return false;
        }
        if(pointer.visible && !Fill({std::max(0,px-3),std::max(0,py-3),
           std::min(7,Width-std::max(0,px-3)),std::min(7,Height-std::max(0,py-3))},RGB(80,240,255)))return false;
        if(!GdiFlush())return false;
        drawnPreferences=options.revision;drawnEntry=entry;
        drawnPointer=pointer.visible;drawnX=px;drawnY=py;++revision;return true;
    }
    bool Laser(const XrCompositionLayerQuad& panel,uint64_t frame,const XrPosef& head,
               XrCompositionLayerQuad& beam) const {
        const auto& s=g_vrSettings.Surface();const auto& p=g_vrSettings.Pointer();
        if(maxLayers<3 || !g_vrSettings.Visible() || !g_vrSettings.Presented() ||
           !p.visible || s.frame!=frame || panel.subImage.swapchain!=chain ||
           panel.space!=s.space || acquired)return false;
        XrPosef pose{};XrExtent2Df size{};
        if(!d2vr_settings::Laser(s,g_vrSettings.Aim(),head,pose,size))return false;
        beam=panel;beam.pose=pose;beam.size=size;
        // The cursor centre is solid cyan in the already released image. Reuse
        // that texel; OpenXR permits multiple references to one released image.
        beam.subImage.imageRect={{std::clamp(p.hit.x,0,d2vr_settings::Width-1),
            std::clamp(p.hit.y,0,d2vr_settings::Height-1)},{1,1}};
        return true;
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
        quad.subImage.imageRect={{0,0},{d2vr_settings::Width,g_vrSettings.Entry()?80:d2vr_settings::Height}};
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

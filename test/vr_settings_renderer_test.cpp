// Actual renderer, real CPU-only GDI, fake XR receipts and D3D11 upload sink.
// No runtime, GPU adapter, Steam session or headset is opened by this fixture.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <windows.h>
#include <d3d11.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "../src/vr_settings.h"
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL %d %s\n",__LINE__,#x);std::exit(1);}}while(0)
static XrInstance g_xrInstance=(XrInstance)1;
static XrSystemId g_xrSystem=1;
static XrSession g_xrSession=(XrSession)2;
static d2vr_settings::Panel g_vrSettings;
static auto g_adaptiveFPSMode=d2vr_adaptive::Mode::Off;
static int g_vrSettingsToggleKey=VK_INSERT;
static void Log(const char*,...){}
static XrResult acquireResult=XR_SUCCESS,waitResult=XR_SUCCESS,releaseResult=XR_SUCCESS;
static XrResult destroyResult=XR_SUCCESS;
static int failStep=0;
static uint32_t layers=2,nextIndex=0;
static unsigned acquiredCalls,waitCalls,releasedCalls,destroyCalls,createdCalls;
static bool owned=false,ready=false;
static XrSwapchainCreateInfo createdInfo{};
XRAPI_ATTR XrResult XRAPI_CALL xrGetSystemProperties(XrInstance,XrSystemId,XrSystemProperties* p){
    if(failStep==1)return XR_ERROR_RUNTIME_FAILURE;
    p->graphicsProperties.maxLayerCount=layers;return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainFormats(XrSession,uint32_t capacity,uint32_t* count,int64_t* formats){
    if(failStep==2)return XR_ERROR_RUNTIME_FAILURE;
    *count=1;if(capacity)formats[0]=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateSwapchain(XrSession,const XrSwapchainCreateInfo* info,XrSwapchain* out){
    if(failStep==3)return XR_ERROR_RUNTIME_FAILURE;
    ++createdCalls;createdInfo=*info;*out=(XrSwapchain)3;return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainImages(XrSwapchain,uint32_t capacity,uint32_t* count,XrSwapchainImageBaseHeader* data){
    if(failStep==4)return XR_ERROR_RUNTIME_FAILURE;
    *count=2;
    if(capacity){auto images=reinterpret_cast<XrSwapchainImageD3D11KHR*>(data);
        for(unsigned i=0;i<2;++i)images[i].texture=(ID3D11Texture2D*)(uintptr_t)(i+16);}
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrDestroySwapchain(XrSwapchain){
    ++destroyCalls;if(destroyResult==XR_SUCCESS)owned=ready=false;return destroyResult;
}
XRAPI_ATTR XrResult XRAPI_CALL xrAcquireSwapchainImage(XrSwapchain,const XrSwapchainImageAcquireInfo*,uint32_t* index){
    ++acquiredCalls;CHECK(!owned);
    if(XR_SUCCEEDED(acquireResult)){owned=true;ready=false;*index=nextIndex;}
    return acquireResult;
}
XRAPI_ATTR XrResult XRAPI_CALL xrWaitSwapchainImage(XrSwapchain,const XrSwapchainImageWaitInfo* info){
    ++waitCalls;CHECK(owned);CHECK(!ready);CHECK(info->timeout==0);
    if(XR_SUCCEEDED(waitResult)&&waitResult!=XR_TIMEOUT_EXPIRED)ready=true;
    return waitResult;
}
XRAPI_ATTR XrResult XRAPI_CALL xrReleaseSwapchainImage(XrSwapchain,const XrSwapchainImageReleaseInfo*){
    ++releasedCalls;CHECK(owned&&ready);
    if(XR_SUCCEEDED(releaseResult))owned=ready=false;return releaseResult;
}
struct UploadSink {
    unsigned calls=0;uint64_t checksum=0;
    void UpdateSubresource(ID3D11Texture2D*,UINT,const D3D11_BOX*,const void* pixels,UINT pitch,UINT){
        CHECK(owned&&ready);CHECK(pixels);CHECK(pitch==640*4);++calls;
        checksum=0;const auto* bytes=static_cast<const unsigned char*>(pixels);
        for(unsigned i=0;i<640*360*4;++i)checksum=checksum*131+bytes[i];
    }
} sink;
static UploadSink* g_ctx11=&sink;
static BOOL CheckedDelete(HGDIOBJ object){
    const auto result=DeleteObject(object);
    CHECK(result);
    return result;
}
static BOOL CheckedDeleteDC(HDC dc){const auto result=DeleteDC(dc);CHECK(result);return result;}
#define DeleteObject CheckedDelete
#define DeleteDC CheckedDeleteDC
#include "../src/vr_settings_renderer.inl"
#undef DeleteObject
#undef DeleteDC
static void Reset(){
    destroyResult=XR_SUCCESS;g_vrSettingsRenderer.Reset();g_vrSettings.Close();
    acquireResult=waitResult=releaseResult=XR_SUCCESS;failStep=0;layers=2;nextIndex=0;
    acquiredCalls=waitCalls=releasedCalls=destroyCalls=createdCalls=0;sink={};
    g_adaptiveFPSMode=d2vr_adaptive::Mode::Off;
    CHECK(g_vrSettings.Toggle({{0,0,0,1},{0,1.6f,0}},(XrSpace)4));
}
int main(){
    // Warm up real font/text drawing before checking repeated allocation growth.
    // Check every deletion receipt as well: GetObjectType on deleted handles
    // can still return a type here, so that is not our deletion oracle.
    XrCompositionLayerQuad warmup{};Reset();
    CHECK(RenderVrSettings(warmup)==VrSettingsRender::Ready);
    ReleaseVrSettingsRenderer();
    CHECK(GdiFlush());
    const DWORD resources=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    XrCompositionLayerQuad quad{};Reset();
    CHECK(RenderVrSettings(quad)==VrSettingsRender::Ready);
    CHECK(sink.calls==1&&sink.checksum!=0);CHECK(!owned);
    // Save the actual DIB for visual review; it never contains game pixels.
    {
        FILE* preview=nullptr;CHECK(fopen_s(&preview,"vr-settings-preview.bmp","wb")==0);
        BITMAPFILEHEADER file{};file.bfType=0x4d42;
        file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);
        file.bfSize=file.bfOffBits+640*360*4;
        BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=640;info.biHeight=-360;
        info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
        CHECK(fwrite(&file,sizeof(file),1,preview)==1);
        CHECK(fwrite(&info,sizeof(info),1,preview)==1);
        CHECK(fwrite(g_vrSettingsRenderer.pixels,640*360*4,1,preview)==1);CHECK(fclose(preview)==0);
    }
    CHECK(createdInfo.width==640&&createdInfo.height==360);
    CHECK(quad.space==g_vrSettings.Surface().space&&quad.pose.position.z==-1.5f);
    CHECK(quad.eyeVisibility==XR_EYE_VISIBILITY_BOTH);
    CHECK(quad.subImage.imageRect.extent.width==640&&quad.subImage.imageRect.extent.height==360);
    CHECK(quad.size.width==1.28f&&quad.size.height==.72f);
    const auto checksum=sink.checksum;
    CHECK(RenderVrSettings(quad)==VrSettingsRender::Ready);CHECK(sink.calls==1);
    nextIndex=1;CHECK(RenderVrSettings(quad)==VrSettingsRender::Ready);CHECK(sink.calls==2);
    g_adaptiveFPSMode=d2vr_adaptive::Mode::Half;
    CHECK(RenderVrSettings(quad)==VrSettingsRender::Ready);
    CHECK(sink.calls==3&&checksum!=sink.checksum);
    // An image timeout stays acquired, never publishes, and is waited again.
    Reset();waitResult=XR_TIMEOUT_EXPIRED;
    CHECK(RenderVrSettings(quad)==VrSettingsRender::Pending);
    CHECK(acquiredCalls==1&&waitCalls==1&&releasedCalls==0&&sink.calls==0);
    CHECK(RenderVrSettings(quad)==VrSettingsRender::Pending);CHECK(acquiredCalls==1);
    waitResult=XR_SUCCESS;CHECK(RenderVrSettings(quad)==VrSettingsRender::Ready);
    CHECK(acquiredCalls==1&&waitCalls==3&&releasedCalls==1&&sink.calls==1);
    // Release failure retains the exact acquired/waited image for retry.
    Reset();releaseResult=XR_ERROR_RUNTIME_FAILURE;
    CHECK(RenderVrSettings(quad)==VrSettingsRender::Failed);
    releaseResult=XR_SUCCESS;CHECK(RenderVrSettings(quad)==VrSettingsRender::Ready);
    CHECK(acquiredCalls==1&&waitCalls==1&&releasedCalls==2&&sink.calls==1);
    Reset();nextIndex=5;CHECK(RenderVrSettings(quad)==VrSettingsRender::Failed);
    CHECK(sink.calls==0&&releasedCalls==1&&!owned);
    Reset();acquireResult=XR_ERROR_RUNTIME_FAILURE;
    CHECK(RenderVrSettings(quad)==VrSettingsRender::Failed);CHECK(waitCalls==0&&releasedCalls==0);
    Reset();acquireResult=XR_SESSION_LOSS_PENDING;
    CHECK(RenderVrSettings(quad)==VrSettingsRender::Failed);CHECK(sink.calls==0&&releasedCalls==1);
    Reset();waitResult=XR_ERROR_RUNTIME_FAILURE;
    CHECK(RenderVrSettings(quad)==VrSettingsRender::Failed);CHECK(releasedCalls==0);
    waitResult=XR_SUCCESS;CHECK(RenderVrSettings(quad)==VrSettingsRender::Ready);CHECK(acquiredCalls==1);
    Reset();layers=1;CHECK(RenderVrSettings(quad)==VrSettingsRender::Failed);CHECK(createdCalls==0);
    for(int stage=1;stage<=4;++stage){Reset();failStep=stage;
        CHECK(RenderVrSettings(quad)==VrSettingsRender::Failed);CHECK(!g_vrSettingsRenderer.chain);}
    Reset();CHECK(RenderVrSettings(quad)==VrSettingsRender::Ready);
    destroyResult=XR_ERROR_RUNTIME_FAILURE;g_vrSettingsRenderer.Reset();CHECK(g_vrSettingsRenderer.chain!=XR_NULL_HANDLE);
    destroyResult=XR_SUCCESS;g_vrSettingsRenderer.Reset();CHECK(!g_vrSettingsRenderer.chain);
    // Repeated opens/stops must release GDI handles, without a GPU/runtime.
    for(unsigned i=0;i<30;++i){Reset();CHECK(RenderVrSettings(quad)==VrSettingsRender::Ready);}
    ReleaseVrSettingsRenderer();
    CHECK(GdiFlush());
    const auto after=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    std::printf("GDI baseline=%lu after=%lu\n",resources,after);
    CHECK(after==resources);
    std::printf("PASS actual VR settings renderer %u checks\n",checks);
}

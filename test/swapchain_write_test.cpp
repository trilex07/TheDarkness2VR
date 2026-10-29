#include <openxr/openxr.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
struct ID3D11Texture2D {unsigned pixels=0;};
static std::vector<char> events;
struct Context {
    void CopyResource(ID3D11Texture2D* dst,ID3D11Texture2D* src){events.push_back('C');dst->pixels=src->pixels;}
} context,*g_ctx11=&context;
static XrSwapchain g_swapchain[2]={XrSwapchain(1),XrSwapchain(2)};
static ID3D11Texture2D images[2][2],source{17};
static ID3D11Texture2D* ptrs[2][2]={{&images[0][0],&images[0][1]},{&images[1][0],&images[1][1]}};
static ID3D11Texture2D** g_scImages[2]={ptrs[0],ptrs[1]};
static uint32_t g_scCount[2]={2,2},indexToAcquire=1;
static XrResult acquireResult=XR_SUCCESS,waitResult=XR_SUCCESS,releaseResult=XR_SUCCESS;
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrAcquireSwapchainImage(XrSwapchain,const XrSwapchainImageAcquireInfo* info,uint32_t* index){
    CHECK(info->type==XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO && !info->next);events.push_back('A');*index=indexToAcquire;return acquireResult;
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrWaitSwapchainImage(XrSwapchain,const XrSwapchainImageWaitInfo* info){
    CHECK(info->type==XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO && !info->next && info->timeout==XR_INFINITE_DURATION);events.push_back('W');return waitResult;
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrReleaseSwapchainImage(XrSwapchain,const XrSwapchainImageReleaseInfo* info){
    CHECK(info->type==XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO && !info->next);events.push_back('R');return releaseResult;
}
#include "actual_swapchain_write.inl"
static bool Write(unsigned eye=0){return WriteTextureToSwapchain(g_swapchain[eye],g_scImages[eye],&source);}
static void Order(const char* expected){CHECK(events==std::vector<char>(expected,expected+std::char_traits<char>::length(expected)));events.clear();}
static void Reset(){g_scWrite[0]={};g_scWrite[1]={};events.clear();acquireResult=waitResult=releaseResult=XR_SUCCESS;indexToAcquire=1;g_ctx11=&context;}
int main(){
    Reset();
    for(unsigned frame=0;frame<1000;++frame)for(unsigned eye=0;eye<2;++eye){
        source.pixels=frame*2+eye;CHECK(Write(eye));Order("AWCR");
        CHECK(images[eye][1].pixels==source.pixels && !g_scWrite[eye].acquired);
    }
    for(XrResult status:{XR_TIMEOUT_EXPIRED,XR_ERROR_RUNTIME_FAILURE,XR_ERROR_SESSION_LOST,XR_ERROR_INSTANCE_LOST}){
        Reset();waitResult=status;CHECK(!Write());Order("AW");CHECK(g_scWrite[0].acquired && !g_scWrite[0].waited);
        waitResult=XR_SUCCESS;CHECK(Write(1));Order("AWCR");waitResult=status; // independent acquisition
        CHECK(!Write());Order("W");waitResult=XR_SUCCESS;CHECK(Write());Order("WCR");
    }
    Reset();acquireResult=XR_ERROR_RUNTIME_FAILURE;CHECK(!Write());Order("A");CHECK(!g_scWrite[0].acquired);
    Reset();releaseResult=XR_ERROR_RUNTIME_FAILURE;CHECK(!Write());Order("AWCR");
    CHECK(g_scWrite[0].acquired && g_scWrite[0].waited);
    source.pixels=989;CHECK(!Write());Order("CR");releaseResult=XR_SUCCESS;CHECK(Write());Order("CR");
    CHECK(images[0][1].pixels==989 && !g_scWrite[0].acquired);
    for(unsigned stage=0;stage<3;++stage){
        Reset();if(stage==0)acquireResult=XR_SESSION_LOSS_PENDING;
        if(stage==1)waitResult=XR_SESSION_LOSS_PENDING;if(stage==2)releaseResult=XR_SESSION_LOSS_PENDING;
        CHECK(!Write());Order(stage==2?"AWCR":"AWR");CHECK(!g_scWrite[0].acquired);
    }
    Reset();indexToAcquire=2;CHECK(!Write());Order("AWR");CHECK(!g_scWrite[0].acquired);
    Reset();auto saved=ptrs[0][1];ptrs[0][1]=nullptr;CHECK(!Write());Order("AWR");ptrs[0][1]=saved;
    Reset();CHECK(!WriteTextureToSwapchain(XR_NULL_HANDLE,g_scImages[0],&source));
    CHECK(!WriteTextureToSwapchain(g_swapchain[0],g_scImages[1],&source));
    CHECK(!WriteTextureToSwapchain(g_swapchain[0],nullptr,&source));
    CHECK(!WriteTextureToSwapchain(g_swapchain[0],g_scImages[0],nullptr));
    g_ctx11=nullptr;CHECK(!Write());Order("");g_ctx11=&context;
    Reset();waitResult=XR_TIMEOUT_EXPIRED;CHECK(!Write());Order("AW");
    g_scWrite[0]={}; // successful swapchain destruction resets the lease
    waitResult=XR_SUCCESS;CHECK(Write());Order("AWCR");
    printf("PASS actual-source swapchain writes: %u checks; 2000 eye writes, timeout/error ownership, release retries, loss cleanup, bounds, reset; no runtime/HMD proof\n",checks);
    return 0;
}

#include "../src/vr_settings.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
static unsigned checks,renderCalls,endCalls,adaptiveCalls;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL %d %s\n",__LINE__,#x);std::exit(1);}}while(0)
static d2vr_settings::Panel g_vrSettings;
static XrSession g_xrSession=(XrSession)1;
static XrSessionState g_xrSessionState=XR_SESSION_STATE_FOCUSED;
enum class VrSettingsRender {Ready,Pending,Failed};
static VrSettingsRender rendered=VrSettingsRender::Ready;
static XrResult ended=XR_SUCCESS;
static uint32_t actualLayers,adaptiveLayers;
static VrSettingsRender RenderVrSettings(XrCompositionLayerQuad& quad){
    ++renderCalls;quad={XR_TYPE_COMPOSITION_LAYER_QUAD};quad.space=g_vrSettings.Surface().space;return rendered;
}
XRAPI_ATTR XrResult XRAPI_CALL xrEndFrame(XrSession session,const XrFrameEndInfo* info){
    ++endCalls;CHECK(session==g_xrSession);CHECK(info->displayTime==123);
    actualLayers=info->layerCount;CHECK(actualLayers<=2);
    CHECK((info->layers!=nullptr)==(actualLayers!=0));
    if(actualLayers==2)CHECK(info->layers[1]->type==XR_TYPE_COMPOSITION_LAYER_QUAD);
    return ended;
}
static void AdaptiveStereoSubmitted(XrResult result,bool pixels,uint32_t count){
    ++adaptiveCalls;CHECK(result==ended);CHECK(pixels);adaptiveLayers=count;
}
static void Actual(uint32_t layerCount,bool shouldRender){
    const XrCompositionLayerBaseHeader* layers[2]{};
    XrCompositionLayerProjection world{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    if(layerCount)layers[0]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&world);
    XrCompositionLayerQuad settingsQuad{XR_TYPE_COMPOSITION_LAYER_QUAD};
    XrFrameState fs{XR_TYPE_FRAME_STATE};fs.predictedDisplayTime=123;fs.shouldRender=shouldRender;
    const bool havePixels=true;
#include "actual_vr_settings_submission.inl"
}
int main(){
    for(bool open:{false,true})for(bool focus:{false,true})for(bool visible:{false,true})
    for(uint32_t world:{0u,1u})for(auto render:{VrSettingsRender::Ready,VrSettingsRender::Pending,VrSettingsRender::Failed})
    for(auto end:{XR_SUCCESS,XR_SESSION_LOSS_PENDING,XR_ERROR_RUNTIME_FAILURE}){
        g_vrSettings.Close();if(open)CHECK(g_vrSettings.Toggle({{0,0,0,1},{0,1.6f,0}},(XrSpace)2));
        g_xrSessionState=focus?XR_SESSION_STATE_FOCUSED:XR_SESSION_STATE_VISIBLE;
        rendered=render;ended=end;renderCalls=endCalls=adaptiveCalls=0;
        Actual(world,visible);
        const bool renderAttempt=open&&focus&&visible&&world;
        const bool submitted=renderAttempt&&render==VrSettingsRender::Ready;
        CHECK(renderCalls==unsigned(renderAttempt));CHECK(endCalls==1&&adaptiveCalls==1);
        CHECK(actualLayers==world+unsigned(submitted));CHECK(adaptiveLayers==world);
        CHECK(g_vrSettings.Presented()==(submitted&&end==XR_SUCCESS));
        const bool remains=open&&focus&&(!renderAttempt||render==VrSettingsRender::Pending||
            (render==VrSettingsRender::Ready&&end==XR_SUCCESS));
        CHECK(g_vrSettings.Open()==remains);
    }
    std::printf("PASS actual VR settings submission %u checks\n",checks);
}

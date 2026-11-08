#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define D2VR_MATCHED_CAPTURE
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "../src/vr_settings.h"
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static d2vr_settings::Panel g_vrSettings;
static char g_activeIni[MAX_PATH]{};
static int g_renderW=2000,g_renderH=2000,g_nisMode=2;
static bool g_foveatedSharpen=false;
static float g_foveaSharpness=.2f,g_nisSharpness=.2f;
namespace d2vr_source_quality {enum class Mode{Supersample};}
static auto g_sourceQualityMode=d2vr_source_quality::Mode::Supersample;
static void Log(const char*,...){}
#include "../src/vr_settings_config.inl"
int main(){
    using namespace d2vr_settings;
    char temp[MAX_PATH]{};CHECK(GetTempPathA(MAX_PATH,temp));
    CHECK(GetTempFileNameA(temp,"d2g",0,g_activeIni));
    CHECK(!g_vrSettings.Options().polishText&&!g_vrSettings.Options().polishVoice);
    LoadVrPreferences(g_activeIni);
    CHECK(!g_vrSettings.Options().polishText&&!g_vrSettings.Options().polishVoice);
    CHECK(!g_vrSettings.Options().headRelativeMovement&&!g_vrSettings.Options().weaponLaser);
    for(bool text:{false,true})for(bool voice:{false,true}) {
        CHECK(WritePrivateProfileStringA("VRSettings","PolishText",text?"1":"0",g_activeIni));
        CHECK(WritePrivateProfileStringA("VRSettings","PolishVoice",voice?"1":"0",g_activeIni));
        LoadVrPreferences(g_activeIni);
        CHECK(g_vrSettings.Options().polishText==text&&g_vrSettings.Options().polishVoice==voice);
    }
    for(auto action:{Action::HeadRelativeMovement,Action::WeaponLaser}) {
        ApplyVrSettingsAction(action);CHECK(!g_vrSettings.Options().failed);
    }
    CHECK(GetPrivateProfileIntA("VRSettings","HeadRelativeMovement",0,g_activeIni)==1);
    CHECK(GetPrivateProfileIntA("VRSettings","WeaponLaser",0,g_activeIni)==1);
    g_vrSettings.Options()={};LoadVrPreferences(g_activeIni);
    CHECK(g_vrSettings.Options().headRelativeMovement&&g_vrSettings.Options().weaponLaser);
    ApplyVrSettingsAction(Action::HeadRelativeMovement);
    CHECK(!g_vrSettings.Options().headRelativeMovement&&g_vrSettings.Options().weaponLaser);
    ApplyVrSettingsAction(Action::WeaponLaser);
    g_vrSettings.Options()={};LoadVrPreferences(g_activeIni);
    CHECK(!g_vrSettings.Options().headRelativeMovement&&!g_vrSettings.Options().weaponLaser);
    CHECK(DeleteFileA(g_activeIni));g_activeIni[0]=0;
    for(auto action:{Action::HeadRelativeMovement,Action::WeaponLaser}) {
        ApplyVrSettingsAction(action);CHECK(g_vrSettings.Options().failed);
        CHECK(!g_vrSettings.Options().headRelativeMovement&&!g_vrSettings.Options().weaponLaser);
    }
    printf("PASS actual gameplay preferences persistence %u checks\n",checks);
}

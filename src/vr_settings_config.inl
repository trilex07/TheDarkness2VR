// Settings persist to the same absolute INI selected by LoadConfig. Resource
// dimensions are startup-only; no resize is attempted from controller polling.
static int g_vrBaseWidth=0,g_vrBaseHeight=0,g_vrNisMode=2;
static HANDLE g_vrLanguageHelper=nullptr;
static bool VrWrite(const char* key,unsigned value) {
    char text[32];sprintf_s(text,"%u",value);
    return g_activeIni[0] && WritePrivateProfileStringA("VRSettings",key,text,g_activeIni)!=FALSE;
}
static void LoadVrPreferences(const char* ini) {
    auto& o=g_vrSettings.Options();
    g_vrBaseWidth=g_renderW;g_vrBaseHeight=g_renderH;
    g_vrNisMode=g_nisMode==3?3:2;
    o.nis=GetPrivateProfileIntA("VRSettings","NIS",g_nisMode!=0,ini)!=0;
    o.nisPercent=std::clamp(GetPrivateProfileIntA("VRSettings","NISPercent",
        unsigned((g_foveatedSharpen?g_foveaSharpness:g_nisSharpness)*100+.5f),ini),0u,100u);
    o.supersample=GetPrivateProfileIntA("VRSettings","Supersampling",100,ini);
    if(o.supersample!=100 && o.supersample!=125 && o.supersample!=150)o.supersample=100;
    o.polishVoice=GetPrivateProfileIntA("VRSettings","PolishVoice",1,ini)!=0;
    o.polishText=GetPrivateProfileIntA("VRSettings","PolishText",1,ini)!=0;
#ifdef D2VR_MATCHED_CAPTURE
    const unsigned width=unsigned(g_vrBaseWidth)*o.supersample/100;
    const unsigned height=unsigned(g_vrBaseHeight)*o.supersample/100;
    if(o.supersample>100 && d2vr_settings::SupersampleFits(g_vrBaseWidth,g_vrBaseHeight,o.supersample)) {
        g_renderW=int(width);g_renderH=int(height);g_sourceQualityMode=d2vr_source_quality::Mode::Supersample;
    } else if(o.supersample>100){o.supersample=100;o.failed=true;}
#else
    o.supersample=100;
#endif
    g_nisMode=o.nis?g_vrNisMode:0;
    g_nisSharpness=g_foveaSharpness=o.nisPercent*.01f;
    o.pending=(GetPrivateProfileIntA("VRSettings","AppliedPolishVoice",1,ini)!=0)!=o.polishVoice ||
        (GetPrivateProfileIntA("VRSettings","AppliedPolishText",1,ini)!=0)!=o.polishText;
    o.failed=o.failed || GetPrivateProfileIntA("VRSettings","LanguageError",0,ini)!=0;
    ++o.revision;
    Log("[VRSettings] loaded NIS=%d strength=%u SS=%u source=%dx%d voicePL=%d textPL=%d",
        int(o.nis),o.nisPercent,o.supersample,g_renderW,g_renderH,int(o.polishVoice),int(o.polishText));
}
static bool StartVrLanguageHelper() {
    if(g_vrLanguageHelper) {
        if(WaitForSingleObject(g_vrLanguageHelper,0)==WAIT_TIMEOUT)return true;
        CloseHandle(g_vrLanguageHelper);g_vrLanguageHelper=nullptr;
    }
    wchar_t game[MAX_PATH]{},system[MAX_PATH]{},ini[MAX_PATH]{};
    if(!GetModuleFileNameW(nullptr,game,MAX_PATH) || !GetSystemDirectoryW(system,MAX_PATH) ||
       !MultiByteToWideChar(CP_ACP,0,g_activeIni,-1,ini,MAX_PATH))return false;
    auto slash=wcsrchr(game,L'\\');if(!slash)return false;*slash=0;
    wchar_t script[MAX_PATH]{};swprintf_s(script,L"%s\\VRSettings\\Apply-Languages.ps1",game);
    if(GetFileAttributesW(script)==INVALID_FILE_ATTRIBUTES)return false;
    FILETIME created{},exit{},kernel{},user{};
    if(!GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user))return false;
    const uint64_t ticks=(uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;
    wchar_t executable[MAX_PATH]{},command[2048]{};
    swprintf_s(executable,L"%s\\WindowsPowerShell\\v1.0\\powershell.exe",system);
    swprintf_s(command,L"\"%s\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"%s\" -OwnerPid %lu -OwnerFileTime %llu -IniPath \"%s\"",
        executable,script,GetCurrentProcessId(),ticks,ini);
    STARTUPINFOW startup{sizeof(startup)};startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable,command,nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,game,&startup,&process))return false;
    CloseHandle(process.hThread);g_vrLanguageHelper=process.hProcess;return true;
}
static void ApplyVrSettingsAction(d2vr_settings::Action action) {
    using namespace d2vr_settings;
    if(action==Action::None)return;
    auto& o=g_vrSettings.Options();const auto previous=o;bool ok=true;
    switch(action) {
    case Action::NisToggle:o.nis=!o.nis;ok=VrWrite("NIS",o.nis);break;
    case Action::NisLess:o.nisPercent=o.nisPercent>=10?o.nisPercent-10:0;ok=VrWrite("NISPercent",o.nisPercent);break;
    case Action::NisMore:o.nisPercent=std::min(100u,o.nisPercent+10);ok=VrWrite("NISPercent",o.nisPercent);break;
    case Action::Supersample: {
#ifdef D2VR_MATCHED_CAPTURE
        const unsigned next=NextSupersample(g_vrBaseWidth,g_vrBaseHeight,o.supersample);
        o.supersample=next;ok=VrWrite("Supersampling",o.supersample);o.pending=true;
#else
        ok=false;
#endif
        break;}
    case Action::Voice:case Action::Text: {
        if(!StartVrLanguageHelper()){ok=false;break;}
        if(action==Action::Voice){o.polishVoice=!o.polishVoice;ok=VrWrite("PolishVoice",o.polishVoice);}
        else {o.polishText=!o.polishText;ok=VrWrite("PolishText",o.polishText);}
        o.pending=true;break;}
    default:break;
    }
    if(!ok)o=previous;
    o.failed=!ok;++o.revision;
    if(ok){g_nisMode=o.nis?g_vrNisMode:0;g_nisSharpness=g_foveaSharpness=o.nisPercent*.01f;}
    Log("[VRSettings] action=%u saved=%d NIS=%d strength=%u SS=%u voicePL=%d textPL=%d",unsigned(action),int(ok),int(o.nis),o.nisPercent,o.supersample,int(o.polishVoice),int(o.polishText));
}

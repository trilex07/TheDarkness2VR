// Read-only native LOD configuration witness. No scene traversal or hooks.
// Included after eye_gaze.inl; called only while XR is running.
static void ObserveGazeLodConfiguration() {
    static ULONGLONG next=0;
    const auto now=GetTickCount64();
    if(now<next)return;
    next=now+5000;
    if(!GetPrivateProfileIntA("Render","GazeLodDiagnostic",0,
        g_activeIni[0]?g_activeIni:".\\Darkness2VR.ini"))return;
    bool matched=false; unsigned enabled=0,lowest=0; int mode=0,bias=0;float scale=0;
    __try {
        static const unsigned char scaleRead[]={0xF3,0x0F,0x59,0x05,0x74,0xE0,0x10,0x01};
        static const unsigned char enabledRead[]={0x80,0x3D,0x54,0xDE,0x10,0x01,0x00};
        matched=(uintptr_t)GetModuleHandleW(nullptr)==0x400000 &&
            !memcmp((void*)0xAE319E,scaleRead,sizeof(scaleRead)) &&
            !memcmp((void*)0x8513A3,enabledRead,sizeof(enabledRead));
        if(matched) {
            enabled=*(const unsigned char*)0x110DE54;lowest=*(const unsigned char*)0x110DF40;
            scale=*(const float*)0x110E074;mode=*(const int*)0x110DFF0;bias=*(const int*)0x110E048;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){matched=false;}
    if(!matched){Log("[GazeLOD] rejected: native signature mismatch; no memory writes");return;}
    XrVector3f gaze{};const bool valid=LatestGaze(gaze);
    Log("[GazeLOD] config-only enable=%u lowest=%u scale=%.4f mode=%d bias=%d gaze=%d dir=(%.3f,%.3f,%.3f) gameplay=%d; object observation NOT connected; quality unchanged",
        enabled,lowest,scale,mode,bias,int(valid),gaze.x,gaze.y,gaze.z,int(g_in3DGameplayMode));
}

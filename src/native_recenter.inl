// Event-only observer: never calls an engine method or writes native memory.
// Capture BEFORE ResetNativeAim, on the L3+R3/F8 event, not after headset removal.
struct NativeRecenterSample {
    DWORD camera=0,avatar=0,parent=0;
    bool cameraValid=false,modelValid=false,bonesValid=false;
    float cameraWorld[16]{},avatarWorld[16]{},modelWorld[16]{};
    float bones[5][16]{}; // camera, left root/tip, right root/tip (model space)
};
static NativeRecenterSample CaptureNativeRecenter(const NativeGameplaySample& s,DWORD cameraVtable=0xFFC7C0) {
    NativeRecenterSample out;
    if(!s.camera || GetTickCount64()-s.time>=150) return out;
    __try {
        if(!NativeAimOwnerMatches((void*)s.camera,cameraVtable)) return out;
        auto camera=(BYTE*)s.camera;
        auto avatar=**(BYTE***)(camera+0x30);
        if(**(BYTE***)(avatar+0x630)!=(BYTE*)s.inventory) return out;
        memcpy(out.cameraWorld,camera+0x80,64);
        memcpy(out.avatarWorld,avatar+0x140,64);
        if(!d2vr_hands::Valid(out.cameraWorld) || !d2vr_hands::Valid(out.avatarWorld)) return {};
        out.camera=s.camera; out.avatar=(DWORD)(uintptr_t)avatar; out.cameraValid=true;
        // Pinned avatar VT+46C -> 6088F0 returns smart ref at +1F8C.
        // Runtime verified even with empty pistol slots; no weapon required.
        auto parent=**(BYTE***)(avatar+0x1F8C);
        if(!parent || *(DWORD*)parent!=0xFFC300) return out;
        memcpy(out.modelWorld,parent+0x140,64);
        if(!d2vr_hands::Valid(out.modelWorld)) return out;
        out.parent=(DWORD)(uintptr_t)parent; out.modelValid=true;
        auto actor=**(BYTE***)(parent+0xF8);
        if(!actor || *(DWORD*)actor!=0xFC5F30) return out;
        auto renderer=**(BYTE***)(actor+8);
        auto resource=**(BYTE***)(renderer+0x5BC);
        if(!resource || *(DWORD*)resource!=0xFC2578) return out;
        auto skeleton=**(BYTE***)(resource+0x5E0);
        if(!skeleton || *(DWORD*)skeleton!=0xFC2338) return out;
        auto records=*(BYTE**)(skeleton+0x90);
        if(*(BYTE**)(skeleton+0x94)-records!=130*12) return out;
        const int indices[]={1,47,78,88,119};
        const char* names[]={"GAME_C1_CAMERA","GAME_L1_DEMONARM1","GAME_L1_DEMONARM32",
            "GAME_R1_DEMONARM1","GAME_R1_DEMONARM32"};
        const DWORD index=*(DWORD*)(renderer+0x424);
        if(index>=3) return out;
        auto vector=renderer+0x3F4+16*index;
        auto palette=*(float**)vector;
        if(*(float**)(vector+4)-palette!=130*16) return out;
        for(int i=0;i<5;++i) {
            if(strcmp(*(char**)(records+12*indices[i]),names[i])) return out;
            memcpy(out.bones[i],palette+16*indices[i],64);
            if(!d2vr_hands::Valid(out.bones[i])) return out;
        }
        out.bonesValid=true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { /* retain only completed stages */ }
    return out;
}
static void RecenterLog(FILE* file,const char* format,...) {
    char line[1400]; va_list args; va_start(args,format);
    vsnprintf(line,sizeof(line),format,args); va_end(args);
    Log("%s",line);
    if(file) fprintf(file,"%s\n",line);
}
static void LogRecenterMatrix(FILE* file,LONG id,const char* label,const float* m) {
    RecenterLog(file,"[RecenterCapture %ld] %s R=(%.6f,%.6f,%.6f) U=(%.6f,%.6f,%.6f) F=(%.6f,%.6f,%.6f) P=(%.6f,%.6f,%.6f)",
        id,label,m[0],m[1],m[2],m[4],m[5],m[6],m[8],m[9],m[10],m[12],m[13],m[14]);
}
static void RecordRecenterDiagnostic(const char* source) {
    static SRWLOCK recordLock=SRWLOCK_INIT;
    if(!TryAcquireSRWLockExclusive(&recordLock)) return;
    static volatile LONG sequence=0;
    const LONG id=InterlockedIncrement(&sequence);
    // Copy first. Later log writes/headset movement cannot change this event.
    const auto left=g_eyePose[0],right=g_eyePose[1];
    const auto head=HeadCenterPose(left,right);
    const auto reference=g_headGeometryReference;
    NativeGameplaySample gameplay;
    const bool fresh=CopyNativeGameplay(gameplay);
    const auto native=fresh ? CaptureNativeRecenter(gameplay):NativeRecenterSample{};
    const auto frame=g_trackedFrame.id;
    const auto time=g_trackedFrame.state.predictedDisplayTime;
    FILE* file=nullptr;
    char path[MAX_PATH]; strcpy_s(path,g_logPath);
    auto slash=strrchr(path,'\\');
    if(slash && strcpy_s(slash+1,sizeof(path)-(slash+1-path),"recenter.log")==0) fopen_s(&file,path,"a");
    SYSTEMTIME utc; GetSystemTime(&utc);
    RecenterLog(file,"[RecenterCapture %ld] UTC=%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",id,
        utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute,utc.wSecond,utc.wMilliseconds);
    RecenterLog(file,"[RecenterCapture %ld] BEGIN pid=%lu source=%s frame=%llu xrTime=%lld views=%d flags=0x%llx nativeAgeMs=%llu camera=%d model=%d bones=%d ViewOffsetX=%.6f archive=%d",
        id,GetCurrentProcessId(),source,(unsigned long long)frame,(long long)time,(int)g_haveViews,
        (unsigned long long)g_trackedFrame.flags,fresh ? (unsigned long long)(GetTickCount64()-gameplay.time):~0ULL,
        (int)native.cameraValid,(int)native.modelValid,(int)native.bonesValid,g_viewOffsetX,(int)(file!=nullptr));
    RecenterLog(file,"[RecenterCapture %ld] geometry=%d previousReferenceValid=%d stereoMode=%d shiftMode=%d ipd=%.6f widen=(%.6f,%.6f) halfFov=(%.6f,%.6f)",
        id,(int)g_headGeometry,(int)g_headGeometryReferenceValid,g_stereoMode,g_stereoShiftMode,g_stereoIPD,
        g_fovWidenX,g_fovWidenY,g_gameHalfFovX,g_gameHalfFovY);
    for(int i=0;i<2;++i) RecenterLog(file,"[RecenterCapture %ld] xrFov%d=(%.6f,%.6f,%.6f,%.6f)",
        id,i,g_eyeFov[i].angleLeft,g_eyeFov[i].angleRight,g_eyeFov[i].angleUp,g_eyeFov[i].angleDown);
    const XrPosef poses[]={head,left,right,reference};
    const char* labels[]={"head-click","eye-left","eye-right","previous-reference"};
    for(int i=0;i<4;++i) {
        const auto& p=poses[i];
        RecenterLog(file,"[RecenterCapture %ld] %s P=(%.6f,%.6f,%.6f) Q=(%.6f,%.6f,%.6f,%.6f)",
            id,labels[i],p.position.x,p.position.y,p.position.z,p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w);
    }
    if(native.cameraValid) {
        LogRecenterMatrix(file,id,"native-camera",native.cameraWorld);
        LogRecenterMatrix(file,id,"native-avatar",native.avatarWorld);
    }
    if(native.modelValid) LogRecenterMatrix(file,id,"native-firstperson",native.modelWorld);
    if(native.bonesValid) {
        const char* bones[]={"bone-camera","bone-left-root","bone-left-tip","bone-right-root","bone-right-tip"};
        for(int i=0;i<5;++i) LogRecenterMatrix(file,id,bones[i],native.bones[i]);
    }
    RecenterLog(file,"[RecenterCapture %ld] END (click-frame XR pose; native matrices read at event, not a GPU-synchronized capture)",id);
    if(file) fclose(file); // persistent across game restarts, append only
    ReleaseSRWLockExclusive(&recordLock);
}

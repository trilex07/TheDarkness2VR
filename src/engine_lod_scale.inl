// Reversible native LOD-scale trial. This changes only the engine's writable
// Graphics.LODScale .data float, never EXE code, static vtables or object data.
// Whole-scene test for CPU benefit, not a gaze/per-object implementation.
static float g_engineLodScaleRequested=0.f; // 0 OFF
static int g_engineLodScaleVerified=-1;
static float g_engineLodScaleOriginal=-1.f;

static void ParseEngineLodScale(const char* text) {
    g_engineLodScaleRequested=0.f;
    if(!text || !*text)return;
    char* end=nullptr;
    const double value=strtod(text,&end);
    while(end && (*end==' ' || *end=='\t'))++end;
    if(end==text || !end || *end || !std::isfinite(value) ||
       (value!=0.0 && (value<1.0 || value>1.5))) {
        Log("[EngineLOD] invalid EngineLODScale; override disabled");return;
    }
    if(value>0)g_engineLodScaleRequested=float(value);
}

static bool VerifyEngineLodScale() {
    if(g_engineLodScaleVerified>=0)return g_engineLodScaleVerified==1;
    bool ok=false;
    __try {
        static const BYTE selector[]={0xf3,0x0f,0x59,0x05,0x74,0xe0,0x10,0x01};
        static const BYTE dispatch[]={0x80,0x3d,0x54,0xde,0x10,0x01,0x00};
        const auto base=reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
        const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
        ok=reinterpret_cast<uintptr_t>(base)==0x400000 &&
           dos->e_magic==IMAGE_DOS_SIGNATURE && pe->Signature==IMAGE_NT_SIGNATURE &&
           pe->FileHeader.Machine==IMAGE_FILE_MACHINE_I386 &&
           pe->FileHeader.NumberOfSections<=96 &&
           !memcmp((void*)0xAE319E,selector,sizeof(selector)) &&
           !memcmp((void*)0x8513A3,dispatch,sizeof(dispatch));
        bool writable=false;
        if(ok) {
            auto* section=IMAGE_FIRST_SECTION(pe);
            for(WORD i=0;i<pe->FileHeader.NumberOfSections;++i,++section) {
                const uintptr_t first=0x400000+section->VirtualAddress;
                const uintptr_t last=first+section->Misc.VirtualSize;
                if(0x110E074>=first && 0x110E078<=last &&
                   (section->Characteristics&IMAGE_SCN_MEM_WRITE) &&
                   !(section->Characteristics&IMAGE_SCN_MEM_EXECUTE))writable=true;
            }
        }
        ok=ok && writable;
    } __except(EXCEPTION_EXECUTE_HANDLER) {ok=false;}
    g_engineLodScaleVerified=ok?1:0;
    Log("[EngineLOD] native selector and writable LODScale %s; requested=%.3f; whole-scene test, not gaze LOD",
        ok?"verified":"REFUSED",double(g_engineLodScaleRequested));
    return ok;
}

static void ApplyEngineLodScale() {
    if(g_engineLodScaleRequested<=0 || !VerifyEngineLodScale())return;
    static bool active=false;
    static unsigned reapplies=0;
    static ULONGLONG nextLog=0;
    __try {
        volatile float* value=reinterpret_cast<volatile float*>(0x110E074);
        const float current=*value;
        if(!std::isfinite(current) || current<.25f || current>4.f) {
            g_engineLodScaleVerified=0;
            Log("[EngineLOD] invalid native value; disabled");return;
        }
        if(g_engineLodScaleOriginal<0)g_engineLodScaleOriginal=current;
        const bool wanted=g_in3DGameplayMode && g_haveViews;
        const float target=wanted?g_engineLodScaleRequested:g_engineLodScaleOriginal;
        if(current!=target) {
            *value=target;
            if(wanted && active)++reapplies;
        }
        if(wanted!=active || (reapplies && GetTickCount64()>=nextLog)) {
            Log("[EngineLOD] active=%d scale=%.3f nativeBefore=%.3f enabled=%u mode=%d lowest=%u reapplies=%u; whole-scene trial",
                int(wanted),double(target),double(current),unsigned(*(const BYTE*)0x110DE54),*(const int*)0x110DFF0,
                unsigned(*(const BYTE*)0x110DF40),reapplies);
            active=wanted;nextLog=GetTickCount64()+10000;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        g_engineLodScaleVerified=0;
        Log("[EngineLOD] native access failed; further writes disabled");
    }
}

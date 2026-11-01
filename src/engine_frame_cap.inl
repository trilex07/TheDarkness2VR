// Engine frame limiter: Graphics.MaxFrameRate (float 0x010EB094, default 60).
// Evidence (DarknessII.exe, base 0x400000):
//  - 0x938A66 asks vfunc+0xEC(mode,time) (0x797F80: FrameCapMode 0 -> mode 2).
//  - 0x938AB2: mode 2 conditionally becomes mode 1; one branch uses 1/MaxFrameRate.
//  - 0x938B61: deadline loop SleepEx((time-elapsed-1ms)*1000) until done.
// Static evidence does NOT establish the active runtime branch or explain measured FPS.
// Only the game's own writable .data value is changed; no code or vtable bytes.
// [Render] EngineMaxFrameRate: 0 = untouched (default), AUTO = headset refresh,
// OBSERVE = read-only values, or a number 30..1000. Re-applied every frame because EE.cfg/menus may reload it.
static float g_engineMaxFrameRate=0.f;   // >0 fixed value
static bool g_engineMaxFrameRateObserve=false;
static bool g_engineMaxFrameRateAuto=false;
static int g_engineFrameCapVerified=-1;

static void ParseEngineMaxFrameRate(const char* text) {
    g_engineMaxFrameRate=0.f; g_engineMaxFrameRateAuto=false; g_engineMaxFrameRateObserve=false;
    if(!text || !*text) return;
    if(!_stricmp(text,"AUTO")) { g_engineMaxFrameRateAuto=true; return; }
    if(!_stricmp(text,"OBSERVE")) { g_engineMaxFrameRateObserve=true; return; }
    char* end=nullptr;
    const double v=strtod(text,&end);
    while(end && (*end==' ' || *end=='\t')) ++end;
    if(end!=text && end && !*end && v>=30.0 && v<=1000.0) g_engineMaxFrameRate=(float)v;
    else if(!(end!=text && end && !*end && v==0.0))
        Log("[FrameCap] invalid EngineMaxFrameRate; override disabled");
}

static bool VerifyEngineFrameCap() {
    if(g_engineFrameCapVerified>=0) return g_engineFrameCapVerified==1;
    bool ok=false;
    __try {
        // 0x938AB2: mov [esp+1C],1; cmp [esp+12],bl; je; movss xmm0,[F72094]; divss xmm0,[10EB094]; movss [esp+14],xmm0
        static const BYTE capBranch[]={0xC7,0x44,0x24,0x1C,0x01,0,0,0,0x38,0x5C,0x24,0x12,0x74,0x18,
            0xF3,0x0F,0x10,0x05,0x94,0x20,0xF7,0x00,0xF3,0x0F,0x5E,0x05,0x94,0xB0,0x0E,0x01,0xF3,0x0F,0x11,0x44,0x24,0x14};
        static const BYTE sleepLoop[]={0x7E,0x74,0x50,0xE8,0xC9,0xF5,0x05,0x00};             // 0x938B5F: jle; push eax; call 998130
        static const BYTE capQuery[]={0xA1,0x68,0xB0,0x0E,0x01,0x89,0x01,0x85,0xC0,0x75,0x17}; // 0x798054: FrameCapMode -> *mode
        static const BYTE sleepWrap[]={0x8B,0x44,0x24,0x04,0x6A,0x01,0x50,0xFF,0x15,0x58,0xB3,0xEE,0x00,0xC3}; // 0x998130 SleepEx(ms,1)
        static const char name[]="MaxFrameRate";
        ok=(uintptr_t)GetModuleHandleW(nullptr)==0x400000 &&
           !memcmp((void*)0x938AB2,capBranch,sizeof(capBranch)) && !memcmp((void*)0x938B5F,sleepLoop,sizeof(sleepLoop)) &&
           !memcmp((void*)0x798054,capQuery,sizeof(capQuery)) && !memcmp((void*)0x998130,sleepWrap,sizeof(sleepWrap)) &&
           !memcmp((void*)0xF8BF18,name,sizeof(name)) && *(const DWORD*)0xFC8034==0x42700000; // registered default 60.0f
        if(ok) {
            // The value must sit inside the image's writable .data section.
            auto* dos=(IMAGE_DOS_HEADER*)0x400000; auto* nt=(IMAGE_NT_HEADERS*)(0x400000+dos->e_lfanew);
            if(dos->e_magic!=IMAGE_DOS_SIGNATURE || nt->Signature!=IMAGE_NT_SIGNATURE ||
               nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 || nt->FileHeader.NumberOfSections>96)
                return false;
            auto* sec=IMAGE_FIRST_SECTION(nt); bool inData=false;
            for(WORD i=0;i<nt->FileHeader.NumberOfSections;++i,++sec) {
                const uintptr_t va=0x400000+sec->VirtualAddress, end=va+sec->Misc.VirtualSize;
                if(0x10EB094>=va && 0x10EB098<=end && (sec->Characteristics&IMAGE_SCN_MEM_WRITE) &&
                   !(sec->Characteristics&IMAGE_SCN_MEM_EXECUTE)) inData=true;
            }
            ok=inData;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { ok=false; }
    g_engineFrameCapVerified=ok ? 1:0;
    Log("[FrameCap] engine limiter layout %s (Graphics.MaxFrameRate@010EB094, FrameCapMode@010EB068, deadline loop 00938B61)",
        ok ? "verified" : "REFUSED: unexpected EXE bytes; MaxFrameRate left untouched");
    return ok;
}

// Called once per frame from the OpenXR event pump.
static void ApplyEngineFrameCap() {
    if(!(g_engineMaxFrameRate>0) && !g_engineMaxFrameRateAuto && !g_engineMaxFrameRateObserve) return;
    if(!VerifyEngineFrameCap()) return;
    if(g_engineMaxFrameRateObserve) {
        static bool observed=false;
        if(!observed) {
            __try {
                Log("[FrameCap] OBSERVE MaxFrameRate=%.3f FrameCapMode=%d; values only, active wait path unproven",
                    double(*(volatile float*)0x10EB094),*(volatile int*)0x10EB068);
            } __except(EXCEPTION_EXECUTE_HANDLER) { Log("[FrameCap] observation failed"); }
            observed=true;
        }
        return;
    }
    float target=g_engineMaxFrameRate;
    if(g_engineMaxFrameRateAuto) {
        const float hz=g_displayRefresh.Hz();
        target=hz>=30.f && hz<=1000.f ? hz : 0.f; // unknown refresh: wait, never guess
    }
    if(!(target>0)) return;
    static float original=-1.f; static unsigned rewrites=0, logged=0; static ULONGLONG nextLog=0;
    __try {
        volatile float* value=(volatile float*)0x10EB094;
        const float current=*value;
        if(current!=target) {
            if(original<0.f) {
                original=current;
                Log("[FrameCap] MaxFrameRate %.1f -> %.1f (FrameCapMode=%d, %s)",current,target,
                    *(volatile int*)0x10EB068,g_engineMaxFrameRateAuto ? "AUTO headset refresh":"fixed");
            } else ++rewrites;
            *value=target;
        }
        const ULONGLONG now=GetTickCount64();
        if(rewrites!=logged && now>=nextLog) {
            Log("[FrameCap] engine reloaded MaxFrameRate %u time(s); re-applied %.1f",rewrites,target);
            logged=rewrites; nextLog=now+10000;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        g_engineFrameCapVerified=0;
        Log("[FrameCap] access failed; further writes disabled");
    }
}

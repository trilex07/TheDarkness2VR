// Darkness2VR v0.4.0 — D3D9/D3D9Ex proxy + OpenXR
// Method 2: Frustum-Aligned Same-Frame Stereo 3D (D3D11 Stereo Shift + INTZ Depth + Comfort Profile)
// Log: %LOCALAPPDATA%\Darkness2VR\d3d9.log
// Config: %LOCALAPPDATA%\Darkness2VR\Darkness2VR.ini or beside DarknessII.exe

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <delayimp.h>
#include <shlobj.h>
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include <unordered_map>
#include <algorithm>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <xinput.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "xinput.lib")

namespace {

CRITICAL_SECTION g_logLock;
char g_logPath[MAX_PATH] = {};
char g_iniPath[MAX_PATH] = {};
bool g_logReady = false;

// Config / Modes
// TestMode: 0 = Normal Game Capture, 1 = Solid Bright Green, 2 = Checkerboard
int g_testMode = 0;
// SubmitMode (F10):
//   0 = VR_STEREO_3D      (Method 2: Same-Frame Stereo Shift + INTZ Depth, Convergent)
//   1 = VR_STEREO_SWAP    (Method 2: Same-Frame Stereo Shift with Swapped Eye Sign)
//   2 = QUAD_WORLD_ANCHOR (World-anchored cinema screen in LOCAL space)
//   3 = QUAD_HEAD_LOCKED  (Head-locked cinema screen)
//   4 = VR_NATIVE_STEREO  (TRUE same-frame dual-eye geometry stereo, draw replay + matrix shear)
int g_submitMode = 0;

// True stereo (SubmitMode 4): frame-record/replay dual-eye rendering
// StereoMode: 0 = off (mono live pass), 1 = dual-eye (live = eye0 + replay = eye1),
//             2 = validation replay with zero shift (image must match live pass)
int g_stereoMode = 1;
int g_hookMask = 15;               // bit0=record hooks, bit1=swapchain hook, bit2=vs analysis, bit3=constF extras
float g_stereoSep = 0.020f;      // shear coefficient per eye (screen-space) - shift mode 1
float g_stereoConv = 2.0f;       // convergence distance in clip-w (world) units - shift mode 1
int g_stereoShiftMode = 0;       // 0 = translation (VR-correct), 1 = shear (HelixMod)
float g_stereoIPD = 0.065f;      // full interpupillary distance in world units - shift mode 0
float g_fovWiden = 2.5f;         // rendered FOV multiplier on tan(FOV/2) (manual, F11)
float g_fovWidenX = 2.5f;        // effective per-axis widen (auto from the HMD frustum)
float g_fovWidenY = 2.5f;
int g_autoFovWiden = 1;          // 1 = derive the widen from xrLocateViews FOV (exact coverage)
float g_viewOffsetX = 0.0f;      // horizontal view centering (F5): radians of frustum rotation
int g_stereoPolarity = 0;        // 0 = left eye +shift, 1 = swapped
// Game camera frustum (radians, half-angles) used for OpenXR per-view fov.
// Measured from engine ViewProj matrices: Px=1.347 Py=2.414 (16:9, FOV slider mid).
float g_gameHalfFovX = 0.637f;
float g_gameHalfFovY = 0.393f;
// Projection measured from WVP matrix uploads: Px=len(col0)/len(w-col)=1/tan(halfFovX)
float g_estPx = 1.347f;
float g_estPy = 2.414f;
volatile LONG g_estValid = 0;
float g_fovOverrideDeg = 100.0f;   // native engine vertical FOV override (0 = disabled)
volatile LONG g_trueStereoFrames = 0;
int g_blitPass = 0;                // 1 = RenderStereoEyeD11 does a 1:1 blit (true-stereo mode)
bool g_eyePairReady = false;       // both eye images captured this frame (SubmitMode 4)
int g_dumpEyeFrames = 0;           // F12: dump N eye pairs to test-output/*.bmp for offline analysis
int g_dumpDelay = 300;             // wait this many frames before dumping (skip black startup)
int g_zeroCopyEnabled = 0;         // Config: Render.ZeroCopy (0 = CPU CopyFrame, 1 = D3D9Ex shared texture)
int g_renderW = 2560;              // Config: Render.RenderWidth (VR backbuffer forced size)
int g_renderH = 1440;              // Config: Render.RenderHeight
int g_fakeClientRect = 1;          // report the VR size to the engine via GetClientRect (RE_FINDINGS)
typedef BOOL(WINAPI* PFN_GetClientRect)(HWND, LPRECT);
static PFN_GetClientRect g_origGetClientRect = nullptr;

// RE (RE_FINDINGS.md): the engine sizes ALL its render targets from the window client rect -
// Manager::SetHwnd (0x00C96440) calls GetClientRect and stores the size at +0x18C/+0x190,
// which feeds FullScreenSizeX/Y (0x010E318C/0x010E31B8) and the RT creation. Forcing only the
// D3D9 backbuffer changed nothing (RT0 stayed 1920x1071). Lying about the client size makes
// the whole engine render at the VR resolution.
static BOOL WINAPI Hook_GetClientRect(HWND hwnd, LPRECT rc) {
    BOOL ok = g_origGetClientRect ? g_origGetClientRect(hwnd, rc) : FALSE;
    if (ok && rc && g_fakeClientRect && g_renderW >= 1280) {
        rc->left = 0;
        rc->top = 0;
        rc->right = g_renderW;
        rc->bottom = g_renderH;
    }
    return ok;
}
bool g_getClientRectHooked = false;
bool g_stateBlockHookEnabled = false; // Config: Render.StateBlockHook (OFF: runtime vtable layout
                                      // of IDirect3DStateBlock9 differs from the SDK header)
bool g_replayTestClear = false;    // Config: Render.ReplayTestClear (diagnostic: flood RT before replay)

// TrackingSubMode (F9):
//   0 = Balanced 16:9 FOV (halfTanX = 0.88, full HUD visible + 6DoF parallax)
//   1 = Immersive Wide FOV (halfTanX = 1.04 + 6DoF parallax)
//   2 = Comfort Compact FOV (halfTanX = 0.76, locked 3DoF)
int g_trackingSubMode = 0;

float g_worldScale = 1.0f;       // Meters -> 6DoF parallax scale
float g_viewScale = 1.45f;        // FOV view scale / pull-back factor (F4 cycles: 1.30x, 1.45x, 1.60x, 1.75x, 1.15x, 1.00x)
float g_stereoStrength = 1.0f;   // Multiplier on stereo shift (F11 cycles 1.0, 1.5, 2.0, 0.5, 0.25, 0.0)
float g_stereoBaseShift = 0.0110f; // Base per-eye horizontal shift in UV (F6 cycles)
float g_stereoDepthScale = 0.0220f; // Depth-modulated per-eye shift in UV (+47% boost)
float g_stereoMaxDisp = 0.038f;  // Hard safety clamp on per-eye UV shift
int g_depthViz = 0;              // F5 toggles live depth map visualization
float g_cinemaDistanceM = 2.0f;
float g_cinemaWidthM = 2.1f;
int g_enableVRGamepad = 1;
int g_remapPhysical = 1;
int g_forceWindowedReset = 0;

// SteamVR / OpenXR recommended view resolution (native headset resolution per eye)
uint32_t g_steamVrRecW = 0;
uint32_t g_steamVrRecH = 0;

// Head-Look camera rotation (turning head turns in-game camera)
// Mode: 1 = Mouse deltas (1:1 smooth camera rotation), 2 = Gamepad right stick, 3 = Both, 0 = Disabled
int g_headLookMode = 1;
int g_headLookInvertYaw = 1;   // 1 = Inverted (standard: turning head left turns camera left)
int g_headLookInvertPitch = 0; // 0 = Normal (looking up looks up)
float g_headLookSensitivityYaw = 750.0f;
float g_headLookSensitivityPitch = 600.0f;
volatile LONG g_headStickX = 0;
volatile LONG g_headStickY = 0;
bool g_headLookReset = true;

// World anchor state for Cinema & Immersive VR (SubmitMode 2 or World-Anchored VR)
bool g_anchorValid = false;
XrPosef g_anchorPose{};
XrVector3f g_anchorHeadOrigin{};

// Immersive VR World Anchor in OpenXR LOCAL space (re-centered via F8 or L3+R3)
bool g_vrWorldAnchorValid = false;
XrPosef g_vrWorldAnchor{};
bool g_vrRefValid = false;
XrVector3f g_vrRefPos{ 0.f, 0.f, 0.f };
float g_vrRefYaw = 0.f;

// Render thread & diagnostic state
volatile DWORD g_renderThreadId = 0;
volatile LONG g_dumpNextFrame = 0;
volatile LONG g_lastStage = 0;
volatile LONG g_lastHookId = 0;   // diagnostics: last hook entered (for crash reports)
PVOID g_vehHandle = nullptr;

void LogInit() {
    InitializeCriticalSection(&g_logLock);
    char base[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, base))) {
        strcat_s(base, "\\Darkness2VR");
        CreateDirectoryA(base, nullptr);
        sprintf_s(g_logPath, "%s\\d3d9.log", base);
        sprintf_s(g_iniPath, "%s\\Darkness2VR.ini", base);

        FILE* f = nullptr;
        if (fopen_s(&f, g_logPath, "w") == 0 && f) {
            fprintf(f, "=== Darkness2VR v0.4.0 (Method 2 Same-Frame Stereo Shift) d3d9.log ===\n");
            fclose(f);
        }
        g_logReady = true;
    }
}

void Log(const char* fmt, ...) {
    if (!g_logReady) return;
    EnterCriticalSection(&g_logLock);
    FILE* f = nullptr;
    if (fopen_s(&f, g_logPath, "a") == 0 && f) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fputc('\n', f);
        fflush(f);
        fclose(f);
    }
    LeaveCriticalSection(&g_logLock);
}

// First-chance Vectored Exception Handler to log exact crash address, registers, and call stack
LONG WINAPI CrashVehHandler(PEXCEPTION_POINTERS ep) {
    if (!ep || !ep->ExceptionRecord || !ep->ContextRecord) return EXCEPTION_CONTINUE_SEARCH;
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    // Log all serious exceptions (>= 0xC0000000). Benign first-chance codes
    // (thread name 0x406D1388, MSVC C++ 0xE06D7363, DBG_*) are below that.
    if (code < 0xC0000000) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (code == 0xC0000420 /* DBG_PRINTEXCEPTION_C */) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    static volatile LONG s_crashLogged = 0;
    if (InterlockedIncrement(&s_crashLogged) <= 3) {
        void* addr = ep->ExceptionRecord->ExceptionAddress;
        HMODULE mod = nullptr;
        char modName[MAX_PATH] = "<unknown>";
        uintptr_t modBase = 0;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(addr), &mod) && mod) {
            GetModuleFileNameA(mod, modName, MAX_PATH);
            modBase = reinterpret_cast<uintptr_t>(mod);
        }
        const CONTEXT* c = ep->ContextRecord;
        Log("!!! EXCEPTION 0x%08lX at %p (%s + 0x%lX) | tid=%lu renderTid=%lu stage=%ld lastHook=%ld !!!",
            code, addr, modName, (unsigned long)((uintptr_t)addr - modBase),
            GetCurrentThreadId(), g_renderThreadId, g_lastStage, g_lastHookId);
        Log("  (first-chance: may be handled by our SEH guards - game continues)");
        if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
            Log("  AccessViolation type=%lu targetAddr=%p",
                (unsigned long)ep->ExceptionRecord->ExceptionInformation[0],
                reinterpret_cast<void*>(ep->ExceptionRecord->ExceptionInformation[1]));
        }
        Log("  EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX ESI=%08lX EDI=%08lX EBP=%08lX ESP=%08lX EIP=%08lX",
            c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp, c->Eip);

        const uintptr_t* stack = reinterpret_cast<const uintptr_t*>(c->Esp);
        Log("  Call Stack (ESP=%p):", stack);
        for (int i = 0; i < 32; ++i) {
            __try {
                uintptr_t retVal = stack[i];
                HMODULE retMod = nullptr;
                char retModName[MAX_PATH] = "";
                uintptr_t retModBase = 0;
                if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                       reinterpret_cast<LPCSTR>(retVal), &retMod) && retMod) {
                    GetModuleFileNameA(retMod, retModName, MAX_PATH);
                    retModBase = reinterpret_cast<uintptr_t>(retMod);
                    Log("    [%02d] %08lX (%s + 0x%lX)", i, (unsigned long)retVal, retModName, (unsigned long)(retVal - retModBase));
                } else if (retVal > 0x10000) {
                    Log("    [%02d] %08lX", i, (unsigned long)retVal);
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                break;
            }
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void LoadConfig() {
    char localIni[MAX_PATH] = ".\\Darkness2VR.ini";
    const char* chosenIni = g_iniPath;
    if (GetFileAttributesA(localIni) != INVALID_FILE_ATTRIBUTES) {
        chosenIni = localIni;
    } else {
        if (GetFileAttributesA(g_iniPath) == INVALID_FILE_ATTRIBUTES) {
            FILE* f = nullptr;
            if (fopen_s(&f, g_iniPath, "w") == 0 && f) {
                fprintf(f, "[Render]\n");
                fprintf(f, "TestMode=0\n");
                fprintf(f, "; SubmitMode: 0 = VR_STEREO_3D (default), 1 = VR_STEREO_SWAP, 2 = QUAD_WORLD_ANCHOR (Cinema), 3 = QUAD_HEAD_LOCKED\n");
                fprintf(f, "SubmitMode=0\n");
                fprintf(f, "; TrackingSubMode: 0 = Balanced FOV, 1 = Wide Immersive FOV, 2 = Compact FOV\n");
                fprintf(f, "TrackingSubMode=0\n");
                fprintf(f, "WorldScale=1.0\n");
                fprintf(f, "StereoStrength=1.0\n");
                fprintf(f, "StereoBaseShift=0.0035\n");
                fprintf(f, "StereoDepthScale=0.0105\n");
                fprintf(f, "StereoMaxDisparity=0.020\n");
                fprintf(f, "DistanceM=2.0\n");
                fprintf(f, "WidthM=2.1\n");
                fprintf(f, "ForceWindowedReset=0\n\n");
                fprintf(f, "[Input]\n");
                fprintf(f, "EnableVRGamepad=1\n");
                fprintf(f, "RemapPhysicalPorts=1\n");
                fclose(f);
            }
        }
    }

    g_testMode = GetPrivateProfileIntA("Render", "TestMode", 0, chosenIni);
    g_submitMode = GetPrivateProfileIntA("Render", "SubmitMode", 0, chosenIni);
    if (g_submitMode < 0 || g_submitMode > 4) g_submitMode = 0;
    g_trackingSubMode = GetPrivateProfileIntA("Render", "TrackingSubMode", 0, chosenIni);
    if (g_trackingSubMode < 0 || g_trackingSubMode > 2) g_trackingSubMode = 0;
    g_forceWindowedReset = GetPrivateProfileIntA("Render", "ForceWindowedReset", 0, chosenIni);
    g_stereoMode = GetPrivateProfileIntA("Render", "StereoMode", 1, chosenIni);
    if (g_stereoMode < 0 || g_stereoMode > 2) g_stereoMode = 1;
    g_stereoPolarity = GetPrivateProfileIntA("Render", "StereoPolarity", 0, chosenIni) ? 1 : 0;
    g_hookMask = GetPrivateProfileIntA("Render", "HookMask", 15, chosenIni);
    if (g_hookMask < 0 || g_hookMask > 15) g_hookMask = 15;
    g_dumpEyeFrames = GetPrivateProfileIntA("Render", "DumpFrames", 0, chosenIni);
    if (g_dumpEyeFrames < 0 || g_dumpEyeFrames > 600) g_dumpEyeFrames = 0;
    g_dumpDelay = GetPrivateProfileIntA("Render", "DumpDelay", 300, chosenIni);
    if (g_dumpDelay < 0 || g_dumpDelay > 100000) g_dumpDelay = 300;
    g_replayTestClear = GetPrivateProfileIntA("Render", "ReplayTestClear", 0, chosenIni) != 0;
    g_autoFovWiden = GetPrivateProfileIntA("Render", "AutoFovWiden", 1, chosenIni) ? 1 : 0;
    g_zeroCopyEnabled = GetPrivateProfileIntA("Render", "ZeroCopy", 0, chosenIni) ? 1 : 0;
    g_renderW = GetPrivateProfileIntA("Render", "RenderWidth", 2560, chosenIni);
    g_renderH = GetPrivateProfileIntA("Render", "RenderHeight", 1440, chosenIni);
    g_stateBlockHookEnabled = GetPrivateProfileIntA("Render", "StateBlockHook", 0, chosenIni) != 0;
    if (g_renderW < 1280 || g_renderW > 7680) g_renderW = 2560;
    if (g_renderH < 720 || g_renderH > 4320) g_renderH = 1440;

    char buf[64]{};
    if (GetPrivateProfileStringA("Render", "ViewOffsetX", "0.0", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= -1.0f && v <= 1.0f) g_viewOffsetX = v;
    }
    if (GetPrivateProfileStringA("Render", "StereoSep", "0.020", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= 0.0f && v <= 0.25f) g_stereoSep = v;
    }
    if (GetPrivateProfileStringA("Render", "StereoConv", "2.0", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= 0.1f && v <= 100.0f) g_stereoConv = v;
    }
    g_stereoShiftMode = GetPrivateProfileIntA("Render", "StereoShiftMode", 0, chosenIni) ? 1 : 0;
    if (GetPrivateProfileStringA("Render", "StereoIPD", "0.065", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= 0.005f && v <= 50.0f) g_stereoIPD = v;   // world units: game scale is not meters
    }
    if (GetPrivateProfileStringA("Render", "FovWiden", "2.5", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= 1.0f && v <= 8.0f) g_fovWiden = v;
    }
    if (GetPrivateProfileStringA("Render", "FovOverride", "100.0", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= 0.0f && v <= 160.0f) g_fovOverrideDeg = v;
    }
    {
        float fovX = 0.f, fovY = 0.f;
        if (GetPrivateProfileStringA("Render", "GameFovX", "73.2", buf, sizeof(buf), chosenIni))
            fovX = (float)atof(buf);
        if (GetPrivateProfileStringA("Render", "GameFovY", "44.9", buf, sizeof(buf), chosenIni))
            fovY = (float)atof(buf);
        if (fovX >= 20.0f && fovX <= 170.0f) g_gameHalfFovX = fovX * 3.14159265f / 360.0f;
        if (fovY >= 10.0f && fovY <= 170.0f) g_gameHalfFovY = fovY * 3.14159265f / 360.0f;
    }
    if (GetPrivateProfileStringA("Render", "WorldScale", "1.0", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= 0.01f && v <= 500.0f) g_worldScale = v;
    }
    if (GetPrivateProfileStringA("Render", "ViewScale", "1.45", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= 0.5f && v <= 3.0f) g_viewScale = v;
    }
    if (GetPrivateProfileStringA("Render", "StereoStrength", "1.0", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= 0.0f && v <= 5.0f) g_stereoStrength = v;
    }
    if (GetPrivateProfileStringA("Render", "StereoBaseShift", "0.0110", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= 0.0f && v <= 0.05f) g_stereoBaseShift = v;
    }
    if (GetPrivateProfileStringA("Render", "StereoDepthScale", "0.0220", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= 0.0f && v <= 0.05f) g_stereoDepthScale = v;
    }
    if (GetPrivateProfileStringA("Render", "StereoMaxDisparity", "0.038", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= 0.002f && v <= 0.08f) g_stereoMaxDisp = v;
    }
    if (GetPrivateProfileStringA("Render", "DistanceM", "2.0", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v > 0.5f && v < 20.0f) g_cinemaDistanceM = v;
    }
    if (GetPrivateProfileStringA("Render", "WidthM", "2.1", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v > 0.5f && v < 20.0f) g_cinemaWidthM = v;
    }
    g_enableVRGamepad = GetPrivateProfileIntA("Input", "EnableVRGamepad", 1, chosenIni);
    g_remapPhysical = GetPrivateProfileIntA("Input", "RemapPhysicalPorts", 1, chosenIni);
    g_headLookMode = GetPrivateProfileIntA("Input", "HeadLookMode", 1, chosenIni);
    if (g_headLookMode < 0 || g_headLookMode > 3) g_headLookMode = 1;
    g_headLookInvertYaw = GetPrivateProfileIntA("Input", "HeadLookInvertYaw", 1, chosenIni);
    g_headLookInvertPitch = GetPrivateProfileIntA("Input", "HeadLookInvertPitch", 0, chosenIni);

    if (GetPrivateProfileStringA("Input", "HeadLookSensitivityYaw", "750.0", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= 50.0f && v <= 5000.0f) g_headLookSensitivityYaw = v;
    }
    if (GetPrivateProfileStringA("Input", "HeadLookSensitivityPitch", "600.0", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= 50.0f && v <= 5000.0f) g_headLookSensitivityPitch = v;
    }

    Log("Config loaded from: %s", chosenIni);
    Log("  Render: SubmitMode=%d TrackingSubMode=%d ViewScale=%.2f StereoStrength=%.2f BaseShift=%.4f DepthScale=%.4f MaxDisp=%.4f",
        g_submitMode, g_trackingSubMode, g_viewScale, g_stereoStrength, g_stereoBaseShift, g_stereoDepthScale, g_stereoMaxDisp);
    Log("  Input: EnableVRGamepad=%d RemapPhysicalPorts=%d HeadLookMode=%d (SensYaw=%.1f SensPitch=%.1f InvertYaw=%d InvertPitch=%d)",
        g_enableVRGamepad, g_remapPhysical, g_headLookMode, g_headLookSensitivityYaw, g_headLookSensitivityPitch, g_headLookInvertYaw, g_headLookInvertPitch);
    Log("  Hotkeys: F2=InvertYaw | F3=HeadLook | F4=ViewScale | F5=DepthViz | F6=Cycle BaseShift | F7=Dump Matrices | F8 (or L3+R3)=Recenter | F9=Cycle FOV Scale | F10=Cycle VR Mode | F11=Cycle Stereo Strength");
}

void RequestRecenter(const char* source) {
    g_anchorValid = false;
    g_vrWorldAnchorValid = false;
    g_vrRefValid = false;
    g_headLookReset = true;
    Beep(1000, 50);
    Log(">>> [%s] VR Recenter requested (World Anchor + 6DoF Reference + HeadLook) <<<", source);
}

void PollHotkeys() {
    static DWORD s_lastF2 = 0;
    static DWORD s_lastF3 = 0;
    static DWORD s_lastF4 = 0;
    static DWORD s_lastF5 = 0;
    static DWORD s_lastF6 = 0;
    static DWORD s_lastF7 = 0;
    static DWORD s_lastF8 = 0;
    static DWORD s_lastF9 = 0;
    static DWORD s_lastF10 = 0;
    static DWORD s_lastF11 = 0;
    static DWORD s_lastF12 = 0;

    DWORD now = GetTickCount();

    if ((GetAsyncKeyState(VK_F12) & 0x8000) && (now - s_lastF12 > 400)) {
        s_lastF12 = now;
        g_dumpEyeFrames = (g_dumpEyeFrames > 0) ? 0 : 5;
        Beep(1400, 50);
        Log(">>> [HOTKEY F12] Eye frame dump: %s <<<", g_dumpEyeFrames ? "5 pairs -> test-output/" : "stopped");
    }

    if ((GetAsyncKeyState(VK_F2) & 0x8000) && (now - s_lastF2 > 400)) {
        s_lastF2 = now;
        g_headLookInvertYaw = !g_headLookInvertYaw;
        Beep(g_headLookInvertYaw ? 1100 : 800, 50);
        Log(">>> [HOTKEY F2] HeadLook Invert Yaw toggled: %d (1 = lewo->lewo, 0 = odwrotnie) <<<", g_headLookInvertYaw);
    }

    if ((GetAsyncKeyState(VK_F3) & 0x8000) && (now - s_lastF3 > 400)) {
        s_lastF3 = now;
        if (g_headLookMode == 0) {
            g_headLookMode = 1;
            g_headLookSensitivityYaw = 750.0f;
            g_headLookSensitivityPitch = 600.0f;
        } else if (g_headLookMode == 1 && g_headLookSensitivityYaw < 900.0f) {
            g_headLookMode = 1;
            g_headLookSensitivityYaw = 1050.0f;
            g_headLookSensitivityPitch = 850.0f;
        } else if (g_headLookMode == 1) {
            g_headLookMode = 2; // Gamepad Stick
        } else if (g_headLookMode == 2) {
            g_headLookMode = 3; // Hybrid
        } else {
            g_headLookMode = 0; // Disabled
        }
        g_headLookReset = true;
        Beep(700 + g_headLookMode * 200, 50);
        const char* hlNames[] = {
            "0 (Wylaczone / Manual)",
            "1 (Mysz 1:1 - standardowa czulosc 750)",
            "2 (Prawy Stick Pad / XInput)",
            "3 (Hybrydowy - Mysz + Stick)"
        };
        const char* name = (g_headLookMode >= 0 && g_headLookMode <= 3) ? hlNames[g_headLookMode] : "Unknown";
        if (g_headLookMode == 1 && g_headLookSensitivityYaw > 900.0f) {
            name = "1 (Mysz Szybka - podwyzszona czulosc 1050)";
        }
        Log(">>> [HOTKEY F3] HeadLook Mode changed to: %s <<<", name);
    }

    if ((GetAsyncKeyState(VK_F4) & 0x8000) && (now - s_lastF4 > 400)) {
        s_lastF4 = now;
        if (g_viewScale < 1.28f) g_viewScale = 1.35f;
        else if (g_viewScale < 1.42f) g_viewScale = 1.45f;
        else if (g_viewScale < 1.55f) g_viewScale = 1.60f;
        else if (g_viewScale < 1.70f) g_viewScale = 1.75f;
        else if (g_viewScale < 1.85f) g_viewScale = 1.15f;
        else g_viewScale = 1.00f;
        Beep(700 + (int)(g_viewScale * 300.f), 50);
        Log(">>> [HOTKEY F4] ViewScale (Oddalenie / Rozmiar Elementow) changed to: %.2fx <<<", g_viewScale);
    }

    if ((GetAsyncKeyState(VK_F5) & 0x8000) && (now - s_lastF5 > 400)) {
        s_lastF5 = now;
        if (g_submitMode == 4) {
            // horizontal view centering
            if (g_viewOffsetX < -0.15f) g_viewOffsetX = -0.10f;
            else if (g_viewOffsetX < -0.05f) g_viewOffsetX = 0.0f;
            else if (g_viewOffsetX < 0.10f) g_viewOffsetX = 0.10f;
            else g_viewOffsetX = -0.20f;
            Beep(900 + (int)(g_viewOffsetX * 3000.f), 50);
            Log(">>> [HOTKEY F5] View Offset X changed to: %.3f rad <<<", g_viewOffsetX);
        } else {
            g_depthViz = !g_depthViz;
            Beep(g_depthViz ? 1300 : 700, 50);
            Log(">>> [HOTKEY F5] Depth Visualization toggled: %d <<<", g_depthViz);
        }
    }

    if ((GetAsyncKeyState(VK_F6) & 0x8000) && (now - s_lastF6 > 400)) {
        s_lastF6 = now;
        if (g_submitMode == 4) {
            if (g_stereoShiftMode == 0) {
                // True-stereo IPD in WORLD UNITS (measured: 1 world unit ~ 10cm, so the
                // physically-natural IPD is ~0.65; F6 cycles the calibration range)
                if (g_stereoIPD < 0.45f) g_stereoIPD = 0.45f;
                else if (g_stereoIPD < 0.85f) g_stereoIPD = 1.0f;
                else if (g_stereoIPD < 1.6f) g_stereoIPD = 2.0f;
                else if (g_stereoIPD < 3.0f) g_stereoIPD = 4.0f;
                else g_stereoIPD = 0.25f;
                Beep(750 + (int)(g_stereoIPD * 8000.f), 50);
                Log(">>> [HOTKEY F6] TrueStereo IPD changed to: %.3f world units (srodkowo: %.1f%% ekranu @2m) <<<",
                    g_stereoIPD, 100.0f * (g_estPx * g_stereoIPD * 0.5f) / 2.0f);
            } else {
                if (g_stereoSep < 0.012f) g_stereoSep = 0.012f;
                else if (g_stereoSep < 0.022f) g_stereoSep = 0.022f;
                else if (g_stereoSep < 0.035f) g_stereoSep = 0.035f;
                else if (g_stereoSep < 0.055f) g_stereoSep = 0.055f;
                else g_stereoSep = 0.004f;
                Beep(750 + (int)(g_stereoSep * 20000.f), 50);
                Log(">>> [HOTKEY F6] TrueStereo Separation changed to: %.4f <<<", g_stereoSep);
            }
        } else {
            if (g_stereoBaseShift < 0.0050f) g_stereoBaseShift = 0.0075f;
            else if (g_stereoBaseShift < 0.0090f) g_stereoBaseShift = 0.0110f;
            else if (g_stereoBaseShift < 0.0130f) g_stereoBaseShift = 0.0145f;
            else if (g_stereoBaseShift < 0.0165f) g_stereoBaseShift = 0.0180f;
            else g_stereoBaseShift = 0.0040f;
            Beep(750 + (int)(g_stereoBaseShift * 40000.f), 50);
            Log(">>> [HOTKEY F6] Stereo Base Shift changed to: %.4f <<<", g_stereoBaseShift);
        }
    }

    if ((GetAsyncKeyState(VK_F7) & 0x8000) && (now - s_lastF7 > 400)) {
        s_lastF7 = now;
        InterlockedExchange(&g_dumpNextFrame, 1);
        Beep(1200, 40);
        Log(">>> [HOTKEY F7] Diagnostic Camera & Matrix Dump requested for next frame <<<");
    }

    if ((GetAsyncKeyState(VK_F8) & 0x8000) && (now - s_lastF8 > 400)) {
        s_lastF8 = now;
        RequestRecenter("HOTKEY F8");
    }

    if ((GetAsyncKeyState(VK_F9) & 0x8000) && (now - s_lastF9 > 400)) {
        s_lastF9 = now;
        g_trackingSubMode = (g_trackingSubMode + 1) % 3;
        g_vrWorldAnchorValid = false;
        Beep(700 + g_trackingSubMode * 200, 50);
        const char* names[] = {
            "0 (Immersive VR Dome - Obraz Dookoła 150° + Śledzenie Głowy w Pokoju)",
            "1 (Ultra-Szeroka Panorama 180° - Pełne Otoczenie VR)",
            "2 (Ekran Kinowy 3D - Framed 3D Cinema Screen)"
        };
        Log(">>> [HOTKEY F9] TrackingSubMode changed to: %s <<<", names[g_trackingSubMode]);
    }

    if ((GetAsyncKeyState(VK_F10) & 0x8000) && (now - s_lastF10 > 400)) {
        s_lastF10 = now;
        g_submitMode = (g_submitMode + 1) % 5;
        g_anchorValid = false;
        Beep(600 + g_submitMode * 150, 50);
        const char* names[] = {
            "0 (VR_STEREO_3D - Method 2 Same-Frame Stereo Shift)",
            "1 (VR_STEREO_SWAP - Method 2 Swapped Eye Sign)",
            "2 (QUAD_WORLD_ANCHOR - World-Anchored Cinema)",
            "3 (QUAD_HEAD_LOCKED - Head-Locked Cinema)",
            "4 (VR_NATIVE_STEREO - True dual-eye geometry stereo, draw replay)"
        };
        Log(">>> [HOTKEY F10] SubmitMode changed to: %s <<<", names[g_submitMode]);
    }

    if ((GetAsyncKeyState(VK_F11) & 0x8000) && (now - s_lastF11 > 400)) {
        s_lastF11 = now;
        if (g_submitMode == 4) {
            if (g_stereoShiftMode == 0) {
                // FOV widen: how much of the HMD frustum the game render covers
                if (g_fovWiden < 1.3f) g_fovWiden = 1.6f;
                else if (g_fovWiden < 1.9f) g_fovWiden = 2.2f;
                else if (g_fovWiden < 2.4f) g_fovWiden = 2.8f;
                else if (g_fovWiden < 3.1f) g_fovWiden = 3.5f;
                else g_fovWiden = 1.0f;
                Beep(700 + (int)(g_fovWiden * 150.f), 50);
                Log(">>> [HOTKEY F11] FovWiden changed to: %.2fx (pionowo: %.0f deg, poziomo: %.0f deg) <<<",
                    g_fovWiden,
                    2.0f * std::atan(g_fovWiden / g_estPy) * 57.29578f,
                    2.0f * std::atan(g_fovWiden / g_estPx) * 57.29578f);
            } else {
                if (g_stereoConv < 1.2f) g_stereoConv = 1.2f;
                else if (g_stereoConv < 1.8f) g_stereoConv = 1.8f;
                else if (g_stereoConv < 2.5f) g_stereoConv = 2.5f;
                else if (g_stereoConv < 4.0f) g_stereoConv = 4.0f;
                else g_stereoConv = 8.0f;
                Beep(800 + (int)(g_stereoConv * 100.f), 50);
                Log(">>> [HOTKEY F11] TrueStereo Convergence changed to: %.2f (world units) <<<", g_stereoConv);
            }
        } else if (g_submitMode <= 1) {
            if (g_stereoStrength < 0.15f) g_stereoStrength = 0.25f;
            else if (g_stereoStrength < 0.40f) g_stereoStrength = 0.5f;
            else if (g_stereoStrength < 0.75f) g_stereoStrength = 1.0f;
            else if (g_stereoStrength < 1.25f) g_stereoStrength = 1.5f;
            else if (g_stereoStrength < 1.75f) g_stereoStrength = 2.0f;
            else g_stereoStrength = 0.0f;
            Beep(800 + (int)(g_stereoStrength * 200.f), 50);
            Log(">>> [HOTKEY F11] Stereo Shift Strength changed to: %.2fx <<<", g_stereoStrength);
        } else {
            g_cinemaDistanceM = (g_cinemaDistanceM >= 3.0f) ? 1.5f : (g_cinemaDistanceM + 0.5f);
            g_anchorValid = false;
            Beep(900, 50);
            Log(">>> [HOTKEY F11] Cinema Distance changed to: %.1fm <<<", g_cinemaDistanceM);
        }
    }
}

// ---------------------------------------------------------------- OpenXR & D3D11 Stereo Resources
XrInstance g_xrInstance = XR_NULL_HANDLE;
XrSession g_xrSession = XR_NULL_HANDLE;
XrSpace g_xrSpace = XR_NULL_HANDLE;      // LOCAL space
XrSpace g_viewSpace = XR_NULL_HANDLE;    // VIEW space
XrSystemId g_xrSystem = XR_NULL_SYSTEM_ID;
ID3D11Device* g_dev11 = nullptr;
ID3D11DeviceContext* g_ctx11 = nullptr;
bool g_xrReady = false;
bool g_xrRunning = false;

XrSwapchain g_swapchain[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE };
uint32_t g_scW = 0, g_scH = 0;
ID3D11Texture2D** g_scImages[2] = { nullptr, nullptr };
uint32_t g_scCount[2] = { 0, 0 };

ID3D11Texture2D* g_eyeCache[2] = { nullptr, nullptr };
ID3D11RenderTargetView* g_eyeRTV[2] = { nullptr, nullptr };

IDirect3DSurface9* g_sysmem = nullptr;
ID3D11Texture2D* g_upload = nullptr;
ID3D11ShaderResourceView* g_uploadSRV = nullptr;
UINT g_srcW = 0, g_srcH = 0;
D3DFORMAT g_srcFmt = D3DFMT_UNKNOWN;

// D3D9 INTZ Depth Capture -> D3D11 Depth SRV (480x270 fast downsample)
static constexpr UINT kDepthW = 480;
static constexpr UINT kDepthH = 270;
static constexpr D3DFORMAT FOURCC_INTZ = static_cast<D3DFORMAT>(0x5A544E49); // 'INTZ'
static constexpr D3DFORMAT FOURCC_RAWZ = static_cast<D3DFORMAT>(0x5A574152); // 'RAWZ'
static constexpr D3DFORMAT FOURCC_DF24 = static_cast<D3DFORMAT>(0x34324644); // 'DF24'

IDirect3DTexture9* g_intzTex9 = nullptr;
IDirect3DTexture9* g_depthFreezeTex9 = nullptr;
IDirect3DSurface9* g_depthFreezeSurf9 = nullptr;
IDirect3DSurface9* g_depthFreezeSys9 = nullptr;
IDirect3DVertexShader9* g_depthFreezeVS9 = nullptr;
IDirect3DPixelShader9* g_depthFreezePS9 = nullptr;
bool g_depthCapturedThisFrame = false;
bool g_depthHasVariance = false;
int g_depthLogCounter = 0;

ID3D11Texture2D* g_depthTex11 = nullptr;
ID3D11ShaderResourceView* g_depthSRV11 = nullptr;

// D3D11 Method 2 Stereo Shader Pipeline
ID3D11VertexShader* g_stereoVS11 = nullptr;
ID3D11PixelShader* g_stereoPS11 = nullptr;
ID3D11Buffer* g_stereoCB11 = nullptr;
ID3D11SamplerState* g_stereoSamp11 = nullptr;
ID3D11RasterizerState* g_stereoRS11 = nullptr;
bool g_stereoPipelineReady = false;

struct alignas(16) StereoCBData {
    float fovTan[4];     // x=tanLeft, y=tanRight, z=tanUp, w=tanDown
    float projParams[4]; // x=invTwoHalfTanX, y=invTwoHalfTanY, z=eyeSign (-1 left, +1 right), w=stereoStrength
    float stereoCfg[4];  // x=baseShift, y=depthScale, z=maxDisp, w=hasRealDepth
    float extraCfg[4];   // x=convDepth, y=kz, z=depthViz, w=posOffsetX
    float posCfg[4];     // x=posOffsetY, y=unused, z=unused, w=unused
};

XrPosef g_eyePose[2]{};
XrFovf g_eyeFov[2]{};
bool g_haveViews = false;
int g_frames = 0;
volatile LONG g_xrInitTried = 0;

XrPosef g_renderEyePose[2]{};
XrFovf g_renderEyeFov[2]{};
bool g_renderHaveViews = false;
bool g_had3DCameraThisFrame = false;
int g_3dCamCallsThisFrame = 0;
int g_consecutive3DFrames = 0;
int g_consecutive2DFrames = 0;
bool g_in3DGameplayMode = false;

static XrVector3f RotVec(const XrQuaternionf& q, float x, float y, float z) {
    const float qx = q.x, qy = q.y, qz = q.z, qw = q.w;
    const float tx = 2.f * (qy * z - qz * y);
    const float ty = 2.f * (qz * x - qx * z);
    const float tz = 2.f * (qx * y - qy * x);
    return { x + qw * tx + (qy * tz - qz * ty), y + qw * ty + (qz * tx - qx * tz),
             z + qw * tz + (qx * ty - qy * tx) };
}

static XrPosef HeadCenterPose(const XrPosef& p0, const XrPosef& p1) {
    XrPosef c = p0;
    c.position.x = (p0.position.x + p1.position.x) * 0.5f;
    c.position.y = (p0.position.y + p1.position.y) * 0.5f;
    c.position.z = (p0.position.z + p1.position.z) * 0.5f;
    return c;
}

static XrPosef CinemaQuadPose(const XrPosef& head) {
    const XrVector3f off = RotVec(head.orientation, 0.f, 0.f, -g_cinemaDistanceM);
    XrPosef p = head;
    p.position.x = head.position.x + off.x;
    p.position.y = head.position.y + off.y;
    p.position.z = head.position.z + off.z;
    return p;
}

static float ComputeHorizontalYaw(const XrQuaternionf& q) {
    const XrVector3f fwd = RotVec(q, 0.f, 0.f, -1.f);
    float fx = fwd.x;
    float fz = fwd.z;
    float len = std::sqrt(fx * fx + fz * fz);
    if (len < 1e-4f) return 0.f;
    return std::atan2(-fx / len, -fz / len);
}

static void ComputeHeadEuler(const XrQuaternionf& q, float* outYaw, float* outPitch) {
    const XrVector3f fwd = RotVec(q, 0.f, 0.f, -1.f);
    if (outYaw) {
        float lenH = std::sqrt(fwd.x * fwd.x + fwd.z * fwd.z);
        if (lenH < 1e-4f) *outYaw = 0.f;
        else *outYaw = std::atan2(-fwd.x, -fwd.z);
    }
    if (outPitch) {
        float lenH = std::sqrt(fwd.x * fwd.x + fwd.z * fwd.z);
        *outPitch = std::atan2(fwd.y, std::max(1e-4f, lenH));
    }
}

static XrPosef ComputeLevelWorldAnchorPose(const XrPosef& head) {
    const XrVector3f fwd = RotVec(head.orientation, 0.f, 0.f, -1.f);
    float fx = fwd.x;
    float fz = fwd.z;
    float len = std::sqrt(fx * fx + fz * fz);
    if (len < 1e-4f) {
        fx = 0.f;
        fz = -1.f;
    } else {
        fx /= len;
        fz /= len;
    }
    const float yaw = std::atan2(-fx, -fz);
    XrPosef p{};
    p.position.x = head.position.x + fx * g_cinemaDistanceM;
    p.position.y = head.position.y;
    p.position.z = head.position.z + fz * g_cinemaDistanceM;
    p.orientation.x = 0.f;
    p.orientation.y = std::sin(yaw * 0.5f);
    p.orientation.z = 0.f;
    p.orientation.w = std::cos(yaw * 0.5f);
    return p;
}

// ---------------------------------------------------------------- True stereo core (analysis + record/replay)
void* PatchVTable(void* obj, int index, void* repl);
#include "stereo_replay.inl"

// Builds the per-eye transform parameters. Eye sign: left eye positive shift by default.
static d2vr_replay::StereoParams MakeEyeParams(int eye) {
    d2vr_replay::StereoParams sp;
    sp.mode = g_stereoShiftMode;
    float sign = (g_stereoPolarity == 0) ? ((eye == 0) ? +1.0f : -1.0f)
                                         : ((eye == 0) ? -1.0f : +1.0f);
    sp.separation = sign * g_stereoSep;
    sp.convergence = g_stereoConv;
    sp.transX = sign * 0.5f * g_stereoIPD;
    sp.transY = 0.0f;
    sp.fovScaleX = (g_fovWidenX > 1.0f) ? (1.0f / g_fovWidenX) : 1.0f;
    sp.fovScaleY = (g_fovWidenY > 1.0f) ? (1.0f / g_fovWidenY) : 1.0f;
    // Validation must use the same unshifted camera for BOTH live and replay.
    // Previously live retained transX while only replay cleared it.
    if (g_stereoMode == 2) {
        sp.separation = sp.transX = sp.transY = 0.0f;
        sp.fovScaleX = sp.fovScaleY = 1.0f;
    }
    return sp;
}

// Updates the OpenXR per-view frustum from the projection measured in the matrix uploads:
// tan(halfFov) = 1/Px ; after the FOV-widen scaling tan' = FovWiden/Px.
// For exact coverage the widen is derived FROM the HMD frustum (xrLocateViews) so the
// rendered image matches the declared per-eye FOV 1:1 (no zoom/offset mismatch).
static void UpdateSubmissionFovFromMatrices() {
    if (InterlockedCompareExchange(&g_estValid, 0, 0) == 0) return;
    if (g_stereoMode == 2) {
        g_gameHalfFovX = std::atan(1.0f / g_estPx);
        g_gameHalfFovY = std::atan(1.0f / g_estPy);
        return;
    }
    if (g_autoFovWiden && g_haveViews) {
        // widen the render to cover the HMD's widest half-angles (auto coverage)
        const float hx = std::max(std::fabs(g_eyeFov[0].angleLeft), std::fabs(g_eyeFov[0].angleRight));
        const float hy = std::max(std::fabs(g_eyeFov[0].angleUp), std::fabs(g_eyeFov[0].angleDown));
        g_fovWidenX = g_estPx * std::tan(hx);
        g_fovWidenY = g_estPy * std::tan(hy);
    } else {
        const float w = (g_fovWiden > 1.0f) ? g_fovWiden : 1.0f;
        g_fovWidenX = w;
        g_fovWidenY = w;
    }
    // the image is rendered with a SYMMETRIC widened frustum, so the declared FOV must be
    // symmetric too (declaring the HMD's asymmetric frustum shifts/distorts the content)
    g_gameHalfFovX = std::atan(g_fovWidenX / g_estPx);
    g_gameHalfFovY = std::atan(g_fovWidenY / g_estPy);
}

// ---------------------------------------------------------------- SetVertexShaderConstantF Hook (Slot 94 - Read-Only 3D Detector)
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetVertexShaderConstantF)(IDirect3DDevice9*, UINT, const float*, UINT);
PFN_SetVertexShaderConstantF g_origSetVertexShaderConstantF = nullptr;

static void LogMatrix4x4(const char* label, const float M[16]) {
    Log("  %s:", label);
    Log("    [%8.4f %8.4f %8.4f %8.4f]", M[0],  M[1],  M[2],  M[3]);
    Log("    [%8.4f %8.4f %8.4f %8.4f]", M[4],  M[5],  M[6],  M[7]);
    Log("    [%8.4f %8.4f %8.4f %8.4f]", M[8],  M[9],  M[10], M[11]);
    Log("    [%8.4f %8.4f %8.4f %8.4f]", M[12], M[13], M[14], M[15]);
}

HRESULT STDMETHODCALLTYPE Hook_SetVertexShaderConstantF(
    IDirect3DDevice9* dev,
    UINT StartRegister,
    const float* pConstantData,
    UINT Vector4fCount)
{
    // Read-only detection of 3D perspective matrices at c0..c3.
    // We NEVER modify c0..c3 because in Evolution Engine (DarknessII.exe 0x00C02F50) c0..c3 is per-object
    // WorldViewProjection while c8..c11 / c20..c23 is ScreenTextureProjection. Keeping D3D9 constants
    // 100% untouched guarantees zero shader/shadow/mesh corruption and zero crashes.
    if (pConstantData && Vector4fCount >= 4 && StartRegister == 0) {
        const float* M = pConstantData;
        const float lenCol3 = std::sqrt(M[3] * M[3] + M[7] * M[7] + M[11] * M[11]);
        const float lenCol0 = std::sqrt(M[0] * M[0] + M[4] * M[4] + M[8] * M[8]);
        const float lenCol1 = std::sqrt(M[1] * M[1] + M[5] * M[5] + M[9] * M[9]);
        if (lenCol3 > 0.1f && lenCol0 > 0.15f && lenCol1 > 0.25f && std::fabs(M[15]) < 500.0f) {
            const float aspect = lenCol1 / lenCol0;
            if (aspect >= 1.2f && aspect <= 2.4f) {
                g_had3DCameraThisFrame = true;
                ++g_3dCamCallsThisFrame;

                static int s_loggedVS = 0;
                const bool f7Dump = (InterlockedCompareExchange(&g_dumpNextFrame, 0, 0) != 0);
                if (s_loggedVS < 10 || f7Dump) {
                    if (!f7Dump) ++s_loggedVS;
                    Log("[VSConstF] 3D WVP at c0 (count=%u) | aspect=%.3f lenCol3=%.3f lenCol0=%.3f lenCol1=%.3f",
                        Vector4fCount, aspect, lenCol3, lenCol0, lenCol1);
                    if (s_loggedVS <= 3 || f7Dump) {
                        LogMatrix4x4("WVP Matrix", M);
                    }
                }
            }
        }
    }

    // ---- True stereo: record original data for frame replay ----
    if ((g_hookMask & 8) && d2vr_replay::g_recActive && pConstantData && Vector4fCount) {
        d2vr_replay::RecEmitData(d2vr_replay::OP_SetVSConstF, pConstantData,
                                 sizeof(float) * 4 * Vector4fCount, StartRegister, Vector4fCount);
    }

    // ---- True stereo: live pass renders eye 0 (eye 1 comes from the replay pass) ----
    if ((g_hookMask & 8) && g_submitMode == 4 && g_stereoMode >= 1 &&
        d2vr_replay::g_replayEye < 0 && pConstantData && Vector4fCount <= 68) {
        static thread_local float s_tmpConst[68 * 4];
        d2vr_replay::StereoParams sp = MakeEyeParams(0);
        if (g_stereoMode == 2) sp.separation = 0.0f;
        if (d2vr_replay::ApplyShiftToUpload(s_tmpConst, pConstantData, StartRegister,
                                            Vector4fCount, d2vr_replay::g_activeVs, sp)) {
            pConstantData = s_tmpConst;
            g_had3DCameraThisFrame = true;
            ++g_3dCamCallsThisFrame;
        }
    }
    return g_origSetVertexShaderConstantF(dev, StartRegister, pConstantData, Vector4fCount);
}

// ---------------------------------------------------------------- INTZ Depth Capture (D3D9 -> 480x270 -> D3D11)
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetDepthStencilSurface)(IDirect3DDevice9*, IDirect3DSurface9*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_SetTexture)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);
PFN_SetDepthStencilSurface g_origSetDepthStencilSurface = nullptr;
PFN_SetTexture g_origSetTexture = nullptr;

static void ReleaseDepthResources9() {
    if (g_intzTex9) { g_intzTex9->Release(); g_intzTex9 = nullptr; }
    if (g_depthFreezeSurf9) { g_depthFreezeSurf9->Release(); g_depthFreezeSurf9 = nullptr; }
    if (g_depthFreezeTex9) { g_depthFreezeTex9->Release(); g_depthFreezeTex9 = nullptr; }
    if (g_depthFreezeSys9) { g_depthFreezeSys9->Release(); g_depthFreezeSys9 = nullptr; }
    if (g_depthFreezeVS9) { g_depthFreezeVS9->Release(); g_depthFreezeVS9 = nullptr; }
    if (g_depthFreezePS9) { g_depthFreezePS9->Release(); g_depthFreezePS9 = nullptr; }
}

static ID3DBlob* CompileShaderBlob(const char* src, const char* entry, const char* profile) {
    static HMODULE s_comp = nullptr;
    if (!s_comp) {
        s_comp = LoadLibraryA("d3dcompiler_47.dll");
        if (!s_comp) s_comp = LoadLibraryA("d3dcompiler_43.dll");
    }
    if (!s_comp) return nullptr;
    typedef HRESULT(WINAPI* PFN_D3DCompile)(const void*, SIZE_T, const char*, const void*, void*,
                                            const char*, const char*, UINT, UINT, ID3DBlob**, ID3DBlob**);
    auto pCompile = reinterpret_cast<PFN_D3DCompile>(GetProcAddress(s_comp, "D3DCompile"));
    if (!pCompile) return nullptr;
    ID3DBlob* blob = nullptr;
    ID3DBlob* err = nullptr;
    HRESULT hr = pCompile(src, strlen(src), "d2vr_shader", nullptr, nullptr, entry, profile, 0, 0, &blob, &err);
    if (FAILED(hr)) {
        Log("Shader compile (%s) failed: %s", profile, err ? (const char*)err->GetBufferPointer() : "unknown");
        if (err) err->Release();
        if (blob) blob->Release();
        return nullptr;
    }
    if (err) err->Release();
    return blob;
}

static bool EnsureDepthFreezeResources(IDirect3DDevice9* dev) {
    if (g_depthFreezeTex9 && g_depthFreezeSurf9 && g_depthFreezeSys9 && g_depthFreezeVS9 && g_depthFreezePS9) {
        return true;
    }
    if (!g_depthFreezeTex9) {
        if (FAILED(dev->CreateTexture(kDepthW, kDepthH, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                      D3DPOOL_DEFAULT, &g_depthFreezeTex9, nullptr)) || !g_depthFreezeTex9) {
            return false;
        }
        g_depthFreezeTex9->GetSurfaceLevel(0, &g_depthFreezeSurf9);
    }
    if (!g_depthFreezeSys9) {
        if (FAILED(dev->CreateOffscreenPlainSurface(kDepthW, kDepthH, D3DFMT_A8R8G8B8,
                                                    D3DPOOL_SYSTEMMEM, &g_depthFreezeSys9, nullptr))) {
            return false;
        }
    }
    if (!g_depthFreezeVS9) {
        const char* vsCode =
            "float4 main(float4 pos : POSITION) : POSITION {\n"
            "    return pos;\n"
            "}\n";
        ID3DBlob* b = CompileShaderBlob(vsCode, "main", "vs_3_0");
        if (!b) return false;
        HRESULT hr = dev->CreateVertexShader(reinterpret_cast<const DWORD*>(b->GetBufferPointer()), &g_depthFreezeVS9);
        b->Release();
        if (FAILED(hr)) return false;
    }
    if (!g_depthFreezePS9) {
        // Properly normalize VPOS (0..479, 0..269) to UV [0..1] using c0 = (1/w, 1/h, 0.5/w, 0.5/h)
        const char* psCode =
            "sampler2D depthTex : register(s0);\n"
            "float4 invSize : register(c0);\n"
            "float4 main(float4 vpos : VPOS) : COLOR0 {\n"
            "    float2 uv = vpos.xy * invSize.xy + invSize.zw;\n"
            "    float d = tex2D(depthTex, uv).r;\n"
            "    return float4(d, d, d, 1.0);\n"
            "}\n";
        ID3DBlob* b = CompileShaderBlob(psCode, "main", "ps_3_0");
        if (!b) return false;
        HRESULT hr = dev->CreatePixelShader(reinterpret_cast<const DWORD*>(b->GetBufferPointer()), &g_depthFreezePS9);
        b->Release();
        if (FAILED(hr)) return false;
    }
    return true;
}

static bool EnsureDepthTexture11() {
    if (g_depthTex11 && g_depthSRV11) return true;
    if (!g_dev11) return false;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = kDepthW;
    td.Height = kDepthH;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DYNAMIC;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(g_dev11->CreateTexture2D(&td, nullptr, &g_depthTex11))) return false;
    if (FAILED(g_dev11->CreateShaderResourceView(g_depthTex11, nullptr, &g_depthSRV11))) return false;
    return true;
}

// Captures the game's INTZ depth texture into our 480x270 D3D11 depth SRV.
static void CaptureIntzDepthToD11(IDirect3DDevice9* dev) {
    if (!dev || !g_intzTex9 || !g_dev11 || !g_ctx11) return;
    if (!EnsureDepthFreezeResources(dev) || !EnsureDepthTexture11()) return;

    // Do not record our own diagnostic draws into the frame replay stream
    struct RecGuard {
        bool was;
        explicit RecGuard(bool a) : was(a) { d2vr_replay::g_recActive = false; }
        ~RecGuard() { d2vr_replay::g_recActive = was; }
    } recGuard(d2vr_replay::g_recActive);

    // Save complete D3D9 state
    IDirect3DSurface9* oldRT = nullptr; dev->GetRenderTarget(0, &oldRT);
    IDirect3DSurface9* oldDS = nullptr; dev->GetDepthStencilSurface(&oldDS);
    D3DVIEWPORT9 oldVP{}; dev->GetViewport(&oldVP);
    IDirect3DVertexShader9* oldVS = nullptr; dev->GetVertexShader(&oldVS);
    IDirect3DPixelShader9* oldPS = nullptr; dev->GetPixelShader(&oldPS);
    IDirect3DVertexDeclaration9* oldDecl = nullptr; dev->GetVertexDeclaration(&oldDecl);
    DWORD oldFVF = 0; dev->GetFVF(&oldFVF);
    IDirect3DBaseTexture9* oldT0 = nullptr; dev->GetTexture(0, &oldT0);
    DWORD oldMin0 = 0, oldMag0 = 0, oldMip0 = 0, oldU0 = 0, oldV0 = 0;
    dev->GetSamplerState(0, D3DSAMP_MINFILTER, &oldMin0);
    dev->GetSamplerState(0, D3DSAMP_MAGFILTER, &oldMag0);
    dev->GetSamplerState(0, D3DSAMP_MIPFILTER, &oldMip0);
    dev->GetSamplerState(0, D3DSAMP_ADDRESSU, &oldU0);
    dev->GetSamplerState(0, D3DSAMP_ADDRESSV, &oldV0);
    DWORD oldScissor = 0, oldZ = 0, oldZWrite = 0, oldBlend = 0, oldCull = 0, oldColorWrite = 0, oldSrgb = 0;
    dev->GetRenderState(D3DRS_SCISSORTESTENABLE, &oldScissor);
    dev->GetRenderState(D3DRS_ZENABLE, &oldZ);
    dev->GetRenderState(D3DRS_ZWRITEENABLE, &oldZWrite);
    dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldBlend);
    dev->GetRenderState(D3DRS_CULLMODE, &oldCull);
    dev->GetRenderState(D3DRS_COLORWRITEENABLE, &oldColorWrite);
    dev->GetRenderState(D3DRS_SRGBWRITEENABLE, &oldSrgb);

    float oldPSConst0[4]{};
    dev->GetPixelShaderConstantF(0, oldPSConst0, 1);

    bool drawOk = false;
    if (SUCCEEDED(dev->SetRenderTarget(0, g_depthFreezeSurf9)) &&
        SUCCEEDED(dev->SetDepthStencilSurface(nullptr))) {
        D3DVIEWPORT9 vp{ 0, 0, kDepthW, kDepthH, 0.0f, 1.0f };
        dev->SetViewport(&vp);
        dev->SetTexture(0, g_intzTex9);
        dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

        dev->SetVertexShader(g_depthFreezeVS9);
        dev->SetPixelShader(g_depthFreezePS9);
        const float invSize[4] = {
            1.0f / (float)kDepthW,
            1.0f / (float)kDepthH,
            0.5f / (float)kDepthW,
            0.5f / (float)kDepthH
        };
        dev->SetPixelShaderConstantF(0, invSize, 1);
        dev->SetFVF(D3DFVF_XYZW);
        dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_ZENABLE, FALSE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0000000F);
        dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

        // Fullscreen quad in NDC [-1..+1] with 4 floats (x, y, z, w) matching D3DFVF_XYZW
        struct NDCVert { float x, y, z, w; };
        static const NDCVert quad[4] = {
            { -1.0f,  1.0f, 0.0f, 1.0f },
            {  1.0f,  1.0f, 0.0f, 1.0f },
            { -1.0f, -1.0f, 0.0f, 1.0f },
            {  1.0f, -1.0f, 0.0f, 1.0f }
        };
        drawOk = SUCCEEDED(dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(NDCVert)));
    }

    // Restore complete D3D9 state
    dev->SetPixelShaderConstantF(0, oldPSConst0, 1);
    dev->SetTexture(0, oldT0);
    if (oldT0) oldT0->Release();
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, oldMin0);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, oldMag0);
    dev->SetSamplerState(0, D3DSAMP_MIPFILTER, oldMip0);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, oldU0);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, oldV0);
    dev->SetVertexShader(oldVS); if (oldVS) oldVS->Release();
    dev->SetPixelShader(oldPS); if (oldPS) oldPS->Release();
    if (oldDecl) {
        dev->SetVertexDeclaration(oldDecl);
        oldDecl->Release();
    } else {
        dev->SetFVF(oldFVF);
    }
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, oldScissor);
    dev->SetRenderState(D3DRS_ZENABLE, oldZ);
    dev->SetRenderState(D3DRS_ZWRITEENABLE, oldZWrite);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, oldBlend);
    dev->SetRenderState(D3DRS_CULLMODE, oldCull);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, oldColorWrite);
    dev->SetRenderState(D3DRS_SRGBWRITEENABLE, oldSrgb);
    dev->SetViewport(&oldVP);
    dev->SetDepthStencilSurface(oldDS); if (oldDS) oldDS->Release();
    dev->SetRenderTarget(0, oldRT); if (oldRT) oldRT->Release();

    if (!drawOk) return;
    if (FAILED(dev->GetRenderTargetData(g_depthFreezeSurf9, g_depthFreezeSys9))) return;

    D3DLOCKED_RECT lr{};
    if (FAILED(g_depthFreezeSys9->LockRect(&lr, nullptr, D3DLOCK_READONLY))) return;

    uint8_t minD = 255, maxD = 0;
    const uint8_t* srcBits = static_cast<const uint8_t*>(lr.pBits);
    for (UINT y = 20; y < kDepthH - 20; y += 8) {
        const uint8_t* row = srcBits + (size_t)y * lr.Pitch;
        for (UINT x = 20; x < kDepthW - 20; x += 8) {
            uint8_t r = row[x * 4 + 2]; // Red channel in B8G8R8A8
            if (r < minD) minD = r;
            if (r > maxD) maxD = r;
        }
    }

    // Upload to D3D11 if depth has non-trivial variation, or on first capture
    const bool hasVar = (maxD > minD + 1) && (minD < 254);
    if (hasVar || !g_depthHasVariance) {
        D3D11_MAPPED_SUBRESOURCE ms{};
        if (SUCCEEDED(g_ctx11->Map(g_depthTex11, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
            uint8_t* dstBits = static_cast<uint8_t*>(ms.pData);
            const size_t rowBytes = (size_t)kDepthW * 4;
            for (UINT y = 0; y < kDepthH; ++y) {
                memcpy(dstBits + (size_t)y * ms.RowPitch, srcBits + (size_t)y * lr.Pitch, rowBytes);
            }
            g_ctx11->Unmap(g_depthTex11, 0);
        }
        if (hasVar) g_depthHasVariance = true;
    }

    g_depthFreezeSys9->UnlockRect();
    g_depthCapturedThisFrame = true;

    if (++g_depthLogCounter % 120 == 1) {
        Log("[DepthINTZ] 480x270 snapshot: minD=%u (%.3f) maxD=%u (%.3f) hasVariance=%d",
            (unsigned)minD, minD / 255.0f, (unsigned)maxD, maxD / 255.0f, (int)g_depthHasVariance);
    }
}

HRESULT STDMETHODCALLTYPE Hook_SetDepthStencilSurface(IDirect3DDevice9* dev, IDirect3DSurface9* pNewZStencil) {
    if (d2vr_replay::g_recActive) {
        d2vr_replay::RecEmit(d2vr_replay::OP_SetDepthStencilSurface, 0, (uint32_t)(uintptr_t)pNewZStencil);
    }
    if (pNewZStencil) {
        D3DSURFACE_DESC sd{};
        if (SUCCEEDED(pNewZStencil->GetDesc(&sd))) {
            if ((sd.Format == FOURCC_INTZ || sd.Format == FOURCC_RAWZ || sd.Format == FOURCC_DF24) &&
                sd.Width >= 640 && sd.Height >= 480 && sd.MultiSampleType == D3DMULTISAMPLE_NONE) {
                IDirect3DTexture9* tex = nullptr;
                if (SUCCEEDED(pNewZStencil->GetContainer(__uuidof(IDirect3DTexture9), reinterpret_cast<void**>(&tex))) && tex) {
                    if (g_intzTex9 != tex) {
                        if (g_intzTex9) g_intzTex9->Release();
                        g_intzTex9 = tex;
                        static int s_intzLog = 0;
                        if (s_intzLog++ < 5) {
                            Log("[DepthINTZ] Latched hardware depth texture %p (%ux%u fmt=0x%08X)",
                                tex, sd.Width, sd.Height, (unsigned)sd.Format);
                        }
                    } else {
                        tex->Release();
                    }
                }
            }
        }
    }
    return g_origSetDepthStencilSurface(dev, pNewZStencil);
}

// Evolution Engine binds INTZ to sampler s0 (SetTexture(0, intzTex)) during deferred lighting/shadows
// right after the opaque 3D geometry pass completes (see PixelShaders/C891DB74.txt & 3FF3A68C.txt).
HRESULT STDMETHODCALLTYPE Hook_SetTexture(IDirect3DDevice9* dev, DWORD Stage, IDirect3DBaseTexture9* pTexture) {
    if (d2vr_replay::g_recActive) {
        d2vr_replay::RecEmit(d2vr_replay::OP_SetTexture, 0, Stage, (uint32_t)(uintptr_t)pTexture);
    }
    if (Stage == 0 && pTexture && !g_intzTex9) {
        if (pTexture->GetType() == D3DRTYPE_TEXTURE) {
            auto* tex2d = static_cast<IDirect3DTexture9*>(pTexture);
            D3DSURFACE_DESC sd{};
            if (SUCCEEDED(tex2d->GetLevelDesc(0, &sd))) {
                if ((sd.Format == FOURCC_INTZ || sd.Format == FOURCC_RAWZ || sd.Format == FOURCC_DF24) &&
                    sd.Width >= 640 && sd.Height >= 480) {
                    tex2d->AddRef();
                    g_intzTex9 = tex2d;
                    Log("[DepthINTZ] Latched INTZ texture %p via SetTexture(0) (%ux%u fmt=0x%08X)",
                        tex2d, sd.Width, sd.Height, (unsigned)sd.Format);
                }
            }
        }
    }
    return g_origSetTexture(dev, Stage, pTexture);
}

// ---------------------------------------------------------------- D3D11 Method 2 Stereo Shader Pipeline
static bool EnsureD11StereoPipeline() {
    if (g_stereoPipelineReady) return true;
    if (!g_dev11) return false;

    const char* vsSrc =
        "struct VSOut {\n"
        "    float4 pos : SV_POSITION;\n"
        "    float2 uv  : TEXCOORD0;\n"
        "};\n"
        "VSOut main(uint id : SV_VertexID) {\n"
        "    VSOut o;\n"
        "    o.uv = float2((id << 1) & 2, id & 2);\n"
        "    o.pos = float4(o.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);\n"
        "    return o;\n"
        "}\n";

    // Method 2 Pixel Shader:
    // 1. Maps each eye's asymmetric OpenXR frustum (fovTan) through tangent space so straight-ahead (0,0)
    //    samples the exact center (0.5, 0.5) of the game frame in BOTH eyes (0.000 deg divergence).
    // 2. In Mode 0 & 1: Immersive Wrap-Around VR ("Obraz Dookoła") with cylindrical/spherical curvature
    //    surrounds the player's head without black cinema borders.
    // 3. In Mode 2: Framed 3D Cinema Screen.
    // 4. Applies calibrated horizontal stereo shift between Left Eye (-disp) and Right Eye (+disp)
    //    combining base stereo convergence + INTZ depth + comfort depth profile.
    const char* psSrc =
        "Texture2D colorTex : register(t0);\n"
        "Texture2D depthTex : register(t1);\n"
        "SamplerState linSamp : register(s0);\n"
        "cbuffer StereoCB : register(b0) {\n"
        "    float4 fovTan;      // x=tanLeft, y=tanRight, z=tanUp, w=tanDown\n"
        "    float4 projParams;  // x=invTwoHalfTanX, y=invTwoHalfTanY, z=eyeSign (-1 left, +1 right), w=stereoStrength\n"
        "    float4 stereoCfg;   // x=baseShift, y=depthScale, z=maxDisp, w=hasRealDepth\n"
        "    float4 extraCfg;    // x=convDepth, y=kz, z=depthViz, w=posOffsetX\n"
        "    float4 posCfg;      // x=posOffsetY, y=viewMode (0=Immersive 150, 1=Ultra-Wide 180, 2=Cinema), z=unused, w=unused\n"
        "};\n"
        "struct PSIn {\n"
        "    float4 pos : SV_POSITION;\n"
        "    float2 uv  : TEXCOORD0;\n"
        "};\n"
        "float4 main(PSIn i) : SV_Target {\n"
        "    if (posCfg.y > 2.5) {\n"
        "        // True-stereo blit: image already rendered per eye, pass through 1:1\n"
        "        return colorTex.SampleLevel(linSamp, i.uv, 0);\n"
        "    }\n"
        "    float tx = lerp(fovTan.x, fovTan.y, i.uv.x);\n"
        "    float ty = lerp(fovTan.z, fovTan.w, i.uv.y);\n"
        "    float2 baseUV;\n"
        "    if (posCfg.y < 1.5) {\n"
        "        // Immersive Wrap-Around VR (\"Obraz Dookoła\")\n"
        "        // Cylindrical & spherical angular projection wraps the game seamlessly around the player\n"
        "        float angX = atan(tx);\n"
        "        float angY = atan2(ty, sqrt(1.0 + tx * tx));\n"
        "        baseUV.x = 0.5 + angX * projParams.x;\n"
        "        baseUV.y = 0.5 - angY * projParams.y;\n"
        "        baseUV.x = clamp(baseUV.x, 0.001, 0.999);\n"
        "        baseUV.y = clamp(baseUV.y, 0.001, 0.999);\n"
        "    } else {\n"
        "        // Framed 3D Cinema Screen\n"
        "        baseUV.x = 0.5 + tx * projParams.x;\n"
        "        baseUV.y = 0.5 - ty * projParams.y;\n"
        "        if (baseUV.x < 0.0 || baseUV.x > 1.0 || baseUV.y < 0.0 || baseUV.y > 1.0) {\n"
        "            return float4(0.0, 0.0, 0.0, 1.0);\n"
        "        }\n"
        "    }\n"
        "    // Comfort depth profile (foreground lower/center hands & weapon vs mid/background)\n"
        "    float vProx = saturate((baseUV.y - 0.12) * 0.90);\n"
        "    float cx = (baseUV.x - 0.5) * 2.0;\n"
        "    float cy = (baseUV.y - 0.5) * 2.0;\n"
        "    float domeProx = saturate(1.0 - (cx * cx + cy * cy) * 0.32);\n"
        "    float synthProx = 0.55 * vProx + 0.45 * domeProx;\n"
        "    float prox = synthProx;\n"
        "    if (stereoCfg.w > 0.5) {\n"
        "        float d0 = depthTex.SampleLevel(linSamp, baseUV, 0).r;\n"
        "        float dL = depthTex.SampleLevel(linSamp, baseUV + float2(-0.003, 0.0), 0).r;\n"
        "        float dR = depthTex.SampleLevel(linSamp, baseUV + float2( 0.003, 0.0), 0).r;\n"
        "        float dMin = min(d0, min(dL, dR));\n"
        "        float oneMinusD = max(0.0, 1.0 - dMin);\n"
        "        float realProx = oneMinusD / (oneMinusD + extraCfg.y);\n"
        "        prox = lerp(synthProx, realProx, 0.75);\n"
        "    }\n"
        "    if (extraCfg.z > 0.5) {\n"
        "        return float4(prox, prox, prox, 1.0);\n"
        "    }\n"
        "    float edgeFeather = smoothstep(0.0, 0.04, baseUV.x) * smoothstep(0.0, 0.04, 1.0 - baseUV.x);\n"
        "    float rawDisp = (stereoCfg.x + stereoCfg.y * (prox - extraCfg.x)) * projParams.w * edgeFeather;\n"
        "    float disp = clamp(rawDisp, -stereoCfg.z, stereoCfg.z) * projParams.z;\n"
        "    float parallaxWeight = 0.35 + 0.65 * prox;\n"
        "    float2 sampleUV;\n"
        "    sampleUV.x = clamp(baseUV.x + disp + extraCfg.w * parallaxWeight, 0.001, 0.999);\n"
        "    sampleUV.y = clamp(baseUV.y + posCfg.x * parallaxWeight, 0.001, 0.999);\n"
        "    return colorTex.SampleLevel(linSamp, sampleUV, 0);\n"
        "}\n";

    ID3DBlob* vsBlob = CompileShaderBlob(vsSrc, "main", "vs_4_0");
    ID3DBlob* psBlob = CompileShaderBlob(psSrc, "main", "ps_4_0");
    if (!vsBlob || !psBlob) {
        if (vsBlob) vsBlob->Release();
        if (psBlob) psBlob->Release();
        return false;
    }

    HRESULT hr1 = g_dev11->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_stereoVS11);
    HRESULT hr2 = g_dev11->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_stereoPS11);
    vsBlob->Release();
    psBlob->Release();
    if (FAILED(hr1) || FAILED(hr2)) return false;

    D3D11_BUFFER_DESC cbd{};
    cbd.ByteWidth = sizeof(StereoCBData);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(g_dev11->CreateBuffer(&cbd, nullptr, &g_stereoCB11))) return false;

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(g_dev11->CreateSamplerState(&sd, &g_stereoSamp11))) return false;

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    if (FAILED(g_dev11->CreateRasterizerState(&rd, &g_stereoRS11))) return false;

    g_stereoPipelineReady = true;
    Log("D3D11 Method 2 Stereo Shader Pipeline initialized OK");
    return true;
}

static bool RenderStereoEyeD11(
    int eyeIdx,
    const XrFovf& eyeFov,
    float invTwoHalfTanX,
    float invTwoHalfTanY,
    float eyeSign,
    float posOffsetX,
    float posOffsetY)
{
    if (!EnsureD11StereoPipeline() || !g_eyeRTV[eyeIdx] || !g_uploadSRV) return false;
    EnsureDepthTexture11();

    D3D11_MAPPED_SUBRESOURCE ms{};
    if (FAILED(g_ctx11->Map(g_stereoCB11, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) return false;
    auto* cb = static_cast<StereoCBData*>(ms.pData);
    cb->fovTan[0] = std::tan(eyeFov.angleLeft);
    cb->fovTan[1] = std::tan(eyeFov.angleRight);
    cb->fovTan[2] = std::tan(eyeFov.angleUp);
    cb->fovTan[3] = std::tan(eyeFov.angleDown);

    cb->projParams[0] = invTwoHalfTanX;
    cb->projParams[1] = invTwoHalfTanY;
    cb->projParams[2] = eyeSign;
    cb->projParams[3] = g_stereoStrength;

    cb->stereoCfg[0] = g_stereoBaseShift;
    cb->stereoCfg[1] = g_stereoDepthScale;
    cb->stereoCfg[2] = g_stereoMaxDisp;
    cb->stereoCfg[3] = (g_depthHasVariance && g_depthSRV11) ? 1.0f : 0.0f;

    cb->extraCfg[0] = 0.0f;   // convDepth = 0 (far plane has baseShift, near adds +depthScale)
    cb->extraCfg[1] = 0.015f; // kz hyperbolic depth linearization constant
    cb->extraCfg[2] = (float)g_depthViz;
    cb->extraCfg[3] = posOffsetX;

    cb->posCfg[0] = posOffsetY;
    cb->posCfg[1] = (g_blitPass != 0) ? 3.0f : (float)g_trackingSubMode;
    cb->posCfg[2] = 0.0f;
    cb->posCfg[3] = 0.0f;
    g_ctx11->Unmap(g_stereoCB11, 0);

    g_ctx11->OMSetRenderTargets(1, &g_eyeRTV[eyeIdx], nullptr);
    D3D11_VIEWPORT vp{};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = (float)g_scW;
    vp.Height = (float)g_scH;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    g_ctx11->RSSetViewports(1, &vp);
    g_ctx11->RSSetState(g_stereoRS11);

    g_ctx11->IASetInputLayout(nullptr);
    g_ctx11->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_ctx11->VSSetShader(g_stereoVS11, nullptr, 0);
    g_ctx11->PSSetShader(g_stereoPS11, nullptr, 0);
    g_ctx11->PSSetConstantBuffers(0, 1, &g_stereoCB11);
    g_ctx11->PSSetSamplers(0, 1, &g_stereoSamp11);

    ID3D11ShaderResourceView* srvs[2] = { g_uploadSRV, g_depthSRV11 };
    g_ctx11->PSSetShaderResources(0, 2, srvs);

    g_ctx11->Draw(3, 0);

    ID3D11ShaderResourceView* nullSrvs[2] = { nullptr, nullptr };
    g_ctx11->PSSetShaderResources(0, 2, nullSrvs);
    ID3D11RenderTargetView* nullRtv = nullptr;
    g_ctx11->OMSetRenderTargets(1, &nullRtv, nullptr);
    return true;
}

// ---------------------------------------------------------------- Native engine FOV override
// RE results (thunk disassembly, thunk_disasm.txt):
//   Lua thunks are cdecl lua_CFunction(lua_State*), NOT avatar thiscall methods.
//   SetView 0x00DC4354 loads L from [esp+0x10] after sub esp,8 / push esi;
//   0x00DC4421 returns with C3. ECX is not an avatar argument.
//   The real engine methods are VIRTUAL and were located inside the thunks:
//     BaseAvatar::GetFOV()              -> vtable + 0x270  (float __thiscall(void*))
//     BaseAvatar::SetBaseFovOverride(f) -> vtable + 0xE4   (void  __thiscall(void*, float))
//   The Lua binding table lives at a fixed address (no ASLR): the (name, func) pair for
//   BaseAvatar::SetView is at 0x10b9800 / 0x10b9804 (.data = writable).
//
// SAFETY: virtual methods are ONLY called on objects the engine itself passes to us through
// the hooked thunk (a genuine BaseAvatar vtable). Memory scanning for candidates is NOT safe:
// a wrong class means a different `ret N` convention and stack corruption (we learned it the
// hard way).
typedef int(__cdecl* PFN_LuaMethod)(void* luaState);
typedef float(__thiscall* PFN_VirtGetFov)(void* self);
typedef void(__thiscall* PFN_VirtSetBaseFov)(void* self, float fov);

static void* g_playerAvatar = nullptr;
static void* g_origLuaSetView = nullptr;
static int g_avatarHookInstalled = 0;

// most-frequent caller wins: the player avatar calls SetView every frame, NPCs rarely
static struct { void* ptr; int count; } g_avatarSeen[8] = {};

static bool IsExeText(void* p) {
    uintptr_t v = (uintptr_t)p;
    return v >= 0x00401000 && v < 0x00EEA730;
}
static bool IsExeData(void* p) {
    uintptr_t v = (uintptr_t)p;
    return v >= 0x00EEB000 && v < 0x01124000;
}

static void* VTableFn(void* obj, size_t byteOffset) {
    if (!obj) return nullptr;
    uintptr_t vt = 0;
    __try { vt = *(uintptr_t*)obj; } __except (1) { return nullptr; }
    if (!IsExeData((void*)vt)) return nullptr;
    void* fn = nullptr;
    __try { fn = *(void**)(vt + byteOffset); } __except (1) { return nullptr; }
    if (!IsExeText(fn)) return nullptr;
    return fn;
}

static int __cdecl Hook_LuaSetView(void* luaState) {
    // Observing a script call does not identify the player. The thunk extracts
    // its avatar userdata internally; ECX at entry is unspecified.
    static LONG calls = 0;
    if (InterlockedIncrement(&calls) <= 3)
        Log("[FovRE] Lua SetView called; native avatar discovery still unresolved");
    return g_origLuaSetView ? ((PFN_LuaMethod)g_origLuaSetView)(luaState) : 0;
}

static float ProbeAvatarFov(void* cand) {
    void* fn = VTableFn(cand, 0x270);
    if (!fn) return -1.0f;
    float fov = -1.0f;
    __try {
        fov = ((PFN_VirtGetFov)fn)(cand);
    } __except (1) {
        fov = -1.0f;
    }
    return fov;
}

static void TryApplyEngineFovOverride() {
    if (g_fovOverrideDeg <= 0.0f) return;
    static bool s_done = false;
    if (s_done) return;

    // 1) hook the Lua SetView thunk table entry (fixed .data address, writable)
    if (!g_avatarHookInstalled) {
        void** slot = (void**)0x10B9804;
        if (IsExeData((void*)0x10B9800)) {
            g_origLuaSetView = *slot;
            if (IsExeText(g_origLuaSetView)) {
                *slot = (void*)&Hook_LuaSetView;
                g_avatarHookInstalled = 1;
                Log("[FovRE] Lua SetView thunk hooked (orig=%p)", g_origLuaSetView);
            } else {
                Log("[FovRE] SetView thunk slot does not contain code (%p) - hook skipped", g_origLuaSetView);
                s_done = true;
                return;
            }
        }
    }

    // 2) pick the avatar the engine uses most (the player's camera calls SetView every frame)
    void* best = nullptr;
    int bestCount = 0;
    for (auto& e : g_avatarSeen) {
        if (e.ptr && e.count > bestCount) { best = e.ptr; bestCount = e.count; }
    }
    if (!best) {
        Log("[FovRE] waiting for BaseAvatar::SetView calls (avatar not seen yet)");
        return;
    }

    // 3) apply the override through the virtual methods (genuine vtable = safe) and verify
    void* setFn = VTableFn(best, 0xE4);
    if (!setFn) {
        Log("[FovRE] SetBaseFovOverride slot (vtable+0xE4) not code for avatar %p - disabled", best);
        s_done = true;
        return;
    }
    __try {
        float oldFov = ProbeAvatarFov(best);
        ((PFN_VirtSetBaseFov)setFn)(best, g_fovOverrideDeg);
        float newFov = ProbeAvatarFov(best);
        Log("[FovRE] avatar %p (SetView x%d): SetBaseFovOverride(%.1f) GetFOV %.2f -> %.2f (vertical deg)",
            best, bestCount, g_fovOverrideDeg, oldFov, newFov);
        if (newFov > 10.0f && newFov < 175.0f) {
            g_playerAvatar = best;
            s_done = true;
        }
    } __except (1) {
        Log("[FovRE] exception during SetBaseFovOverride - disabled");
        s_done = true;
    }
}

// ---------------------------------------------------------------- Eye frame dumps (offline stereo validation)
// Writes both eye images to test-output/frame_NNN_eye0/1.bmp (F12). A Python pass then
// measures the horizontal disparity map: real stereo must show 10-80px near objects with
// disparity decreasing toward the horizon (zero at infinity).
static bool DumpTextureToBmp(ID3D11Texture2D* tex, const char* path) {
    if (!tex || !g_dev11 || !g_ctx11) return false;
    D3D11_TEXTURE2D_DESC td{};
    tex->GetDesc(&td);
    D3D11_TEXTURE2D_DESC sd = td;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.BindFlags = 0;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    sd.MiscFlags = 0;
    ID3D11Texture2D* staging = nullptr;
    if (FAILED(g_dev11->CreateTexture2D(&sd, nullptr, &staging)) || !staging) return false;
    g_ctx11->CopyResource(staging, tex);
    D3D11_MAPPED_SUBRESOURCE ms{};
    bool ok = false;
    if (SUCCEEDED(g_ctx11->Map(staging, 0, D3D11_MAP_READ, 0, &ms))) {
        FILE* f = nullptr;
        if (fopen_s(&f, path, "wb") == 0 && f) {
            const UINT w = td.Width, h = td.Height;
            const UINT rowBytes = w * 3;
            const UINT pad = (4 - (rowBytes % 4)) % 4;
            const UINT imgBytes = (rowBytes + pad) * h;
            BITMAPFILEHEADER fh{};
            fh.bfType = 0x4D42;
            fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
            fh.bfSize = fh.bfOffBits + imgBytes;
            BITMAPINFOHEADER ih{};
            ih.biSize = sizeof(BITMAPINFOHEADER);
            ih.biWidth = w;
            ih.biHeight = h;
            ih.biPlanes = 1;
            ih.biBitCount = 24;
            ih.biCompression = BI_RGB;
            ih.biSizeImage = imgBytes;
            fwrite(&fh, 1, sizeof(fh), f);
            fwrite(&ih, 1, sizeof(ih), f);
            std::vector<uint8_t> row(rowBytes + pad, 0);
            for (UINT y = 0; y < h; ++y) {
                const uint8_t* src = (const uint8_t*)ms.pData + (size_t)(h - 1 - y) * ms.RowPitch;
                for (UINT x = 0; x < w; ++x) {
                    row[x * 3 + 0] = src[x * 4 + 0];
                    row[x * 3 + 1] = src[x * 4 + 1];
                    row[x * 3 + 2] = src[x * 4 + 2];
                }
                fwrite(row.data(), 1, rowBytes + pad, f);
            }
            fclose(f);
            ok = true;
        }
        g_ctx11->Unmap(staging, 0);
    }
    staging->Release();
    return ok;
}

static void DumpEyeFramesIfRequested() {
    if (g_dumpEyeFrames <= 0) return;
    if ((int)g_trueStereoFrames < g_dumpDelay) return;
    // The game runs two passes per frame (a small UI pass + the real 3D pass).
    // Only dump pairs built from the real 3D pass (record-rich windows).
    if (d2vr_replay::g_rec.size() < 1000) return;
    --g_dumpEyeFrames;
    static int s_frameIdx = 0;
    char p0[MAX_PATH], p1[MAX_PATH];
    CreateDirectoryA("test-output", nullptr);
    sprintf_s(p0, "test-output\\frame_%03d_eye0.bmp", s_frameIdx);
    sprintf_s(p1, "test-output\\frame_%03d_eye1.bmp", s_frameIdx);
    bool a = DumpTextureToBmp(g_eyeCache[0], p0);
    bool b = DumpTextureToBmp(g_eyeCache[1], p1);
    Log("[EyeDump] %s + %s (eye0=%d eye1=%d)", p0, p1, (int)a, (int)b);
    ++s_frameIdx;
}

// ---------------------------------------------------------------- Zero-copy capture (D3D9Ex shared texture)
// The old path (GetRenderTargetData -> sysmem -> memcpy -> D3D11) stalls the CPU waiting
// for the GPU twice per frame (GPU idle, FPS low). D3D9Ex shared textures let D3D11 read
// the backbuffer copy with zero CPU involvement.
IDirect3DTexture9* g_sharedTex9 = nullptr;
IDirect3DSurface9* g_sharedSurf9 = nullptr;
ID3D11Texture2D* g_sharedTex11 = nullptr;
ID3D11ShaderResourceView* g_sharedSRV11 = nullptr;
HANDLE g_sharedHandle = nullptr;
UINT g_sharedW = 0, g_sharedH = 0;
int g_zeroCopyFailures = 0;

static bool EnsureSharedCapture(IDirect3DDevice9* dev) {
    if (!g_zeroCopyEnabled) return false;
    if (g_sharedSurf9 && g_sharedSRV11) return true;
    if (!dev || !g_dev11 || g_zeroCopyFailures > 2) return false;

    // shared texture must match the backbuffer size/format
    IDirect3DSurface9* bb = nullptr;
    if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) return false;
    D3DSURFACE_DESC bd{};
    bb->GetDesc(&bd);
    bb->Release();

    if (!g_sharedTex9) {
        HANDLE shared = nullptr;
        HRESULT hr = dev->CreateTexture(bd.Width, bd.Height, 1, D3DUSAGE_RENDERTARGET,
                                        bd.Format, D3DPOOL_DEFAULT, &g_sharedTex9, &shared);
        if (FAILED(hr) || !g_sharedTex9) {
            ++g_zeroCopyFailures;
            Log("[ZeroCopy] CreateTexture(shared) failed hr=0x%08lX - using CPU capture", hr);
            return false;
        }
        g_sharedHandle = shared;
        g_sharedTex9->GetSurfaceLevel(0, &g_sharedSurf9);
        g_sharedW = bd.Width;
        g_sharedH = bd.Height;
    }
    if (!g_sharedTex11) {
        HRESULT hr = g_dev11->OpenSharedResource(g_sharedHandle, __uuidof(ID3D11Texture2D),
                                                 reinterpret_cast<void**>(&g_sharedTex11));
        if (FAILED(hr) || !g_sharedTex11) {
            ++g_zeroCopyFailures;
            Log("[ZeroCopy] OpenSharedResource failed hr=0x%08lX - using CPU capture", hr);
            return false;
        }
        g_dev11->CreateShaderResourceView(g_sharedTex11, nullptr, &g_sharedSRV11);
        Log("[ZeroCopy] shared capture texture ready %ux%u (D3D9Ex -> D3D11)", g_sharedW, g_sharedH);
    }
    return g_sharedSurf9 && g_sharedSRV11;
}

static bool CaptureBackbufferToShared(IDirect3DDevice9* dev) {
    if (!g_sharedSurf9) return false;
    IDirect3DSurface9* src = nullptr;
    // Darkness II renders offscreen; check active RenderTarget0 first
    if (FAILED(dev->GetRenderTarget(0, &src)) || !src) {
        if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &src)) || !src) return false;
    }
    HRESULT hr = dev->StretchRect(src, nullptr, g_sharedSurf9, nullptr, D3DTEXF_NONE);
    src->Release();
    if (FAILED(hr)) return false;

    // GPU fence: D3D9Ex and D3D11 run on separate queues; flush D3D9 batch before D3D11 reads
    static IDirect3DQuery9* s_flushQuery = nullptr;
    if (!s_flushQuery) {
        dev->CreateQuery(D3DQUERYTYPE_EVENT, &s_flushQuery);
    }
    if (s_flushQuery) {
        s_flushQuery->Issue(D3DISSUE_END);
        while (s_flushQuery->GetData(nullptr, 0, D3DGETDATA_FLUSH) == S_FALSE) {}
    }
    return true;
}

// ---------------------------------------------------------------- True stereo eye pair (live pass + replay pass)
// Eye 0 is the live game pass (already in the backbuffer), eye 1 is produced by
// replaying the recorded device call stream with the opposite eye shift.
bool CopyFrame(IDirect3DDevice9* dev);
bool EnsureSwapchains(UINT w, UINT h);

// Keep the right output across scene segments. Restoring the live (left)
// pixels alone makes a later UI-only replay inherit the wrong eye's world.
struct EyeColorHistory {
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* pixels = nullptr;
    void Release() {
        if (pixels) pixels->Release();
        if (target) target->Release();
        pixels = nullptr; target = nullptr;
    }
    bool Restore(IDirect3DDevice9* dev, IDirect3DSurface9* output) {
        if (target != output) { Release(); return true; }
        return !pixels || SUCCEEDED(dev->StretchRect(pixels, nullptr, output, nullptr, D3DTEXF_NONE));
    }
    bool Save(IDirect3DDevice9* dev, IDirect3DSurface9* output) {
        if (!output) { Release(); return false; }
        if (target != output) Release();
        if (!pixels) {
            D3DSURFACE_DESC d{};
            if (FAILED(output->GetDesc(&d)) || FAILED(dev->CreateRenderTarget(
                d.Width, d.Height, d.Format, d.MultiSampleType, d.MultiSampleQuality,
                FALSE, &pixels, nullptr))) return false;
            target = output; target->AddRef();
        }
        if (FAILED(dev->StretchRect(output, nullptr, pixels, nullptr, D3DTEXF_NONE))) {
            Release(); return false;
        }
        return true;
    }
};
static EyeColorHistory g_rightColorHistory;
static IDirect3DSurface9* g_liveColorBackup = nullptr;
static bool BackupLiveColor(IDirect3DDevice9* dev, IDirect3DSurface9* source) {
    if (!source) return false;
    D3DSURFACE_DESC wanted{}, current{};
    if (FAILED(source->GetDesc(&wanted))) return false;
    if (g_liveColorBackup) {
        g_liveColorBackup->GetDesc(&current);
        if (current.Width != wanted.Width || current.Height != wanted.Height ||
            current.Format != wanted.Format || current.MultiSampleType != wanted.MultiSampleType ||
            current.MultiSampleQuality != wanted.MultiSampleQuality) {
            g_liveColorBackup->Release(); g_liveColorBackup = nullptr;
        }
    }
    if (!g_liveColorBackup && FAILED(dev->CreateRenderTarget(wanted.Width, wanted.Height,
            wanted.Format, wanted.MultiSampleType, wanted.MultiSampleQuality,
            FALSE, &g_liveColorBackup, nullptr))) return false;
    return SUCCEEDED(dev->StretchRect(source, nullptr, g_liveColorBackup, nullptr, D3DTEXF_NONE));
}

static void RenderEyePairFromReplay(IDirect3DDevice9* dev) {
    static int s_logged = 0;
    const size_t recCount = d2vr_replay::g_rec.size();
    const size_t recBytes = d2vr_replay::g_recData.size();

    UINT targetW = g_steamVrRecW ? g_steamVrRecW : (g_srcW ? g_srcW : 1920);
    UINT targetH = g_steamVrRecH ? g_steamVrRecH : (g_srcH ? g_srcH : 1080);
    if (!EnsureSwapchains(targetW, targetH)) {
        d2vr_replay::g_recActive = false;
        return;
    }
    const bool zeroCopy = EnsureSharedCapture(dev);

    g_blitPass = 1;
    bool eye0Ok = zeroCopy ? CaptureBackbufferToShared(dev) : CopyFrame(dev);
    {
        ID3D11ShaderResourceView* saved = g_uploadSRV;
        if (zeroCopy) g_uploadSRV = g_sharedSRV11;
        if (eye0Ok) eye0Ok = RenderStereoEyeD11(0, g_eyeFov[0], 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
        g_uploadSRV = saved;
    }

    d2vr_replay::StereoParams sp1 = MakeEyeParams(1);
    if (g_stereoMode == 2) {
        sp1.separation = 0.0f;
        sp1.transX = 0.0f;
        sp1.transY = 0.0f;
    }
    d2vr_replay::g_recActive = false;
    d2vr_replay::DeviceSnapshot liveEnd;
    if (!liveEnd.Capture(dev)) { g_blitPass = 0; g_eyePairReady = false; return; }
    // Replay overwrites the live output. Preserve it for later UI/scene segments
    // that reuse the existing color image without redrawing world geometry.
    if (!BackupLiveColor(dev, liveEnd.targets[0])) {
        liveEnd.Release(); g_blitPass = 0; g_eyePairReady = false; return;
    }
    const bool historyOk = g_rightColorHistory.Restore(dev, liveEnd.targets[0]);
    d2vr_replay::g_frameStart.Apply(dev);
    d2vr_replay::ReplayStream(dev, 1, sp1);

    bool eye1Ok = historyOk && g_rightColorHistory.Save(dev, liveEnd.targets[0]);
    if (eye1Ok) eye1Ok = zeroCopy ? CaptureBackbufferToShared(dev) : CopyFrame(dev);
    {
        ID3D11ShaderResourceView* saved = g_uploadSRV;
        if (zeroCopy) g_uploadSRV = g_sharedSRV11;
        if (eye1Ok) eye1Ok = RenderStereoEyeD11(1, g_eyeFov[1], 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
        g_uploadSRV = saved;
    }
    g_blitPass = 0;
    if (FAILED(dev->StretchRect(g_liveColorBackup, nullptr, liveEnd.targets[0], nullptr, D3DTEXF_NONE)))
        eye1Ok = false;
    liveEnd.Apply(dev);
    liveEnd.Release();

    DumpEyeFramesIfRequested();

    d2vr_replay::g_recActive = false;
    g_eyePairReady = eye0Ok && eye1Ok;
    ++g_trueStereoFrames;

    if (s_logged < 10 || (g_trueStereoFrames % 600) == 0) {
        ++s_logged;
        Log("[TrueStereo] frame %d: records=%zu data=%zuKB overflow=%d eye0=%d eye1=%d shifts=%ld vsKnown=%zu zeroCopy=%d testClear=%d",
            (int)g_trueStereoFrames, recCount, recBytes / 1024,
            (int)d2vr_replay::g_recOverflow, (int)eye0Ok, (int)eye1Ok,
            (long)InterlockedExchange(&d2vr_replay::g_shiftCount, 0),
            d2vr_replay::g_vsInfo.size(), (int)zeroCopy, (int)g_replayTestClear);
        const uint64_t* c = d2vr_replay::g_recCounts;
        Log("[TrueStereo] ops: Begin=%llu End=%llu Clear=%llu RT=%llu DSS=%llu Draw=%llu DIP=%llu DPUP=%llu DIPUP=%llu VS=%llu VSCF=%llu Tex=%llu RS=%llu TSS=%llu Smp=%llu VP=%llu SB=%llu",
            c[3], c[4], c[5], c[1], c[2], c[13], c[14], c[15], c[16], c[19], c[20], c[9], c[8], c[10], c[11], c[7], c[36]);
        Log("[TrueStereo] cfg: IPD=%.3f widenX=%.2f widenY=%.2f autoFov=%d shiftMode=%d polarity=%d",
            g_stereoIPD, g_fovWidenX, g_fovWidenY, g_autoFovWiden, g_stereoShiftMode, g_stereoPolarity);
    }
    if (d2vr_replay::g_recOverflow && s_logged < 20) {
        Log("[TrueStereo] WARNING: record overflow - replay skipped this frame");
    }
}

// ---------------------------------------------------------------- XInput & OpenXR Gamepad
HWND g_gameHwnd = nullptr;
XINPUT_STATE g_cachedXrPad{};
XINPUT_GAMEPAD g_lastGamepadState{};
volatile LONG g_cachedXrPadValid = 0;

XrActionSet g_padActionSet = XR_NULL_HANDLE;
XrAction g_actLeftStick = XR_NULL_HANDLE;
XrAction g_actRightStick = XR_NULL_HANDLE;
XrAction g_actLeftTrig = XR_NULL_HANDLE;
XrAction g_actRightTrig = XR_NULL_HANDLE;
XrAction g_actA = XR_NULL_HANDLE;
XrAction g_actB = XR_NULL_HANDLE;
XrAction g_actX = XR_NULL_HANDLE;
XrAction g_actY = XR_NULL_HANDLE;
XrAction g_actMenu = XR_NULL_HANDLE;
XrAction g_actView = XR_NULL_HANDLE;
XrAction g_actStickL = XR_NULL_HANDLE;
XrAction g_actStickR = XR_NULL_HANDLE;
XrPath g_pathHandLeft = XR_NULL_PATH;
XrPath g_pathHandRight = XR_NULL_PATH;
bool g_padActionsReady = false;
DWORD g_padPacket = 1;
volatile LONG g_padFromXrLogged = 0;

static XrPath XrPathFrom(const char* str) {
    XrPath p = XR_NULL_PATH;
    if (g_xrInstance) xrStringToPath(g_xrInstance, str, &p);
    return p;
}

static bool CreatePadAction(XrActionSet set, const char* name, XrActionType type, XrPath sub,
                            XrAction* out) {
    XrActionCreateInfo ci{ XR_TYPE_ACTION_CREATE_INFO };
    strcpy_s(ci.actionName, name);
    strcpy_s(ci.localizedActionName, name);
    ci.actionType = type;
    ci.countSubactionPaths = 1;
    ci.subactionPaths = &sub;
    return XR_SUCCEEDED(xrCreateAction(set, &ci, out));
}

static bool SuggestBindings(const char* profile, const XrActionSuggestedBinding* binds,
                            uint32_t count) {
    XrInteractionProfileSuggestedBinding ipb{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
    ipb.interactionProfile = XrPathFrom(profile);
    ipb.countSuggestedBindings = count;
    ipb.suggestedBindings = binds;
    XrResult r = xrSuggestInteractionProfileBindings(g_xrInstance, &ipb);
    if (XR_FAILED(r)) {
        Log("SuggestBindings '%s' failed (%d)", profile, (int)r);
        return false;
    }
    Log("SuggestBindings '%s' OK (%u bindings)", profile, count);
    return true;
}

bool InitXrGamepadActions() {
    if (g_padActionsReady || !g_xrInstance || !g_xrSession) return g_padActionsReady;

    g_pathHandLeft = XrPathFrom("/user/hand/left");
    g_pathHandRight = XrPathFrom("/user/hand/right");

    XrActionSetCreateInfo asci{ XR_TYPE_ACTION_SET_CREATE_INFO };
    strcpy_s(asci.actionSetName, "darkness_pad");
    strcpy_s(asci.localizedActionSetName, "Gamepad");
    asci.priority = 0;
    if (XR_FAILED(xrCreateActionSet(g_xrInstance, &asci, &g_padActionSet))) return false;

    if (!CreatePadAction(g_padActionSet, "left_stick", XR_ACTION_TYPE_VECTOR2F_INPUT, g_pathHandLeft,
                         &g_actLeftStick) ||
        !CreatePadAction(g_padActionSet, "right_stick", XR_ACTION_TYPE_VECTOR2F_INPUT, g_pathHandRight,
                         &g_actRightStick) ||
        !CreatePadAction(g_padActionSet, "left_trigger", XR_ACTION_TYPE_FLOAT_INPUT, g_pathHandLeft,
                         &g_actLeftTrig) ||
        !CreatePadAction(g_padActionSet, "right_trigger", XR_ACTION_TYPE_FLOAT_INPUT, g_pathHandRight,
                         &g_actRightTrig) ||
        !CreatePadAction(g_padActionSet, "a", XR_ACTION_TYPE_BOOLEAN_INPUT, g_pathHandRight, &g_actA) ||
        !CreatePadAction(g_padActionSet, "b", XR_ACTION_TYPE_BOOLEAN_INPUT, g_pathHandRight, &g_actB) ||
        !CreatePadAction(g_padActionSet, "x", XR_ACTION_TYPE_BOOLEAN_INPUT, g_pathHandLeft, &g_actX) ||
        !CreatePadAction(g_padActionSet, "y", XR_ACTION_TYPE_BOOLEAN_INPUT, g_pathHandLeft, &g_actY) ||
        !CreatePadAction(g_padActionSet, "menu", XR_ACTION_TYPE_BOOLEAN_INPUT, g_pathHandLeft,
                         &g_actMenu) ||
        !CreatePadAction(g_padActionSet, "view", XR_ACTION_TYPE_BOOLEAN_INPUT, g_pathHandLeft,
                         &g_actView) ||
        !CreatePadAction(g_padActionSet, "stick_l", XR_ACTION_TYPE_BOOLEAN_INPUT, g_pathHandLeft,
                         &g_actStickL) ||
        !CreatePadAction(g_padActionSet, "stick_r", XR_ACTION_TYPE_BOOLEAN_INPUT, g_pathHandRight,
                         &g_actStickR)) {
        return false;
    }

    const char* touch = "/interaction_profiles/oculus/touch_controller";
    XrActionSuggestedBinding touchB[] = {
        { g_actLeftStick,  XrPathFrom("/user/hand/left/input/thumbstick") },
        { g_actRightStick, XrPathFrom("/user/hand/right/input/thumbstick") },
        { g_actLeftTrig,   XrPathFrom("/user/hand/left/input/trigger/value") },
        { g_actRightTrig,  XrPathFrom("/user/hand/right/input/trigger/value") },
        { g_actA,          XrPathFrom("/user/hand/right/input/a/click") },
        { g_actB,          XrPathFrom("/user/hand/right/input/b/click") },
        { g_actX,          XrPathFrom("/user/hand/left/input/x/click") },
        { g_actY,          XrPathFrom("/user/hand/left/input/y/click") },
        { g_actMenu,       XrPathFrom("/user/hand/left/input/menu/click") },
        { g_actStickL,     XrPathFrom("/user/hand/left/input/thumbstick/click") },
        { g_actStickR,     XrPathFrom("/user/hand/right/input/thumbstick/click") },
    };
    SuggestBindings(touch, touchB, static_cast<uint32_t>(sizeof(touchB) / sizeof(touchB[0])));

    const char* index = "/interaction_profiles/valve/index_controller";
    XrActionSuggestedBinding indexB[] = {
        { g_actLeftStick,  XrPathFrom("/user/hand/left/input/thumbstick") },
        { g_actRightStick, XrPathFrom("/user/hand/right/input/thumbstick") },
        { g_actLeftTrig,   XrPathFrom("/user/hand/left/input/trigger/value") },
        { g_actRightTrig,  XrPathFrom("/user/hand/right/input/trigger/value") },
        { g_actA,          XrPathFrom("/user/hand/right/input/a/click") },
        { g_actB,          XrPathFrom("/user/hand/right/input/b/click") },
        { g_actX,          XrPathFrom("/user/hand/left/input/a/click") },
        { g_actY,          XrPathFrom("/user/hand/left/input/b/click") },
        { g_actStickL,     XrPathFrom("/user/hand/left/input/thumbstick/click") },
        { g_actStickR,     XrPathFrom("/user/hand/right/input/thumbstick/click") },
    };
    SuggestBindings(index, indexB, static_cast<uint32_t>(sizeof(indexB) / sizeof(indexB[0])));

    const char* wmr = "/interaction_profiles/hp/mixed_reality_controller";
    SuggestBindings(wmr, touchB, static_cast<uint32_t>(sizeof(touchB) / sizeof(touchB[0])));

    const char* simple = "/interaction_profiles/khr/simple_controller";
    XrActionSuggestedBinding simpleB[] = {
        { g_actLeftTrig,   XrPathFrom("/user/hand/left/input/select/click") },
        { g_actRightTrig,  XrPathFrom("/user/hand/right/input/select/click") },
        { g_actMenu,       XrPathFrom("/user/hand/left/input/menu/click") },
    };
    SuggestBindings(simple, simpleB, static_cast<uint32_t>(sizeof(simpleB) / sizeof(simpleB[0])));

    XrSessionActionSetsAttachInfo attach{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
    attach.countActionSets = 1;
    attach.actionSets = &g_padActionSet;
    if (XR_FAILED(xrAttachSessionActionSets(g_xrSession, &attach))) return false;

    g_padActionsReady = true;
    Log("OpenXR gamepad actions attached (VR controllers -> XInput port 0)");
    return true;
}

static SHORT StickShort(float v) {
    if (v > 1.f) v = 1.f;
    if (v < -1.f) v = -1.f;
    return static_cast<SHORT>(v * 32767.f);
}

static bool ReadVec2(XrAction act, XrPath sub, float* x, float* y) {
    XrActionStateGetInfo gi{ XR_TYPE_ACTION_STATE_GET_INFO };
    gi.action = act;
    gi.subactionPath = sub;
    XrActionStateVector2f st{ XR_TYPE_ACTION_STATE_VECTOR2F };
    if (XR_FAILED(xrGetActionStateVector2f(g_xrSession, &gi, &st)) || !st.isActive) return false;
    *x = st.currentState.x;
    *y = st.currentState.y;
    return true;
}

static bool ReadFloat(XrAction act, XrPath sub, float* v) {
    XrActionStateGetInfo gi{ XR_TYPE_ACTION_STATE_GET_INFO };
    gi.action = act;
    gi.subactionPath = sub;
    XrActionStateFloat st{ XR_TYPE_ACTION_STATE_FLOAT };
    if (XR_FAILED(xrGetActionStateFloat(g_xrSession, &gi, &st)) || !st.isActive) return false;
    *v = st.currentState;
    return true;
}

static bool ReadBool(XrAction act, XrPath sub) {
    XrActionStateGetInfo gi{ XR_TYPE_ACTION_STATE_GET_INFO };
    gi.action = act;
    gi.subactionPath = sub;
    XrActionStateBoolean st{ XR_TYPE_ACTION_STATE_BOOLEAN };
    return XR_SUCCEEDED(xrGetActionStateBoolean(g_xrSession, &gi, &st)) && st.isActive &&
           st.currentState;
}

bool SyncOpenXRGamepad(XINPUT_STATE* out) {
    if (!out || !g_padActionsReady || !g_xrRunning) return false;
    XrActiveActionSet active{ g_padActionSet, XR_NULL_PATH };
    XrActionsSyncInfo sync{ XR_TYPE_ACTIONS_SYNC_INFO };
    sync.countActiveActionSets = 1;
    sync.activeActionSets = &active;
    if (XR_FAILED(xrSyncActions(g_xrSession, &sync))) return false;

    float lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
    const bool haveL = ReadVec2(g_actLeftStick, g_pathHandLeft, &lx, &ly);
    const bool haveR = ReadVec2(g_actRightStick, g_pathHandRight, &rx, &ry);
    ReadFloat(g_actLeftTrig, g_pathHandLeft, &lt);
    ReadFloat(g_actRightTrig, g_pathHandRight, &rt);

    XINPUT_GAMEPAD gp{};
    gp.sThumbLX = StickShort(lx);
    gp.sThumbLY = StickShort(ly);
    gp.sThumbRX = StickShort(rx);
    gp.sThumbRY = StickShort(ry);
    gp.bLeftTrigger = static_cast<BYTE>(lt * 255.f);
    gp.bRightTrigger = static_cast<BYTE>(rt * 255.f);
    if (ReadBool(g_actA, g_pathHandRight)) gp.wButtons |= XINPUT_GAMEPAD_A;
    if (ReadBool(g_actB, g_pathHandRight)) gp.wButtons |= XINPUT_GAMEPAD_B;
    if (ReadBool(g_actX, g_pathHandLeft))  gp.wButtons |= XINPUT_GAMEPAD_X;
    if (ReadBool(g_actY, g_pathHandLeft))  gp.wButtons |= XINPUT_GAMEPAD_Y;
    if (ReadBool(g_actMenu, g_pathHandLeft)) gp.wButtons |= XINPUT_GAMEPAD_START;
    if (ReadBool(g_actView, g_pathHandLeft)) gp.wButtons |= XINPUT_GAMEPAD_BACK;
    if (ReadBool(g_actStickL, g_pathHandLeft)) gp.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
    if (ReadBool(g_actStickR, g_pathHandRight)) gp.wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;

    static DWORD s_lastPadRecenter = 0;
    if ((gp.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) && (gp.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)) {
        DWORD now = GetTickCount();
        if (now - s_lastPadRecenter > 600) {
            s_lastPadRecenter = now;
            RequestRecenter("VR PAD L3+R3");
        }
    }

    if (!haveL && !haveR) return false;

    out->Gamepad = gp;
    if (InterlockedCompareExchange(&g_padFromXrLogged, 1, 0) == 0)
        Log("feeding XInput port 0 from OpenXR controllers");

    static int s_diag = 0;
    if (++s_diag % 300 == 0) {
        Log("XR Pad live: LStick=(%d, %d) RStick=(%d, %d) Trig=(%u, %u) Btns=0x%04X",
            gp.sThumbLX, gp.sThumbLY, gp.sThumbRX, gp.sThumbRY, gp.bLeftTrigger, gp.bRightTrigger, gp.wButtons);
    }
    return true;
}

static bool PadHasInput(const XINPUT_GAMEPAD& g) {
    const int deadzone = 7000;
    return g.wButtons != 0 || g.bLeftTrigger > 30 || g.bRightTrigger > 30 ||
           abs(g.sThumbLX) > deadzone || abs(g.sThumbLY) > deadzone ||
           abs(g.sThumbRX) > deadzone || abs(g.sThumbRY) > deadzone;
}

void UpdateCachedXrPad() {
    if (!g_padActionsReady || !g_xrRunning) return;
    XINPUT_STATE s{};
    if (SyncOpenXRGamepad(&s)) {
        if (memcmp(&s.Gamepad, &g_lastGamepadState, sizeof(XINPUT_GAMEPAD)) != 0) {
            g_lastGamepadState = s.Gamepad;
            ++g_padPacket;
        }
        s.dwPacketNumber = g_padPacket;
        g_cachedXrPad = s;
        InterlockedExchange(&g_cachedXrPadValid, 1);
    }
}

// ---------------------------------------------------------------- XInput Hooking (Delay IAT + Standard IAT)
typedef DWORD(WINAPI* PFN_XInputGetState)(DWORD, XINPUT_STATE*);
PFN_XInputGetState g_origXInputGetState = nullptr;
volatile LONG g_xinputHooked = 0;
volatile LONG g_xinputAttempted = 0;
volatile LONG g_xinputRemapLogged = 0;

DWORD WINAPI Hook_XInputGetState(DWORD userIndex, XINPUT_STATE* state) {
    if (!state) return ERROR_DEVICE_NOT_CONNECTED;

    XINPUT_STATE real{};
    DWORD r = ERROR_DEVICE_NOT_CONNECTED;
    if (g_origXInputGetState) {
        r = g_origXInputGetState(userIndex, &real);
        if (g_remapPhysical && userIndex == 0 && r != ERROR_SUCCESS) {
            for (DWORD i = 1; i < 4; ++i) {
                if (g_origXInputGetState(i, &real) == ERROR_SUCCESS) {
                    r = ERROR_SUCCESS;
                    if (InterlockedCompareExchange(&g_xinputRemapLogged, 1, 0) == 0)
                        Log("XInput: remapping physical gamepad from port %lu -> port 0", i);
                    break;
                }
            }
        }
    }

    if (r == ERROR_SUCCESS &&
        (real.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) &&
        (real.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)) {
        static DWORD s_lastPhysRecenter = 0;
        DWORD now = GetTickCount();
        if (now - s_lastPhysRecenter > 600) {
            s_lastPhysRecenter = now;
            RequestRecenter("PHYSICAL PAD L3+R3");
        }
    }

    DWORD res = ERROR_DEVICE_NOT_CONNECTED;
    if (g_enableVRGamepad && userIndex == 0 && InterlockedCompareExchange(&g_cachedXrPadValid, 0, 0)) {
        if (r == ERROR_SUCCESS && PadHasInput(real.Gamepad)) {
            *state = real;
            res = ERROR_SUCCESS;
        } else {
            *state = g_cachedXrPad;
            res = ERROR_SUCCESS;
        }
    } else if (r == ERROR_SUCCESS) {
        *state = real;
        res = ERROR_SUCCESS;
    }

    if (res == ERROR_SUCCESS) {
        if ((g_headLookMode == 2 || g_headLookMode == 3) && g_in3DGameplayMode && userIndex == 0) {
            int addX = (int)InterlockedCompareExchange(&g_headStickX, 0, 0);
            int addY = (int)InterlockedCompareExchange(&g_headStickY, 0, 0);
            if (addX != 0 || addY != 0) {
                int newRX = (int)state->Gamepad.sThumbRX + addX;
                int newRY = (int)state->Gamepad.sThumbRY + addY;
                state->Gamepad.sThumbRX = (SHORT)std::max(-32767, std::min(32767, newRX));
                state->Gamepad.sThumbRY = (SHORT)std::max(-32767, std::min(32767, newRY));
            }
        }
        return ERROR_SUCCESS;
    }
    return r;
}

static bool HookDelayIATOrdinal(HMODULE module, const char* dllName, WORD ordinal, void* hook,
                                void** origOut) {
    if (!module) return false;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<BYTE*>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    IMAGE_DATA_DIRECTORY delayDir =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
    if (!delayDir.VirtualAddress) return false;

    auto* desc = reinterpret_cast<IMAGE_DELAYLOAD_DESCRIPTOR*>(
        reinterpret_cast<BYTE*>(module) + delayDir.VirtualAddress);

    for (; desc->DllNameRVA; ++desc) {
        const char* name = reinterpret_cast<const char*>(reinterpret_cast<BYTE*>(module) + desc->DllNameRVA);
        if (_stricmp(name, dllName) != 0) continue;

        auto* intTable = reinterpret_cast<IMAGE_THUNK_DATA*>(
            reinterpret_cast<BYTE*>(module) + desc->ImportNameTableRVA);
        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(
            reinterpret_cast<BYTE*>(module) + desc->ImportAddressTableRVA);

        for (; intTable->u1.Ordinal; ++intTable, ++iat) {
            if (!(intTable->u1.Ordinal & IMAGE_ORDINAL_FLAG)) continue;
            if (IMAGE_ORDINAL(intTable->u1.Ordinal) != ordinal) continue;

            void* prior = reinterpret_cast<void*>(static_cast<uintptr_t>(iat->u1.Function));
            if (origOut && !*origOut) {
                uintptr_t fnAddr = reinterpret_cast<uintptr_t>(prior);
                uintptr_t modBase = reinterpret_cast<uintptr_t>(module);
                uintptr_t modEnd = modBase + nt->OptionalHeader.SizeOfImage;
                if (fnAddr < modBase || fnAddr >= modEnd) {
                    *origOut = prior;
                } else {
                    HMODULE realMod = LoadLibraryA(dllName);
                    if (realMod) {
                        *origOut = reinterpret_cast<void*>(GetProcAddress(
                            realMod, reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(ordinal))));
                    }
                }
            }

            DWORD oldProt = 0;
            if (!VirtualProtect(&iat->u1.Function, sizeof(iat->u1.Function), PAGE_READWRITE, &oldProt))
                return false;
            iat->u1.Function = reinterpret_cast<ULONG_PTR>(hook);
            VirtualProtect(&iat->u1.Function, sizeof(iat->u1.Function), oldProt, &oldProt);
            return true;
        }
    }
    return false;
}

static bool HookStandardIATOrdinal(HMODULE module, const char* dllName, WORD ordinal, void* hook,
                                   void** origOut) {
    if (!module) return false;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<BYTE*>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    IMAGE_DATA_DIRECTORY impDir =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!impDir.VirtualAddress) return false;

    auto* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        reinterpret_cast<BYTE*>(module) + impDir.VirtualAddress);
    for (; imp->Name; ++imp) {
        const char* name = reinterpret_cast<const char*>(reinterpret_cast<BYTE*>(module) + imp->Name);
        if (_stricmp(name, dllName) != 0) continue;

        auto* origThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
            reinterpret_cast<BYTE*>(module) + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(
            reinterpret_cast<BYTE*>(module) + imp->FirstThunk);
        for (; origThunk->u1.Ordinal; ++origThunk, ++iat) {
            if (!(origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) continue;
            if (IMAGE_ORDINAL(origThunk->u1.Ordinal) != ordinal) continue;

            void* prior = reinterpret_cast<void*>(static_cast<uintptr_t>(iat->u1.Function));
            if (origOut && !*origOut) {
                HMODULE realMod = LoadLibraryA(dllName);
                if (realMod) {
                    *origOut = reinterpret_cast<void*>(GetProcAddress(
                        realMod, reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(ordinal))));
                } else {
                    *origOut = prior;
                }
            }

            DWORD oldProt = 0;
            if (!VirtualProtect(&iat->u1.Function, sizeof(iat->u1.Function), PAGE_READWRITE, &oldProt))
                return false;
            iat->u1.Function = reinterpret_cast<ULONG_PTR>(hook);
            VirtualProtect(&iat->u1.Function, sizeof(iat->u1.Function), oldProt, &oldProt);
            return true;
        }
    }
    return false;
}

void TryInstallXInputHook() {
    if (InterlockedCompareExchange(&g_xinputAttempted, 1, 0) != 0) return;
    HMODULE exe = GetModuleHandleW(nullptr);

    bool ok = HookDelayIATOrdinal(exe, "XINPUT1_3.dll", 2, reinterpret_cast<void*>(&Hook_XInputGetState),
                                  reinterpret_cast<void**>(&g_origXInputGetState));
    const char* method = "Delay-Import";
    if (!ok) {
        ok = HookStandardIATOrdinal(exe, "XINPUT1_3.dll", 2, reinterpret_cast<void*>(&Hook_XInputGetState),
                                    reinterpret_cast<void**>(&g_origXInputGetState));
        method = "Standard-Import";
    }

    if (ok) {
        InterlockedExchange(&g_xinputHooked, 1);
        Log("XInput IAT hooked (%s) ordinal 2 -> Hook_XInputGetState (orig=%p)", method, g_origXInputGetState);
    } else {
        Log("XInput IAT hook failed to locate XINPUT1_3.dll ordinal 2 in Delay or Standard IAT (will not retry)");
    }
}

// ---------------------------------------------------------------- OpenXR Setup
bool InitXR() {
    const char* exts[] = { XR_KHR_D3D11_ENABLE_EXTENSION_NAME };
    XrVersion vers[] = { XR_MAKE_VERSION(1, 0, 34), XR_CURRENT_API_VERSION };
    XrResult r = XR_ERROR_RUNTIME_FAILURE;
    for (XrVersion v : vers) {
        XrInstanceCreateInfo ici{ XR_TYPE_INSTANCE_CREATE_INFO };
        strcpy_s(ici.applicationInfo.applicationName, "Darkness2VR");
        ici.applicationInfo.apiVersion = v;
        ici.enabledExtensionCount = 1;
        ici.enabledExtensionNames = exts;
        r = xrCreateInstance(&ici, &g_xrInstance);
        if (XR_SUCCEEDED(r)) break;
    }
    if (XR_FAILED(r)) {
        Log("xrCreateInstance failed (%d) — connect headset / OpenXR runtime first", (int)r);
        return false;
    }

    XrSystemGetInfo sgi{ XR_TYPE_SYSTEM_GET_INFO };
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (XR_FAILED(xrGetSystem(g_xrInstance, &sgi, &g_xrSystem))) {
        Log("xrGetSystem failed — no HMD");
        return false;
    }

    PFN_xrGetD3D11GraphicsRequirementsKHR pfn = nullptr;
    xrGetInstanceProcAddr(g_xrInstance, "xrGetD3D11GraphicsRequirementsKHR",
                          reinterpret_cast<PFN_xrVoidFunction*>(&pfn));
    if (!pfn) return false;
    XrGraphicsRequirementsD3D11KHR req{ XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
    if (XR_FAILED(pfn(g_xrInstance, g_xrSystem, &req))) return false;

    IDXGIFactory1* fac = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&fac))))
        return false;
    IDXGIAdapter1* ad = nullptr;
    IDXGIAdapter1* pick = nullptr;
    for (UINT i = 0; fac->EnumAdapters1(i, &ad) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 d{};
        ad->GetDesc1(&d);
        if (d.AdapterLuid.LowPart == req.adapterLuid.LowPart &&
            d.AdapterLuid.HighPart == req.adapterLuid.HighPart) {
            pick = ad;
            break;
        }
        ad->Release();
    }
    fac->Release();
    if (!pick) {
        Log("no DXGI adapter matching OpenXR LUID");
        return false;
    }
    D3D_FEATURE_LEVEL fl[] = { D3D_FEATURE_LEVEL_11_0 }, got{};
    if (FAILED(D3D11CreateDevice(pick, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                                 D3D11_CREATE_DEVICE_BGRA_SUPPORT, fl, 1, D3D11_SDK_VERSION,
                                 &g_dev11, &got, &g_ctx11))) {
        pick->Release();
        return false;
    }
    pick->Release();

    XrGraphicsBindingD3D11KHR bind{ XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
    bind.device = g_dev11;
    XrSessionCreateInfo sci{ XR_TYPE_SESSION_CREATE_INFO };
    sci.next = &bind;
    sci.systemId = g_xrSystem;
    if (XR_FAILED(xrCreateSession(g_xrInstance, &sci, &g_xrSession))) return false;

    XrReferenceSpaceCreateInfo rs{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    rs.poseInReferenceSpace.orientation.w = 1.f;
    if (XR_FAILED(xrCreateReferenceSpace(g_xrSession, &rs, &g_xrSpace))) return false;

    XrReferenceSpaceCreateInfo viewRs{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    viewRs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    viewRs.poseInReferenceSpace.orientation.w = 1.f;
    xrCreateReferenceSpace(g_xrSession, &viewRs, &g_viewSpace);

    InitXrGamepadActions();

    uint32_t viewConfigCount = 0;
    if (XR_SUCCEEDED(xrEnumerateViewConfigurationViews(g_xrInstance, g_xrSystem,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewConfigCount, nullptr)) && viewConfigCount >= 2) {
        std::vector<XrViewConfigurationView> vcv(viewConfigCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
        if (XR_SUCCEEDED(xrEnumerateViewConfigurationViews(g_xrInstance, g_xrSystem,
                XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewConfigCount, &viewConfigCount, vcv.data()))) {
            g_steamVrRecW = vcv[0].recommendedImageRectWidth;
            g_steamVrRecH = vcv[0].recommendedImageRectHeight;
            Log("SteamVR / OpenXR recommended view resolution: %ux%u per eye (max: %ux%u)",
                g_steamVrRecW, g_steamVrRecH, vcv[0].maxImageRectWidth, vcv[0].maxImageRectHeight);
        }
    }

    Log("OpenXR session ready (LOCAL + VIEW spaces initialized)");
    g_xrReady = true;
    return true;
}

void PumpXR() {
    XrEventDataBuffer ev{ XR_TYPE_EVENT_DATA_BUFFER };
    while (xrPollEvent(g_xrInstance, &ev) == XR_SUCCESS) {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto* s = reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);
            if (s->state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo bi{ XR_TYPE_SESSION_BEGIN_INFO };
                bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                if (XR_SUCCEEDED(xrBeginSession(g_xrSession, &bi))) {
                    g_xrRunning = true;
                    g_anchorValid = false;
                    g_vrRefValid = false;
                    Log("XR session running (TestMode=%d SubmitMode=%d TrackingSubMode=%d)",
                        g_testMode, g_submitMode, g_trackingSubMode);
                }
            } else if (s->state == XR_SESSION_STATE_STOPPING) {
                xrEndSession(g_xrSession);
                g_xrRunning = false;
            }
        }
        ev = { XR_TYPE_EVENT_DATA_BUFFER };
    }
}

bool EnsureSwapchains(UINT w, UINT h) {
    if (g_swapchain[0] && g_swapchain[1] && w == g_scW && h == g_scH) return true;
    for (int e = 0; e < 2; ++e) {
        if (g_swapchain[e]) {
            xrDestroySwapchain(g_swapchain[e]);
            g_swapchain[e] = XR_NULL_HANDLE;
        }
        if (g_scImages[e]) {
            delete[] g_scImages[e];
            g_scImages[e] = nullptr;
        }
        if (g_eyeRTV[e]) {
            g_eyeRTV[e]->Release();
            g_eyeRTV[e] = nullptr;
        }
        if (g_eyeCache[e]) {
            g_eyeCache[e]->Release();
            g_eyeCache[e] = nullptr;
        }
    }

    uint32_t nfmt = 0;
    xrEnumerateSwapchainFormats(g_xrSession, 0, &nfmt, nullptr);
    int64_t* fmts = new int64_t[nfmt ? nfmt : 1];
    xrEnumerateSwapchainFormats(g_xrSession, nfmt, &nfmt, fmts);
    int64_t fmt = nfmt ? fmts[0] : 0;
    Log("OpenXR runtime exposes %u swapchain formats:", nfmt);
    for (uint32_t i = 0; i < nfmt; ++i) {
        Log("  format[%u]: %lld (0x%llX)", i, (long long)fmts[i], (long long)fmts[i]);
        if (fmts[i] == (int64_t)DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
            fmt = fmts[i];
        }
    }
    delete[] fmts;
    Log("Chosen swapchain format: %lld (0x%llX)", (long long)fmt, (long long)fmt);

    for (int e = 0; e < 2; ++e) {
        XrSwapchainCreateInfo sci{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
        sci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        sci.format = fmt;
        sci.sampleCount = 1;
        sci.width = w;
        sci.height = h;
        sci.faceCount = 1;
        sci.arraySize = 1;
        sci.mipCount = 1;
        if (XR_FAILED(xrCreateSwapchain(g_xrSession, &sci, &g_swapchain[e]))) return false;

        xrEnumerateSwapchainImages(g_swapchain[e], 0, &g_scCount[e], nullptr);
        auto* imgs = new XrSwapchainImageD3D11KHR[g_scCount[e]];
        for (uint32_t i = 0; i < g_scCount[e]; ++i) imgs[i] = { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR };
        xrEnumerateSwapchainImages(g_swapchain[e], g_scCount[e], &g_scCount[e],
                                   reinterpret_cast<XrSwapchainImageBaseHeader*>(imgs));
        g_scImages[e] = new ID3D11Texture2D*[g_scCount[e]];
        for (uint32_t i = 0; i < g_scCount[e]; ++i) g_scImages[e][i] = imgs[i].texture;
        delete[] imgs;

        D3D11_TEXTURE2D_DESC td{};
        td.Width = w;
        td.Height = h;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        if (FAILED(g_dev11->CreateTexture2D(&td, nullptr, &g_eyeCache[e]))) {
            Log("CreateTexture2D for eyeCache[%d] failed", e);
            return false;
        }
        if (FAILED(g_dev11->CreateRenderTargetView(g_eyeCache[e], nullptr, &g_eyeRTV[e]))) {
            Log("CreateRenderTargetView for eyeRTV[%d] failed", e);
            return false;
        }
    }

    g_scW = w;
    g_scH = h;
    Log("Stereo swapchains [0,1] + RTVs created %ux%u count=(%u,%u)", w, h, g_scCount[0], g_scCount[1]);
    return true;
}

bool EnsureUploadTexture(UINT w, UINT h) {
    if (g_upload && g_uploadSRV && w == g_srcW && h == g_srcH) return true;
    if (g_uploadSRV) {
        g_uploadSRV->Release();
        g_uploadSRV = nullptr;
    }
    if (g_upload) {
        g_upload->Release();
        g_upload = nullptr;
    }
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w;
    td.Height = h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DYNAMIC;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(g_dev11->CreateTexture2D(&td, nullptr, &g_upload))) {
        Log("CreateTexture2D for upload failed (%ux%u)", w, h);
        return false;
    }
    if (FAILED(g_dev11->CreateShaderResourceView(g_upload, nullptr, &g_uploadSRV))) {
        Log("CreateShaderResourceView for uploadSRV failed");
        return false;
    }
    g_srcW = w;
    g_srcH = h;
    return true;
}

bool EnsureCopy(IDirect3DDevice9* dev, UINT w, UINT h, D3DFORMAT fmt) {
    // Upload dimensions are updated by EnsureUploadTexture; they cannot describe
    // the independently allocated D3D9 readback surface after a resize.
    if (!EnsureUploadTexture(w, h)) return false;
    if (g_sysmem) {
        D3DSURFACE_DESC current{};
        if (SUCCEEDED(g_sysmem->GetDesc(&current)) && current.Width == w &&
            current.Height == h && current.Format == fmt) return true;
    }
    if (g_sysmem) {
        g_sysmem->Release();
        g_sysmem = nullptr;
    }
    if (FAILED(dev->CreateOffscreenPlainSurface(w, h, fmt, D3DPOOL_SYSTEMMEM, &g_sysmem, nullptr))) {
        Log("CreateOffscreenPlainSurface failed (%ux%u fmt=%d)", w, h, (int)fmt);
        return false;
    }
    g_srcFmt = fmt;
    Log("[Capture] readback surface created %ux%u fmt=%d", w, h, (int)fmt);
    return true;
}

bool FillTestPattern(UINT w, UINT h, int mode) {
    if (!EnsureUploadTexture(w, h)) return false;
    D3D11_MAPPED_SUBRESOURCE ms{};
    if (FAILED(g_ctx11->Map(g_upload, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) return false;

    uint8_t* d = static_cast<uint8_t*>(ms.pData);
    for (UINT y = 0; y < h; ++y) {
        uint32_t* row = reinterpret_cast<uint32_t*>(d + (size_t)y * ms.RowPitch);
        for (UINT x = 0; x < w; ++x) {
            if (mode == 1) {
                row[x] = 0xFF00FF00;
            } else {
                bool check = (((x / 64) ^ (y / 64)) & 1) != 0;
                if (x < 16 || x >= w - 16 || y < 16 || y >= h - 16 ||
                    (x >= w / 2 - 4 && x <= w / 2 + 4) ||
                    (y >= h / 2 - 4 && y <= h / 2 + 4)) {
                    row[x] = 0xFFFFFFFF;
                } else {
                    row[x] = check ? 0xFF00FF00 : 0xFFFF00FF;
                }
            }
        }
    }
    g_ctx11->Unmap(g_upload, 0);
    return true;
}

static void SampleSurfaceStats(const D3DLOCKED_RECT& lr, UINT w, UINT h,
                               uint8_t* outAvgR, uint8_t* outAvgG, uint8_t* outAvgB,
                               uint8_t* outMaxR, uint8_t* outMaxG, uint8_t* outMaxB) {
    uint32_t totalR = 0, totalG = 0, totalB = 0;
    uint8_t maxR = 0, maxG = 0, maxB = 0;
    const int samples = 300;
    const uint8_t* p = static_cast<const uint8_t*>(lr.pBits);
    for (int i = 0; i < samples; ++i) {
        UINT sy = (i * 17) % h;
        UINT sx = (i * 31) % w;
        const uint8_t* px = p + (size_t)sy * lr.Pitch + (size_t)sx * 4;
        uint8_t b = px[0], g = px[1], r = px[2];
        totalB += b; totalG += g; totalR += r;
        if (b > maxB) maxB = b;
        if (g > maxG) maxG = g;
        if (r > maxR) maxR = r;
    }
    *outAvgR = static_cast<uint8_t>(totalR / samples);
    *outAvgG = static_cast<uint8_t>(totalG / samples);
    *outAvgB = static_cast<uint8_t>(totalB / samples);
    *outMaxR = maxR;
    *outMaxG = maxG;
    *outMaxB = maxB;
}

bool CopyFrame(IDirect3DDevice9* dev) {
    // RenderTarget0 is the surface the frame replay re-renders per eye - it MUST be the
    // capture source (the backbuffer keeps the live-pass composite = identical eyes).
    // Its size can differ from the backbuffer (the engine picks its own G-buffer size),
    // so the sysmem surface follows RT0 and the D3D11 blit handles the scaling.
    IDirect3DSurface9* rt0 = nullptr;
    D3DSURFACE_DESC sd{};
    const char* srcMethod = "RenderTarget0:GetRenderTargetData";
    HRESULT hr = E_FAIL;
    if (SUCCEEDED(dev->GetRenderTarget(0, &rt0)) && rt0) {
        rt0->GetDesc(&sd);
        if (sd.MultiSampleType != D3DMULTISAMPLE_NONE || !EnsureCopy(dev, sd.Width, sd.Height, sd.Format)) {
            rt0->Release();
            rt0 = nullptr;
        } else {
            hr = dev->GetRenderTargetData(rt0, g_sysmem);
            if (FAILED(hr)) {
                static int rtFail = 0;
                if (rtFail++ < 5) Log("RT0 capture failed hr=0x%08lX (%ux%u fmt=%d)", hr, sd.Width, sd.Height, (int)sd.Format);
                rt0->Release();
                rt0 = nullptr;
            }
        }
    }

    // GetRenderTarget owns a reference even when readback succeeds.
    const bool capturedRt0 = rt0 != nullptr;
    if (rt0) { rt0->Release(); rt0 = nullptr; }

    if (!capturedRt0) {
        // fallback: the backbuffer (may be black during gameplay - only a safety net)
        IDirect3DSurface9* back = nullptr;
        if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) || !back) return false;
        back->GetDesc(&sd);
        if (!EnsureCopy(dev, sd.Width, sd.Height, sd.Format)) {
            back->Release();
            return false;
        }
        srcMethod = "BackBuffer0:GetRenderTargetData";
        hr = dev->GetRenderTargetData(back, g_sysmem);
        back->Release();
        if (FAILED(hr)) return false;
    }

    // remember the captured size for the D3D11 upload
    g_srcW = sd.Width;
    g_srcH = sd.Height;

    D3DLOCKED_RECT lr{};
    if (FAILED(g_sysmem->LockRect(&lr, nullptr, D3DLOCK_READONLY))) return false;

    uint8_t avgR = 0, avgG = 0, avgB = 0, maxR = 0, maxG = 0, maxB = 0;
    SampleSurfaceStats(lr, sd.Width, sd.Height, &avgR, &avgG, &avgB, &maxR, &maxG, &maxB);

    static int s_statCounter = 0;
    if (++s_statCounter % 120 == 0) {
        Log("capture stats [frame %d]: %ux%u (via %s) mean RGB=(%u,%u,%u) max RGB=(%u,%u,%u) 3dCams=%d 3dMode=%d depthVar=%d",
            s_statCounter, sd.Width, sd.Height, srcMethod,
            avgR, avgG, avgB, maxR, maxG, maxB, g_3dCamCallsThisFrame, (int)g_in3DGameplayMode, (int)g_depthHasVariance);
    }

    if (!EnsureUploadTexture(sd.Width, sd.Height)) {
        g_sysmem->UnlockRect();
        return false;
    }
    D3D11_MAPPED_SUBRESOURCE ms{};
    if (FAILED(g_ctx11->Map(g_upload, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        g_sysmem->UnlockRect();
        return false;
    }

    const size_t row = (size_t)g_srcW * 4;
    const uint8_t* s = static_cast<const uint8_t*>(lr.pBits);
    uint8_t* d = static_cast<uint8_t*>(ms.pData);
    for (UINT y = 0; y < g_srcH; ++y)
        memcpy(d + (size_t)y * ms.RowPitch, s + (size_t)y * lr.Pitch, row);
    g_ctx11->Unmap(g_upload, 0);
    g_sysmem->UnlockRect();

    // Snapshot INTZ depth into 480x270 D3D11 texture if available (F5 visualization only)
    if (g_depthViz && g_intzTex9 && g_had3DCameraThisFrame && !g_depthCapturedThisFrame) {
        CaptureIntzDepthToD11(dev);
    }

    return true;
}

static bool WriteTextureToSwapchain(XrSwapchain sc, ID3D11Texture2D** scImages, ID3D11Texture2D* srcTex) {
    uint32_t idx = 0;
    XrSwapchainImageAcquireInfo ai{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    if (XR_FAILED(xrAcquireSwapchainImage(sc, &ai, &idx))) return false;
    XrSwapchainImageWaitInfo wi{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    wi.timeout = XR_INFINITE_DURATION;
    bool ok = false;
    if (XR_SUCCEEDED(xrWaitSwapchainImage(sc, &wi))) {
        g_ctx11->CopyResource(scImages[idx], srcTex);
        ok = true;
    }
    XrSwapchainImageReleaseInfo ri{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    xrReleaseSwapchainImage(sc, &ri);
    return ok;
}

void SubmitXR(IDirect3DDevice9* dev) {
    g_lastStage = 20;

    if (!g_xrReady) return;
    PumpXR();
    if (!g_xrRunning) return;

    XrFrameState fs{ XR_TYPE_FRAME_STATE };
    if (XR_FAILED(xrWaitFrame(g_xrSession, nullptr, &fs))) return;
    xrBeginFrame(g_xrSession, nullptr);

    XrView views[2] = { { XR_TYPE_VIEW }, { XR_TYPE_VIEW } };
    uint32_t n = 0;
    XrViewLocateInfo vli{ XR_TYPE_VIEW_LOCATE_INFO };
    vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    vli.displayTime = fs.predictedDisplayTime;
    vli.space = g_xrSpace;
    XrViewState vs{ XR_TYPE_VIEW_STATE };
    g_haveViews = false;
    if (XR_SUCCEEDED(xrLocateViews(g_xrSession, &vli, &vs, 2, &n, views)) && n >= 2 &&
        (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {
        for (uint32_t i = 0; i < 2; ++i) {
            g_eyePose[i] = views[i].pose;
            g_eyeFov[i] = views[i].fov;
        }
        g_haveViews = true;
    }

    UpdateCachedXrPad();

    if (g_had3DCameraThisFrame) {
        ++g_consecutive3DFrames;
        g_consecutive2DFrames = 0;
        if (!g_in3DGameplayMode && g_consecutive3DFrames >= 10) {
            g_in3DGameplayMode = true;
            g_vrRefValid = false;
            Log(">>> Entered 3D Gameplay VR mode (consecutive3DFrames=%d) <<<", g_consecutive3DFrames);
        }
    } else {
        ++g_consecutive2DFrames;
        g_consecutive3DFrames = 0;
        if (g_in3DGameplayMode && g_consecutive2DFrames >= 90) {
            g_in3DGameplayMode = false;
            g_anchorValid = false;
            Log(">>> Returned to 2D mode (consecutive2DFrames=%d) <<<", g_consecutive2DFrames);
        }
    }

    const XrCompositionLayerBaseHeader* layers[1] = { nullptr };
    uint32_t layerCount = 0;
    XrCompositionLayerQuad quad{ XR_TYPE_COMPOSITION_LAYER_QUAD };
    XrCompositionLayerProjection proj{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    XrCompositionLayerProjectionView projViews[2]{};

    bool havePixels = false;
    if (fs.shouldRender && g_haveViews) {
        UINT targetW = g_steamVrRecW ? g_steamVrRecW : (g_srcW ? g_srcW : 1920);
        UINT targetH = g_steamVrRecH ? g_steamVrRecH : (g_srcH ? g_srcH : 1080);
        if (g_submitMode == 4) {
            // TRUE same-frame stereo: both eyes captured during Present (live pass + replay pass)
            havePixels = g_eyePairReady && EnsureSwapchains(targetW, targetH);
            g_eyePairReady = false;
        } else if (g_testMode > 0) {
            havePixels = FillTestPattern(targetW, targetH, g_testMode) && EnsureSwapchains(targetW, targetH);
        } else if (dev) {
            havePixels = CopyFrame(dev) && EnsureSwapchains(targetW, targetH);
        }
    }

    if (havePixels) {
        const float aspect = (g_srcW && g_srcH) ? ((float)g_srcW / (float)g_srcH) : (16.f / 9.f);
        const XrPosef head = HeadCenterPose(g_eyePose[0], g_eyePose[1]);

        // ---------------- Head-Look Camera Rotation (Turning Head Rotates In-Game Camera) ----------------
        // NOTE: active whenever HeadLookMode > 0 (the old 3D-mode gate silently disabled head
        // tracking outside the detector path - the user reported "no head tracking").
        if (g_headLookMode > 0) {
            static float s_lastHeadYaw = 0.0f;
            static float s_lastHeadPitch = 0.0f;
            static DWORD s_lastHeadTime = 0;
            DWORD nowMs = GetTickCount();

            float curYaw = 0.0f, curPitch = 0.0f;
            ComputeHeadEuler(head.orientation, &curYaw, &curPitch);

            if (g_headLookReset) {
                s_lastHeadYaw = curYaw;
                s_lastHeadPitch = curPitch;
                s_lastHeadTime = nowMs;
                g_headLookReset = false;
                InterlockedExchange(&g_headStickX, 0);
                InterlockedExchange(&g_headStickY, 0);
            } else {
                float dYaw = curYaw - s_lastHeadYaw;
                while (dYaw > 3.14159265f) dYaw -= 6.2831853f;
                while (dYaw < -3.14159265f) dYaw += 6.2831853f;

                float dPitch = curPitch - s_lastHeadPitch;
                while (dPitch > 3.14159265f) dPitch -= 6.2831853f;
                while (dPitch < -3.14159265f) dPitch += 6.2831853f;

                s_lastHeadYaw = curYaw;
                s_lastHeadPitch = curPitch;

                float dt = (nowMs > s_lastHeadTime) ? (float)(nowMs - s_lastHeadTime) * 0.001f : 0.011f;
                s_lastHeadTime = nowMs;
                if (dt < 0.002f) dt = 0.002f;
                if (dt > 0.100f) dt = 0.100f;

                // Micro-deadzone to suppress sensor tracking noise jitter (~0.05 deg)
                const float noiseFloor = 0.00087f;
                if (std::abs(dYaw) < noiseFloor) dYaw = 0.0f;
                if (std::abs(dPitch) < noiseFloor) dPitch = 0.0f;

                // 1. Mouse Injection (1:1 smooth camera rotation)
                if (g_headLookMode == 1 || g_headLookMode == 3) {
                    float yawSign = g_headLookInvertYaw ? -1.0f : 1.0f;
                    float pitchSign = g_headLookInvertPitch ? 1.0f : -1.0f;
                    int mouseDx = (int)(dYaw * g_headLookSensitivityYaw * yawSign);
                    int mouseDy = (int)(dPitch * g_headLookSensitivityPitch * pitchSign);
                    if (mouseDx != 0 || mouseDy != 0) {
                        mouse_event(MOUSEEVENTF_MOVE, mouseDx, mouseDy, 0, 0);
                    }
                }

                // 2. Gamepad Right Stick Injection
                if (g_headLookMode == 2 || g_headLookMode == 3) {
                    float yawSign = g_headLookInvertYaw ? -1.0f : 1.0f;
                    float pitchSign = g_headLookInvertPitch ? 1.0f : -1.0f;
                    float yawRate = (dYaw / dt) * yawSign;
                    float pitchRate = (dPitch / dt) * (-pitchSign);
                    int stickX = (int)(yawRate * 10000.0f);
                    int stickY = (int)(pitchRate * 10000.0f);
                    stickX = std::max(-32000, std::min(32000, stickX));
                    stickY = std::max(-32000, std::min(32000, stickY));
                    InterlockedExchange(&g_headStickX, stickX);
                    InterlockedExchange(&g_headStickY, stickY);
                } else {
                    InterlockedExchange(&g_headStickX, 0);
                    InterlockedExchange(&g_headStickY, 0);
                }
            }
        } else {
            g_headLookReset = true;
            InterlockedExchange(&g_headStickX, 0);
            InterlockedExchange(&g_headStickY, 0);
        }

        // ---------------- TRUE same-frame stereo (SubmitMode 4): dual-eye geometry render ----------------
        if (g_submitMode == 4) {
            UpdateSubmissionFovFromMatrices();
            if (WriteTextureToSwapchain(g_swapchain[0], g_scImages[0], g_eyeCache[0]) &&
                WriteTextureToSwapchain(g_swapchain[1], g_scImages[1], g_eyeCache[1])) {
                // Both eye images were rendered from the SAME game frame with per-eye
                // matrix shear; submit one shared pose and the game's own frustum so the
                // OpenXR compositor maps each image exactly onto the eye view.
                for (int e = 0; e < 2; ++e) {
                    projViews[e] = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
                    projViews[e].pose = head;
                    projViews[e].fov.angleLeft = -g_gameHalfFovX + g_viewOffsetX;
                    projViews[e].fov.angleRight = +g_gameHalfFovX + g_viewOffsetX;
                    projViews[e].fov.angleUp = +g_gameHalfFovY;
                    projViews[e].fov.angleDown = -g_gameHalfFovY;
                    projViews[e].subImage.swapchain = g_swapchain[e];
                    projViews[e].subImage.imageRect.offset = { 0, 0 };
                    projViews[e].subImage.imageRect.extent = { (int32_t)g_scW, (int32_t)g_scH };
                }
                proj.layerFlags = 0;
                proj.space = g_xrSpace;
                proj.viewCount = 2;
                proj.views = projViews;
                layers[0] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&proj);
                layerCount = 1;
            }
        } else
        // Method 2 Stereo Projection is used whenever SubmitMode is 0 or 1.
        // Using the same frustum-aligned projection in both menus and gameplay prevents any jarring
        // transition flashes between Cinema Quad and Stereo Projection!
        if (g_submitMode == 0 || g_submitMode == 1) {
            const bool swapEyes = (g_submitMode == 1);
            // Polarity confirmed working by user: left eye positive disparity, right eye negative disparity
            const float leftSign = swapEyes ? -1.0f : +1.0f;
            const float rightSign = swapEyes ? +1.0f : -1.0f;

            // Compute projection span parameters based on TrackingSubMode (F9)
            // Mode 0: Immersive VR Dome 150° (Obraz Dookoła - room-anchored head tracking)
            // Mode 1: Ultra-Szeroka Panorama 180° (Pełne Otoczenie VR)
            // Mode 2: Ekran Kinowy 3D (Framed 3D Cinema Screen)
            // g_viewScale (F4) scales the FOV span: higher = smaller on-screen elements, pulled back view, more HUD visible
            float invTwoHalfTanX = (0.5f / 1.309f) * g_viewScale;
            float invTwoHalfTanY = invTwoHalfTanX * aspect;
            if (g_trackingSubMode == 1) {
                // 180° cylindrical panorama (halfAngleX = pi / 2 = 1.570796 rad)
                invTwoHalfTanX = (0.5f / 1.5707963f) * g_viewScale;
                invTwoHalfTanY = invTwoHalfTanX * aspect;
            } else if (g_trackingSubMode == 2) {
                // Framed cinema screen (planar tangent projection)
                const float halfTanX = 0.88f / g_viewScale;
                const float halfTanY = halfTanX / aspect;
                invTwoHalfTanX = 1.0f / (2.0f * halfTanX);
                invTwoHalfTanY = 1.0f / (2.0f * halfTanY);
            }

            // Compute subtle 6DoF positional head-lean parallax offset
            float posOffX = 0.0f, posOffY = 0.0f;
            if (g_trackingSubMode != 2) {
                if (!g_vrRefValid) {
                    g_vrRefPos = head.position;
                    g_vrRefYaw = ComputeHorizontalYaw(head.orientation);
                    g_vrRefValid = true;
                }
                const float rawDx = head.position.x - g_vrRefPos.x;
                const float rawDy = head.position.y - g_vrRefPos.y;
                const float rawDz = head.position.z - g_vrRefPos.z;
                const float cYaw = std::cos(-g_vrRefYaw);
                const float sYaw = std::sin(-g_vrRefYaw);
                const float localX = cYaw * rawDx - sYaw * rawDz;
                posOffX = std::max(-0.025f, std::min(0.025f, -localX * 0.04f * g_worldScale));
                posOffY = std::max(-0.020f, std::min(0.020f,  rawDy  * 0.04f * g_worldScale));
            }

            const bool leftOk  = RenderStereoEyeD11(0, g_eyeFov[0], invTwoHalfTanX, invTwoHalfTanY, leftSign,  posOffX, posOffY);
            const bool rightOk = RenderStereoEyeD11(1, g_eyeFov[1], invTwoHalfTanX, invTwoHalfTanY, rightSign, posOffX, posOffY);

            if (leftOk && rightOk &&
                WriteTextureToSwapchain(g_swapchain[0], g_scImages[0], g_eyeCache[0]) &&
                WriteTextureToSwapchain(g_swapchain[1], g_scImages[1], g_eyeCache[1])) {

                // Determine projection camera pose:
                // SubMode 0: Immersive VR Dome anchored in room space with comfort yaw deadzone follow (+-28 deg)
                // SubMode 1: Ultra-Wide 180° Panorama following head orientation directly
                // SubMode 2: 3D Cinema Screen
                XrPosef viewPose = head;
                if (g_trackingSubMode == 0 && g_headLookMode == 0) {
                    if (!g_vrWorldAnchorValid) {
                        g_vrWorldAnchor = head;
                        const float initYaw = ComputeHorizontalYaw(head.orientation);
                        g_vrWorldAnchor.orientation.x = 0.0f;
                        g_vrWorldAnchor.orientation.y = std::sin(initYaw * 0.5f);
                        g_vrWorldAnchor.orientation.z = 0.0f;
                        g_vrWorldAnchor.orientation.w = std::cos(initYaw * 0.5f);
                        g_vrWorldAnchorValid = true;
                        Log("VR World Anchor latched at yaw=%.1f deg (frame=%d)", initYaw * 57.29578f, g_frames);
                    }

                    // Always anchor position to current head position for natural 6DoF parallax
                    g_vrWorldAnchor.position = head.position;

                    // Comfort yaw deadzone follow (+- 28 degrees)
                    const float currentYaw = ComputeHorizontalYaw(head.orientation);
                    float anchorYaw = ComputeHorizontalYaw(g_vrWorldAnchor.orientation);
                    float deltaYaw = currentYaw - anchorYaw;
                    while (deltaYaw > 3.14159265f) deltaYaw -= 6.2831853f;
                    while (deltaYaw < -3.14159265f) deltaYaw += 6.2831853f;

                    const float deadzone = 0.49f; // ~28 degrees
                    if (deltaYaw > deadzone) {
                        anchorYaw += (deltaYaw - deadzone) * 0.12f;
                    } else if (deltaYaw < -deadzone) {
                        anchorYaw += (deltaYaw + deadzone) * 0.12f;
                    }
                    g_vrWorldAnchor.orientation.x = 0.0f;
                    g_vrWorldAnchor.orientation.y = std::sin(anchorYaw * 0.5f);
                    g_vrWorldAnchor.orientation.z = 0.0f;
                    g_vrWorldAnchor.orientation.w = std::cos(anchorYaw * 0.5f);

                    viewPose = g_vrWorldAnchor;
                }

                // CRITICAL FOR EYE FUSION:
                // Both views share the exact same viewPose while using their respective g_eyeFov[e].
                // Because RenderStereoEyeD11 mapped g_eyeFov[e]'s tangent space so (tx=0, ty=0) -> UV (0.5, 0.5),
                // the optical axes of Left Eye and Right Eye are 100% parallel at infinity with 0.000 deg divergence!
                for (int e = 0; e < 2; ++e) {
                    projViews[e] = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
                    projViews[e].pose = viewPose;
                    projViews[e].fov = g_eyeFov[e];
                    projViews[e].subImage.swapchain = g_swapchain[e];
                    projViews[e].subImage.imageRect.offset = { 0, 0 };
                    projViews[e].subImage.imageRect.extent = { (int32_t)g_scW, (int32_t)g_scH };
                }

                proj.layerFlags = 0;
                proj.space = g_xrSpace;
                proj.viewCount = 2;
                proj.views = projViews;
                layers[0] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&proj);
                layerCount = 1;
            }
        } else {
            g_ctx11->CopyResource(g_eyeCache[0], g_upload);
            if (WriteTextureToSwapchain(g_swapchain[0], g_scImages[0], g_eyeCache[0])) {
                if (g_submitMode != 3) {
                    float dx = head.position.x - g_anchorHeadOrigin.x;
                    float dy = head.position.y - g_anchorHeadOrigin.y;
                    float dz = head.position.z - g_anchorHeadOrigin.z;
                    float distSq = dx * dx + dy * dy + dz * dz;
                    if (!g_anchorValid || g_frames == 30 || g_frames == 120 || distSq > (1.2f * 1.2f)) {
                        g_anchorPose = ComputeLevelWorldAnchorPose(head);
                        g_anchorHeadOrigin = head.position;
                        g_anchorValid = true;
                        Log("Cinema World Anchor latched at pos=(%.2f, %.2f, %.2f) (frame=%d)",
                            g_anchorPose.position.x, g_anchorPose.position.y, g_anchorPose.position.z, g_frames);
                    }
                    quad.pose = g_anchorPose;
                } else {
                    quad.pose = CinemaQuadPose(head);
                }
                quad.layerFlags = 0;
                quad.space = g_xrSpace;
                quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                quad.subImage.swapchain = g_swapchain[0];
                quad.subImage.imageRect.offset = { 0, 0 };
                quad.subImage.imageRect.extent = { (int32_t)g_scW, (int32_t)g_scH };
                quad.size = { g_cinemaWidthM, g_cinemaWidthM / aspect };
                layers[0] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
                layerCount = 1;
            }
        }

        if (g_frames == 0 && layerCount > 0) {
            Log("*** FIRST FRAME SUBMITTED *** mode=%d size=%ux%u leftFov=(%.2f,%.2f) rightFov=(%.2f,%.2f)",
                g_submitMode, g_scW, g_scH,
                g_eyeFov[0].angleLeft, g_eyeFov[0].angleRight,
                g_eyeFov[1].angleLeft, g_eyeFov[1].angleRight);
        }
        ++g_frames;
        if ((g_frames % 600) == 0) {
            Log("xr submit ok frames=%d shouldRender=%d layers=%u (mode=%d subMode=%d 3dCams=%d 3dGameplay=%d depthVar=%d)",
                g_frames, (int)fs.shouldRender, layerCount, g_submitMode, g_trackingSubMode,
                g_3dCamCallsThisFrame, (int)g_in3DGameplayMode, (int)g_depthHasVariance);
        }
    }

    XrFrameEndInfo fei{ XR_TYPE_FRAME_END_INFO };
    fei.displayTime = fs.predictedDisplayTime;
    fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    fei.layerCount = layerCount;
    fei.layers = layerCount ? layers : nullptr;
    const XrResult endR = xrEndFrame(g_xrSession, &fei);
    static int endFail = 0;
    if (XR_FAILED(endR) && endFail++ < 5) Log("xrEndFrame failed %d", (int)endR);

    if (g_haveViews) {
        g_renderEyePose[0] = g_eyePose[0];
        g_renderEyePose[1] = g_eyePose[1];
        g_renderEyeFov[0] = g_eyeFov[0];
        g_renderEyeFov[1] = g_eyeFov[1];
        g_renderHaveViews = true;
    }
    g_had3DCameraThisFrame = false;
    g_3dCamCallsThisFrame = 0;
    g_depthCapturedThisFrame = false;
    g_lastStage = 21;
}

// ---------------------------------------------------------------- D3D9 Proxy Hooks
typedef HRESULT(STDMETHODCALLTYPE* PFN_Present)(IDirect3DDevice9*, const RECT*, const RECT*, HWND,
                                                const RGNDATA*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_Reset)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_CreateDevice)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD,
                                                     D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
typedef HRESULT(STDMETHODCALLTYPE* PFN_CreateDeviceEx)(IDirect3D9Ex*, UINT, D3DDEVTYPE, HWND, DWORD,
                                                       D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*,
                                                       IDirect3DDevice9Ex**);
typedef HRESULT(STDMETHODCALLTYPE* PFN_EndScene)(IDirect3DDevice9*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_BeginScene)(IDirect3DDevice9*);
typedef HRESULT(STDMETHODCALLTYPE* PFN_PresentEx)(IDirect3DDevice9Ex*, const RECT*, const RECT*, HWND,
                                                  const RGNDATA*, DWORD);
typedef HRESULT(STDMETHODCALLTYPE* PFN_ResetEx)(IDirect3DDevice9Ex*, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*);

PFN_Present g_origPresent = nullptr;
PFN_Reset g_origReset = nullptr;
PFN_EndScene g_origEndScene = nullptr;
PFN_BeginScene g_origBeginScene = nullptr;
PFN_PresentEx g_origPresentEx = nullptr;
PFN_ResetEx g_origResetEx = nullptr;
PFN_CreateDevice g_origCreateDevice = nullptr;
PFN_CreateDeviceEx g_origCreateDeviceEx = nullptr;

volatile LONG g_frameId = 0;
volatile LONG g_submittedFrameId = -1;
volatile LONG g_backupFrameCounter = 0;
volatile LONG g_presentSeen = 0;
volatile LONG g_devHooked = 0;
volatile LONG g_factoryHooked = 0;

static void TweakPresentParams(D3DPRESENT_PARAMETERS* pp);

void* PatchVTable(void* obj, int index, void* repl) {
    void** vt = *reinterpret_cast<void***>(obj);
    DWORD old = 0;
    if (!VirtualProtect(&vt[index], sizeof(void*), PAGE_READWRITE, &old)) return nullptr;
    void* orig = vt[index];
    vt[index] = repl;
    VirtualProtect(&vt[index], sizeof(void*), old, &old);
    return orig;
}

HRESULT STDMETHODCALLTYPE Hook_Reset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp) {
    if (g_forceWindowedReset && pp) {
        TweakPresentParams(pp);
    }
    if (pp) {
        // keep the VR render resolution: the game resets to its own size otherwise
        if (pp->BackBufferWidth < (UINT)g_renderW) pp->BackBufferWidth = g_renderW;
        if (pp->BackBufferHeight < (UINT)g_renderH) pp->BackBufferHeight = g_renderH;
        pp->Windowed = TRUE;
    }
    if (g_sysmem) {
        g_sysmem->Release();
        g_sysmem = nullptr;
    }
    d2vr_replay::ReleaseRecording();
    g_rightColorHistory.Release();
    if (g_liveColorBackup) { g_liveColorBackup->Release(); g_liveColorBackup = nullptr; }
    ReleaseDepthResources9();
    g_anchorValid = false;
    g_vrWorldAnchorValid = false;
    g_vrRefValid = false;
    HRESULT hr = g_origReset(dev, pp);
    Log("Device Reset (%ux%u fmt=%d windowed=%d refresh=%u) -> hr=0x%08lX",
        pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0,
        pp ? (int)pp->BackBufferFormat : 0, pp ? (int)pp->Windowed : 0,
        pp ? pp->FullScreen_RefreshRateInHz : 0, hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE Hook_ResetEx(IDirect3DDevice9Ex* dev, D3DPRESENT_PARAMETERS* pp, D3DDISPLAYMODEEX* dm) {
    if (g_forceWindowedReset && pp) {
        TweakPresentParams(pp);
        dm = nullptr;
    }
    if (pp) {
        // keep the VR render resolution: the game resets to its own size otherwise
        if (pp->BackBufferWidth < (UINT)g_renderW) pp->BackBufferWidth = g_renderW;
        if (pp->BackBufferHeight < (UINT)g_renderH) pp->BackBufferHeight = g_renderH;
        pp->Windowed = TRUE;
    }
    if (g_sysmem) {
        g_sysmem->Release();
        g_sysmem = nullptr;
    }
    d2vr_replay::ReleaseRecording();
    g_rightColorHistory.Release();
    if (g_liveColorBackup) { g_liveColorBackup->Release(); g_liveColorBackup = nullptr; }
    ReleaseDepthResources9();
    g_anchorValid = false;
    g_vrWorldAnchorValid = false;
    g_vrRefValid = false;
    HRESULT hr = g_origResetEx ? g_origResetEx(dev, pp, dm) : D3D_OK;
    Log("Device ResetEx (%ux%u fmt=%d windowed=%d refresh=%u) -> hr=0x%08lX",
        pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0,
        pp ? (int)pp->BackBufferFormat : 0, pp ? (int)pp->Windowed : 0,
        pp ? pp->FullScreen_RefreshRateInHz : 0, hr);
    return hr;
}

static void VrFrame(IDirect3DDevice9* dev, const char* caller) {
    g_renderThreadId = GetCurrentThreadId();
    PollHotkeys();
    TryInstallXInputHook();

    LONG id = g_frameId;
    if (id == 0) {
        id = InterlockedIncrement(&g_backupFrameCounter);
    }

    if (InterlockedCompareExchange(&g_submittedFrameId, id, id) == id) return;
    InterlockedExchange(&g_submittedFrameId, id);

    static int frameLogs = 0;
    if (frameLogs++ < 15) {
        Log("VrFrame called by %s (frameId=%ld tid=%lu)", caller, id, g_renderThreadId);
    }

    if (InterlockedCompareExchange(&g_xrInitTried, 1, 0) == 0) InitXR();

    // True stereo: build the eye pair from the recorded frame stream BEFORE submitting.
    // Runs once per frame thanks to the g_submittedFrameId dedupe above.
    if (g_submitMode == 4 && g_stereoMode >= 1 && d2vr_replay::g_recActive) {
        RenderEyePairFromReplay(dev);
    }

    // Native engine FOV override: the avatar only exists once a level is loaded - retry.
    {
        static LONG s_fovTick = 0;
        if (g_fovOverrideDeg > 0.0f && !g_playerAvatar && (InterlockedIncrement(&s_fovTick) % 300) == 1) {
            TryApplyEngineFovOverride();
        }
    }
    SubmitXR(dev);
    if (g_submitMode == 4 && g_stereoMode >= 1) {
        d2vr_replay::RecBeginFrame(dev);   // open the recording window for the next frame
    } else if (d2vr_replay::g_recActive) {
        d2vr_replay::g_recActive = false;
        d2vr_replay::g_rec.clear();
        d2vr_replay::g_recData.clear();
    }

}

HRESULT STDMETHODCALLTYPE Hook_BeginScene(IDirect3DDevice9* dev) {
    g_renderThreadId = GetCurrentThreadId();
    PollHotkeys();
    InterlockedIncrement(&g_frameId);
    if (g_submitMode == 4 && g_stereoMode >= 1) {
        d2vr_replay::RecEmit(d2vr_replay::OP_BeginScene, 0);
    }
    HRESULT hrBS = g_origBeginScene(dev);
    if (FAILED(hrBS)) Log("!!! BeginScene FAILED hr=0x%08lX (game may stop rendering)", hrBS);
    return hrBS;
}

HRESULT STDMETHODCALLTYPE Hook_EndScene(IDirect3DDevice9* dev) {
    PollHotkeys();
    if (g_submitMode == 4 && g_stereoMode >= 1) {
        d2vr_replay::RecEmit(d2vr_replay::OP_EndScene, 0);
    }
    // Snapshot INTZ depth before EndScene completes while scene depth is still intact
    // (F5 depth visualization only - the readback is expensive)
    if (g_depthViz && g_intzTex9 && g_had3DCameraThisFrame && !g_depthCapturedThisFrame) {
        CaptureIntzDepthToD11(dev);
    }
    HRESULT hr = g_origEndScene(dev);
    if (FAILED(hr)) Log("!!! EndScene FAILED hr=0x%08lX", hr);
    if (InterlockedCompareExchange(&g_presentSeen, 0, 0) == 0) {
        VrFrame(dev, "EndScene@42");
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE Hook_Present(IDirect3DDevice9* dev, const RECT* a, const RECT* b, HWND w,
                                       const RGNDATA* r) {
    PollHotkeys();
    if (InterlockedCompareExchange(&g_presentSeen, 1, 0) == 0) {
        Log("Hook_Present@17 detected — capturing frames at Present@17 instead of EndScene@42");
    }
    InterlockedIncrement(&g_frameId);
    VrFrame(dev, "Present@17");
    return g_origPresent(dev, a, b, w, r);
}

HRESULT STDMETHODCALLTYPE Hook_PresentEx(IDirect3DDevice9Ex* dev, const RECT* a, const RECT* b, HWND w,
                                         const RGNDATA* r, DWORD flags) {
    PollHotkeys();
    if (InterlockedCompareExchange(&g_presentSeen, 1, 0) == 0) {
        Log("Hook_PresentEx@121 detected — capturing frames at PresentEx@121 instead of EndScene@42");
    }
    InterlockedIncrement(&g_frameId);
    VrFrame(dev, "PresentEx@121");
    return g_origPresentEx ? g_origPresentEx(dev, a, b, w, r, flags) : D3D_OK;
}

// The game presents through IDirect3DSwapChain9::Present (not the device) - evidence only.
typedef HRESULT(STDMETHODCALLTYPE* PFN_SwapChainPresent)(IDirect3DSwapChain9*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);
PFN_SwapChainPresent g_origSwapChainPresent = nullptr;
HRESULT STDMETHODCALLTYPE Hook_SwapChainPresent(IDirect3DSwapChain9* sc, const RECT* a, const RECT* b, HWND w,
                                                const RGNDATA* r, DWORD flags) {
    static int s_logged = 0;
    if (s_logged++ < 3) Log("SwapChain::Present detected (presentation path confirmed)");
    return g_origSwapChainPresent ? g_origSwapChainPresent(sc, a, b, w, r, flags) : D3D_OK;
}

void HookDevice(IDirect3DDevice9* dev) {
    g_renderThreadId = GetCurrentThreadId();
    if (InterlockedCompareExchange(&g_devHooked, 1, 0) != 0) return;
    g_origPresent = reinterpret_cast<PFN_Present>(PatchVTable(dev, 17, reinterpret_cast<void*>(&Hook_Present)));
    g_origReset = reinterpret_cast<PFN_Reset>(PatchVTable(dev, 16, reinterpret_cast<void*>(&Hook_Reset)));
    g_origBeginScene = reinterpret_cast<PFN_BeginScene>(
        PatchVTable(dev, 41, reinterpret_cast<void*>(&Hook_BeginScene)));
    g_origEndScene =
        reinterpret_cast<PFN_EndScene>(PatchVTable(dev, 42, reinterpret_cast<void*>(&Hook_EndScene)));

    g_origPresentEx = reinterpret_cast<PFN_PresentEx>(
        PatchVTable(dev, 121, reinterpret_cast<void*>(&Hook_PresentEx)));
    g_origResetEx = reinterpret_cast<PFN_ResetEx>(
        PatchVTable(dev, 132, reinterpret_cast<void*>(&Hook_ResetEx)));

    g_origSetVertexShaderConstantF = reinterpret_cast<PFN_SetVertexShaderConstantF>(
        PatchVTable(dev, 94, reinterpret_cast<void*>(&Hook_SetVertexShaderConstantF)));
    g_origSetDepthStencilSurface = reinterpret_cast<PFN_SetDepthStencilSurface>(
        PatchVTable(dev, 39, reinterpret_cast<void*>(&Hook_SetDepthStencilSurface)));
    g_origSetTexture = reinterpret_cast<PFN_SetTexture>(
        PatchVTable(dev, 65, reinterpret_cast<void*>(&Hook_SetTexture)));

    // True stereo: draw/state record + replay hooks (frame duplication for dual-eye render)
    d2vr_replay::g_orig.SetDepthStencilSurface = g_origSetDepthStencilSurface;
    d2vr_replay::g_orig.SetTexture = g_origSetTexture;
    d2vr_replay::g_orig.BeginScene = g_origBeginScene;
    d2vr_replay::g_orig.EndScene = g_origEndScene;
    d2vr_replay::g_orig.SetVertexShaderConstantF = g_origSetVertexShaderConstantF;
    d2vr_replay::InstallReplayHooks(dev);

    // Swapchain boundary experiment failed in-game (black output); retain EndScene
    // until the engine's final composition/presentation path is mapped.

    Log("Method 2 Hooks active on device %p: Present@17, Reset@16, SetDSS@39, BeginScene@41, EndScene@42, SetTex@65, SetVSConstF@94, PresentEx@121, ResetEx@132",
        dev);

    // Evidence check of the RE map (ENGINE_REVERSE_ENGINEERING.md): engine D3D9 device global
    __try {
        uintptr_t devGlobal = *(uintptr_t*)(0x010dee84 + 0x0c);
        Log("[FovRE] engine device global @0x010DEE90 = %p (hooked device = %p) -> %s",
            (void*)devGlobal, dev, (devGlobal == (uintptr_t)dev) ? "MATCH" : "mismatch");
    } __except (1) {
        Log("[FovRE] engine device global not readable");
    }

    // RE: render-target sizing follows the window client rect - patch the game's IAT for
    // GetClientRect (fixed slot, no ASLR) so the engine renders at the VR resolution.
    if (!g_getClientRectHooked && g_fakeClientRect) {
        __try {
            void** slot = (void**)0x00EEB478;
            void* cur = *slot;
            // sanity: the IAT slot must hold a system-DLL function
            if (cur && (uintptr_t)cur > 0x70000000) {
                g_origGetClientRect = reinterpret_cast<PFN_GetClientRect>(cur);
                DWORD old = 0;
                if (VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) {
                    *slot = reinterpret_cast<void*>(&Hook_GetClientRect);
                    VirtualProtect(slot, sizeof(void*), old, &old);
                    g_getClientRectHooked = true;
                    Log("[VrRes] GetClientRect IAT@0x00EEB478 hooked (orig=%p) -> engine renders %dx%d",
                        cur, g_renderW, g_renderH);
                }
            } else {
                Log("[VrRes] IAT slot 0x00EEB478 does not look like GetClientRect (%p) - skipped", cur);
            }
        } __except (1) {
            Log("[VrRes] GetClientRect IAT hook failed");
        }
    }
}

static void TweakPresentParams(D3DPRESENT_PARAMETERS* pp) {
    if (!pp) return;
    pp->Windowed = TRUE;
    pp->FullScreen_RefreshRateInHz = 0;
    // VR needs pixels: render the game bigger than its native 16:9 request (the eye images
    // are stretched over a ~100x100 deg frustum, every pixel counts).
    if (pp->BackBufferWidth < (UINT)g_renderW) pp->BackBufferWidth = g_renderW;
    if (pp->BackBufferHeight < (UINT)g_renderH) pp->BackBufferHeight = g_renderH;
}

HRESULT STDMETHODCALLTYPE Hook_CreateDevice(IDirect3D9* self, UINT ad, D3DDEVTYPE type, HWND hwnd,
                                            DWORD flags, D3DPRESENT_PARAMETERS* pp,
                                            IDirect3DDevice9** out) {
    Log("Hook_CreateDevice adapter=%u flags=0x%lX", ad, flags);
    TweakPresentParams(pp);
    HRESULT hr = g_origCreateDevice(self, ad, type, hwnd, flags, pp, out);
    if (SUCCEEDED(hr) && out && *out) HookDevice(*out);
    else Log("Hook_CreateDevice failed hr=0x%08lX", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE Hook_CreateDeviceEx(IDirect3D9Ex* self, UINT ad, D3DDEVTYPE type, HWND hwnd,
                                              DWORD flags, D3DPRESENT_PARAMETERS* pp,
                                              D3DDISPLAYMODEEX* fullMode, IDirect3DDevice9Ex** out) {
    if (hwnd) g_gameHwnd = hwnd;
    Log("Hook_CreateDeviceEx adapter=%u flags=0x%lX windowed=%d hwnd=%p", ad, flags,
        pp ? (int)pp->Windowed : -1, hwnd);
    TweakPresentParams(pp);
    HRESULT hr = g_origCreateDeviceEx(self, ad, type, hwnd, flags, pp, nullptr, out);
    if (SUCCEEDED(hr) && out && *out) HookDevice(*out);
    else Log("Hook_CreateDeviceEx failed hr=0x%08lX", hr);
    return hr;
}

void HookFactory(IDirect3D9* d3d) {
    if (!d3d) return;
    if (InterlockedCompareExchange(&g_factoryHooked, 1, 0) != 0) return;
    g_origCreateDevice = reinterpret_cast<PFN_CreateDevice>(
        PatchVTable(d3d, 16, reinterpret_cast<void*>(&Hook_CreateDevice)));
    g_origCreateDeviceEx = reinterpret_cast<PFN_CreateDeviceEx>(
        PatchVTable(d3d, 20, reinterpret_cast<void*>(&Hook_CreateDeviceEx)));
    Log("Factory hooked %p CreateDevice@%p CreateDeviceEx@%p", d3d, g_origCreateDevice,
        g_origCreateDeviceEx);
    TryInstallXInputHook();
}

HMODULE g_real = nullptr;
typedef IDirect3D9*(WINAPI* PFN_C9)(UINT);
typedef HRESULT(WINAPI* PFN_C9Ex)(UINT, IDirect3D9Ex**);
typedef int(WINAPI* PFN_icw)(D3DCOLOR, LPCWSTR);
typedef int(WINAPI* PFN_iv)(void);
typedef DWORD(WINAPI* PFN_dv)(void);
typedef void(WINAPI* PFN_vcw)(D3DCOLOR, LPCWSTR);
typedef BOOL(WINAPI* PFN_bv)(void);
typedef void(WINAPI* PFN_vd)(DWORD);
typedef void(WINAPI* PFN_vv)(void);

PFN_C9 p_C9 = nullptr;
PFN_C9Ex p_C9Ex = nullptr;
PFN_icw p_Begin = nullptr;
PFN_iv p_End = nullptr;
PFN_dv p_Status = nullptr;
PFN_vcw p_Marker = nullptr;
PFN_vcw p_Region = nullptr;
PFN_bv p_Repeat = nullptr;
PFN_vd p_Options = nullptr;
PFN_vv p_Mute = nullptr;

bool LoadReal() {
    char sys[MAX_PATH] = {};
    if (!GetSystemDirectoryA(sys, MAX_PATH)) return false;
    strcat_s(sys, "\\d3d9.dll");
    g_real = LoadLibraryA(sys);
    if (!g_real) return false;
    p_C9 = reinterpret_cast<PFN_C9>(GetProcAddress(g_real, "Direct3DCreate9"));
    p_C9Ex = reinterpret_cast<PFN_C9Ex>(GetProcAddress(g_real, "Direct3DCreate9Ex"));
    p_Begin = reinterpret_cast<PFN_icw>(GetProcAddress(g_real, "D3DPERF_BeginEvent"));
    p_End = reinterpret_cast<PFN_iv>(GetProcAddress(g_real, "D3DPERF_EndEvent"));
    p_Status = reinterpret_cast<PFN_dv>(GetProcAddress(g_real, "D3DPERF_GetStatus"));
    p_Marker = reinterpret_cast<PFN_vcw>(GetProcAddress(g_real, "D3DPERF_SetMarker"));
    p_Region = reinterpret_cast<PFN_vcw>(GetProcAddress(g_real, "D3DPERF_SetRegion"));
    p_Repeat = reinterpret_cast<PFN_bv>(GetProcAddress(g_real, "D3DPERF_QueryRepeatFrame"));
    p_Options = reinterpret_cast<PFN_vd>(GetProcAddress(g_real, "D3DPERF_SetOptions"));
    p_Mute = reinterpret_cast<PFN_vv>(GetProcAddress(g_real, "DebugSetMute"));
    return p_C9 != nullptr;
}

}  // namespace

extern "C" {

IDirect3D9* WINAPI Direct3DCreate9(UINT sdk) {
    if (!p_C9) return nullptr;
    Log("Direct3DCreate9(sdk=%u)", sdk);
    if (p_C9Ex) {
        IDirect3D9Ex* ex = nullptr;
        if (SUCCEEDED(p_C9Ex(sdk, &ex)) && ex) {
            Log("using IDirect3D9Ex factory via Direct3DCreate9");
            HookFactory(ex);
            return ex;
        }
    }
    IDirect3D9* d3d = p_C9(sdk);
    if (d3d) HookFactory(d3d);
    return d3d;
}

HRESULT WINAPI Direct3DCreate9Ex(UINT sdk, IDirect3D9Ex** o) {
    if (!p_C9Ex || !o) return E_INVALIDARG;
    Log("Direct3DCreate9Ex(sdk=%u)", sdk);
    IDirect3D9Ex* ex = nullptr;
    HRESULT hr = p_C9Ex(sdk, &ex);
    if (FAILED(hr)) {
        Log("Direct3DCreate9Ex failed hr=0x%08lX", hr);
        return hr;
    }
    HookFactory(ex);
    *o = ex;
    return S_OK;
}

int WINAPI D3DPERF_BeginEvent(D3DCOLOR c, LPCWSTR n) { return p_Begin ? p_Begin(c, n) : 0; }
int WINAPI D3DPERF_EndEvent(void) { return p_End ? p_End() : 0; }
DWORD WINAPI D3DPERF_GetStatus(void) { return p_Status ? p_Status() : 0; }
void WINAPI D3DPERF_SetMarker(D3DCOLOR c, LPCWSTR n) {
    if (p_Marker) p_Marker(c, n);
}
void WINAPI D3DPERF_SetRegion(D3DCOLOR c, LPCWSTR n) {
    if (p_Region) p_Region(c, n);
}
BOOL WINAPI D3DPERF_QueryRepeatFrame(void) { return p_Repeat ? p_Repeat() : FALSE; }
void WINAPI D3DPERF_SetOptions(DWORD o) {
    if (p_Options) p_Options(o);
}
void WINAPI DebugSetMute(void) {
    if (p_Mute) p_Mute();
}

}  // extern "C"

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(mod);
        LogInit();
        g_vehHandle = AddVectoredExceptionHandler(1, &CrashVehHandler);
        LoadConfig();
        Log("Darkness2VR v0.4.0 attached (pid=%lu)", GetCurrentProcessId());
        if (!LoadReal()) {
            Log("FATAL: could not load system d3d9.dll");
            return FALSE;
        }
        TryInstallXInputHook();
    } else if (reason == DLL_PROCESS_DETACH) {
        Log("Darkness2VR detaching (pid=%lu)", GetCurrentProcessId());
        if (g_vehHandle) {
            RemoveVectoredExceptionHandler(g_vehHandle);
            g_vehHandle = nullptr;
        }
    }
    return TRUE;
}

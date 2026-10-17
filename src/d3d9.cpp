// Darkness2VR v0.1.9 — D3D9/D3D9Ex proxy + OpenXR (world-anchored cinema / head-locked / projection + delay-load XInput hook).
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
// SubmitMode:
//   0 = QUAD_WORLD_ANCHOR (world-anchored cinema screen in LOCAL space, recenter with F8 or L3+R3)
//   1 = QUAD_HEAD_LOCKED  (v0.1.3 cinema screen following head center)
//   2 = PROJECTION_MONO   (v0.1.2 full-FOV stereo projection layer)
int g_submitMode = 0;
float g_cinemaDistanceM = 2.0f;
float g_cinemaWidthM = 2.1f;
int g_enableVRGamepad = 1;
int g_remapPhysical = 1;
int g_forceWindowedReset = 0; // 0 = match v0.1.3 (do not mutate pp in Hook_Reset)

// World anchor state for SubmitMode 0
bool g_anchorValid = false;
XrPosef g_anchorPose{};
XrVector3f g_anchorHeadOrigin{};

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
            fprintf(f, "=== Darkness2VR d3d9.log ===\n");
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
        fclose(f);
    }
    LeaveCriticalSection(&g_logLock);
}

void LoadConfig() {
    char localIni[MAX_PATH] = ".\\Darkness2VR.ini";
    const char* chosenIni = g_iniPath;
    if (GetFileAttributesA(localIni) != INVALID_FILE_ATTRIBUTES) {
        chosenIni = localIni;
    } else {
        // Always ensure g_iniPath exists; if missing, write default config
        if (GetFileAttributesA(g_iniPath) == INVALID_FILE_ATTRIBUTES) {
            FILE* f = nullptr;
            if (fopen_s(&f, g_iniPath, "w") == 0 && f) {
                fprintf(f, "[Render]\n");
                fprintf(f, "; TestMode: 0 = Real Game Capture (default), 1 = Solid Bright Green, 2 = Checkerboard\n");
                fprintf(f, "TestMode=0\n");
                fprintf(f, "; SubmitMode: 0 = QUAD_WORLD_ANCHOR (cinema with world anchor), 1 = QUAD_HEAD_LOCKED (v0.1.3 cinema), 2 = PROJECTION_MONO (v0.1.2 stereo projection)\n");
                fprintf(f, "SubmitMode=0\n");
                fprintf(f, "DistanceM=2.0\n");
                fprintf(f, "WidthM=2.1\n");
                fprintf(f, "ForceWindowedReset=0\n\n");
                fprintf(f, "[Input]\n");
                fprintf(f, "; EnableVRGamepad: 1 = map VR motion controllers to virtual Xbox gamepad on port 0\n");
                fprintf(f, "EnableVRGamepad=1\n");
                fprintf(f, "; RemapPhysicalPorts: 1 = remap physical Xbox pad on port 1..3 to port 0 if port 0 is empty\n");
                fprintf(f, "RemapPhysicalPorts=1\n");
                fclose(f);
            }
        }
    }

    g_testMode = GetPrivateProfileIntA("Render", "TestMode", 0, chosenIni);
    g_submitMode = GetPrivateProfileIntA("Render", "SubmitMode", 0, chosenIni);
    g_forceWindowedReset = GetPrivateProfileIntA("Render", "ForceWindowedReset", 0, chosenIni);
    char buf[64]{};
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

    Log("Config loaded from: %s", chosenIni);
    Log("  Render: TestMode=%d SubmitMode=%d Distance=%.2fm Width=%.2fm ForceWindowedReset=%d",
        g_testMode, g_submitMode, g_cinemaDistanceM, g_cinemaWidthM, g_forceWindowedReset);
    Log("  Input: EnableVRGamepad=%d RemapPhysicalPorts=%d", g_enableVRGamepad, g_remapPhysical);
    Log("  Hotkeys: F8 = Recenter Cinema Anchor | F9 = cycle TestMode (0,1,2) | F10 = cycle SubmitMode (0,1,2) | F11 = cycle Distance (1.5-3.0m)");
}

void PollHotkeys() {
    static DWORD s_lastF8 = 0;
    static DWORD s_lastF9 = 0;
    static DWORD s_lastF10 = 0;
    static DWORD s_lastF11 = 0;

    DWORD now = GetTickCount();

    if ((GetAsyncKeyState(VK_F8) & 0x8000) && (now - s_lastF8 > 400)) {
        s_lastF8 = now;
        g_anchorValid = false;
        Beep(1000, 50);
        Log(">>> [HOTKEY F8] Cinema World Anchor recenter requested <<<");
    }

    if ((GetAsyncKeyState(VK_F9) & 0x8000) && (now - s_lastF9 > 400)) {
        s_lastF9 = now;
        g_testMode = (g_testMode + 1) % 3;
        Beep(700 + g_testMode * 200, 50);
        const char* names[] = { "0 (Real Game Capture)", "1 (Solid Bright Green)", "2 (Checkerboard)" };
        Log(">>> [HOTKEY F9] TestMode changed to: %s <<<", names[g_testMode]);
    }

    if ((GetAsyncKeyState(VK_F10) & 0x8000) && (now - s_lastF10 > 400)) {
        s_lastF10 = now;
        g_submitMode = (g_submitMode + 1) % 3;
        g_anchorValid = false;
        Beep(600 + g_submitMode * 200, 50);
        const char* names[] = {
            "0 (QUAD_WORLD_ANCHOR - Anchored Cinema)",
            "1 (QUAD_HEAD_LOCKED - v0.1.3 Cinema)",
            "2 (PROJECTION_MONO - v0.1.2 Stereo Projection)"
        };
        Log(">>> [HOTKEY F10] SubmitMode changed to: %s <<<", names[g_submitMode]);
    }

    if ((GetAsyncKeyState(VK_F11) & 0x8000) && (now - s_lastF11 > 400)) {
        s_lastF11 = now;
        g_cinemaDistanceM = (g_cinemaDistanceM >= 3.0f) ? 1.5f : (g_cinemaDistanceM + 0.5f);
        g_anchorValid = false;
        Beep(900, 50);
        Log(">>> [HOTKEY F11] Cinema Distance changed to: %.1fm <<<", g_cinemaDistanceM);
    }
}

// ---------------------------------------------------------------- OpenXR
XrInstance g_xrInstance = XR_NULL_HANDLE;
XrSession g_xrSession = XR_NULL_HANDLE;
XrSpace g_xrSpace = XR_NULL_HANDLE;      // LOCAL space
XrSpace g_viewSpace = XR_NULL_HANDLE;    // VIEW space
XrSystemId g_xrSystem = XR_NULL_SYSTEM_ID;
ID3D11Device* g_dev11 = nullptr;
ID3D11DeviceContext* g_ctx11 = nullptr;
bool g_xrReady = false;
bool g_xrRunning = false;

XrSwapchain g_swapchain = XR_NULL_HANDLE;
uint32_t g_scW = 0, g_scH = 0;
ID3D11Texture2D** g_scImages = nullptr;
uint32_t g_scCount = 0;

IDirect3DSurface9* g_sysmem = nullptr;
ID3D11Texture2D* g_upload = nullptr;
UINT g_srcW = 0, g_srcH = 0;
D3DFORMAT g_srcFmt = D3DFMT_UNKNOWN;

XrPosef g_eyePose[2]{};
XrFovf g_eyeFov[2]{};
bool g_haveViews = false;
int g_frames = 0;
volatile LONG g_xrInitTried = 0;

static XrVector3f RotVec(const XrQuaternionf& q, float x, float y, float z) {
    const float qx = q.x, qy = q.y, qz = q.z, qw = q.w;
    const float tx = 2.f * (qy * z - qz * y);
    const float ty = 2.f * (qz * x - qx * z);
    const float tz = 2.f * (qx * y - qy * x);
    return { x + qw * tx + (qy * tz - qz * ty), y + qw * ty + (qz * tx - qx * tz),
             z + qw * tz + (qx * ty - qy * tx) };
}

static XrPosef HeadCenterPose(const XrView* views, uint32_t n) {
    XrPosef c = views[0].pose;
    if (n >= 2) {
        c.position.x = (views[0].pose.position.x + views[1].pose.position.x) * 0.5f;
        c.position.y = (views[0].pose.position.y + views[1].pose.position.y) * 0.5f;
        c.position.z = (views[0].pose.position.z + views[1].pose.position.z) * 0.5f;
    }
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

// Computes a horizon-level world-anchored cinema pose in LOCAL space from the current head pose.
// Keeps only horizontal Yaw so the cinema screen is never tilted by head pitch/roll.
static XrPosef ComputeLevelWorldAnchorPose(const XrPosef& head) {
    const XrVector3f fwd = RotVec(head.orientation, 0.f, 0.f, -1.f);
    float fx = fwd.x;
    float fz = fwd.z;
    float len = std::sqrt(fx * fx + fz * fz);
    if (len < 1e-4f) {
        fx = 0.f;
        fz = -1.f;
        len = 1.f;
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

HWND g_gameHwnd = nullptr;
XINPUT_STATE g_cachedXrPad{};
XINPUT_GAMEPAD g_lastGamepadState{};
volatile LONG g_cachedXrPadValid = 0;

// OpenXR → virtual Xbox pad (VR controllers)
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

    // Oculus Touch & SteamVR PSVR2 controller profile
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

    // Valve Index
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

    // HP WMR
    const char* wmr = "/interaction_profiles/hp/mixed_reality_controller";
    SuggestBindings(wmr, touchB, static_cast<uint32_t>(sizeof(touchB) / sizeof(touchB[0])));

    // Simple Controller
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

    // L3 + R3 combo on VR controllers recenters the cinema world anchor
    static DWORD s_lastPadRecenter = 0;
    if ((gp.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) && (gp.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)) {
        DWORD now = GetTickCount();
        if (now - s_lastPadRecenter > 600) {
            s_lastPadRecenter = now;
            g_anchorValid = false;
            Beep(1000, 50);
            Log(">>> [VR PAD L3+R3] Cinema World Anchor recenter requested <<<");
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

    if (g_enableVRGamepad && userIndex == 0 && InterlockedCompareExchange(&g_cachedXrPadValid, 0, 0)) {
        if (r == ERROR_SUCCESS && PadHasInput(real.Gamepad)) {
            *state = real;
            return ERROR_SUCCESS;
        }
        *state = g_cachedXrPad;
        return ERROR_SUCCESS;
    }

    if (r == ERROR_SUCCESS) {
        *state = real;
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
                    Log("XR session running (TestMode=%d SubmitMode=%d)", g_testMode, g_submitMode);
                }
            } else if (s->state == XR_SESSION_STATE_STOPPING) {
                xrEndSession(g_xrSession);
                g_xrRunning = false;
            }
        }
        ev = { XR_TYPE_EVENT_DATA_BUFFER };
    }
}

bool EnsureSwapchain(UINT w, UINT h) {
    if (g_swapchain && w == g_scW && h == g_scH) return true;
    if (g_swapchain) {
        xrDestroySwapchain(g_swapchain);
        g_swapchain = XR_NULL_HANDLE;
    }
    if (g_scImages) {
        delete[] g_scImages;
        g_scImages = nullptr;
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

    XrSwapchainCreateInfo sci{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
    sci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    sci.format = fmt;
    sci.sampleCount = 1;
    sci.width = w;
    sci.height = h;
    sci.faceCount = 1;
    sci.arraySize = 1;
    sci.mipCount = 1;
    if (XR_FAILED(xrCreateSwapchain(g_xrSession, &sci, &g_swapchain))) return false;

    xrEnumerateSwapchainImages(g_swapchain, 0, &g_scCount, nullptr);
    auto* imgs = new XrSwapchainImageD3D11KHR[g_scCount];
    for (uint32_t i = 0; i < g_scCount; ++i) imgs[i] = { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR };
    xrEnumerateSwapchainImages(g_swapchain, g_scCount, &g_scCount,
                               reinterpret_cast<XrSwapchainImageBaseHeader*>(imgs));
    g_scImages = new ID3D11Texture2D*[g_scCount];
    for (uint32_t i = 0; i < g_scCount; ++i) g_scImages[i] = imgs[i].texture;
    delete[] imgs;
    g_scW = w;
    g_scH = h;
    Log("Swapchain created %ux%u count=%u", w, h, g_scCount);
    return true;
}

bool EnsureUploadTexture(UINT w, UINT h) {
    if (g_upload && w == g_srcW && h == g_srcH) return true;
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
    g_srcW = w;
    g_srcH = h;
    return true;
}

bool EnsureCopy(IDirect3DDevice9* dev, UINT w, UINT h, D3DFORMAT fmt) {
    if (!EnsureUploadTexture(w, h)) return false;
    if (g_sysmem && w == g_srcW && h == g_srcH && fmt == g_srcFmt) return true;
    if (g_sysmem) {
        g_sysmem->Release();
        g_sysmem = nullptr;
    }
    if (FAILED(dev->CreateOffscreenPlainSurface(w, h, fmt, D3DPOOL_SYSTEMMEM, &g_sysmem, nullptr))) {
        Log("CreateOffscreenPlainSurface failed (%ux%u fmt=%d)", w, h, (int)fmt);
        return false;
    }
    g_srcFmt = fmt;
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
                row[x] = 0xFF00FF00; // Solid Bright Green
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
    IDirect3DSurface9* back = nullptr;
    if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) || !back) return false;
    D3DSURFACE_DESC sd{};
    back->GetDesc(&sd);
    if (!EnsureCopy(dev, sd.Width, sd.Height, sd.Format)) {
        back->Release();
        return false;
    }

    const char* srcMethod = "BackBuffer0:GetRenderTargetData";
    HRESULT hr = dev->GetRenderTargetData(back, g_sysmem);
    if (FAILED(hr)) {
        srcMethod = "BackBuffer0:GetFrontBufferData";
        hr = dev->GetFrontBufferData(0, g_sysmem);
    }
    back->Release();
    if (FAILED(hr)) {
        static int failLog = 0;
        if (failLog++ < 5) {
            Log("capture failed hr=0x%08lX (%ux%u fmt=%d)", hr, sd.Width, sd.Height, (int)sd.Format);
        }
        return false;
    }

    D3DLOCKED_RECT lr{};
    if (FAILED(g_sysmem->LockRect(&lr, nullptr, D3DLOCK_READONLY))) return false;

    uint8_t avgR = 0, avgG = 0, avgB = 0, maxR = 0, maxG = 0, maxB = 0;
    SampleSurfaceStats(lr, sd.Width, sd.Height, &avgR, &avgG, &avgB, &maxR, &maxG, &maxB);

    // If BackBuffer(0) is 100% pitch black, check if RenderTarget(0) is a separate active surface with pixels!
    if (maxR == 0 && maxG == 0 && maxB == 0) {
        g_sysmem->UnlockRect();
        IDirect3DSurface9* rt0 = nullptr;
        if (SUCCEEDED(dev->GetRenderTarget(0, &rt0)) && rt0) {
            D3DSURFACE_DESC rtd{};
            rt0->GetDesc(&rtd);
            if (rtd.Width == sd.Width && rtd.Height == sd.Height && rtd.Format == sd.Format &&
                rtd.MultiSampleType == D3DMULTISAMPLE_NONE) {
                if (SUCCEEDED(dev->GetRenderTargetData(rt0, g_sysmem)) &&
                    SUCCEEDED(g_sysmem->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
                    uint8_t rtAvgR = 0, rtAvgG = 0, rtAvgB = 0, rtMaxR = 0, rtMaxG = 0, rtMaxB = 0;
                    SampleSurfaceStats(lr, sd.Width, sd.Height, &rtAvgR, &rtAvgG, &rtAvgB, &rtMaxR, &rtMaxG, &rtMaxB);
                    if (rtMaxR > 0 || rtMaxG > 0 || rtMaxB > 0) {
                        avgR = rtAvgR; avgG = rtAvgG; avgB = rtAvgB;
                        maxR = rtMaxR; maxG = rtMaxG; maxB = rtMaxB;
                        srcMethod = "RenderTarget0:GetRenderTargetData";
                    }
                    rt0->Release();
                    goto have_locked_rect;
                }
            }
            rt0->Release();
        }
        if (FAILED(g_sysmem->LockRect(&lr, nullptr, D3DLOCK_READONLY))) return false;
    }

have_locked_rect:
    static int s_statCounter = 0;
    if (++s_statCounter % 60 == 0) {
        Log("capture stats [frame %d]: %ux%u (via %s) mean RGB=(%u,%u,%u) max RGB=(%u,%u,%u)",
            s_statCounter, sd.Width, sd.Height, srcMethod,
            avgR, avgG, avgB, maxR, maxG, maxB);
        if (maxR == 0 && maxG == 0 && maxB == 0) {
            Log("WARNING: captured surface is 100%% pitch black (all samples 0)!");
        }
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
    return true;
}

void SubmitXR(IDirect3DDevice9* dev) {
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

    const XrCompositionLayerBaseHeader* layers[1] = { nullptr };
    uint32_t layerCount = 0;
    XrCompositionLayerQuad quad{ XR_TYPE_COMPOSITION_LAYER_QUAD };
    XrCompositionLayerProjection proj{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    XrCompositionLayerProjectionView projViews[2]{};

    bool havePixels = false;
    if (fs.shouldRender && g_haveViews) {
        if (g_testMode > 0) {
            UINT tw = g_srcW ? g_srcW : 1920;
            UINT th = g_srcH ? g_srcH : 1080;
            havePixels = FillTestPattern(tw, th, g_testMode) && EnsureSwapchain(tw, th);
        } else if (dev) {
            havePixels = CopyFrame(dev) && EnsureSwapchain(g_srcW, g_srcH);
        }
    }

    if (havePixels) {
        uint32_t idx = 0;
        XrSwapchainImageAcquireInfo ai{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
        if (XR_SUCCEEDED(xrAcquireSwapchainImage(g_swapchain, &ai, &idx))) {
            XrSwapchainImageWaitInfo wi{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
            wi.timeout = XR_INFINITE_DURATION;
            if (XR_SUCCEEDED(xrWaitSwapchainImage(g_swapchain, &wi))) {
                g_ctx11->CopyResource(g_scImages[idx], g_upload);
                XrSwapchainImageReleaseInfo ri{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
                xrReleaseSwapchainImage(g_swapchain, &ri);

                const float aspect = g_scH ? (float)g_scW / (float)g_scH : (16.f / 9.f);
                XrView vcopy[2] = { { XR_TYPE_VIEW }, { XR_TYPE_VIEW } };
                for (int i = 0; i < 2; ++i) {
                    vcopy[i].pose = g_eyePose[i];
                    vcopy[i].fov = g_eyeFov[i];
                }
                const XrPosef head = HeadCenterPose(vcopy, 2);

                if (g_submitMode == 0) {
                    // 0: QUAD_WORLD_ANCHOR (horizon-level world-anchored cinema screen in LOCAL space)
                    // Re-latch on initial warmup frames (0, 30, 120) or if head moved >1.2m from anchor origin
                    float dx = head.position.x - g_anchorHeadOrigin.x;
                    float dy = head.position.y - g_anchorHeadOrigin.y;
                    float dz = head.position.z - g_anchorHeadOrigin.z;
                    float distSq = dx * dx + dy * dy + dz * dz;
                    if (!g_anchorValid || g_frames == 30 || g_frames == 120 || distSq > (1.2f * 1.2f)) {
                        g_anchorPose = ComputeLevelWorldAnchorPose(head);
                        g_anchorHeadOrigin = head.position;
                        g_anchorValid = true;
                        Log("Cinema World Anchor latched at pos=(%.2f, %.2f, %.2f) orient=(%.2f, %.2f, %.2f, %.2f) (frame=%d)",
                            g_anchorPose.position.x, g_anchorPose.position.y, g_anchorPose.position.z,
                            g_anchorPose.orientation.x, g_anchorPose.orientation.y,
                            g_anchorPose.orientation.z, g_anchorPose.orientation.w, g_frames);
                    }

                    quad.layerFlags = 0;
                    quad.space = g_xrSpace;
                    quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                    quad.subImage.swapchain = g_swapchain;
                    quad.subImage.imageRect.offset = { 0, 0 };
                    quad.subImage.imageRect.extent = { (int32_t)g_scW, (int32_t)g_scH };
                    quad.pose = g_anchorPose;
                    quad.size = { g_cinemaWidthM, g_cinemaWidthM / aspect };
                    layers[0] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
                    layerCount = 1;
                } else if (g_submitMode == 1) {
                    // 1: QUAD_HEAD_LOCKED (exact v0.1.3 style: quad in LOCAL space tracking head center)
                    quad.layerFlags = 0;
                    quad.space = g_xrSpace;
                    quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                    quad.subImage.swapchain = g_swapchain;
                    quad.subImage.imageRect.offset = { 0, 0 };
                    quad.subImage.imageRect.extent = { (int32_t)g_scW, (int32_t)g_scH };
                    quad.pose = CinemaQuadPose(head);
                    quad.size = { g_cinemaWidthM, g_cinemaWidthM / aspect };
                    layers[0] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
                    layerCount = 1;
                } else {
                    // 2: PROJECTION_MONO (exact v0.1.2 style: full projection layer)
                    for (int i = 0; i < 2; ++i) {
                        projViews[i] = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
                        projViews[i].pose = g_eyePose[i];
                        projViews[i].fov = g_eyeFov[i];
                        projViews[i].subImage.swapchain = g_swapchain;
                        projViews[i].subImage.imageRect.offset = { 0, 0 };
                        projViews[i].subImage.imageRect.extent = { (int32_t)g_scW, (int32_t)g_scH };
                    }
                    proj.layerFlags = 0;
                    proj.space = g_xrSpace;
                    proj.viewCount = 2;
                    proj.views = projViews;
                    layers[0] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&proj);
                    layerCount = 1;
                }

                if (g_frames == 0) {
                    Log("*** FIRST FRAME SUBMITTED *** mode=%d (test=%d) distance=%.1fm size=%.2fx%.2fm",
                        g_submitMode, g_testMode, g_cinemaDistanceM, g_cinemaWidthM, g_cinemaWidthM / aspect);
                }
                ++g_frames;
                if ((g_frames % 600) == 0) {
                    Log("xr submit ok frames=%d shouldRender=%d layers=%u (mode=%d test=%d)",
                        g_frames, (int)fs.shouldRender, layerCount, g_submitMode, g_testMode);
                }
            } else {
                XrSwapchainImageReleaseInfo ri{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
                xrReleaseSwapchainImage(g_swapchain, &ri);
            }
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
    // CRITICAL (proven from v0.1.3 vs v0.1.4..v0.1.8 & DarknessII.exe 0x00B3DD90 disassembly):
    // Do NOT mutate pp->Windowed in Hook_Reset by default! DarknessII.exe checks this->presentParams.Windowed
    // at 0x00B3E05E immediately after Reset() returns and configures its fullscreen swapchain state from it.
    if (g_forceWindowedReset && pp) {
        TweakPresentParams(pp);
    }
    if (g_sysmem) {
        g_sysmem->Release();
        g_sysmem = nullptr;
    }
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
    if (g_sysmem) {
        g_sysmem->Release();
        g_sysmem = nullptr;
    }
    HRESULT hr = g_origResetEx ? g_origResetEx(dev, pp, dm) : D3D_OK;
    Log("Device ResetEx (%ux%u fmt=%d windowed=%d refresh=%u) -> hr=0x%08lX",
        pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0,
        pp ? (int)pp->BackBufferFormat : 0, pp ? (int)pp->Windowed : 0,
        pp ? pp->FullScreen_RefreshRateInHz : 0, hr);
    return hr;
}

static void VrFrame(IDirect3DDevice9* dev, const char* caller) {
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
        Log("VrFrame called by %s (frameId=%ld)", caller, id);
    }

    if (InterlockedCompareExchange(&g_xrInitTried, 1, 0) == 0) InitXR();
    SubmitXR(dev);
}

HRESULT STDMETHODCALLTYPE Hook_BeginScene(IDirect3DDevice9* dev) {
    PollHotkeys();
    InterlockedIncrement(&g_frameId);
    return g_origBeginScene(dev);
}

HRESULT STDMETHODCALLTYPE Hook_EndScene(IDirect3DDevice9* dev) {
    PollHotkeys();
    HRESULT hr = g_origEndScene(dev);
    // Only capture in EndScene if Present/PresentEx is not being called directly on the device
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

void HookDevice(IDirect3DDevice9* dev) {
    if (InterlockedCompareExchange(&g_devHooked, 1, 0) != 0) return;
    g_origPresent = reinterpret_cast<PFN_Present>(PatchVTable(dev, 17, reinterpret_cast<void*>(&Hook_Present)));
    g_origReset = reinterpret_cast<PFN_Reset>(PatchVTable(dev, 16, reinterpret_cast<void*>(&Hook_Reset)));
    g_origBeginScene = reinterpret_cast<PFN_BeginScene>(
        PatchVTable(dev, 41, reinterpret_cast<void*>(&Hook_BeginScene)));
    g_origEndScene =
        reinterpret_cast<PFN_EndScene>(PatchVTable(dev, 42, reinterpret_cast<void*>(&Hook_EndScene)));

    // Also hook PresentEx (slot 121) and ResetEx (slot 132) for IDirect3DDevice9Ex
    g_origPresentEx = reinterpret_cast<PFN_PresentEx>(
        PatchVTable(dev, 121, reinterpret_cast<void*>(&Hook_PresentEx)));
    g_origResetEx = reinterpret_cast<PFN_ResetEx>(
        PatchVTable(dev, 132, reinterpret_cast<void*>(&Hook_ResetEx)));

    Log("Present@17, Reset@16, BeginScene@41, EndScene@42, PresentEx@121, ResetEx@132 hooked on device %p (origPresentEx=%p)",
        dev, g_origPresentEx);
}

static void TweakPresentParams(D3DPRESENT_PARAMETERS* pp) {
    if (!pp) return;
    pp->Windowed = TRUE;
    pp->FullScreen_RefreshRateInHz = 0;
    if (pp->BackBufferWidth < 1280) pp->BackBufferWidth = 1280;
    if (pp->BackBufferHeight < 720) pp->BackBufferHeight = 720;
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
    // IDirect3D9Ex vtable: CreateDeviceEx is slot 20
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
        LoadConfig();
        Log("Darkness2VR v0.1.9 attached (pid=%lu)", GetCurrentProcessId());
        if (!LoadReal()) {
            Log("FATAL: could not load system d3d9.dll");
            return FALSE;
        }
        TryInstallXInputHook();
    }
    return TRUE;
}

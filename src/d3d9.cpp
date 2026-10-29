// Darkness2VR v0.4.0 — D3D9/D3D9Ex proxy + OpenXR
// Method 2: Frustum-Aligned Same-Frame Stereo 3D (D3D11 Stereo Shift + INTZ Depth + Comfort Profile)
// Log: %LOCALAPPDATA%\Darkness2VR\d3d9.log
// Config: %LOCALAPPDATA%\Darkness2VR\Darkness2VR.ini or beside DarknessII.exe

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "shared_reader_fence.h"
#include <delayimp.h>
#include <shlobj.h>
#include <d3d9.h>
#include "query_preload_pool.h"
#include <d3d11.h>
#include "stereo_transfer_worker.h"
#include <dxgi.h>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <string>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "xr_display_refresh.h"
#include "steamvr_refresh_native.h"
#include "adaptive_fps.h"
#include "vr_settings.h"

#include <xinput.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <intrin.h>
#include <unordered_set>
#include <atomic>
#include "persistent_log.h"
#include "async_log.h"
#include "small_record_copy.h"
#include "native_constant_source.h"
#include "native_constant_gate.h"
#include "startup_shader_warmup.h"
#include "native_image_guard.h"
#include "native_menu_input_snapshot.h"
#include "vr_input.h"
#include "native_aim_math.h"
#include "stereo_geometry.h"
#include "composite_shader_patch.h"
#include "nis_upscale.h"
#include "frame_boundary.h"
static bool g_protectedHeadTracking=false; // OFF; GPU view only, native visibility separate
#ifdef D2VR_MATCHED_CAPTURE
#include "capture_present_witness.h"
#include "diagnostic_present_patch.h"
#include "tracked_gameplay_gate.h"
#include "native_camera_cover.h"
#include "native_view_boundary.h"
#include "native_view_witness.h"
#include "native_projection_upload.h"
#include "native_execution_boundary.h"
#include "native_culling_cover.h"
#include "native_view_clone_plan.h"
#include "native_view_instance.h"
#include "native_throw_clone_plan.h"
#include "native_throw_compare.h"
#include "native_weapon_bridge.h"
#include "native_weapon_prefix_plan.h"
static int g_nativeViewBoundaryMode=0; // 0 OFF, 1 observe, 2 HW boundary, 3 experimental owned method copies
static d2vr_native_view::Boundary g_nativeViewBoundary;
static d2vr_native_execution::Boundary g_nativeFrustumBoundary;
// Process-lifetime code/tables after publication. QPC boundary pins this DLL;
// never free callable copies or dereference a native instance on unload.
static d2vr_native_clone::Code* g_nativeViewCode=nullptr;
static d2vr_native_view::InstanceTable g_nativeViewTables[2];
static bool g_nativeTranslatedViewPrepared=false;
#include "exception_record_witness.h"
#include "image_present_observer.h"
#include "protected_xinput.h"
#include "protected_source_size.h"
static d2vr_source_quality::Mode g_sourceQualityMode=d2vr_source_quality::Mode::Off;
static d2vr_source_quality::ClientSize g_protectedSourceSize;
static void ObserveProtectedSourceSize(unsigned w,unsigned h,unsigned format) {
    if(g_sourceQualityMode!=d2vr_source_quality::Mode::Off) g_protectedSourceSize.Source(w,h,format);
}
static bool g_imagePreservingPresent=false;
static bool g_imagePreservingInput=false;
static bool g_protectedGestureActions=false;
static bool g_protectedLeftAim=false;
static bool g_protectedLeftThrow=false;
static bool g_protectedTrackedHands=false;
static d2vr_input::Tentacles g_protectedRightTentacle;
static volatile LONG g_protectedRecenterRequested=0;
static d2vr_protected_input::ExternalXInput g_protectedXInput;
static d2vr_protected_input::Grips g_protectedGrips;
static d2vr_image_present::Observer g_imagePresent;
static bool g_restoredTrackedGameplay = false;
static thread_local d2vr_capture::PresentWitness g_capturePresent;
static bool g_presentReadiness = false;
static thread_local uintptr_t g_presentCaller = 0;
static volatile LONG g_captureResetGeneration=0, g_captureResetting=0;
#endif
#include "render_pipeline_trace.inl"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "xinput.lib")

#include "psvr2/IpcClient.h"
#include "psvr2/HapticDelivery.h"
#include "psvr2/ShotEvents.h"

namespace {

CRITICAL_SECTION g_logLock;
d2vr_log::File g_logFile;
d2vr_log::Async g_asyncLog;
d2vr_log::Emergency g_emergencyLog;
bool g_asyncLogRequested = false; // opt-in, started only at HookDevice outside loader lock
std::atomic<bool> g_asyncLogAdmissionFailed{false};
char g_logPath[MAX_PATH] = {};
char g_iniPath[MAX_PATH] = {};
std::atomic<bool> g_logReady{false};
d2vr_native_image::Guard g_nativeImageGuard; // startup-latched; diagnostic OFF by default

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
bool g_disableDepthOfField = true;
bool g_disableCompositeHeightFog = false;
bool g_disableFxaa = true;
// Diagnostic shader policies are immutable after startup; creation hooks may
// run on native worker threads and must not reopen configuration files.
bool g_shadowScreenDepth = false;
bool g_shaderDebugDump = false;
bool g_clipScaledUi = true;
bool g_fixMotionVectorProjection = false;
bool g_worldDiffuseAnisotropy = false;
bool g_gestureBridge = false; // diagnostic detection until native context/weapon state is verified
bool g_adaptiveTriggers = true;
bool g_tentacleBridge = false; // separate, opt-in stock-game action bridge
bool g_tentacleAimMarker=true,g_tentacleAimVisible=false;
bool g_nativeLeftAim=false;
bool g_nativeWeaponPose=false;
bool g_nativeHandsEnabled=false;
bool g_nativeHandsDirtyBoth=false;
static bool g_nativeHandsPackInstalled=false;
#ifdef D2VR_MATCHED_CAPTURE
static PVOID volatile g_nativeHandsSelectedResource=nullptr;
#endif
float g_throwAimAssist=.20f;
static void ResetNativeAim();
static void ResetNativeThrow();
static void RecordRecenterDiagnostic(const char* source);
static void ResetNativeWeaponPoses();
static bool ReadNativeAimDirection(float out[3]);
static bool NativeHandsAvailable();
static bool NativeHandsFromWeapon(void* model,DWORD cameraVtable);
static void CaptureClickRecenterReference();
bool g_aimMarkerValid[2]{};
float g_aimMarkerUV[2][2]{};
bool g_headGeometry = true; // Enabled on startup; Ctrl+F8 toggles, F8/L3+R3 recentres.
bool g_headGeometryReferenceValid = false;
bool g_lastWorldHeadContextValid = false;
bool g_recenterHeadLock = false;
int g_textureAnisotropy = 16;
bool g_samplerCapsCacheEnabled = false; // Render.SamplerCapsCache: isolated A/B, default legacy query path
static bool g_startupShaderWarmup = false; // OFF; valid-device startup only, no native slot writes
static bool g_startupShaderPool = false; // OFF; startup-owned shader objects, transferred once
static unsigned g_startupShaderCopies=1; // independent startup creation references per exact bytecode
static bool g_reuseStateSnapshots=false; // refresh frame-start state via Capture, release before Reset
static unsigned g_queryPreloadCount=0; // bounded unused occlusion queries; startup only
static bool g_skipShadowAtlasDraws = false; // diagnostic A/B only; changes shadow image
static std::atomic<bool> g_skipObjectDraws{false}; // diagnostic: omit triangle batches >2 primitives
static bool g_objectDrawPadToggle = false; // diagnostic chord override; F8 retains recenter
static bool g_submissionPadToggle = false; // image-preserving replay submission A/B
static bool g_submissionPackage = false; // include wide owned capture + frame refs in A/B
static bool g_submissionStatus = false;
static std::atomic<bool> g_wideRecordCopy{false};
static std::atomic<bool> g_directRecordCopy{false}; // diagnostic opt-in; 0 restores legacy copies
static bool g_triangleHoldRecenter = false; // short triangle tap retains menu
static volatile LONG g_objectDrawToggleRequests = 0;
static bool g_replayBindingFilter = false; // independent opt-in until PSVR2 acceptance
static bool g_replayStateFilter = false; // independent opt-in until PSVR2 acceptance
static bool g_replayBufferVersions = false; // quarantined after b6d2bfbf runtime regression; restart required
static bool g_replayConstantBatch = false; // OFF; adjacent replay float4 uploads only
static bool g_replayConstantWindow = false; // OFF; independent follow-up, not part of adjacent A/B
static bool g_replayConstantBindWindow = false; // OFF; float packets may cross inert material bindings
static bool g_nativeOwnedConstantCopyRequested = false;
static bool g_nativeOwnedMaterialCopyRequested = false;

// True stereo (SubmitMode 4): frame-record/replay dual-eye rendering
// StereoMode: 0 = off (mono live pass), 1 = dual-eye (live = eye0 + replay = eye1),
//             2 = validation replay with zero shift (image must match live pass)
int g_stereoMode = 1;
int g_hookMask = 15;               // bit0=record hooks, bit1=swapchain hook, bit2=vs analysis, bit3=constF extras
float g_stereoSep = 0.020f;      // shear coefficient per eye (screen-space) - shift mode 1
float g_stereoConv = 2.0f;       // convergence distance in clip-w (world) units - shift mode 1
int g_stereoShiftMode = 0;       // 0 = translation (VR-correct), 1 = shear (HelixMod)
float g_stereoIPD = 0.065f;      // full interpupillary distance in world units - shift mode 0
float g_gameUnitsPerMeter = 1.0f; // metres -> game units for OpenXR-driven IPD (sweep in tests)
int g_stereoIpdSource = 0;        // 0 = ini/manual, 1 = xrLocateViews inter-eye distance
// StereoDebugMode: 0=Normal 1=ZeroIPD 2=DuplicateLeft 3=DuplicateRight 4=SwapEyes
int g_stereoDebugMode = 0;
int g_stereoLayerPose = 0;       // 0=head center (clip-shift textures), 1=per-eye OpenXR pose
int g_nativeStereoHeadGeometry = 0; // 0=OpenXR only for mode 4 (no headClip on WVP)
static volatile LONG g_stereoPresentBoundarySeen = 0;
static d2vr_frame::BoundaryMachine g_frameBoundary(d2vr_frame::Policy::TraceOnly);
static int g_frameBoundaryPolicy = 2; // TraceOnly until Present path is proven (see FrameBoundaryPolicy ini)
static int g_renderPipelineTrace = 0;
// PresentHookLevel: 0=pass-through only (orig then optional counter), 1=+boundary FSM, 2=+CSV trace (post-present).
static int g_presentHookLevel = 0;
static volatile LONG g_swapChainPresentCount = 0;
static volatile LONG g_devicePresentCount = 0;
static volatile LONG g_swapChainPresentMaxDepth = 0;
static bool g_enginePresentInstalled = false;
static XrPosef g_pairSubmitHead{};
static XrPosef g_pairSubmitEyePose[2]{};
static XrFovf g_pairSubmitFov[2]{};
static bool g_pairSubmitMetaValid = false;
float g_fovWiden = 2.5f;         // rendered FOV multiplier on tan(FOV/2) (manual, F11)
float g_fovWidenX = 2.5f;        // effective per-axis widen (auto from the HMD frustum)
float g_fovWidenY = 2.5f;
float g_fovCoverX = 1.0f;        // widen that exactly covers the HMD (declared FOV)
float g_fovCoverY = 1.0f;
static float g_fovCoverEyeX[2] = {1.f, 1.f}, g_fovCoverEyeY[2] = {1.f, 1.f};
static float g_fovWidenEyeX[2] = {1.f, 1.f}, g_fovWidenEyeY[2] = {1.f, 1.f};
static float g_perEyeCropX[2] = {1.f, 1.f}, g_perEyeCropY[2] = {1.f, 1.f};
float g_edgeCrop = 0.0f;         // render wider than the HMD so the compositor crops
                                 // the outermost band (undefined deferred lighting there)
int g_autoFovWiden = 1;          // 1 = derive the widen from xrLocateViews FOV (exact coverage)
float g_autoFovCoverage = 1.0f;  // 0.5..1.0 fraction of the HMD half-angle to cover
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
float g_fovOverrideDeg = 0.0f;   // native engine vertical FOV override (0 = disabled; experimental path, opt-in only)
float g_engineFovFactor = 1.0f;    // engine FOV multiplier (1 = untouched)
volatile LONG g_trueStereoFrames = 0;
int g_blitPass = 0;                // 1 = RenderStereoEyeD11 does a 1:1 blit (true-stereo mode)
bool g_eyePairReady = false;       // both eye images captured this frame (SubmitMode 4)
uint64_t g_eyePairFrameId = 0;
uint32_t g_eyePairW = 0, g_eyePairH = 0;
int g_dumpEyeFrames = 0;           // F12: dump N eye pairs to test-output/*.bmp for offline analysis
int g_dumpDelay = 300;             // wait this many frames before dumping (skip black startup)
bool g_uniqueReplayRefs = false; // opt-in per-frame COM lifetime dedup; no draw/state elision
int g_zeroCopyEnabled = 0;         // Config: Render.ZeroCopy (0 = CPU CopyFrame, 1 = D3D9Ex shared texture)
int g_projectionFix = 0;          // Rejected lighting/FOV experiment; opt-in diagnostics only.
uint32_t g_diagnosticSkipVs = 0;
int g_lightPosShift = 0;  // F5: shift the shadow-light PS c0 light position per eye
int g_skipLightVsShift = 0;  // F6: do not shift the light-volume VS constants (world = depth frame)
int g_noInvProj = 0;         // F7: no InverseProjection compensation (raw reconstruction)
int g_lightingPreset = -1;  // -1 configured behavior; 0..9 = Shift+1..0 experiments.
int g_pendingLightingPreset = -1;
char g_lightingChoicePath[MAX_PATH]{};
static const char* const kLightingPresetNames[10] = {
    "baseline: full correction", "raw PS: no inverse/extents correction",
    "inverse only: raw ViewExtents", "ViewExtents only: raw inverse",
    "inverse FOV only: no eye translation", "inverse eye translation only: no FOV scaling",
    "inverse reversed eye translation", "no ScreenTextureProjection correction",
    "ScreenTextureProjection FOV only", "view-space inverse only: preserve world-space fog"
};
int g_diagnosticSameEye = -1; // -1 normal, 0/1 use that camera for both passes.
float g_diagnosticFovWiden = 0.0f; // >=1 forces render widen; 0 uses normal configuration.
int g_scale2DContent = 1;         // keep 2D UI/sprites at their native angular size/position
int g_renderW = 2560;              // Config: Render.RenderWidth (VR backbuffer forced size)
int g_renderH = 1440;              // Config: Render.RenderHeight
int g_fakeClientRect = 1;          // report the VR size to the engine via GetClientRect (RE_FINDINGS)
static bool g_diagnosticProductionResolution = false; // explicit diagnostic opt-in, never enables FOV/gameplay hooks
static HWND g_renderSizeHwnd = nullptr;
typedef BOOL(WINAPI* PFN_GetClientRect)(HWND, LPRECT);
static PFN_GetClientRect g_origGetClientRect = nullptr;

// RE (RE_FINDINGS.md): the engine sizes ALL its render targets from the window client rect -
// Manager::SetHwnd (0x00C96440) calls GetClientRect and stores the size at +0x18C/+0x190,
// which feeds FullScreenSizeX/Y (0x010E318C/0x010E31B8) and the RT creation. Forcing only the
// D3D9 backbuffer changed nothing (RT0 stayed 1920x1071). Lying about the client size makes
// the whole engine render at the VR resolution.
static BOOL WINAPI Hook_GetClientRect(HWND hwnd, LPRECT rc) {
    BOOL ok = g_origGetClientRect ? g_origGetClientRect(hwnd, rc) : FALSE;
    if (ok && rc && hwnd == g_renderSizeHwnd && g_fakeClientRect && g_renderW >= 1280) {
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
bool g_lockHeadPosition = true;   // anchor the layer position (see LockedViewPose)
float g_lockRadiusM = 0.5f;       // how far the head may drift before the anchor follows
float g_viewScale = 1.45f;        // FOV view scale / pull-back factor (F4 cycles: 1.30x, 1.45x, 1.60x, 1.75x, 1.15x, 1.00x)
float g_stereoStrength = 1.0f;   // Multiplier on stereo shift (F11 cycles 1.0, 1.5, 2.0, 0.5, 0.25, 0.0)
float g_stereoBaseShift = 0.0110f; // Base per-eye horizontal shift in UV (F6 cycles)
float g_stereoDepthScale = 0.0220f; // Depth-modulated per-eye shift in UV (+47% boost)
float g_stereoMaxDisp = 0.038f;  // Hard safety clamp on per-eye UV shift
int g_depthViz = 0;              // F5 toggles live depth map visualization
float g_cinemaDistanceM = 2.0f;
float g_cinemaWidthM = 2.1f;
int g_enableVRGamepad = 1;
static int g_vrSettingsToggleKey=VK_INSERT; // [Input] VRSettingsKey; 0 disables
int g_remapPhysical = 1;
int g_forceWindowedReset = 0;

// Runtime recommendation includes its configured supersampling/lens overhead.
uint32_t g_steamVrRecW = 0;
uint32_t g_steamVrRecH = 0;
uint32_t g_xrMaxEyeW = 0, g_xrMaxEyeH = 0;
bool g_matchEyeResolutionToSource = false;

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
// Startup isolation (Render.StartupMode in INI). See docs/research/startup-crash.md.
int g_startupMode = 0;            // 0=default safe path, 1=skip native exe hooks, 2=skip FOV exe patches
int g_fovFrustumHook = 0;         // 0=off (10-byte patch @B23F7F risky at intro); 1=enable when validated
int g_hideGameWindow = 0;         // opt-in: hide desktop window when XR runs
int g_suppressDesktopPresent = 0; // opt-in: skip desktop Present when OpenXR has the frame
int g_disableOpenXR = 0;          // research: never call InitXR / xrWaitFrame (trace-only runs)
static d2vr_adaptive::Mode g_adaptiveFPSMode=d2vr_adaptive::Mode::Off;
static volatile LONG g_fovProbeInstalled = 0;
static volatile LONG g_xrSubmittedThisFrame = 0;
static volatile LONG g_needRecBegin = 1; // Next native surface write/BeginScene, never mid-frame EndScene.
static bool g_preSceneWriteMissed=false;

void LogInit() {
    InitializeCriticalSection(&g_logLock);
    char base[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, base))) {
        strcat_s(base, "\\Darkness2VR");
        CreateDirectoryA(base, nullptr);
        sprintf_s(g_logPath, "%s\\d3d9.log", base);
        sprintf_s(g_iniPath, "%s\\Darkness2VR.ini", base);
        sprintf_s(g_lightingChoicePath, "%s\\last-lighting.ini", base);

        if (g_logFile.Open(g_logPath)) {
            g_logFile.Write("=== Darkness2VR v0.4.0 (Method 2 Same-Frame Stereo Shift) d3d9.log ===");
            g_logReady = true;
        }
    }
}

void Log(const char* fmt, ...) {
    if (!g_logReady) return;
    va_list ap; va_start(ap, fmt);
    if (g_asyncLog.Started()) {
        g_asyncLog.WriteV(fmt, ap);
    } else {
        // Startup/default remain synchronous. Recheck after taking the startup
        // lock so a racing producer cannot touch FILE after worker publication.
        EnterCriticalSection(&g_logLock);
        if (g_logReady) {
            if (g_asyncLog.Started()) g_asyncLog.WriteV(fmt, ap);
            else g_logFile.WriteV(fmt, ap);
        }
        LeaveCriticalSection(&g_logLock);
    }
    va_end(ap);
}

static void LogEmergency(const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    g_emergencyLog.WriteV(fmt, args);
    va_end(args);
}

// Native VEH only, for a NOT_ADMITTED async start with no emergency handle.
// Ordinary Log stays disabled. No worker exists that could own this FILE.
static void LogNativeAdmissionFallback(const char* fmt, ...) {
    va_list args; va_start(args, fmt);
    EnterCriticalSection(&g_logLock);
    if (g_asyncLogAdmissionFailed.load(std::memory_order_acquire) &&
        !g_asyncLog.Started() && !g_emergencyLog.Ready())
        g_logFile.WriteV(fmt, args);
    LeaveCriticalSection(&g_logLock);
    va_end(args);
}

static bool AsyncLogSink(void* context, const char* text, size_t size) {
    return static_cast<d2vr_log::File*>(context)->WriteLine(text, size);
}
static void StartAsyncLogging() {
    static volatile LONG attempted = 0;
    if (!g_asyncLogRequested || !g_logReady || InterlockedCompareExchange(&attempted, 1, 0)) return;
    // HookDevice only; never DllMain. Complete existing startup writes before
    // publishing the pinned worker, then release this lock permanently from
    // the ordinary producer path. The worker never acquires g_logLock.
    EnterCriticalSection(&g_logLock);
    char emergencyPath[MAX_PATH]; strcpy_s(emergencyPath, g_logPath);
    char* slash = strrchr(emergencyPath, '\\');
    if (slash) strcpy_s(slash + 1, sizeof(emergencyPath) - (slash + 1 - emergencyPath), "d3d9-emergency.log");
    const bool emergencyReady = slash && g_emergencyLog.Open(emergencyPath);
    if (emergencyReady && g_asyncLog.Start(AsyncLogSink, &g_logFile)) {
        g_asyncLog.Write("[AsyncLog] active capacity=%u lineBytes=%u emergency=%s; overflow/truncation counted",
            d2vr_log::Async::Capacity, d2vr_log::Async::LineBytes, emergencyPath);
    } else {
        // Failure is a startup event, before any async publication. Do not
        // silently fall back to render-thread I/O after an explicit opt-in.
        g_asyncLogAdmissionFailed = true;
        g_logFile.Write("[AsyncLog] NOT_ADMITTED: start failed; ordinary logging disabled; emergencyReady=%d", emergencyReady);
        g_emergencyLog.Write("[AsyncLog] NOT_ADMITTED: start failed; ordinary logging disabled");
        g_logReady = false;
    }
    LeaveCriticalSection(&g_logLock);
}

static bool NativeImageWriteAllowed(uintptr_t target, size_t length) {
    if (g_nativeImageGuard.Allows(target,length)) return true;
    Log("[NativeImageGuard] REFUSED image write target=%p bytes=%zu; native hash inputs must remain original",
        reinterpret_cast<void*>(target),length);
    return false;
}

static void LogStartupStage(int stage, const char* name) {
    InterlockedExchange(&g_lastStage, stage);
    Log("[Startup] stage=%d %s", stage, name);
}

// First-chance Vectored Exception Handler to log exact crash address, registers, and call stack
LONG WINAPI CrashVehHandler(PEXCEPTION_POINTERS ep) {
    if (!ep || !ep->ExceptionRecord || !ep->ContextRecord) return EXCEPTION_CONTINUE_SEARCH;
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    // This includes MSVC C++ 0xE06D7363. A first-chance notification alone
    // does not establish an unhandled crash; do not discard its provenance.
    if (code < 0xC0000000) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (code == 0xC0000420 /* DBG_PRINTEXCEPTION_C */) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // Independent preopened sink before any ordinary logger/module lookup.
    // E06D7363 is deliberately excluded: handled C++ remains ordinary queue work.
    g_emergencyLog.Record(*ep->ExceptionRecord, *ep->ContextRecord,
        g_renderThreadId, g_lastStage, g_lastHookId);

    const bool nativeAdmissionFallback = d2vr_log::EmergencyException(code) &&
        g_asyncLogAdmissionFailed.load(std::memory_order_acquire) &&
        !g_asyncLog.Started() && !g_emergencyLog.Ready();
    void (*write)(const char*, ...) = Log;
    if (d2vr_log::EmergencyException(code) && g_emergencyLog.Ready()) write = LogEmergency;
    else if (nativeAdmissionFallback) write = LogNativeAdmissionFallback;

    static volatile LONG s_crashLogged = 0, s_asyncNativeLogged = 0, s_asyncCppLogged = 0;
    volatile LONG* budget = &s_crashLogged; // preserve default shared three-record policy
    if (g_asyncLog.Started() && g_emergencyLog.Ready())
        budget = d2vr_log::EmergencyException(code) ? &s_asyncNativeLogged : &s_asyncCppLogged;
    else if (nativeAdmissionFallback) budget = &s_asyncNativeLogged;
    // Explicit async admission/failure cannot lose native detail to handled C++.
    if (InterlockedIncrement(budget) <= 3) {
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
        write("!!! EXCEPTION 0x%08lX at %p (%s + 0x%lX) | tid=%lu renderTid=%lu stage=%ld lastHook=%ld !!!",
            code, addr, modName, (unsigned long)((uintptr_t)addr - modBase),
            GetCurrentThreadId(), g_renderThreadId, g_lastStage, g_lastHookId);
        write("  (first-chance: may be handled by our SEH guards - game continues)");
#ifdef D2VR_MATCHED_CAPTURE
        d2vr_exception_witness::LogRecord(*ep->ExceptionRecord, write);
#endif
        if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
            write("  AccessViolation type=%lu targetAddr=%p",
                (unsigned long)ep->ExceptionRecord->ExceptionInformation[0],
                reinterpret_cast<void*>(ep->ExceptionRecord->ExceptionInformation[1]));
#ifdef D2VR_MATCHED_CAPTURE
            MEMORY_BASIC_INFORMATION region{};
            const void* target=reinterpret_cast<const void*>(ep->ExceptionRecord->ExceptionInformation[1]);
            if(VirtualQuery(target,&region,sizeof(region))==sizeof(region))
                write("[ExceptionWitness] targetPage base=%p allocation=%p size=%lu state=%08lX protect=%08lX type=%08lX",
                    region.BaseAddress,region.AllocationBase,(unsigned long)region.RegionSize,
                    region.State,region.Protect,region.Type);
#endif
        }
        write("  EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX ESI=%08lX EDI=%08lX EBP=%08lX ESP=%08lX EIP=%08lX",
            c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp, c->Eip);

        const uintptr_t* stack = reinterpret_cast<const uintptr_t*>(c->Esp);
        write("  Call Stack (ESP=%p):", stack);
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
                    write("    [%02d] %08lX (%s + 0x%lX)", i, (unsigned long)retVal, retModName, (unsigned long)(retVal - retModBase));
                } else if (retVal > 0x10000) {
                    write("    [%02d] %08lX", i, (unsigned long)retVal);
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                break;
            }
        }
#ifdef D2VR_MATCHED_CAPTURE
        // The raw stack scan above is not an unwind. Follow only a bounded,
        // monotonic x86 EBP chain and label it as a candidate; omit FPO frames.
        uintptr_t frame=c->Ebp;
        for(int depth=0;depth<12;++depth) {
            if(frame<c->Esp || frame-c->Esp>0x10000 || (frame&3)) break;
            __try {
                const auto words=reinterpret_cast<const uintptr_t*>(frame);
                const uintptr_t next=words[0],returnAddress=words[1];
                write("[ExceptionWitness] ebpFrame=%d ebp=%p return=%p next=%p",
                    depth,(void*)frame,(void*)returnAddress,(void*)next);
                if(next<=frame || next-frame>0x10000 || (next&3)) break;
                frame=next;
            } __except(EXCEPTION_EXECUTE_HANDLER) { break; }
        }
#endif
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

#ifdef D2VR_MATCHED_CAPTURE
static std::string g_matchedIniPath;
static volatile LONG g_cameraUploadWitnessBudget=0;
#endif
static int g_nisMode = 0; // 0=off, 2=eye-cache sharpen, 3=shared-source scaler
static float g_nisSharpness = .20f;
static bool g_headAttachedTentacles = false;
static bool g_gpuSplitTiming = false;
static bool g_smallRecordCopyRequested = false; // OFF; bounded <=192-byte RPM experiment
static bool g_smallRecordCopyAdmitted = false;
static d2vr_record_copy::Admission g_smallRecordCopyAdmission;
static d2vr_record_copy::RuntimeControl g_smallRecordCopyControl;
static inline bool SmallRecordCopyIsEnabled() {
#ifdef D2VR_MATCHED_CAPTURE
    return g_smallRecordCopyControl.Effective(g_smallRecordCopyAdmitted);
#else
    return g_smallRecordCopyAdmitted;
#endif
}

static bool g_lastConsumerReaderFence = false; // OFF by default; isolated timing hypothesis
static bool g_deferStereoTransferReader = false; // startup-only; exact reader fence before shared reuse
static int g_stereoTransferMode = 0; // 0 baseline,1 D3D9/11 worker,2 serial MT control,3 D3D11-only worker
static bool g_livePassProbe = false; // explicit diagnostic only; never poll per-object RT state by default
// A diagnostic lighting preset can invert only the deferred eye offset. A
// tracked profile must choose the matching configured reconstruction explicitly;
// never alter or delete the user's persisted choice to make that profile load.
static int EffectiveSavedLightingKey(int storedKey, bool restore) {
    return restore ? storedKey : -1;
}
static bool ProtectedHeadConfigurationAllowed() {
    return g_headGeometry && g_nativeStereoHeadGeometry && g_stereoShiftMode==0 &&
        g_projectionFix==3 && g_stereoLayerPose==0 && g_headLookMode==0 &&
        !g_noInvProj && !g_skipLightVsShift && !g_lightPosShift && g_lightingPreset<=0;
}
void LoadConfig() {
    g_shadowScreenDepth=GetPrivateProfileIntA("Diagnostics","ShadowScreenDepth",0,
        ".\\Darkness2VR-diagnostic.ini")==1;
    g_shaderDebugDump=GetPrivateProfileIntA("Diagnostics","ShaderDebugDump",0,
        ".\\Darkness2VR-diagnostic.ini")==1;
    Log("[ShaderDiagnostics] ShadowScreenDepth=%d ShaderDebugDump=%d (startup only)",
        int(g_shadowScreenDepth),int(g_shaderDebugDump));
#ifdef D2VR_MATCHED_CAPTURE
    g_smallRecordCopyControl.Configure(GetPrivateProfileIntA("Diagnostics",
        "SmallRecordCopyLiveControl",0,".\\Darkness2VR-diagnostic.ini")!=0);
#endif
    g_livePassProbe=GetPrivateProfileIntA("Diagnostics","LivePassProbe",0,
        ".\\Darkness2VR-diagnostic.ini")!=0;
    Log("[LivePassProbe] enabled=%d (diagnostic per-upload RT queries)",(int)g_livePassProbe);
#ifdef D2VR_MATCHED_CAPTURE
    // Read once, before any installer or trace worker is started. Asynchronous
    // INI edits must never toggle patch/rollback ownership.
    g_imagePreservingPresent=GetPrivateProfileIntA("Diagnostics","ImagePreservingPresent",0,".\\Darkness2VR-diagnostic.ini")!=0;
    g_protectedHeadTracking=GetPrivateProfileIntA("Diagnostics","ProtectedHeadTracking",0,".\\Darkness2VR-diagnostic.ini")!=0;
    g_imagePreservingInput=GetPrivateProfileIntA("Diagnostics","ImagePreservingInput",0,".\\Darkness2VR-diagnostic.ini")!=0;
    g_protectedGestureActions=GetPrivateProfileIntA("Diagnostics","ProtectedGestureActions",0,".\\Darkness2VR-diagnostic.ini")!=0;
    g_protectedLeftAim=GetPrivateProfileIntA("Diagnostics","ProtectedLeftAim",0,".\\Darkness2VR-diagnostic.ini")!=0;
    g_protectedLeftThrow=GetPrivateProfileIntA("Diagnostics","ProtectedLeftThrow",0,".\\Darkness2VR-diagnostic.ini")!=0;
    g_protectedTrackedHands=GetPrivateProfileIntA("Diagnostics","ProtectedTrackedHands",0,".\\Darkness2VR-diagnostic.ini")!=0;
    Log("[ProtectedLeftAim] requested=%d; private getter, legacy NativeLeftAim stays OFF; separate ProtectedLeftThrow=%d",int(g_protectedLeftAim),int(g_protectedLeftThrow));
    Log("[ProtectedHands] requested=%d; private selected-resource packer only; legacy hands/weapon hooks remain independent",int(g_protectedTrackedHands));
    if (g_imagePreservingPresent || GetPrivateProfileIntA("Diagnostics","BlockNativeImagePatches",0,".\\Darkness2VR-diagnostic.ini")) {
        const auto image=reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
        const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS32*>(image+dos->e_lfanew);
        g_nativeImageGuard={true,reinterpret_cast<uintptr_t>(image),pe->OptionalHeader.SizeOfImage};
        Log("[NativeImageGuard] enabled: externalObserverRequested=%d; native EXE/IAT/static vtable writes forbidden; renderer parameters unchanged",g_imagePreservingPresent);
    }
#endif
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

    g_asyncLogRequested = GetPrivateProfileIntA("Render", "AsyncLog", 0, chosenIni) != 0;
    char adaptive[32]{};
    GetPrivateProfileStringA("Render","AdaptiveFPS","OFF",adaptive,sizeof(adaptive),chosenIni);
    g_adaptiveFPSMode=(!_stricmp(adaptive,"AUTO") || !strcmp(adaptive,"1")) ? d2vr_adaptive::Mode::Auto:
        (!_stricmp(adaptive,"HALF") || !strcmp(adaptive,"2")) ? d2vr_adaptive::Mode::Half:d2vr_adaptive::Mode::Off;
    if(g_adaptiveFPSMode!=d2vr_adaptive::Mode::Off)
        Log("[AdaptiveFPS] requested=%s; runtime pacing adapter unavailable; will report unsupported, never sleep or skip XR frames",
            g_adaptiveFPSMode==d2vr_adaptive::Mode::Auto ? "AUTO":"HALF");
    Log("[AsyncLog] requested=%d; startup synchronous, worker starts only at HookDevice", (int)g_asyncLogRequested);
    g_testMode = GetPrivateProfileIntA("Render", "TestMode", 0, chosenIni);
    g_submitMode = GetPrivateProfileIntA("Render", "SubmitMode", 0, chosenIni);
    if (g_submitMode < 0 || g_submitMode > 4) g_submitMode = 0;
    g_textureAnisotropy = std::clamp((int)GetPrivateProfileIntA("Render", "TextureAnisotropy", 16, chosenIni), 1, 16);
    g_samplerCapsCacheEnabled = GetPrivateProfileIntA("Render", "SamplerCapsCache", 0, chosenIni) != 0;
    Log("[SamplerCaps] cache=%d (OFF retains per-call GetDeviceCaps)", (int)g_samplerCapsCacheEnabled);
    g_worldDiffuseAnisotropy = GetPrivateProfileIntA("Render", "WorldDiffuseAnisotropy", 0, chosenIni) != 0;
    Log("[WorldDiffuse] scoped anisotropy=%d (exact VS293F456B/PSCA08BDC4, s0 only)", (int)g_worldDiffuseAnisotropy);
    Log("[Sharpness] texture anisotropy target=%d", g_textureAnisotropy);
    g_disableFxaa = GetPrivateProfileIntA("Render", "DisableFXAA", 1, chosenIni) != 0;
    const int requestedNisMode=GetPrivateProfileIntA("Render", "NisMode", 0, chosenIni);
    g_nisMode=(requestedNisMode==2 || requestedNisMode==3) ? requestedNisMode : 0;
    char nisSharpness[32]{};
    GetPrivateProfileStringA("Render", "NisSharpness", "0.20", nisSharpness, sizeof(nisSharpness), chosenIni);
    g_nisSharpness = nis_upscale::ClampSharpness((float)atof(nisSharpness));
    Log("[NIS] mode=%d sharpness=%.2f (2=cache sharpen, 3=shared-source scale)",g_nisMode,g_nisSharpness);
    g_headAttachedTentacles = GetPrivateProfileIntA("Render", "HeadAttachedTentacles", 0, chosenIni)!=0;
    g_gpuSplitTiming = GetPrivateProfileIntA("Render", "GpuSplitTiming", 0, chosenIni)!=0;
    g_smallRecordCopyRequested = GetPrivateProfileIntA("Render", "SmallRecordCopy", 0, chosenIni)!=0;
    g_startupShaderWarmup = GetPrivateProfileIntA("Render", "StartupShaderWarmup", 0, chosenIni)!=0;
    g_startupShaderPool = GetPrivateProfileIntA("Render", "StartupShaderPool", 0, chosenIni)!=0;
    g_startupShaderCopies=GetPrivateProfileIntA("Render","StartupShaderCopies",1,chosenIni)==2?2:1;
    g_reuseStateSnapshots=GetPrivateProfileIntA("Render","ReuseStateSnapshots",0,chosenIni)!=0;
    g_queryPreloadCount=GetPrivateProfileIntA("Render","QueryPreloadCount",0,chosenIni);
    if(g_queryPreloadCount>d2vr_query_preload::MaxQueries)g_queryPreloadCount=0;
    g_skipShadowAtlasDraws = GetPrivateProfileIntA("Render", "SkipShadowAtlasDraws", 0, chosenIni)!=0;
    g_skipObjectDraws = GetPrivateProfileIntA("Render", "SkipObjectDraws", 0, chosenIni)!=0;
    g_objectDrawPadToggle = GetPrivateProfileIntA("Render", "ObjectDrawPadToggle", 0, chosenIni)!=0;
    g_submissionPadToggle = GetPrivateProfileIntA("Render", "SubmissionPadToggle", 0, chosenIni)!=0;
    g_submissionPackage = GetPrivateProfileIntA("Render", "SubmissionPackage", 0, chosenIni)!=0;
    g_submissionStatus = GetPrivateProfileIntA("Render", "SubmissionStatus", 0, chosenIni)!=0;
    g_directRecordCopy=false;
#ifdef D2VR_MATCHED_CAPTURE
    g_directRecordCopy = GetPrivateProfileIntA("Render", "DirectRecordCopy", 0, ".\\Darkness2VR-diagnostic.ini")!=0;
#endif
    Log("[DirectRecordCopy] enabled=%d; diagnostic opt-in; fault fallback retained; PAGE_GUARD restoration/concurrency not runtime-verified",
        int(g_directRecordCopy.load()));
    g_wideRecordCopy = GetPrivateProfileIntA("Render", "WideRecordCopy", 0, chosenIni)!=0;
    if(g_wideRecordCopy) Log("[WideRecordCopy] requested=1 limit=65536; requires SmallRecordCopy guard admission; exact-count RPM, legacy fallback; no permission cache");
    g_triangleHoldRecenter = GetPrivateProfileIntA("Render", "TriangleHoldRecenter", 0, chosenIni)!=0;
    if(g_triangleHoldRecenter) Log("[RecenterInput] triangle hold 1s=recenter; short release=menu");
    if(g_submissionPadToggle) {
        g_objectDrawPadToggle=false;g_skipObjectDraws=false;g_skipShadowAtlasDraws=false;
        Log("[SubmissionAB] L3+R3 armed; geometry retained, F8 recenter; draw omission disabled");
    }
    if(g_objectDrawPadToggle) Log("[ObjectDrawTest] L3+R3 toggle armed; replaces chord recenter, F8 still recenters; switch applies at next frame boundary");
    if(g_skipObjectDraws) Log("[ObjectDrawTest] enabled=1; omit triangle batches >2 primitives in both eyes; includes world geometry, CPU preparation retained");
    if(g_skipShadowAtlasDraws) Log("[ShadowDrawTest] enabled=1; omit RGB565 square subviewport atlas draws in both eyes; heuristic coverage, altered lighting; CPU preparation retained");
    g_replayBindingFilter = GetPrivateProfileIntA("Render", "ReplayBindingFilter", 0, chosenIni)!=0;
    Log("[ReplayBindingFilter] enabled=%d; replay-local resource/shader binding filtering",int(g_replayBindingFilter));
    g_replayStateFilter = GetPrivateProfileIntA("Render", "ReplayStateFilter", 0, chosenIni)!=0;
    g_replayBufferVersions = GetPrivateProfileIntA("Render", "ReplayBufferVersions", 0, chosenIni)!=0;
    Log("[ReplayStateFilter] enabled=%d; independent replay-local scalar filtering",int(g_replayStateFilter));
    Log("[ReplayBufferVersions] enabled=%d; startup-only; quarantined after reported hang/CPU regression",int(g_replayBufferVersions));
    g_replayConstantBatch = GetPrivateProfileIntA("Render", "ReplayConstantBatch", 0, chosenIni)!=0;
    g_replayConstantWindow = GetPrivateProfileIntA("Render", "ReplayConstantWindow", 0, chosenIni)!=0;
    g_replayConstantBindWindow = GetPrivateProfileIntA("Render", "ReplayConstantBindWindow", 0, chosenIni)!=0;
    g_nativeOwnedConstantCopyRequested = GetPrivateProfileIntA("Render", "NativeOwnedConstantCopy", 0, chosenIni)!=0;
    d2vr_native_constants::enabled = g_nativeOwnedConstantCopyRequested;
    g_nativeOwnedMaterialCopyRequested = GetPrivateProfileIntA("Render", "NativeOwnedMaterialCopy", 0, chosenIni)!=0;
    d2vr_native_constants::materialEnabled = g_nativeOwnedMaterialCopyRequested;
    if(g_replayConstantBindWindow) Log("[ReplayConstantBindWindow] requested=1; independent bindings + validated public render states; draw/resource/stateblock/vendor-command barriers retained");
    if(g_replayConstantWindow) Log("[ReplayConstantWindow] requested=1; requires ReplayConstantBatch and no active matched capture");
    if(g_replayConstantBatch) Log("[ReplayConstantBatch] enabled: adjacent float4 packets only; capture keeps original calls");
    g_lastConsumerReaderFence = GetPrivateProfileIntA("Render", "LastConsumerReaderFence", 0, chosenIni)!=0;
    g_stereoTransferMode = GetPrivateProfileIntA("Render", "StereoTransferWorker", 0, chosenIni);
    g_deferStereoTransferReader = GetPrivateProfileIntA("Render", "DeferStereoTransferReader", 0, chosenIni)!=0;
    Log("[StereoTransferWorker] deferReader=%d requires worker mode=1 and LastConsumerReaderFence=1",int(g_deferStereoTransferReader));
    if(g_stereoTransferMode<0 || g_stereoTransferMode>3) g_stereoTransferMode=0;
    Log("[StereoTransferWorker] requestedMode=%d (startup only; 2=serial MT control; 3=D3D11-only worker/native D3D9 flags)",g_stereoTransferMode);
    Log("[SharedReader] lastConsumerFence=%d (0=legacy overwrite-time fence)",(int)g_lastConsumerReaderFence);
    g_gestureBridge = GetPrivateProfileIntA("Input", "GestureGamepadBridge", 0, chosenIni) != 0;
    g_adaptiveTriggers = GetPrivateProfileIntA("Input", "AdaptiveTriggers", 1, chosenIni) != 0;
    g_tentacleBridge = GetPrivateProfileIntA("Input", "TentacleGestureBridge", 0, chosenIni) != 0;
    g_tentacleAimMarker = GetPrivateProfileIntA("Input", "TentacleAimMarker", 1, chosenIni) != 0;
    g_nativeLeftAim = GetPrivateProfileIntA("Input", "NativeLeftAim", 0, chosenIni) != 0;
    g_nativeWeaponPose = GetPrivateProfileIntA("Input", "NativeWeaponPose", 0, chosenIni) != 0;
    g_nativeHandsEnabled = GetPrivateProfileIntA("Input", "NativeTrackedHands", 1, chosenIni) != 0;
    g_nativeHandsDirtyBoth = GetPrivateProfileIntA("Input", "NativeHandsDirtyBoth", 0, chosenIni) != 0;
    g_throwAimAssist = std::min(100u,GetPrivateProfileIntA("Input", "ThrowAimAssist", 20, chosenIni))*.01f;
    g_headGeometry = GetPrivateProfileIntA("Render", "HeadGeometry", 1, chosenIni) != 0;
    Log("[Tentacle] bridge=%d axisMarker=%d; native grab observer and tracked pistol configured separately",(int)g_tentacleBridge,(int)g_tentacleAimMarker);
    g_clipScaledUi = GetPrivateProfileIntA("Render", "ClipScaledUI", 1, chosenIni) != 0;
    Log("[Sharpness] DisableFXAA=%d ClipScaledUI=%d", (int)g_disableFxaa, (int)g_clipScaledUi);
    g_disableDepthOfField = GetPrivateProfileIntA("Render", "DisableDepthOfField", 1, chosenIni) != 0;
    g_disableCompositeHeightFog = GetPrivateProfileIntA("Render", "DisableCompositeHeightFog", 0, chosenIni) != 0;
    Log("[Composite] DisableCompositeHeightFog=%d (exact PS33DE98C1; restart required)",
        (int)g_disableCompositeHeightFog);
    g_fixMotionVectorProjection = GetPrivateProfileIntA("Render", "FixMotionVectorProjection", 0, chosenIni) != 0;
    Log("[MotionVectors] FixMotionVectorProjection=%d (verified 73EBA724; head geometry excluded)",
        (int)g_fixMotionVectorProjection);
    Log("[Sharpness] DisableDepthOfField=%d (known native-stereo composites)", (int)g_disableDepthOfField);
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
#ifdef D2VR_MATCHED_CAPTURE
    g_nativeViewBoundaryMode=GetPrivateProfileIntA("Render","NativeViewBoundary",0,chosenIni);
    if(g_nativeViewBoundaryMode<0 || g_nativeViewBoundaryMode>3)g_nativeViewBoundaryMode=0;
#endif
    {
        g_startupMode = GetPrivateProfileIntA("Render", "StartupMode", 0, chosenIni);
    }
    if (g_startupMode < 0 || g_startupMode > 2) g_startupMode = 0;
    g_fovFrustumHook = GetPrivateProfileIntA("Render", "FovFrustumHook", 0, chosenIni) ? 1 : 0;
    g_hideGameWindow = GetPrivateProfileIntA("Render", "HideGameWindow", 0, chosenIni) ? 1 : 0;
    g_suppressDesktopPresent = GetPrivateProfileIntA("Render", "SuppressDesktopPresent", 0, chosenIni) ? 1 : 0;
    char fovBuf[32]{};
    if (GetPrivateProfileStringA("Render", "AutoFovCoverage", "1.0", fovBuf, sizeof(fovBuf), chosenIni)) {
        const float v = (float)atof(fovBuf);
        if (v >= 0.5f && v <= 1.0f) g_autoFovCoverage = v;
    }
    g_scale2DContent = GetPrivateProfileIntA("Render", "Scale2DContent", 1, chosenIni) ? 1 : 0;
    {
        char cropBuf[32]{};
        if (GetPrivateProfileStringA("Render", "EdgeCrop", "0.0", cropBuf, sizeof(cropBuf), chosenIni)) {
            const float v = (float)atof(cropBuf);
            if (v >= 0.0f && v <= 0.25f) g_edgeCrop = v;
        }
    }
    g_zeroCopyEnabled = GetPrivateProfileIntA("Render", "ZeroCopy", 0, chosenIni) ? 1 : 0;
    g_uniqueReplayRefs = GetPrivateProfileIntA("Render", "UniqueReplayResourceRefs", 0, chosenIni) != 0;
    g_matchEyeResolutionToSource = GetPrivateProfileIntA("Render", "MatchEyeResolutionToSource", 0, chosenIni) != 0;
    // Bit 1 = named VS/ScreenTextureProjection handling, bit 2 = PS reconstruction
    // compensation (InverseProjection + ViewExtents). Both are required for correct
    // deferred lighting in the two-eye render; the Diagnostics bridge can override.
    g_projectionFix = GetPrivateProfileIntA("Render", "ProjectionFix", 3, chosenIni) & 3;
    g_renderW = GetPrivateProfileIntA("Render", "RenderWidth", 2560, chosenIni);
    g_renderH = GetPrivateProfileIntA("Render", "RenderHeight", 1440, chosenIni);
#ifdef D2VR_MATCHED_CAPTURE
    const unsigned sourceQualityRequested=GetPrivateProfileIntA("Render","SourceQuality",0,chosenIni);
    g_sourceQualityMode=d2vr_source_quality::Parse(sourceQualityRequested);
    if(sourceQualityRequested>4) Log("[SourceQuality] invalid mode=%u; OFF retained",sourceQualityRequested);
#endif
    g_stateBlockHookEnabled = GetPrivateProfileIntA("Render", "StateBlockHook", 0, chosenIni) != 0;
    if (g_renderW < 1280 || g_renderW > 7680) g_renderW = 2560;
    if (g_renderH < 720 || g_renderH > 4320) g_renderH = 1440;
#ifdef D2VR_MATCHED_CAPTURE
    if(g_sourceQualityMode==d2vr_source_quality::Mode::Exact)
        Log("[NativeSourceTarget] requested=%dx%d per eye; engine sizing mode=Exact; actual geometry/depth/output verification required",g_renderW,g_renderH);
#endif

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
    g_stereoIpdSource = GetPrivateProfileIntA("Render", "StereoIpdSource", 0, chosenIni) ? 1 : 0;
#ifdef D2VR_MATCHED_CAPTURE
    g_matchedIniPath=chosenIni;
    g_cameraUploadWitnessBudget=GetPrivateProfileIntA("Diagnostics","CameraUploadWitness",0,
        ".\\Darkness2VR-diagnostic.ini") ? 4 : 0;
    if(g_cameraUploadWitnessBudget) Log("[CameraUpload] enabled: four original world WVP uploads; camera ownership UNKNOWN");
#endif
    g_stereoDebugMode = GetPrivateProfileIntA("Render", "StereoDebugMode", 0, chosenIni);
    if (g_stereoDebugMode < 0 || g_stereoDebugMode > 4) g_stereoDebugMode = 0;
    g_stereoLayerPose = GetPrivateProfileIntA("Render", "StereoLayerPose", 0, chosenIni) ? 1 : 0;
    g_nativeStereoHeadGeometry =
        GetPrivateProfileIntA("Render", "NativeStereoHeadGeometry", 0, chosenIni) ? 1 : 0;
    if (GetPrivateProfileStringA("Render", "GameUnitsPerMeter", "1.0", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= 0.01f && v <= 5000.0f) g_gameUnitsPerMeter = v;
    }
    if (GetPrivateProfileStringA("Render", "FovWiden", "2.5", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= 1.0f && v <= 8.0f) g_fovWiden = v;
    }
    g_fovWidenX = g_fovWidenY = g_fovWiden;
    if (GetPrivateProfileStringA("Render", "FovOverride", "0.0", buf, sizeof(buf), chosenIni)) {
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
    g_lockHeadPosition = GetPrivateProfileIntA("Render", "LockHeadPosition", 1, chosenIni) != 0;
    if (GetPrivateProfileStringA("Render", "LockRadiusM", "0.5", buf, sizeof(buf), chosenIni)) {
        float v = (float)atof(buf);
        if (v >= 0.05f && v <= 2.0f) g_lockRadiusM = v;
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
    g_vrSettingsToggleKey=GetPrivateProfileIntA("Input","VRSettingsKey",VK_INSERT,chosenIni);
    if(g_vrSettingsToggleKey<0 || g_vrSettingsToggleKey>254 ||
       (g_vrSettingsToggleKey>=VK_F1 && g_vrSettingsToggleKey<=VK_F12)) {
        Log("[VRSettings] toggle binding conflicts/invalid; preserving INSERT default");
        g_vrSettingsToggleKey=VK_INSERT;
    }
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

    const bool restoreSavedLighting=GetPrivateProfileIntA("Render", "RestoreSavedLighting", 1, chosenIni)!=0;
    const int savedLighting=EffectiveSavedLightingKey(
        GetPrivateProfileIntA("Lighting", "LastKey", -1, g_lightingChoicePath),restoreSavedLighting);
    if(!restoreSavedLighting)
        Log("[LightingPreset] explicit configured reconstruction; persisted lighting selection retained on disk");
    if (savedLighting >= 0 && savedLighting <= 9) {
        g_lightingPreset = savedLighting == 0 ? 9 : savedLighting - 1;
        g_projectionFix = 3;
        // Restoring lighting must not override the requested mono/replay
        // validation mode. Only an explicit live preset selection enables stereo.
        g_noInvProj = g_lightPosShift = g_skipLightVsShift = 0;
        g_diagnosticSameEye = -1; g_diagnosticSkipVs = 0;
        Log("[LightingPreset] restored Shift+%d: %s", savedLighting,
            kLightingPresetNames[g_lightingPreset]);
    }
    Log("Config loaded from: %s", chosenIni);
    Log("  Render: SubmitMode=%d TrackingSubMode=%d ViewScale=%.2f StereoStrength=%.2f BaseShift=%.4f DepthScale=%.4f MaxDisp=%.4f",
        g_submitMode, g_trackingSubMode, g_viewScale, g_stereoStrength, g_stereoBaseShift, g_stereoDepthScale, g_stereoMaxDisp);
    Log("  Input: EnableVRGamepad=%d RemapPhysicalPorts=%d HeadLookMode=%d (SensYaw=%.1f SensPitch=%.1f InvertYaw=%d InvertPitch=%d)",
        g_enableVRGamepad, g_remapPhysical, g_headLookMode, g_headLookSensitivityYaw, g_headLookSensitivityPitch, g_headLookInvertYaw, g_headLookInvertPitch);
    Log("  Hotkeys: F2=InvertYaw | F3=HeadLook | F4=ViewScale | F5=DepthViz | F6=Cycle BaseShift | F7=Dump Matrices | F8 (or L3+R3)=Recenter | F9=Cycle FOV Scale | F10=Cycle VR Mode | F11=Cycle Stereo Strength");

    {
        int traceIni = GetPrivateProfileIntA("Render", "RenderPipelineTrace", -1, chosenIni);
        if (traceIni < 0) traceIni = GetPrivateProfileIntA(nullptr, "RenderPipelineTrace", 0, chosenIni);
        g_renderPipelineTrace = traceIni ? 1 : 0;
    }
    g_frameBoundaryPolicy = GetPrivateProfileIntA("Render", "FrameBoundaryPolicy", 2, chosenIni);
    if (g_frameBoundaryPolicy < 0 || g_frameBoundaryPolicy > 2) g_frameBoundaryPolicy = 2;
    g_frameBoundary.Reset(static_cast<d2vr_frame::Policy>(g_frameBoundaryPolicy));
    d2vr_trace::g_enabled = g_renderPipelineTrace;
    d2vr_trace::g_maxEvents = GetPrivateProfileIntA("Render", "RenderPipelineTraceMax", 200000, chosenIni);
    d2vr_trace::g_traceScenes = GetPrivateProfileIntA("Render", "RenderPipelineTraceScenes", 0, chosenIni) ? 1 : 0;
    d2vr_trace::g_traceAllScenes = GetPrivateProfileIntA("Render", "RenderPipelineTraceAllScenes", 0, chosenIni) ? 1 : 0;
    if (g_nativeImageGuard.enabled && g_renderPipelineTrace) {
        Log("[NativeImageGuard] pipeline trace writer disabled for isolated admission");
        g_renderPipelineTrace=0;
    }
    if (g_renderPipelineTrace) {
        // Trace capture: enable swapchain present hook unless HookMask explicitly zero.
        if (g_hookMask != 0) g_hookMask |= 2;
        if (g_stereoMode > 0) {
            Log("[RenderPipelineTrace] forcing StereoMode=0 (trace without replay/XR)");
            g_stereoMode = 0;
        }
        char traceDir[MAX_PATH];
        strcpy_s(traceDir, g_logPath);
        char* slash = strrchr(traceDir, '\\');
        if (slash) *slash = '\0';
        d2vr_trace::Init(traceDir);
        Log("[RenderPipelineTrace] csv=%s\\render_pipeline_trace.csv maxEvents=%d scenes=%d allScenes=%d",
            traceDir, d2vr_trace::g_maxEvents, d2vr_trace::g_traceScenes, d2vr_trace::g_traceAllScenes);
    }
    {
        int dx = GetPrivateProfileIntA("Render", "DisableOpenXR", -1, chosenIni);
        if (dx < 0) dx = GetPrivateProfileIntA(nullptr, "DisableOpenXR", 0, chosenIni);
        g_disableOpenXR = dx ? 1 : 0;
    }
    g_presentHookLevel = GetPrivateProfileIntA("Render", "PresentHookLevel", 0, chosenIni);
    if (g_presentHookLevel < 0 || g_presentHookLevel > 2) g_presentHookLevel = 0;
    Log("  FrameBoundaryPolicy=%d (%s) debounceMs=%llu (legacy max ~%.1f Hz)",
        g_frameBoundaryPolicy,
        g_frameBoundaryPolicy == 0   ? "PresentPrimary"
        : g_frameBoundaryPolicy == 1 ? "LegacyRc20b"
                                       : "TraceOnly",
        (unsigned long long)d2vr_frame::BoundaryMachine::kLegacyEndSceneDebounceMs,
        d2vr_frame::MaxVrFrameHzFromLegacyDebounce());
    Log("[EffectiveConfig] RenderPipelineTrace=%d FrameBoundaryPolicy=%d DisableOpenXR=%d SubmitMode=%d StereoMode=%d StartupMode=%d HookMask=0x%X PresentHookLevel=%d",
        g_renderPipelineTrace, g_frameBoundaryPolicy, g_disableOpenXR, g_submitMode, g_stereoMode, g_startupMode,
        g_hookMask, g_presentHookLevel);
}

void RequestRecenter(const char* source) {
    // Preserve the click-frame evidence before ResetNativeAim clears ownership.
    RecordRecenterDiagnostic(source);
    ResetNativeAim();
    g_headGeometryReferenceValid = false;
    CaptureClickRecenterReference();
    g_lastWorldHeadContextValid = false;
    g_recenterHeadLock = true;
    g_anchorValid = false;
    g_vrWorldAnchorValid = false;
    g_vrRefValid = false;
    g_headLookReset = true;
    MessageBeep(MB_OK); // asynchronous: do not stall rendering for 50 ms
    Log(">>> [%s] VR Recenter requested (World Anchor + 6DoF Reference + HeadLook) <<<", source);
}

void PollHotkeys() {
    // Narrow local diagnostic bridge: capture/neutral comparison without
    // focusing the game or injecting input into the user's desktop.
    static ULONGLONG diagnosticPoll = 0;
    static int captureSerial = 0;
    const ULONGLONG diagnosticNow = GetTickCount64();
    if (diagnosticNow - diagnosticPoll >= 1000) {
        diagnosticPoll = diagnosticNow;
#ifdef D2VR_MATCHED_CAPTURE
        if (g_smallRecordCopyControl.Armed()) {
            const int request=GetPrivateProfileIntA("Diagnostics","SmallRecordCopyRuntime",1,
                ".\\Darkness2VR-diagnostic.ini");
            if (g_smallRecordCopyControl.Request(request)) {
                LARGE_INTEGER qpc{},frequency{};
                QueryPerformanceCounter(&qpc); QueryPerformanceFrequency(&frequency);
                Log("[SmallRecordCopyControl] requested=%d admitted=%d effective=%d qpc=%lld frequency=%lld",
                    request,int(g_smallRecordCopyAdmitted),int(SmallRecordCopyIsEnabled()),
                    qpc.QuadPart,frequency.QuadPart);
            }
        }
#endif

        int sameEye = GetPrivateProfileIntA("Diagnostics", "SameEye", -1, ".\\Darkness2VR-diagnostic.ini");
        if (g_lightingPreset < 0 && sameEye >= -1 && sameEye <= 1 && sameEye != g_diagnosticSameEye) {
            g_diagnosticSameEye = sameEye;
            Log("[Diagnostic] SameEye=%d", sameEye);
        }
        char skipVs[24]{};
        GetPrivateProfileStringA("Diagnostics", "SkipVS", "0", skipVs, sizeof(skipVs), ".\\Darkness2VR-diagnostic.ini");
        if (g_lightingPreset < 0) g_diagnosticSkipVs = (uint32_t)strtoul(skipVs, nullptr, 16);
        // Candidate fixes are key-controlled (Shift+F1..F3); the INI only seeds the start.
        static bool s_candidateLog = false;
        if (!s_candidateLog) {
            s_candidateLog = true;
            if (g_lightingPreset < 0) {
            g_lightPosShift = GetPrivateProfileIntA("Diagnostics", "LightPosShift", 0, ".\\Darkness2VR-diagnostic.ini") ? 1 : 0;
            g_skipLightVsShift = GetPrivateProfileIntA("Diagnostics", "SkipLightVs", 0, ".\\Darkness2VR-diagnostic.ini") ? 1 : 0;
            g_noInvProj = GetPrivateProfileIntA("Diagnostics", "NoInvProj", 0, ".\\Darkness2VR-diagnostic.ini") ? 1 : 0;
            }
            Log("[Diagnostic] candidates: LightPosShift=%d SkipLightVs=%d NoInvProj=%d (Shift+F1/F2/F3)",
                g_lightPosShift, g_skipLightVsShift, g_noInvProj);
        }
        int projection = GetPrivateProfileIntA("Diagnostics", "ProjectionFix", -1, ".\\Darkness2VR-diagnostic.ini");
        if (g_lightingPreset < 0 && projection >= 0 && projection <= 3 && projection != g_projectionFix) {
            g_projectionFix = projection; Log("[Diagnostic] ProjectionFix=%d", projection);
        }
        int neutral = GetPrivateProfileIntA("Diagnostics", "Neutral", -1, ".\\Darkness2VR-diagnostic.ini");
        if (g_lightingPreset < 0 && (neutral == 0 || neutral == 1)) {
            int mode = neutral ? 2 : 1;
            if (g_stereoMode != mode) { g_stereoMode = mode; Log("[Diagnostic] StereoMode=%d", mode); }
        }
        char fovWiden[24]{};
        GetPrivateProfileStringA("Diagnostics", "FovWiden", "0", fovWiden, sizeof(fovWiden),
                                 ".\\Darkness2VR-diagnostic.ini");
        {
            const float w = (float)atof(fovWiden);
            if (w != g_diagnosticFovWiden && w >= 0.0f && w <= 8.0f) {
                g_diagnosticFovWiden = w;
                Log("[Diagnostic] FovWiden=%.3f", w);
                // Apply/clear the override AT CHANGE TIME. The per-frame tail in
                // UpdateSubmissionFovFromMatrices does not reliably run without an
                // active HMD / valid estimates, so clearing the override to 1.0 used
                // to leave g_fovWidenX stuck at the last forced value.
                if (w > 1.0f) {
                    g_fovWidenX = g_fovWidenY = w;
                    g_fovCoverX = g_fovCoverY = w;
                } else {
                    const float m = (g_fovWiden > 1.0f) ? g_fovWiden : 1.0f;
                    g_fovWidenX = g_fovWidenY = m;
                    g_fovCoverX = g_fovCoverY = m;
                }
            }
            // The FOV update normally happens inside the XR submit path (needs an active
            // HMD); apply the diagnostic widen here too so capture sessions without a
            // headset still render the widened frame.
            if (g_diagnosticFovWiden >= 1.0f) {
                g_fovWidenX = g_diagnosticFovWiden;
                g_fovWidenY = g_diagnosticFovWiden;
            }
        }
        int serial = GetPrivateProfileIntA("Diagnostics", "CaptureSerial", 0, ".\\Darkness2VR-diagnostic.ini");
        if (serial > 0 && serial != captureSerial) {
            captureSerial = serial; g_dumpEyeFrames = 2;
            g_dumpDelay = (int)g_trueStereoFrames + 120;
            Log("[Diagnostic] capture request=%d after segment=%d", serial, g_dumpDelay);
        }
    }
    static DWORD s_lastF1 = 0;
    static DWORD s_lastSF1 = 0;
    static DWORD s_lastSF2 = 0;
    static DWORD s_lastSF3 = 0;
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

    // Edge-triggered chords: holding a digit never cycles/repeats a preset.
    // Read digit state even when Shift is released to avoid stale key edges.
    static bool presetHeld[10]{};
    const bool presetShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const HWND foreground = GetForegroundWindow();
    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(foreground, &foregroundPid);
    for (int preset = 0; preset < 10; ++preset) {
        const int key = preset == 9 ? '0' : '1' + preset;
        const bool held = (GetAsyncKeyState(key) & 0x8000) != 0;
        if (presetShift && held && !presetHeld[preset] && foregroundPid == GetCurrentProcessId()) {
            g_pendingLightingPreset = preset;
        }
        presetHeld[preset] = held;
    }

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

    static bool s_geometryKeyHeld=false;
    const bool geometryKey=(GetAsyncKeyState(VK_CONTROL)&0x8000) && (GetAsyncKeyState(VK_F8)&0x8000);
    if(geometryKey && !s_geometryKeyHeld) {
        g_headGeometry=!g_headGeometry;
        RequestRecenter("Ctrl+F8 geometry diagnostic");
        Log("[HeadGeometry] requested=%d; GPU diagnostic only, scale/culling/collision unverified",(int)g_headGeometry);
    }
    s_geometryKeyHeld=geometryKey;
    if (!(GetAsyncKeyState(VK_CONTROL)&0x8000) && (GetAsyncKeyState(VK_F8) & 0x8000) && (now - s_lastF8 > 400)) {
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

    if ((GetAsyncKeyState(VK_F1) & 0x8000) && (now - s_lastF1 > 400)) {
        s_lastF1 = now;
        // A/B toggle for the render/HMD coverage gap (EdgeCrop). 0 = render exactly covers
        // the HMD frustum; 0.08 = render 8% wider, compositor crops the outer band.
        if (g_edgeCrop < 0.02f) g_edgeCrop = 0.04f;
        else if (g_edgeCrop < 0.06f) g_edgeCrop = 0.08f;
        else g_edgeCrop = 0.0f;
        Beep(600 + (int)(g_edgeCrop * 5000.f), 60);
        Log(">>> [HOTKEY F1] EdgeCrop changed to %.2f (fovScaleA/B test) <<<", g_edgeCrop);
    }
    // ---------------- candidate fixes under test (Shift+F1..F3, all OFF by default) -----
    const bool shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    if (shiftHeld && (GetAsyncKeyState(VK_F1) & 0x8000) && (now - s_lastSF1 > 400)) {
        s_lastSF1 = now;
        g_lightPosShift = !g_lightPosShift;
        Beep(g_lightPosShift ? 900 : 500, 60);
        Log(">>> [HOTKEY Shift+F1] LightPosShift=%d (PS c0 light position shift per eye) <<<",
            g_lightPosShift);
    }
    if (shiftHeld && (GetAsyncKeyState(VK_F2) & 0x8000) && (now - s_lastSF2 > 400)) {
        s_lastSF2 = now;
        g_skipLightVsShift = !g_skipLightVsShift;
        Beep(g_skipLightVsShift ? 900 : 500, 60);
        Log(">>> [HOTKEY Shift+F2] SkipLightVsShift=%d (light-volume VS unshifted) <<<",
            g_skipLightVsShift);
    }
    if (shiftHeld && (GetAsyncKeyState(VK_F3) & 0x8000) && (now - s_lastSF3 > 400)) {
        s_lastSF3 = now;
        g_noInvProj = !g_noInvProj;
        Beep(g_noInvProj ? 900 : 500, 60);
        Log(">>> [HOTKEY Shift+F3] NoInvProj=%d (raw reconstruction, no compensation) <<<",
            g_noInvProj);
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
XrSessionState g_xrSessionState = XR_SESSION_STATE_UNKNOWN;
static d2vr_refresh::Display g_displayRefresh;
static d2vr_refresh::SteamReader<d2vr_refresh::SteamNative> g_steamRefresh;
static ULONGLONG g_steamRefreshNext=0;
static d2vr_adaptive::Controller g_adaptiveFPS;
static thread_local d2vr_adaptive::Presentation g_adaptivePresentation;
static uint64_t BeginAdaptivePresentation() {
    return g_adaptiveFPSMode!=d2vr_adaptive::Mode::Off && g_submitMode==4 && g_frameBoundaryPolicy==0 ?
        g_adaptivePresentation.Begin():0;
}
static void EndAdaptivePresentation(uint64_t serial,HRESULT hr,bool forwarded=true) {
    const auto frame=g_adaptivePresentation.End(serial,forwarded && hr==D3D_OK);
    if(!frame || GetCurrentThreadId()!=g_renderThreadId || g_xrSessionState!=XR_SESSION_STATE_FOCUSED)return;
    const DWORD savedError=GetLastError();
    __declspec(align(16)) unsigned char floating[512];
    _fxsave(floating);
    __asm fninit
    // No runtime adapter is bound yet. This activates truthful unique-frame
    // feedback and policy status, not an emulated cap or compositor control.
    const double now=GetTickCount64()*.001;
    g_adaptiveFPS.Configure(g_adaptiveFPSMode,g_displayRefresh.Hz(),now);
    g_adaptiveFPS.Frame(frame,now);
    const auto state=g_adaptiveFPS.State();
    static int lastStatus=-1;
    if(lastStatus!=int(state.status) || frame%300==0) {
        lastStatus=int(state.status);
        Log("[AdaptiveFPS] status=%s physicalHz=%.3f uniqueAppFPS=%.2f halfApplied=%d frame=%llu; requires fresh stereo XR + forwarded D3D_OK; complete CPU/GPU recovery input not connected",
            d2vr_adaptive::Name(state.status),state.refresh,state.fps,int(state.half),frame);
    }
    _fxrstor(floating);
    SetLastError(savedError);
}
class AdaptivePresentationScope {
    uint64_t serial_=BeginAdaptivePresentation();
public:
    AdaptivePresentationScope()=default;
    ~AdaptivePresentationScope(){if(serial_)g_adaptivePresentation.End(serial_,false);}
    AdaptivePresentationScope(const AdaptivePresentationScope&)=delete;
    AdaptivePresentationScope& operator=(const AdaptivePresentationScope&)=delete;
    void Returned(HRESULT hr,bool forwarded=true){if(serial_)EndAdaptivePresentation(serial_,hr,forwarded);serial_=0;}
};

XrSwapchain g_swapchain[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE };
uint32_t g_scW = 0, g_scH = 0;
ID3D11Texture2D** g_scImages[2] = { nullptr, nullptr };
uint32_t g_scCount[2] = { 0, 0 };
struct SwapchainWriteState {
    uint32_t index=0;
    bool acquired=false,waited=false,publishable=false;
};
static SwapchainWriteState g_scWrite[2];

ID3D11Texture2D* g_eyeCache[2] = { nullptr, nullptr };
ID3D11RenderTargetView* g_eyeRTV[2] = { nullptr, nullptr };
static UINT g_eyeCacheW=0,g_eyeCacheH=0;
#include "nis_eye.inl"

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
    float aimCfg[4];     // output UV, viewport aspect, 0=off/1=axis/2=offscreen direction
    float statusCfg[4];  // x=0 disabled, 1 selected OFF, 2 selected ON
};

XrPosef g_eyePose[2]{};
XrFovf g_eyeFov[2]{};
bool g_haveViews = false;
// One XR frame spans the ENTIRE recorded game frame, including both eye passes.
// Locate before RecBeginFrame; never attach a newly located pose to old pixels.
struct TrackedFrame {
    XrFrameState state{ XR_TYPE_FRAME_STATE };
    uint64_t id = 0;
    ULONGLONG startedMs = 0;
    XrViewStateFlags flags = 0;
    bool open = false;
    bool renderTrackingValid = true;
    XrPosef layerPose{};
    float halfFovX=0,halfFovY=0,offsetX=0,cropX=1,cropY=1;
};
TrackedFrame g_trackedFrame;
static void AdaptiveStereoSubmitted(XrResult result,bool havePixels,uint32_t layerCount) {
    if(g_adaptiveFPSMode!=d2vr_adaptive::Mode::Off && result==XR_SUCCESS &&
       g_submitMode==4 && havePixels && layerCount==1 && g_pairSubmitMetaValid &&
       g_eyePairFrameId==g_trackedFrame.id)
        g_adaptivePresentation.StereoSubmitted(g_trackedFrame.id);
}
static TrackedFrame g_lastWorldHeadContext;
static void ResolveHeadGeometryBackgroundPose(bool world, bool geometry) {
    if(world) {
        g_lastWorldHeadContext=g_trackedFrame;
        g_lastWorldHeadContextValid=geometry;
    } else if(geometry && g_lastWorldHeadContextValid) {
        // UI segments reuse pixels from the last world image. Its camera pose
        // and FOV remain the metadata for those pixels, not the newly located head.
        g_trackedFrame.layerPose=g_lastWorldHeadContext.layerPose;
        g_trackedFrame.halfFovX=g_lastWorldHeadContext.halfFovX;
        g_trackedFrame.halfFovY=g_lastWorldHeadContext.halfFovY;
        g_trackedFrame.offsetX=g_lastWorldHeadContext.offsetX;
        g_trackedFrame.cropX=g_lastWorldHeadContext.cropX;
        g_trackedFrame.cropY=g_lastWorldHeadContext.cropY;
    }
}
static void BeginTrackedFrame();
#include "gpu_timing.inl"
#ifdef D2VR_MATCHED_CAPTURE
#include "cpu_sync_timing.inl"
#endif
int g_frames = 0;
volatile LONG g_xrInitTried = 0;

XrPosef g_renderEyePose[2]{};
XrFovf g_renderEyeFov[2]{};
bool g_renderHaveViews = false;
bool g_had3DCameraThisFrame = false;
bool g_hadWorldDrawThisFrame = false;
int g_3dCamCallsThisFrame = 0;
bool g_in3DGameplayMode = false;
d2vr_input::WorldActivity g_worldActivity;

// Frozen with the rest of FrameContext, never updated between live and replay.
static float g_headDeltaView[16]{};
static bool g_headDeltaValid=false;
static XrPosef g_headGeometryReference{};
static void UpdateHeadGeometry(const XrPosef& head);

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
    // xrLocateViews returns EYE orientations (which may be canted), not a
    // centre-head orientation. Use the shortest-arc midpoint of both eyes.
    const auto& a=p0.orientation; const auto& b=p1.orientation;
    const float sign=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0 ? -1.f:1.f;
    XrQuaternionf q={a.x+sign*b.x,a.y+sign*b.y,a.z+sign*b.z,a.w+sign*b.w};
    const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    if(std::isfinite(n) && n>1e-6f) c.orientation={q.x/n,q.y/n,q.z/n,q.w/n};
    c.position.x = (p0.position.x + p1.position.x) * 0.5f;
    c.position.y = (p0.position.y + p1.position.y) * 0.5f;
    c.position.z = (p0.position.z + p1.position.z) * 0.5f;
    return c;
}

// The game camera does not follow the HMD position. Submitting the live head position as
// the layer pose makes the compositor treat the image as rendered FROM the head, so the
// whole world - lights and shadows included - rides along with every head movement.
// The layer position is therefore anchored; only the orientation follows the head, and
// the anchor drags along only once the head leaves LockRadiusM around it.
static bool g_lockedHeadValid = false;
static XrVector3f g_lockedHeadPos{ 0.f, 0.f, 0.f };

static XrPosef LockedViewPose(const XrPosef& head) {
    XrPosef p = head;
    if (!g_lockHeadPosition) return p;
    if (!g_lockedHeadValid) {
        g_lockedHeadPos = head.position;
        g_lockedHeadValid = true;
        Log("[HeadLock] layer position anchored at (%.3f, %.3f, %.3f)",
            g_lockedHeadPos.x, g_lockedHeadPos.y, g_lockedHeadPos.z);
    }
    const float dx = head.position.x - g_lockedHeadPos.x;
    const float dy = head.position.y - g_lockedHeadPos.y;
    const float dz = head.position.z - g_lockedHeadPos.z;
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len > g_lockRadiusM) {
        const float k = g_lockRadiusM / len;
        g_lockedHeadPos.x = head.position.x - dx * k;
        g_lockedHeadPos.y = head.position.y - dy * k;
        g_lockedHeadPos.z = head.position.z - dz * k;
    }
    p.position = g_lockedHeadPos;
    return p;
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
namespace d2vr_replay { struct StereoParams; }
static d2vr_replay::StereoParams MakeEyeParams(int eye);
#ifdef D2VR_MATCHED_CAPTURE
extern volatile LONG g_frameId;
static void MatchedCaptureDraw(IDirect3DDevice9*,int,bool);
static void MatchedCaptureBlit(int,const StereoCBData&);
static void MatchedProjectionObserved(const float*,const float*,UINT,UINT,int,HRESULT);
static bool MatchedCaptureRecordingActive();
static void MatchedCaptureInput(IDirect3DSurface9*,bool);
#endif
#ifdef D2VR_MATCHED_CAPTURE
static void NativeCameraUiScale(float& x,float& y);
#endif
#include "shader_preload_pool.inl"
static std::atomic<d2vr_query_preload::Pool*> g_queryPreload{nullptr};
static d2vr_query_preload::CreateFn g_queryCreateOriginal=nullptr;
__declspec(noinline) static HRESULT STDMETHODCALLTYPE Hook_CreateQuery(IDirect3DDevice9* device,D3DQUERYTYPE type,IDirect3DQuery9** out) {
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    auto* pool=g_queryPreload.load(std::memory_order_acquire);
    return pool ? pool->Create(device,type,out,caller) : g_queryCreateOriginal(device,type,out);
}
static void QueryPreloadBeginReset(IDirect3DDevice9* device) {
    if(auto* pool=g_queryPreload.load())pool->BeginReset(device);
}
static void QueryPreloadEndReset(IDirect3DDevice9* device,HRESULT hr) {
    if(auto* pool=g_queryPreload.load()) {
        pool->EndReset(device,hr);
        Log("[QueryPreload] Reset hr=%08lX pending=%u requests=%llu hits=%llu",hr,pool->Pending(),pool->requests.load(),pool->hits.load());
    }
}
static void QueryPreloadTick() {
    auto* pool=g_queryPreload.load();if(!pool)return;
    static ULONGLONG next=0;const auto now=GetTickCount64();if(now<next)return;next=now+5000;
    Log("[QueryPreload] pending=%u requests=%llu hits=%llu; one-use native occlusion objects",pool->Pending(),pool->requests.load(),pool->hits.load());
}
#ifdef D2VR_MATCHED_CAPTURE
static void NativeViewProjectionObserved(const float* inverse);
#endif
static bool NativeTentacleHeadAttachmentAllowed();
static void RecordPreSceneWrite(IDirect3DDevice9* dev);
#include "stereo_replay.inl"

static void UpdateHeadGeometry(const XrPosef& head) {
    g_headDeltaValid=false;
#ifdef D2VR_MATCHED_CAPTURE
    if(g_imagePreservingPresent && (!g_protectedHeadTracking || !ProtectedHeadConfigurationAllowed())) return;
#endif
    const XrViewStateFlags required=XR_VIEW_STATE_POSITION_TRACKED_BIT | XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
    if(!g_headGeometry || !g_haveViews || !g_in3DGameplayMode ||
        (g_trackedFrame.flags & required)!=required || g_stereoMode!=1 || g_stereoShiftMode!=0 ||
        !d2vr_replay::g_cameraProjectionKnown) return;
    if(!g_headGeometryReferenceValid) {
        g_headGeometryReference=head;
        // Recenter the horizontal heading and position. Preserve gravity: a
        // tilted headset must not bake pitch/roll into the world's reference.
        const float yaw=ComputeHorizontalYaw(head.orientation);
        g_headGeometryReference.orientation={0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
        g_headGeometryReferenceValid=true;
        const float eyeDot=std::fabs(g_eyePose[0].orientation.x*g_eyePose[1].orientation.x+
            g_eyePose[0].orientation.y*g_eyePose[1].orientation.y+
            g_eyePose[0].orientation.z*g_eyePose[1].orientation.z+
            g_eyePose[0].orientation.w*g_eyePose[1].orientation.w);
        Log("[HeadGeometry] LEVEL yaw-only reference captured frame=%llu xrTime=%lld midpoint=(%.3f,%.3f,%.3f) yaw=%.2fdeg eyeRotationDifference=%.2fdeg ViewOffsetX=%.4f; units/metre=%.4f, native culling/collision absent",
            (unsigned long long)g_trackedFrame.id,(long long)g_trackedFrame.state.predictedDisplayTime,
            head.position.x,head.position.y,head.position.z,ComputeHorizontalYaw(head.orientation)*57.29578f,
            2.f*std::acos(std::min(1.f,eyeDot))*57.29578f,g_viewOffsetX,g_gameUnitsPerMeter);
    }
    const auto& ref=g_headGeometryReference;
    const float q[4]={head.orientation.x,head.orientation.y,head.orientation.z,head.orientation.w};
    const float p[3]={head.position.x,head.position.y,head.position.z};
    const float rq[4]={ref.orientation.x,ref.orientation.y,ref.orientation.z,ref.orientation.w};
    const float rp[3]={ref.position.x,ref.position.y,ref.position.z};
    // Base-camera coordinates -> current head. Same metre scale as XR IPD.
    if(!d2vr_math::RelativeTrackingView(q,p,rq,rp,g_gameUnitsPerMeter,g_headDeltaView)) return;
    g_headDeltaValid=true;
}

static void CaptureClickRecenterReference() {
    const auto tracked=XR_VIEW_STATE_POSITION_TRACKED_BIT | XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
    if(!g_trackedFrame.open || !g_headGeometry || !g_haveViews || !g_in3DGameplayMode ||
       (g_trackedFrame.flags & tracked)!=tracked) return;
    const auto head=HeadCenterPose(g_eyePose[0],g_eyePose[1]);
    const float yaw=ComputeHorizontalYaw(head.orientation);
    g_headGeometryReference=head;
    g_headGeometryReference.orientation={0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
    g_headGeometryReferenceValid=true;
    // Keep this pair's already frozen eye matrices. Only the next frame uses
    // the new reference, but moving/removing the HMD cannot change its origin.
    Log("[HeadGeometry] LEVEL reference latched on recenter click frame=%llu position=(%.4f,%.4f,%.4f) yaw=%.2fdeg",
        (unsigned long long)g_trackedFrame.id,head.position.x,head.position.y,head.position.z,yaw*57.29578f);
}

// Builds the per-eye transform parameters. Eye sign: left eye positive shift by default.
static void SyncStereoIpdFromOpenXr() {
    if (g_stereoIpdSource != 1 || !g_haveViews) return;
    const float ipdM = d2vr_stereo::InterEyeDistanceMeters(
        g_eyePose[0].position.x, g_eyePose[0].position.y, g_eyePose[0].position.z,
        g_eyePose[1].position.x, g_eyePose[1].position.y, g_eyePose[1].position.z);
    if (ipdM >= 0.040f && ipdM <= 0.090f)
        g_stereoIPD = d2vr_stereo::GameUnitsFromMeters(ipdM, g_gameUnitsPerMeter);
}

#ifdef D2VR_MATCHED_CAPTURE
static bool NativeCameraEyeScale(int eye,float& x,float& y);
#endif
static d2vr_replay::StereoParams BuildEyeParams(int eye) {
    d2vr_replay::StereoParams sp;
    if (g_diagnosticSameEye >= 0) eye = g_diagnosticSameEye;
    sp.mode = g_stereoShiftMode;
    float sign = (g_stereoPolarity == 0) ? ((eye == 0) ? +1.0f : -1.0f)
                                         : ((eye == 0) ? -1.0f : +1.0f);
    sp.separation = sign * g_stereoSep;
    sp.convergence = g_stereoConv;
    sp.transX = sign * 0.5f * g_stereoIPD;
    sp.transY = 0.0f;
    if (g_stereoDebugMode == 1) {
        sp.separation = 0.0f;
        sp.transX = 0.0f;
        sp.transY = 0.0f;
    }
    const int ei = eye & 1;
    const float widenX = g_fovWidenEyeX[ei] > 1.0f ? g_fovWidenEyeX[ei] : g_fovWidenX;
    const float widenY = g_fovWidenEyeY[ei] > 1.0f ? g_fovWidenEyeY[ei] : g_fovWidenY;
    sp.fovScaleX = widenX > 1.0f ? (1.0f / widenX) : 1.0f;
    sp.fovScaleY = widenY > 1.0f ? (1.0f / widenY) : 1.0f;
#ifdef D2VR_MATCHED_CAPTURE
    NativeCameraEyeScale(eye,sp.fovScaleX,sp.fovScaleY);
#endif
    const bool allowHeadClip = g_nativeStereoHeadGeometry ||
                               (g_submitMode != 4 && g_headGeometry);
    if (g_headDeltaValid && g_stereoMode == 1 && allowHeadClip) {
        float wideInverse[16];
        memcpy(wideInverse,d2vr_replay::g_cameraInverseProjection,sizeof(wideInverse));
        for(int c=0;c<4;++c) { wideInverse[c]/=sp.fovScaleX; wideInverse[4+c]/=sp.fovScaleY; }
        sp.headGeometry=d2vr_math::HeadClipTransform(wideInverse,g_headDeltaView,sp.headClip,sp.inverseHeadClip);
    }
    // Validation must use the same unshifted camera for BOTH live and replay.
    // Previously live retained transX while only replay cleared it.
    if (g_stereoMode == 2) {
        sp.separation = sp.transX = sp.transY = 0.0f;
        sp.fovScaleX = sp.fovScaleY = 1.0f;
    }
    return sp;
}
static d2vr_replay::StereoParams g_frameEyeParams[2];
static bool g_frameEyeParamsValid = false;
static d2vr_replay::StereoParams MakeEyeParams(int eye) {
    return g_frameEyeParamsValid ? g_frameEyeParams[eye ? 1:0] : BuildEyeParams(eye);
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
    if (g_autoFovWiden && g_haveViews && g_engineFovFactor <= 1.0f) {
        // Per-eye cover/widen so replay + compositor match each lens (PSVR2 is asymmetric).
        const float crop = 1.0f + g_edgeCrop;
        g_fovCoverX = g_fovCoverY = g_fovWidenX = g_fovWidenY = 1.0f;
        for (int e = 0; e < 2; ++e) {
            const float hx = g_autoFovCoverage * std::max(std::fabs(g_eyeFov[e].angleLeft),
                std::fabs(g_eyeFov[e].angleRight));
            const float hy = g_autoFovCoverage * std::max(std::fabs(g_eyeFov[e].angleUp),
                std::fabs(g_eyeFov[e].angleDown));
            g_fovCoverEyeX[e] = g_estPx * std::tan(hx);
            g_fovCoverEyeY[e] = g_estPy * std::tan(hy);
            g_fovWidenEyeX[e] = g_fovCoverEyeX[e] * crop;
            g_fovWidenEyeY[e] = g_fovCoverEyeY[e] * crop;
            g_fovCoverX = std::max(g_fovCoverX, g_fovCoverEyeX[e]);
            g_fovCoverY = std::max(g_fovCoverY, g_fovCoverEyeY[e]);
            g_fovWidenX = std::max(g_fovWidenX, g_fovWidenEyeX[e]);
            g_fovWidenY = std::max(g_fovWidenY, g_fovWidenEyeY[e]);
        }
    } else if (g_engineFovFactor > 1.0f) {
        // The engine itself renders the wide frustum (GetFOV patched), so the GPU must
        // NOT widen again - the declared FOV has to match the rendered one 1:1.
        g_fovWidenX = 1.0f;
        g_fovWidenY = 1.0f;
        g_fovCoverX = 1.0f;
        g_fovCoverY = 1.0f;
    } else {
        const float w = (g_fovWiden > 1.0f) ? g_fovWiden : 1.0f;
        g_fovWidenX = w;
        g_fovWidenY = w;
        g_fovCoverX = w;
        g_fovCoverY = w;
    }
    // Diagnostic override (no HMD required): force the render widen so the widened frame
    // can be captured and inspected for frustum-culling gaps at the edges.
    if (g_diagnosticFovWiden >= 1.0f) {
        g_fovWidenX = g_diagnosticFovWiden;
        g_fovWidenY = g_diagnosticFovWiden;
        g_fovCoverX = g_diagnosticFovWiden;
        g_fovCoverY = g_diagnosticFovWiden;
    }
    for (int e = 0; e < 2; ++e) {
        if (g_fovWidenEyeX[e] <= 1.0f) g_fovWidenEyeX[e] = g_fovWidenX;
        if (g_fovWidenEyeY[e] <= 1.0f) g_fovWidenEyeY[e] = g_fovWidenY;
        if (g_fovCoverEyeX[e] <= 1.0f) g_fovCoverEyeX[e] = g_fovCoverX;
        if (g_fovCoverEyeY[e] <= 1.0f) g_fovCoverEyeY[e] = g_fovCoverY;
    }
    // the image is rendered with a SYMMETRIC widened frustum, so the declared FOV must be
    // symmetric too (declaring the HMD's asymmetric frustum shifts/distorts the content)
    g_gameHalfFovX = std::atan(g_fovCoverX / g_estPx);
    g_gameHalfFovY = std::atan(g_fovCoverY / g_estPy);
    if (g_engineFovFactor > 1.0f) {
        // Verification of the native engine FOV override: the projection coefficients
        // measured from the matrix uploads must show the WIDER frustum (smaller Px/Py).
        static DWORD s_lastFovLog = 0;
        const DWORD now = GetTickCount();
        if (now - s_lastFovLog > 2000) {
            s_lastFovLog = now;
            Log("[FovRE] engine FOV active: estPx=%.4f estPy=%.4f -> halfFov %.1f x %.1f deg "
                "(factor %.3f, widen %.2f/%.2f)",
                g_estPx, g_estPy,
                2.0f * g_gameHalfFovX * 57.29578f, 2.0f * g_gameHalfFovY * 57.29578f,
                g_engineFovFactor, g_fovWidenX, g_fovWidenY);
        }
    }
}

#ifdef D2VR_MATCHED_CAPTURE
#include "native_camera_cover.inl"
#endif

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

#ifdef D2VR_MATCHED_CAPTURE
// Uses the existing COM constant hook, not another executable interception.
// Copy only the API payload; never chase its pointer as a camera/object.
struct CameraUploadWitness {
    void* stack[16];
    const float* source;
    void* caller;
    float matrix[16];
    USHORT depth;
    LONG sample;
};
static bool BeginCameraUploadWitness(CameraUploadWitness& out,const float* data,UINT start,UINT count,void* caller) {
    if(InterlockedCompareExchange(&g_cameraUploadWitnessBudget,0,0)<=0 || !data || start!=0 || count<4 ||
       d2vr_replay::g_replayEye>=0 || !d2vr_replay::g_activeVs ||
       d2vr_replay::g_activeVs->hash!=0x293F456Bu) return false;
    if(InterlockedDecrement(&g_cameraUploadWitnessBudget)<0) return false;
    out.source=data;out.caller=caller;
    memcpy(out.matrix,data,sizeof(out.matrix));
    out.depth=CaptureStackBackTrace(1,16,out.stack,nullptr);
    static volatile LONG samples=0;
    out.sample=InterlockedIncrement(&samples);
    return true;
}
static void EndCameraUploadWitness(const CameraUploadWitness& sample,HRESULT hr) {
    Log("[CameraUpload] sample=%ld frame=%ld thread=%lu VS=293F456B c0..3 source=%p caller=%p hr=%08lX cameraOwnership=UNKNOWN",
        sample.sample,g_frameId,GetCurrentThreadId(),sample.source,sample.caller,hr);
    LogMatrix4x4("original native upload",sample.matrix);
    for(USHORT i=0;i<sample.depth;++i) {
        HMODULE module=nullptr;char path[MAX_PATH]{};
        if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(sample.stack[i]),&module)) GetModuleFileNameA(module,path,MAX_PATH);
        Log("[CameraUpload] sample=%ld stack=%u return=%p module=%s offset=%08lX",
            sample.sample,i,sample.stack[i],path,
            module?(unsigned long)((uintptr_t)sample.stack[i]-(uintptr_t)module):0);
    }
}
#endif

#ifdef D2VR_MATCHED_CAPTURE
struct MatchedSkinUploadWitness {
    bool valid=false;
    uint64_t frame=0,bankHash=0;
    unsigned beforeDraw=0,vsHash=0;
    UINT start=0,count=0,bankFirst=0,bankCount=0;
    void* caller=nullptr;
};
static MatchedSkinUploadWitness BeginMatchedSkinUpload(const float*,UINT,UINT,void*);
static void EndMatchedSkinUpload(const MatchedSkinUploadWitness&,const float*,HRESULT);
#endif
#pragma optimize("y", off)
__declspec(noinline) static HRESULT STDMETHODCALLTYPE Native_SetVertexShaderConstantF(
    IDirect3DDevice9* dev,
    UINT StartRegister,
    const float* pConstantData,
    UINT Vector4fCount,const uintptr_t* nativeSlot,uintptr_t nativeFrame,uintptr_t nativeOwner)
{
    d2vr_shader_registers::Reference rawReference;
    if(!(g_hookMask&8) || !d2vr_replay::g_recActive) {
        d2vr_replay::RecLockGuard lock;
        d2vr_replay::g_vsConstantHistory.InvalidateUnrecorded(StartRegister,Vector4fCount);
    }
#ifdef D2VR_MATCHED_CAPTURE
    CameraUploadWitness cameraSample;
    const bool witness=BeginCameraUploadWitness(cameraSample,pConstantData,StartRegister,Vector4fCount,reinterpret_cast<void*>(nativeSlot[0]));
    const auto skinUpload=BeginMatchedSkinUpload(pConstantData,StartRegister,Vector4fCount,reinterpret_cast<void*>(nativeSlot[0]));
#endif
    // Detect perspective WVP independently of desktop aspect ratio. The VR render
    // target is near-square; the old 1.2..2.4 gate marked real world frames as UI.
    // Only trusted main-view shader matrices count; shadow atlases do not.
    if (pConstantData && Vector4fCount >= 4 && StartRegister == 0) {
        const float* M = pConstantData;
        const float lenCol3 = std::sqrt(M[3] * M[3] + M[7] * M[7] + M[11] * M[11]);
        const float lenCol0 = std::sqrt(M[0] * M[0] + M[4] * M[4] + M[8] * M[8]);
        const float lenCol1 = std::sqrt(M[1] * M[1] + M[5] * M[5] + M[9] * M[9]);
        if (lenCol3 > 0.1f && lenCol0 > 0.15f && lenCol1 > 0.25f && std::fabs(M[15]) < 500.0f) {
            const float aspect = lenCol1 / lenCol0;
            const auto* active=d2vr_replay::g_activeVs;
            if (aspect >= .25f && aspect <= 4.f && active && active->wvpBase==0 &&
                !active->depthOnly && !d2vr_replay::IsShadowAtlasPass(dev)) {
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
        const auto proof=d2vr_native_constants::InspectCurrent(
            nativeSlot,nativeFrame,pConstantData,
            sizeof(float)*4*Vector4fCount,d2vr_native_constants::Stage::Vertex,nativeOwner);
        // Spare record words retain the exact live camera coefficients without
        // another allocation/lock. Zero means the live pass inferred them from
        // this WVP; a later PS inverse-projection upload must not change replay.
        uint32_t camera[2]{};
        if(d2vr_replay::g_cameraProjectionKnown) {
            memcpy(&camera[0],&d2vr_replay::g_cameraPx,sizeof(float));
            memcpy(&camera[1],&d2vr_replay::g_cameraPy,sizeof(float));
        }
        d2vr_replay::RecEmitData(d2vr_replay::OP_SetVSConstF, pConstantData,
                                 sizeof(float) * 4 * Vector4fCount, StartRegister, Vector4fCount,camera[0],camera[1],proof,
                                 4,UINT32_MAX,d2vr_replay::ActiveWvpRegister(d2vr_replay::g_activeVs),&rawReference,
                                 d2vr_replay::ActiveStpRegister(d2vr_replay::g_activeVs));
    }

    // ---- True stereo: live pass renders eye 0 (eye 1 comes from the replay pass) ----
    if (g_livePassProbe && d2vr_replay::g_replayEye < 0 && StartRegister == 0 && Vector4fCount >= 4 && pConstantData) {
        static std::vector<uint64_t> observedPasses;
        if (observedPasses.size() < 512) {
            IDirect3DSurface9* rt = nullptr;
            if (SUCCEEDED(dev->GetRenderTarget(0, &rt)) && rt) {
                D3DSURFACE_DESC desc{}; rt->GetDesc(&desc); rt->Release();
                auto* vs = d2vr_replay::g_activeVs;
                uint32_t hash = vs ? vs->hash : 0;
                uint64_t key = (uint64_t(hash) << 32) ^ (uint64_t(desc.Width) << 16) ^ desc.Height;
                if (std::find(observedPasses.begin(), observedPasses.end(), key) == observedPasses.end()) {
                    observedPasses.push_back(key);
                    D3DVIEWPORT9 vp{}; dev->GetViewport(&vp);
                    Log("[PassProbe] VS=%08X RT=%ux%u fmt=%u VP=%ux%u WVP=%d STP=%d depthOnly=%d mW=[%.4f %.4f %.4f %.4f]",
                        hash, desc.Width, desc.Height, desc.Format, vp.Width, vp.Height,
                        vs ? vs->wvpBase : -1, vs ? vs->stpBase : -1, vs ? vs->depthOnly : 0,
                        pConstantData[3],pConstantData[7],pConstantData[11],pConstantData[15]);
                }
            }
        }
    }
    if ((g_hookMask & 8) && g_submitMode == 4 && g_stereoMode >= 1 &&
        d2vr_replay::g_replayEye < 0 && pConstantData && Vector4fCount <= 68 &&
        !d2vr_replay::IsShadowAtlasPass(dev)) {
        static thread_local float s_tmpConst[d2vr_replay::kVertexStereoUploadRows * 4];
        d2vr_replay::StereoParams sp = MakeEyeParams(0);
        if (g_stereoMode == 2) sp.separation = 0.0f;
        bool perspectiveUpload=false;
        UINT uploadStart=StartRegister,uploadCount=Vector4fCount;
        if (d2vr_replay::ApplyVertexStereoUpload(s_tmpConst, pConstantData, uploadStart,
                                            uploadCount, d2vr_replay::g_activeVs, sp,
                                            &rawReference,&perspectiveUpload)) {
            pConstantData = s_tmpConst;
            StartRegister=uploadStart;Vector4fCount=uploadCount;
            if(perspectiveUpload) {
                g_had3DCameraThisFrame = true;
                ++g_3dCamCallsThisFrame;
            }
        }
    }
    const HRESULT result=g_origSetVertexShaderConstantF(dev, StartRegister, pConstantData, Vector4fCount);
    if(FAILED(result)) {
        d2vr_replay::RecLockGuard lock;
        d2vr_replay::g_vsConstantHistory.live.Invalidate(StartRegister,Vector4fCount);
        if(d2vr_replay::g_recActive)d2vr_replay::g_recOverflow=true;
    }
#ifdef D2VR_MATCHED_CAPTURE
    if(witness) EndCameraUploadWitness(cameraSample,result);
    // This witness retains the original native range, even after completion.
    EndMatchedSkinUpload(skinUpload,skinUpload.valid ?
        pConstantData+4*(skinUpload.start-StartRegister):pConstantData,result);
#endif
    return result;
}

#pragma optimize("y", on)
D2VR_CONSTANT_GATE(Hook_SetVertexShaderConstantF,Native_SetVertexShaderConstantF)

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
        // depth surface -> owning texture (INTZ), once per surface - diagnostic trace link
        static std::unordered_map<const void*, int> s_dssContLogged;
        if (pNewZStencil && s_dssContLogged.size() < 400 && s_dssContLogged.emplace(pNewZStencil, 1).second) {
            static const GUID kIidTexture9 = { 0x85C31227, 0x3DE5, 0x4F00, { 0x9B, 0x3A, 0xF1, 0x1A, 0xC3, 0x8C, 0x18, 0xB5 } };
            IDirect3DTexture9* cont = nullptr;
            if (SUCCEEDED(pNewZStencil->GetContainer(kIidTexture9, reinterpret_cast<void**>(&cont))) && cont) {
                Log("[DSSCont] %p %p", (void*)pNewZStencil, (void*)cont);
                cont->Release();
            }
        }
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
        // texture -> level-0 surface map for the traces (once per texture, capped)
        static std::unordered_map<const void*, int> s_texSurfLogged;
        if (pTexture && pTexture->GetType() == D3DRTYPE_TEXTURE && s_texSurfLogged.size() < 400 &&
            s_texSurfLogged.emplace(pTexture, 1).second) {
            IDirect3DSurface9* lvl = nullptr;
            if (SUCCEEDED(static_cast<IDirect3DTexture9*>(pTexture)->GetSurfaceLevel(0, &lvl)) && lvl) {
                Log("[TexSurf] %p %p", (void*)pTexture, (void*)lvl);
                lvl->Release();
            }
        }
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
#include "submission_status_shader.h"
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
        "    float4 aimCfg;\n"
        "    float4 statusCfg;\n"
        "};\n"
        "struct PSIn {\n"
        "    float4 pos : SV_POSITION;\n"
        "    float2 uv  : TEXCOORD0;\n"
        "};\n"
        D2VR_SUBMISSION_STATUS_HLSL
        "float4 main(PSIn i) : SV_Target {\n"
        "    float4 status=SubmissionStatus(i.uv); if(status.a>0.5) return status;\n"
        "    if (posCfg.y > 2.5) {\n"
        "        // Match the rendered tangent span to the declared composition FOV.\n"
        "        float2 uv = 0.5 + (i.uv - 0.5) * posCfg.zw;\n"
        "        float4 color = colorTex.SampleLevel(linSamp, uv, 0);\n"
        "        if (aimCfg.w > 0.5) {\n"
        "            float2 d = (i.uv-aimCfg.xy)*float2(aimCfg.z,1);\n"
        "            float r = length(d);\n"
        "            if (r > 0.003 && r < 0.007) {\n"
        "                color = (r > 0.004 && r < 0.006) ?\n"
        "                    (aimCfg.w > 1.5 ? float4(1,0.65,0,1):float4(0.2,1,0.3,1)) : float4(0,0,0,1);\n"
        "            }\n"
        "        }\n"
        "        return color;\n"
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

// Visualize the selected aiming AXIS, not a raycast or hit
// prediction. Project a direction at infinity through the same per-eye matrices.
// HUD-only segments reuse both world pixels and this projected world direction.
static bool ComputeBaseAimMarker(int eye,float cropX,float cropY,float out[4]) {
    if(g_had3DCameraThisFrame) {
        g_aimMarkerValid[eye]=false;
        float p[16];
        if(d2vr_replay::g_cameraProjectionKnown && d2vr_math::Invert(d2vr_replay::g_cameraInverseProjection,p)) {
            d2vr_replay::ApplyEyeShiftToMatrix(p,4,MakeEyeParams(eye),false);
            float dir[3]={0,0,1}; ReadNativeAimDirection(dir);
            float clip[4]{};
            for(int c=0;c<4;++c) for(int r=0;r<3;++r) clip[c]+=dir[r]*p[r*4+c];
            if(clip[3]>.0001f && std::isfinite(clip[0]/clip[3]) && std::isfinite(clip[1]/clip[3])) {
                g_aimMarkerUV[eye][0]=.5f+.5f*clip[0]/clip[3];
                g_aimMarkerUV[eye][1]=.5f-.5f*clip[1]/clip[3];
                g_aimMarkerValid[eye]=true;
            }
        }
    }
    if(!g_aimMarkerValid[eye] || cropX<=0 || cropY<=0) return false;
    const float x=.5f+(g_aimMarkerUV[eye][0]-.5f)/cropX;
    const float y=.5f+(g_aimMarkerUV[eye][1]-.5f)/cropY;
    out[0]=std::max(.025f,std::min(.975f,x));
    out[1]=std::max(.025f,std::min(.975f,y));
    out[2]=g_scH ? float(g_scW)/g_scH:1;
    out[3]=(x!=out[0] || y!=out[1]) ? 2.f:1.f;
    return true;
}

static bool CanReadSharedUpload(ID3D11ShaderResourceView*);
static bool SealSharedUpload(ID3D11ShaderResourceView*);

static StereoCBData PrepareStereoEyeConstants(
    int eyeIdx,
    const XrFovf& eyeFov,
    float invTwoHalfTanX,
    float invTwoHalfTanY,
    float eyeSign,
    float posOffsetX,
    float posOffsetY)
{
    StereoCBData value{};
    auto* cb=&value;
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

    cb->extraCfg[0] = (g_blitPass != 0) ? 1.0f : 0.0f;
    cb->extraCfg[1] = 0.015f; // kz hyperbolic depth linearization constant
    cb->extraCfg[2] = (float)g_depthViz;
    cb->extraCfg[3] = posOffsetX;

    cb->posCfg[0] = posOffsetY;
    cb->posCfg[1] = (g_blitPass != 0) ? 3.0f : (float)g_trackingSubMode;
    cb->posCfg[2] = g_trackedFrame.open && g_blitPass ? g_perEyeCropX[eyeIdx] :
        (g_fovWidenEyeX[eyeIdx] > 0.0f ? std::min(1.0f, g_fovCoverEyeX[eyeIdx] / g_fovWidenEyeX[eyeIdx])
                                       : (g_fovWidenX > 0.0f ? std::min(1.0f, g_fovCoverX / g_fovWidenX) : 1.0f));
    cb->posCfg[3] = g_trackedFrame.open && g_blitPass ? g_perEyeCropY[eyeIdx] :
        (g_fovWidenEyeY[eyeIdx] > 0.0f ? std::min(1.0f, g_fovCoverEyeY[eyeIdx] / g_fovWidenEyeY[eyeIdx])
                                       : (g_fovWidenY > 0.0f ? std::min(1.0f, g_fovCoverY / g_fovWidenY) : 1.0f));
    memset(cb->aimCfg,0,sizeof(cb->aimCfg));
    cb->statusCfg[0]=g_submissionStatus ? ((g_replayConstantBatch && g_replayConstantWindow)?2.0f:1.0f):0.0f;
    cb->statusCfg[1]=0;
    cb->statusCfg[2]=0;
#ifdef D2VR_MATCHED_CAPTURE
    if(g_submissionStatus && g_nativeViewBoundaryMode>=2) {
        cb->statusCfg[1]=g_trackedFrame.open && g_in3DGameplayMode && g_trackedFrame.id &&
            g_nativeCullingAppliedFrame==g_trackedFrame.id ? 2.f:1.f;
        cb->statusCfg[2]=float(static_cast<unsigned>(g_nativeCullingReason));
        if(g_blitPass && eyeIdx==0)ReportNativeCullingStatus();
    }
#endif
    if(g_blitPass && g_tentacleBridge && g_tentacleAimMarker && g_tentacleAimVisible && g_in3DGameplayMode)
        ComputeBaseAimMarker(eyeIdx,cb->posCfg[2],cb->posCfg[3],cb->aimCfg);
#ifdef D2VR_MATCHED_CAPTURE
    if (g_blitPass) MatchedCaptureBlit(eyeIdx,*cb);
#endif
    return value;
}

static bool RenderStereoEyeD11(int eyeIdx,const XrFovf& eyeFov,
    float invTwoHalfTanX,float invTwoHalfTanY,float eyeSign,float posOffsetX,float posOffsetY)
{
#ifdef D2VR_MATCHED_CAPTURE
    d2vr_cpu_sync::Scope cpuEye(d2vr_cpu_sync::D3D11Eye);
#endif
    if (!EnsureD11StereoPipeline() || !g_eyeRTV[eyeIdx] || !g_uploadSRV) return false;
    if(g_lastConsumerReaderFence && !CanReadSharedUpload(g_uploadSRV)) return false;
    EnsureDepthTexture11();
    const auto cb=PrepareStereoEyeConstants(eyeIdx,eyeFov,invTwoHalfTanX,invTwoHalfTanY,eyeSign,posOffsetX,posOffsetY);
    D3D11_MAPPED_SUBRESOURCE ms{};
    if (FAILED(g_ctx11->Map(g_stereoCB11,0,D3D11_MAP_WRITE_DISCARD,0,&ms))) return false;
    memcpy(ms.pData,&cb,sizeof(cb));
    g_ctx11->Unmap(g_stereoCB11, 0);

    g_ctx11->OMSetRenderTargets(1, &g_eyeRTV[eyeIdx], nullptr);
    D3D11_VIEWPORT vp{};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = (float)g_eyeCacheW;
    vp.Height = (float)g_eyeCacheH;
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
    // This Draw is the complete shared-SRV consumer. Later NIS/capture/XR copies
    // read the eye caches, so their unrelated work need not precede this event.
    return !g_lastConsumerReaderFence || SealSharedUpload(g_uploadSRV);
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
static int g_avatarHookInstalled = 0;

// The engine image is mapped at a fixed base (no ASLR for the EXE): 0x00401000 .text,
// 0x00EEB000 .data/.rdata. Anything outside those ranges is not engine code/data.
static bool IsExeText(void* p) {
    uintptr_t v = (uintptr_t)p;
    return v >= 0x00401000 && v < 0x00EEA730;
}
static bool IsExeData(void* p) {
    uintptr_t v = (uintptr_t)p;
    return v >= 0x00EEB000 && v < 0x01124000;
}

// -------------------------------------------------------------------------------------
// Native engine FOV widening.
//
// The engine publishes the camera FOV through BaseAvatar::GetFOV (vtable slot 0x270).
// Everything that needs the FOV - the projection matrix builder (007BF850: "call eax;
// fmul [00FFBA08h]" with 0.5 = tan(FOV/2)) and the CPU culling frustum - reads it from
// there, so widening GetFOV widens the projection AND the culling together.
//
// An earlier attempt patched the *call sites*; that crashed the engine (a boolean method
// of an unrelated class shares the 0x270 offset, and the "jmp" variants call with a
// different receiver). The avatar Lua bindings never fire during gameplay either, so the
// avatar object was never found and the type descriptor does not expose the class vtable
// (it only holds names + the binding table; the class vtable lives in the per-Lua-type
// info that the engine fetch returns).
//
// Reliable approach: DISCOVER the avatar's GetFOV with a pass-through probe on the call
// sites whose receiver is provably ECX ("mov eax,[edx+0x270]; call eax" right after
// "mov edx,[ecx]" - the vtable of the object in ECX). The probe forwards the original
// value unchanged, so it cannot corrupt anything. As soon as it sees a camera-like FOV
// (the game runs at 45 deg vertical) it remembers the class vtable, and that vtable's
// slot 0x270 is then replaced by a stub which scales the value - the engine rebuilds its
// projection matrix because GetFOV now returns a different number.
// Only sites where the virtual call result is consumed as float (fld/fmul), not "test al, al"
// (boolean slot 0x270 on unrelated classes). Verified against DarknessII.exe bytes.
struct FovCallSite {
    uint32_t va;
    bool widenCull; // false = projection path (native FOV only)
};
static const FovCallSite kGetFovCallSites[] = {
    {0x007BF850, false}, // projection: call; fmul [00FFBA08]
    {0x0076968A, true},  // camera / frustum float path
    {0x00DC47BC, true},  // Lua GetFOV (avatar in ECX)
};
static constexpr uint32_t kFrustumGetFovSite = 0x00B23F7F; // mov edx,[eax+270]; mov ecx,esi; call edx
// "mov eax,[edx+0x270]; call eax" - the call must be direct, otherwise ECX does not hold
// the object at the call (the "jmp" variants call a different receiver and crashed).
static const uint8_t kGetFovInstr[6] = { 0x8B, 0x82, 0x70, 0x02, 0x00, 0x00 };
struct FovPatch { uint32_t va; uint8_t saved[6]; };
static FovPatch g_fovPatches[8];
static int g_fovPatchCount = 0;
static void* g_avatarVtable = nullptr;
static PFN_VirtGetFov g_origGetFovImpl = nullptr;
static float g_lastBaseFovApplied = 0.0f;
static float g_lastNativeFovDeg = 45.0f;

// Vertical engine FOV (deg) that covers the HMD half-angles on both axes (record-pass cull).
static float ComputeHmdCoverEngineFovDeg() {
    if (InterlockedCompareExchange(&g_estValid, 0, 0) == 0 || g_estPy <= 0.0f || g_estPx <= 0.0f)
        return 0.0f;
    const float aspect = g_estPx / g_estPy;
    const float fromY = 2.0f * g_gameHalfFovY * 57.29578f;
    const float fromX = 2.0f * std::atan(std::tan(g_gameHalfFovX) / aspect) * 57.29578f;
    float t = std::max(fromX, fromY);
    if (t > 130.0f) t = 130.0f;
    return t;
}

static float NativeFovWidenFactor(float nativeDeg) {
    if (nativeDeg < 5.0f) nativeDeg = g_lastNativeFovDeg;
    float factor = g_engineFovFactor;
    if (g_autoFovWiden) {
        float target = ComputeHmdCoverEngineFovDeg();
        if (target < 40.0f && g_fovWidenY > 1.01f) target = nativeDeg * g_fovWidenY;
        if (target > nativeDeg + 0.05f) factor = std::max(factor, target / nativeDeg);
    }
    if (g_fovOverrideDeg > 0.0f && nativeDeg > 0.0f)
        factor = std::max(factor, g_fovOverrideDeg / nativeDeg);
    return factor;
}

static float ApplyNativeFovWiden(float v) {
    if (v < 5.0f || v > 170.0f) return v;
    g_lastNativeFovDeg = v;
    const float factor = NativeFovWidenFactor(v);
    if (factor <= 1.0001f) return v * factor;
    // SubmitMode 4 widens WVP on GPU; never disable HMD widen via g_engineFovFactor.
    if (g_submitMode != 4 && g_engineFovFactor <= 1.0001f) g_engineFovFactor = factor;
    return v * factor;
}

static void TryNativeBaseFovOverride() {
#ifdef D2VR_MATCHED_CAPTURE
    return;
#endif
    if (g_submitMode == 4) return;
    if (!g_autoFovWiden || !g_haveViews || !g_playerAvatar || !g_avatarVtable) return;
    const float target = ComputeHmdCoverEngineFovDeg();
    if (target < 40.0f || std::fabs(target - g_lastBaseFovApplied) < 0.25f) return;
    PFN_VirtSetBaseFov set = nullptr;
    __try {
        set = (PFN_VirtSetBaseFov) * (void**)((uint8_t*)g_avatarVtable + 0xE4);
    } __except (1) {
        return;
    }
    if (!set || !IsExeText((void*)set)) return;
    __try {
        set(g_playerAvatar, target);
        g_lastBaseFovApplied = target;
        Log("[FovRE] SetBaseFovOverride(%.1f deg) on avatar %p (HMD cover X/Y half %.1f/%.1f deg)",
            target, g_playerAvatar, g_gameHalfFovX * 57.29578f, g_gameHalfFovY * 57.29578f);
    } __except (1) {}
}

static float __fastcall InvokeAvatarGetFov(void* self, void** outVt) {
    float v = 0.0f;
    void* vt = nullptr;
    __try {
        vt = *(void**)self;
        PFN_VirtGetFov orig = (PFN_VirtGetFov) * (void**)((uint8_t*)vt + 0x270);
        v = orig(self);
    } __except (1) {
        if (outVt) *outVt = nullptr;
        return 0.0f;
    }
    if (outVt) *outVt = vt;
    return v;
}

static void NoteAvatarFromGetFov(void* self, void* vt, float v) {
    if (v <= 30.0f || v >= 70.0f || !vt || !IsExeData(vt)) return;
    g_playerAvatar = self;
    if (!g_avatarVtable) {
        PFN_VirtGetFov impl = nullptr;
        __try { impl = (PFN_VirtGetFov) * (void**)((uint8_t*)vt + 0x270); } __except (1) {}
        if (impl && IsExeText((void*)impl)) {
            g_avatarVtable = vt;
            g_origGetFovImpl = impl;
            Log("[FovRE] avatar GetFOV identified: vtable %p impl %p FOV %.2f deg (no vtable patch)",
                vt, impl, v);
        }
    }
}

static float __fastcall Probe_GetFov_Projection(void* self) {
    void* vt = nullptr;
    const float v = InvokeAvatarGetFov(self, &vt);
    NoteAvatarFromGetFov(self, vt, v);
    return v;
}

static float __fastcall Probe_GetFov_Cull(void* self) {
    void* vt = nullptr;
    const float v = InvokeAvatarGetFov(self, &vt);
    NoteAvatarFromGetFov(self, vt, v);
    if (v < 5.0f || v > 170.0f) return v;
    const float out = ApplyNativeFovWiden(v);
    static DWORD s_probeLog = 0;
    const DWORD now = GetTickCount();
    if (out > v + 0.05f && now - s_probeLog > 4000) {
        s_probeLog = now;
        Log("[FovRE] cull call-site widen: %.1f -> %.1f deg (GPU widenX=%.2f)",
            v, out, g_fovWidenX);
    }
    return out;
}

static bool PatchExeCallSite(uint32_t va, void* hook, uint8_t* saved, size_t savedLen) {
    if (!NativeImageWriteAllowed(va,6)) return false;
    uint8_t* p = (uint8_t*)va;
    uint8_t cur[6];
    __try { memcpy(cur, p, 6); } __except (1) { return false; }
    uint8_t follow[2] = { 0, 0 };
    __try { memcpy(follow, p + 6, 2); } __except (1) { return false; }
    if (memcmp(cur, kGetFovInstr, 6) != 0 || follow[0] != 0xFF || follow[1] != 0xD0) return false;
    uint8_t use[2] = { 0, 0 };
    __try { memcpy(use, p + 8, 2); } __except (1) {}
    if (use[0] == 0x84 && use[1] == 0xC0) return false;
    const uint32_t hookAddr = (uint32_t)(uintptr_t)hook;
    uint8_t patch[6] = { 0xB8, 0, 0, 0, 0, 0x90 };
    memcpy(patch + 1, &hookAddr, 4);
    __try { memcpy(p, patch, 6); } __except (1) { return false; }
    if (saved && savedLen >= 6) memcpy(saved, cur, 6);
    return true;
}

static bool InstallFrustumGetFovHook() {
    if (!NativeImageWriteAllowed(kFrustumGetFovSite,10)) return false;
    constexpr uint8_t kExpect[10] = { 0x8B, 0x90, 0x70, 0x02, 0x00, 0x00, 0x8B, 0xCE, 0xFF, 0xD2 };
    uint8_t* p = (uint8_t*)kFrustumGetFovSite;
    uint8_t cur[10];
    __try { memcpy(cur, p, 10); } __except (1) { return false; }
    if (memcmp(cur, kExpect, 10) != 0) {
        Log("[FovRE] frustum site %08X byte mismatch - skipped", kFrustumGetFovSite);
        return false;
    }
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    const uintptr_t page = kFrustumGetFovSite & ~(si.dwPageSize - 1);
    DWORD oldProtect = 0;
    if (!VirtualProtect((void*)page, si.dwPageSize, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
    const uint32_t hookAddr = (uint32_t)(uintptr_t)&Probe_GetFov_Cull;
    uint8_t patch[10] = { 0x8B, 0xCE, 0xB8, 0, 0, 0, 0, 0xFF, 0xD0, 0x90 };
    memcpy(patch + 3, &hookAddr, 4);
    bool ok = false;
    __try { memcpy(p, patch, 10); ok = true; } __except (1) {}
    DWORD restore = 0;
    VirtualProtect((void*)page, si.dwPageSize, oldProtect, &restore);
    if (ok) Log("[FovRE] frustum GetFOV hook @%08X (cull widen, ecx=esi)", kFrustumGetFovSite);
    return ok;
}

static bool InstallGetFovProbe() {
#ifdef D2VR_MATCHED_CAPTURE
    return false;
#endif
    if (g_fovPatchCount > 0) return true;
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    const uintptr_t pageSize = si.dwPageSize;
    uintptr_t minPage = 0xFFFFFFFF, maxPage = 0;
    for (const FovCallSite& site : kGetFovCallSites) {
        const uintptr_t pg = (uintptr_t)site.va & ~(pageSize - 1);
        if (pg < minPage) minPage = pg;
        if (pg > maxPage) maxPage = pg;
    }
    const uintptr_t frustumPage = kFrustumGetFovSite & ~(pageSize - 1);
    if (frustumPage < minPage) minPage = frustumPage;
    if (frustumPage > maxPage) maxPage = frustumPage;
    if (!NativeImageWriteAllowed(minPage,maxPage-minPage+pageSize)) return false;
    DWORD oldProtect = 0;
    if (!VirtualProtect((void*)minPage, (SIZE_T)(maxPage - minPage + pageSize),
                        PAGE_EXECUTE_READWRITE, &oldProtect)) {
        Log("[FovRE] VirtualProtect for FOV sites failed (err %lu)", GetLastError());
        return false;
    }
    int cullSites = 0, projSites = 0;
    for (const FovCallSite& site : kGetFovCallSites) {
        void* hook = site.widenCull ? (void*)&Probe_GetFov_Cull : (void*)&Probe_GetFov_Projection;
        if (g_fovPatchCount >= 8) break;
        if (!PatchExeCallSite(site.va, hook, g_fovPatches[g_fovPatchCount].saved, 6)) {
            Log("[FovRE] call site %08X does not match GetFOV pattern - skipped", site.va);
            continue;
        }
        g_fovPatches[g_fovPatchCount].va = site.va;
        ++g_fovPatchCount;
        if (site.widenCull) ++cullSites;
        else ++projSites;
    }
    bool frustumOk = false;
    if (g_fovFrustumHook) frustumOk = InstallFrustumGetFovHook();
    DWORD restore = 0;
    VirtualProtect((void*)minPage, (SIZE_T)(maxPage - minPage + pageSize), oldProtect, &restore);
    Log("[FovRE] GetFOV hooks: %d cull + %d projection exe sites; frustum=%d (deferred=%d)",
        cullSites, projSites, (int)frustumOk, (int)(g_fovFrustumHook == 0));
    InterlockedExchange(&g_fovProbeInstalled, 1);
    return g_fovPatchCount > 0 || frustumOk;
}

static void MaybeInstallFovProbeAfterDevice() {
    if (g_startupMode >= 2 || !g_autoFovWiden) return;
    if (InterlockedCompareExchange(&g_fovProbeInstalled, 1, 1) == 1) return;
    LogStartupStage(12, "InstallGetFovProbe (post-CreateDevice, not DllMain)");
    InstallGetFovProbe();
}

static void InstallAvatarSetViewHook() {
    if (g_avatarHookInstalled) return;
    // The avatar Lua bindings never fire during gameplay (the HUD is C++), so hooking
    // them is pointless. The GetFOV implementation is patched directly instead.
    g_avatarHookInstalled = 1;
}

static void TryApplyEngineFovOverride() {
    if (!g_autoFovWiden && g_fovOverrideDeg <= 0.0f) return;
    if (!g_avatarVtable) InstallGetFovProbe();
    TryNativeBaseFovOverride();
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
    sprintf_s(p0, "test-output\\run_%lu_frame_%03d_eye0.bmp", GetCurrentProcessId(), s_frameIdx);
    sprintf_s(p1, "test-output\\run_%lu_frame_%03d_eye1.bmp", GetCurrentProcessId(), s_frameIdx);
    bool a = DumpTextureToBmp(g_eyeCache[0], p0);
    bool b = DumpTextureToBmp(g_eyeCache[1], p1);
    char tracePath[MAX_PATH];
    sprintf_s(tracePath, "test-output\\run_%lu_frame_%03d_trace.txt", GetCurrentProcessId(), s_frameIdx);
    FILE* trace = nullptr;
    if (fopen_s(&trace, tracePath, "w") == 0 && trace) {
        fprintf(trace, "# LightingPreset=%d name=%s ProjectionFix=%d widen=%.4f,%.4f IPD=%.5f\n",
            g_lightingPreset < 0 ? -1 : (g_lightingPreset == 9 ? 0 : g_lightingPreset + 1),
            g_lightingPreset < 0 ? "configured" : kLightingPresetNames[g_lightingPreset],
            g_projectionFix, g_fovWidenX, g_fovWidenY, g_stereoIPD);
        fprintf(trace, "# Sharpness DOFBypass=%d FXAABypass=%d Anisotropy=%d ClipScaledUI=%d source=%dx%d\n",
            (int)g_disableDepthOfField, (int)g_disableFxaa, g_textureAnisotropy,
            (int)g_clipScaledUi, g_renderW, g_renderH);
        for (size_t n = 0; n < d2vr_replay::g_rec.size(); ++n) {
            const auto& r = d2vr_replay::g_rec[n];
            fprintf(trace, "%zu op=%u a=%u,%u,%u,%u,%u\n", n, r.op,r.a[0],r.a[1],r.a[2],r.a[3],r.a[4]);
            if (r.op == d2vr_replay::OP_SetClipPlane) {
                const float* plane = (const float*)(d2vr_replay::g_recData.data() + r.a[4]);
                fprintf(trace, " ClipPlane index=%u equation=%.9g %.9g %.9g %.9g\n",
                    r.a[0], plane[0], plane[1], plane[2], plane[3]);
            }
            if (r.op == d2vr_replay::OP_SetRenderTarget && r.a[1]) {
                auto* target = (IDirect3DSurface9*)(uintptr_t)r.a[1];
                D3DSURFACE_DESC desc{};
                if (SUCCEEDED(target->GetDesc(&desc)))
                    fprintf(trace, " RT slot=%u size=%ux%u format=%u\n",
                        r.a[0], desc.Width, desc.Height, (unsigned)desc.Format);
            }
            if (r.op == d2vr_replay::OP_SetVertexShader) {
                const auto metadata=d2vr_replay::g_vsInfo.Lookup(reinterpret_cast<void*>(uintptr_t(r.a[0])));
                if (metadata) fprintf(trace, " VS %08X guessed=%d,%d named=%d,%d\n",metadata->hash,
                    metadata->wvpBase,metadata->stpBase,metadata->reflectedWvp,metadata->reflectedStp);
            }
            if (r.op == d2vr_replay::OP_SetPixelShader) {
                const auto metadata=d2vr_replay::g_pixelShaders.Lookup(reinterpret_cast<void*>(uintptr_t(r.a[0])));
                fprintf(trace," PS inverse=%d hash=%08X\n",metadata.inverse,metadata.hash);
            }
            if (r.op == d2vr_replay::OP_SetTexture && r.a[1]) {
                auto* texture = (IDirect3DBaseTexture9*)(uintptr_t)r.a[1];
                if (texture->GetType() == D3DRTYPE_TEXTURE) {
                    D3DSURFACE_DESC desc{};
                    if (SUCCEEDED(((IDirect3DTexture9*)texture)->GetLevelDesc(0, &desc)))
                        fprintf(trace, " Texture stage=%u size=%ux%u levels=%u format=%u usage=%08lX\n",
                            r.a[0], desc.Width, desc.Height, texture->GetLevelCount(),
                            (unsigned)desc.Format, desc.Usage);
                }
            }
            if (r.op == d2vr_replay::OP_SetVSConstF || r.op == d2vr_replay::OP_SetPSConstF) {
                size_t bytes = size_t(r.a[1])*16;
                if (r.a[1] <= 32 && r.a[4] <= d2vr_replay::g_recData.size() && bytes <= d2vr_replay::g_recData.size()-r.a[4]) {
                    const float* f = (const float*)(d2vr_replay::g_recData.data()+r.a[4]);
                    for (UINT c = 0; c < r.a[1]; ++c) fprintf(trace," c%u %.7g %.7g %.7g %.7g\n",r.a[0]+c,f[c*4],f[c*4+1],f[c*4+2],f[c*4+3]);
                }
            }
        }
        fclose(trace);
    }
    Log("[EyeDump] %s + %s (eye0=%d eye1=%d)", p0, p1, (int)a, (int)b);
    ++s_frameIdx;
}

// ---------------------------------------------------------------- Zero-copy capture (D3D9Ex shared texture)
// The old path (GetRenderTargetData -> sysmem -> memcpy -> D3D11) stalls the CPU waiting
// for the GPU twice per frame (GPU idle, FPS low). D3D9Ex shared textures let D3D11 read
// the backbuffer copy with zero CPU involvement.
#include "shared_capture.inl"

static bool StereoTransferAvailable(IDirect3DDevice9* dev) {
    if((g_stereoTransferMode!=1 && g_stereoTransferMode!=3) || g_stereoTransferFault) return false;
    if(g_stereoTransferQueue) return g_stereoTransferOwner==GetCurrentThreadId() && g_stereoTransferDevice==dev;
    D3DDEVICE_CREATION_PARAMETERS parameters{};
    if(FAILED(dev->GetCreationParameters(&parameters)) ||
       (g_stereoTransferMode==1 && !(parameters.BehaviorFlags&D3DCREATE_MULTITHREADED))) return false;
    // Worker stays idle between frame-local leases. Pin code for its process
    // lifetime; never join a C++ thread from the DLL loader-lock destructor.
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&StereoTransferAvailable),&pinned)) return false;
    try { g_stereoTransferQueue=new d2vr_transfer::Queue; }
    catch(...) { return false; }
    g_stereoTransferOwner=GetCurrentThreadId();
    g_stereoTransferDevice=dev;
    Log("[StereoTransferWorker] admitted mode=%d ownerTid=%lu flags=0x%lX; same-frame interop only",
        g_stereoTransferMode,g_stereoTransferOwner,parameters.BehaviorFlags);
    return true;
}

static bool StartStereoTransfer(IDirect3DDevice9* dev,int eye,d2vr_transfer::Pending& pending,
    std::shared_ptr<d2vr_transfer::Job>& job,unsigned capture=0) {
    if(eye<0 || eye>1 || capture>=2)return false;
    auto& slot=g_sharedCapture[capture];
    if(!EnsureD11StereoPipeline() || !g_eyeRTV[eye] || !slot.srv11) return false;
    EnsureDepthTexture11();
    // No global scene/camera state is consulted by the worker.
    ID3D11ShaderResourceView* saved=g_uploadSRV; g_uploadSRV=slot.srv11;
    const auto constants=PrepareStereoEyeConstants(eye,g_eyeFov[eye],1,1,1,0,0);
    g_uploadSRV=saved;
    try { job=std::make_shared<d2vr_transfer::Job>(); } catch(...) { return false; }
    using namespace d2vr_transfer;
    if(g_stereoTransferMode!=3)job->producer=Ref<IDirect3DQuery9>(slot.producer);
    job->context=Ref<ID3D11DeviceContext>(g_ctx11);
    job->reader=Ref<ID3D11Query>(slot.reader);
    job->target=Ref<ID3D11RenderTargetView>(g_eyeRTV[eye]);
    job->constants=Ref<ID3D11Buffer>(g_stereoCB11);
    job->vs=Ref<ID3D11VertexShader>(g_stereoVS11);
    job->ps=Ref<ID3D11PixelShader>(g_stereoPS11);
    job->raster=Ref<ID3D11RasterizerState>(g_stereoRS11);
    job->sampler=Ref<ID3D11SamplerState>(g_stereoSamp11);
    job->source=Ref<ID3D11ShaderResourceView>(slot.srv11);
    job->depth=Ref<ID3D11ShaderResourceView>(g_depthSRV11);
    static_assert(sizeof(constants)<=256,"worker packet constants too small");
    memcpy(job->bytes.data(),&constants,sizeof(constants)); job->size=sizeof(constants);
    job->width=g_eyeCacheW; job->height=g_eyeCacheH;
    job->deferReader=g_deferStereoTransferReader && g_lastConsumerReaderFence;
    if(!CaptureBackbufferToShared(dev,g_stereoTransferMode!=3,capture)) { job.reset(); return false; }
    job->producerComplete=g_stereoTransferMode==3;
    if(!pending.Start(*g_stereoTransferQueue,job,g_stereoTransferFault)) {
        g_stereoTransferFault=true; return false;
    }
    if(job->deferReader) {
        slot.deferred=job;
        SharedReaderFenceOps ops{slot};
        if(!slot.fence.TrackIssuedRead(ops)) {g_stereoTransferFault=true;return false;}
    }
    return true;
}

static bool FinishStereoTransfer(d2vr_transfer::Pending& pending,
    const std::shared_ptr<d2vr_transfer::Job>& job) {
    if(!pending.Active()) return false;
    const auto begin=d2vr_transfer::Job::Clock();
    const bool ok=pending.Finish();
    const auto end=d2vr_transfer::Job::Clock();
    static unsigned count=0; static double joinMs=0,workerMs=0; static LARGE_INTEGER frequency{};
    if(!frequency.QuadPart) QueryPerformanceFrequency(&frequency);
    if(frequency.QuadPart) {
        joinMs+=(end-begin)*1000.0/frequency.QuadPart;
        workerMs+=(job->end-job->start)*1000.0/frequency.QuadPart;
    }
    if(++count==240 || !ok) {
        Log("[StereoTransferWorker] jobs=%u workerTid=%lu mainJoinMean=%.3fms workerMean=%.3fms ok=%d hr=0x%08lX; overlapping scopes, not additive",
            count,job->thread,joinMs/count,workerMs/count,int(ok),job->result);
        count=0; joinMs=workerMs=0;
    }
    return ok;
}

// ---------------------------------------------------------------- True stereo eye pair (live pass + replay pass)
// Eye 0 is the live game pass (already in the backbuffer), eye 1 is produced by
// replaying the recorded device call stream with the opposite eye shift.
bool CopyFrame(IDirect3DDevice9* dev);
bool EnsureSwapchains(UINT w, UINT h);

// Keep the right output across scene segments. A draw can leave untouched
// pixels even when it uses a perspective world matrix (scissor, depth, alpha
// rejection or limited geometry). Those pixels must not inherit the left eye.
struct EyeColorHistory {
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* pixels = nullptr;
    bool rejected = false;
    void Release() {
        if (pixels) pixels->Release();
        if (target) target->Release();
        pixels = nullptr; target = nullptr; rejected = false;
    }
    void Invalidate() {
        Release(); rejected = true;
    }
    bool Restore(IDirect3DDevice9* dev, IDirect3DSurface9* output) {
        // A failed segment may already have changed the live world's pixels.
        // An older right image must not make the following UI segment valid.
        if (rejected) return false;
        if (target != output) { Release(); return true; }
        return !pixels || SUCCEEDED(dev->StretchRect(pixels, nullptr, output, nullptr, D3DTEXF_NONE));
    }
    bool Save(IDirect3DDevice9* dev, IDirect3DSurface9* output) {
        if (!output) { Invalidate(); return false; }
        if (target != output) Release();
        if (!pixels) {
            D3DSURFACE_DESC d{};
            if (FAILED(output->GetDesc(&d)) || FAILED(dev->CreateRenderTarget(
                d.Width, d.Height, d.Format, d.MultiSampleType, d.MultiSampleQuality,
                FALSE, &pixels, nullptr))) { Invalidate(); return false; }
            target = output; target->AddRef();
        }
        if (FAILED(dev->StretchRect(output, nullptr, pixels, nullptr, D3DTEXF_NONE))) {
            Invalidate(); return false;
        }
        rejected = false;
        return true;
    }
};
static EyeColorHistory g_rightColorHistory;
static bool SeedRightEyeBackground(IDirect3DDevice9* dev, IDirect3DSurface9* output,
                                   bool hasWorldDraw) {
    // Preserve the existing fresh-world recovery after a rejected segment.
    // Otherwise always carry available right-eye history: the world activity
    // flag proves neither full-screen coverage nor a full color clear. A full
    // replay will overwrite the seed; a partial replay needs its untouched area.
    if(hasWorldDraw && g_rightColorHistory.rejected)return true;
    return g_rightColorHistory.Restore(dev, output);
}
static bool FinishRightEyeHistory(IDirect3DDevice9* dev, IDirect3DSurface9* output,
                                 bool replayOk, bool historyOk) {
    if (!replayOk || !historyOk) {
        g_rightColorHistory.Invalidate();
        return false;
    }
    return g_rightColorHistory.Save(dev, output);
}
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

#ifdef D2VR_MATCHED_CAPTURE
static void MatchedCaptureHandsWitness(IDirect3DVertexBuffer9*,IDirect3DIndexBuffer9*,
    UINT,const D3DVIEWPORT9&,unsigned,const float*,int,unsigned);
#include "matched_capture.inl"
static bool MatchedCaptureRecordingActive() { return matched_capture::active; }
static uint64_t MatchedSkinHash(const float* data,UINT vectorCount) {
    const auto* bytes=(const unsigned char*)data;
    uint64_t hash=14695981039346656037ull;
    for(size_t i=0;i<(size_t)vectorCount*16;++i) hash=(hash^bytes[i])*1099511628211ull;
    return hash;
}
static MatchedSkinUploadWitness BeginMatchedSkinUpload(const float* data,UINT start,UINT count,void* caller) {
    MatchedSkinUploadWitness w{};
    // A full upload may begin below c45. Record its intersection with the
    // 34 matrices consumed by the verified arm VB, on the live eye only.
    const uint64_t end=(uint64_t)start+count;
    if(!data || !count || start>=147 || end<=45 || d2vr_replay::g_replayEye>=0 ||
       !matched_capture::active || g_capturePresent.thread!=GetCurrentThreadId() ||
       !matched_capture::Current()) return w;
    ++matched_capture::skinUploadsSeen;
    if(count>256 || matched_capture::skinUploadsSeen>256) return w;
    const UINT first=std::max(start,45u),last=(UINT)std::min(end,(uint64_t)147);
    w.valid=true;w.frame=matched_capture::frame;w.beforeDraw=matched_capture::draws[0];
    w.start=start;w.count=count;w.bankFirst=first;w.bankCount=last-first;
    w.bankHash=MatchedSkinHash(data+4*(first-start),last-first);w.caller=caller;
    w.vsHash=d2vr_replay::g_activeVs ? d2vr_replay::g_activeVs->hash:0;
    return w;
}
static void EndMatchedSkinUpload(const MatchedSkinUploadWitness& w,const float* forwarded,HRESULT hr) {
    if(!w.valid || !matched_capture::Current() || !matched_capture::meta) return;
    const uint64_t forwardedHash=MatchedSkinHash(forwarded+4*(w.bankFirst-w.start),w.bankCount);
    const uintptr_t base=(uintptr_t)GetModuleHandleA(nullptr),addr=(uintptr_t)w.caller;
    unsigned long rva=0;bool exeCaller=false;
    if(base) {
        const auto* dos=(const IMAGE_DOS_HEADER*)base;
        if(dos->e_magic==IMAGE_DOS_SIGNATURE && dos->e_lfanew>0 && dos->e_lfanew<0x1000) {
            const auto* nt=(const IMAGE_NT_HEADERS32*)(base+dos->e_lfanew);
            if(nt->Signature==IMAGE_NT_SIGNATURE && addr>=base && addr-base<nt->OptionalHeader.SizeOfImage) {
                exeCaller=true;rva=(unsigned long)(addr-base);
            }
        }
    }
    fprintf(matched_capture::meta,
        "{\"kind\":\"hand_skin_upload\",\"frame\":%llu,\"eye\":0,\"before_draw\":%u,"
        "\"start\":%u,\"count\":%u,\"bank_first\":%u,\"bank_count\":%u,"
        "\"bank_fnv64\":\"%016llx\",\"forwarded_bank_fnv64\":\"%016llx\","
        "\"vs_hash\":%u,\"caller\":\"%p\","
        "\"caller_exe_rva\":%s,\"hr\":%ld}\n",
        w.frame,w.beforeDraw,w.start,w.count,w.bankFirst,w.bankCount,
        w.bankHash,forwardedHash,w.vsHash,w.caller,exeCaller?std::to_string(rva).c_str():"null",hr);
    ++matched_capture::skinUploadsSaved;
}
static void NativeCameraCaptureChanged() {
    if(matched_capture::active) matched_capture::Reject("selected native camera changed after capture header; request a stable frame");
}
#endif
// Explicit Reset cleanup; avoid COM destruction from static teardown/DllMain.
static d2vr_replay::DeviceSnapshot* g_liveEndSnapshot=nullptr;
static void RenderEyePairFromReplay(IDirect3DDevice9* dev) {
    if(g_stereoTransferFault) { d2vr_replay::g_recActive=false; g_eyePairReady=false; return; }
#ifdef D2VR_MATCHED_CAPTURE
    if(g_nativeCameraCoverInstalled && g_nativeCameraCoverConfirmed && g_had3DCameraThisFrame &&
       g_nativeCoveredFrame!=g_trackedFrame.id) {
        g_trackedFrame.renderTrackingValid=false;
        NativeCameraCaptureChanged();
        Log("[NativeCameraCover] REFUSED stale camera frame=%llu expected=%llu; no XR world layer",
            g_nativeCoveredFrame,g_trackedFrame.id);
    }
#endif
    // Seal engine commands BEFORE our own capture/transfer calls. In particular,
    // the left-eye StretchRect into its capture slot must never enter the replay:
    // replaying that copy overwrites the shared texture while D3D11 may still
    // be reading the left eye, bypassing CaptureBackbufferToShared's reader fence.
    d2vr_replay::g_recActive = false;
    g_pairSubmitMetaValid = false;
    if (g_haveViews) {
        g_pairSubmitHead = HeadCenterPose(g_eyePose[0], g_eyePose[1]);
        g_pairSubmitEyePose[0] = g_eyePose[0];
        g_pairSubmitEyePose[1] = g_eyePose[1];
        g_pairSubmitFov[0] = g_eyeFov[0];
        g_pairSubmitFov[1] = g_eyeFov[1];
        // The game projection is symmetric and the D3D11 blit only center-crops.
        // Raw asymmetric HMD FOVs describe different rays and split the optical
        // axes. Derive submission angles from the actual projection and the
        // frozen transform/crop used for this pair.
        if (d2vr_replay::g_cameraProjectionKnown) {
            for (int e = 0; e < 2; ++e) {
                const auto sp = MakeEyeParams(e);
                float hx = 0, hy = 0;
                if (d2vr_stereo::RenderedHalfFov(d2vr_replay::g_cameraPx,
                        sp.fovScaleX, g_perEyeCropX[e], &hx) &&
                    d2vr_stereo::RenderedHalfFov(d2vr_replay::g_cameraPy,
                        sp.fovScaleY, g_perEyeCropY[e], &hy)) {
                    g_pairSubmitFov[e] = {-hx, hx, hy, -hy};
                    if ((g_trueStereoFrames % 600) == 0)
                        Log("[RenderedFov] eye=%d PxPy=%.7g,%.7g scale=%.7g,%.7g crop=%.7g,%.7g submit=%.7g,%.7g,%.7g,%.7g hmd=%.7g,%.7g,%.7g,%.7g",
                            e,d2vr_replay::g_cameraPx,d2vr_replay::g_cameraPy,
                            sp.fovScaleX,sp.fovScaleY,g_perEyeCropX[e],g_perEyeCropY[e],
                            -hx,hx,hy,-hy,g_eyeFov[e].angleLeft,g_eyeFov[e].angleRight,
                            g_eyeFov[e].angleUp,g_eyeFov[e].angleDown);
                }
            }
        }
        g_pairSubmitMetaValid = true;
    }
    LARGE_INTEGER perf[5]{}, frequency{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&perf[0]);
    static int s_logged = 0;
    const size_t recCount = d2vr_replay::g_rec.size();
    const size_t recBytes = d2vr_replay::g_recData.size();

    UINT targetW = g_steamVrRecW ? g_steamVrRecW : (g_srcW ? g_srcW : 1920);
    UINT targetH = g_steamVrRecH ? g_steamVrRecH : (g_srcH ? g_srcH : 1080);
    if (g_matchEyeResolutionToSource && g_stereoMode==1) {
        // Read the current RT, not the previous frame's upload dimensions.
        // A reset/first frame must not size the pair from stale g_srcW/H.
        IDirect3DSurface9* source=nullptr; D3DSURFACE_DESC desc{};
        if (SUCCEEDED(dev->GetRenderTarget(0,&source)) && source) {
            if (SUCCEEDED(source->GetDesc(&desc))) {
                const auto size=d2vr_stereo::SelectEyeOutputSize({targetW,targetH},
                    {desc.Width,desc.Height},{g_xrMaxEyeW,g_xrMaxEyeH},true);
                targetW=size.width; targetH=size.height;
            }
            source->Release();
        }
        static UINT loggedW=0,loggedH=0;
        if (targetW!=loggedW || targetH!=loggedH) {
            Log("[EyeResolution] matchSource=1 source=%ux%u output=%ux%u recommended=%ux%u maxBothEyes=%ux%u",
                desc.Width,desc.Height,targetW,targetH,g_steamVrRecW,g_steamVrRecH,g_xrMaxEyeW,g_xrMaxEyeH);
            loggedW=targetW; loggedH=targetH;
        }
    }
    static int s_earlyLog = 0;
    auto warnEarly = [&](const char* why) {
        if (s_earlyLog++ < 8) Log("[StereoPair] early return: %s (rec=%zu) - no eye pair, no dumps", why, recCount);
    };
    if (!EnsureSwapchains(targetW, targetH)) {
        warnEarly("EnsureSwapchains failed");
        d2vr_replay::g_recActive = false;
        return;
    }
    const bool zeroCopy = EnsureSharedCapture(dev);
    const bool transferWorker=zeroCopy && StereoTransferAvailable(dev);
    // Prepare the second slot while the owner still holds its D3D11 context.
    // Failure keeps the established one-slot route; no pending read is bypassed.
    const bool splitCapture=transferWorker && g_deferStereoTransferReader &&
        g_lastConsumerReaderFence && PrepareSplitSharedCapture(dev);
    const unsigned rightCapture=splitCapture?1u:0u;
    static int reportedSplit=-1;
    if(reportedSplit!=int(splitCapture)) {
        reportedSplit=int(splitCapture);
        Log("[SharedCapture] eyeSlots=%u; exact per-slot reader fences; same-frame images, no added frame latency",splitCapture?2u:1u);
    }
    d2vr_transfer::Pending leftTransfer,rightTransfer;
    std::shared_ptr<d2vr_transfer::Job> leftJob,rightJob;

#ifdef D2VR_MATCHED_CAPTURE
    matched_capture::Source(dev,0);
#endif
    g_blitPass = 1;
    bool eye0Ok = transferWorker ? StartStereoTransfer(dev,0,leftTransfer,leftJob) :
        (zeroCopy ? CaptureBackbufferToShared(dev) : CopyFrame(dev));
    if(!transferWorker) {
        ID3D11ShaderResourceView* saved = g_uploadSRV;
        if (zeroCopy) g_uploadSRV = g_sharedCapture[0].srv11;
        if (eye0Ok) eye0Ok = RenderStereoEyeD11(0, g_eyeFov[0], 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
        g_uploadSRV = saved;
    }

    d2vr_replay::StereoParams sp1 = MakeEyeParams(1);
    QueryPerformanceCounter(&perf[1]);
    if (g_stereoMode == 2) {
        sp1.separation = 0.0f;
        sp1.transX = 0.0f;
        sp1.transY = 0.0f;
    }
    d2vr_replay::DeviceSnapshot localLiveEnd;
    if(g_reuseStateSnapshots && !g_liveEndSnapshot)
        g_liveEndSnapshot=new(std::nothrow) d2vr_replay::DeviceSnapshot;
    auto& liveEnd=(g_reuseStateSnapshots && g_liveEndSnapshot)?*g_liveEndSnapshot:localLiveEnd;
    if (!liveEnd.Capture(dev,g_reuseStateSnapshots)) {
        warnEarly("liveEnd.Capture failed");
        g_blitPass = 0; g_eyePairReady = false; return;
    }
    // Preserve live pixels for restoration. A complete world render uses current
    // pixels, but UI-only segments must retain the last RIGHT-eye background.
    if (!BackupLiveColor(dev, liveEnd.targets[0])) {
        warnEarly("BackupLiveColor failed");
        liveEnd.Release(); g_blitPass = 0; g_eyePairReady = false; return;
    }
    const bool historyOk = SeedRightEyeBackground(dev, liveEnd.targets[0], g_hadWorldDrawThisFrame);
    const bool frameStateOk=d2vr_replay::g_frameStart.Apply(dev);
    if(!frameStateOk)warnEarly("frame-start state restore failed");
    g_gpuTiming.MarkLiveEnd();
    d2vr_replay::InvalidateShadowAtlasPass();
    const bool replayOk=frameStateOk && d2vr_replay::ReplayStream(dev, 1, sp1);
    if(!replayOk) {
        static unsigned rejectedReplays=0;
        if(++rejectedReplays<=8 || rejectedReplays%600==0)
            Log("[StereoPair] incomplete replay count=%u; no right history save or XR layer",rejectedReplays);
    }
    QueryPerformanceCounter(&perf[2]);

    // This interval used D3D9 only. Reclaim the D3D11 immediate context.
    // With split inputs the right copy waits only its own prior reader;
    // fallback slot zero retains the original left-reader overwrite fence.
    if(leftTransfer.Active()) eye0Ok=FinishStereoTransfer(leftTransfer,leftJob) && eye0Ok;
    leftJob.reset();

#ifdef D2VR_MATCHED_CAPTURE
    matched_capture::Source(dev,1);
#endif
    bool eye1Ok = FinishRightEyeHistory(dev, liveEnd.targets[0], replayOk, historyOk);
    if (eye1Ok) eye1Ok = transferWorker ? StartStereoTransfer(dev,1,rightTransfer,rightJob,rightCapture) :
        (zeroCopy ? CaptureBackbufferToShared(dev) : CopyFrame(dev));
    if(!transferWorker) {
        ID3D11ShaderResourceView* saved = g_uploadSRV;
        if (zeroCopy) g_uploadSRV = g_sharedCapture[0].srv11;
        if (eye1Ok) eye1Ok = RenderStereoEyeD11(1, g_eyeFov[1], 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
        g_uploadSRV = saved;
    }
    g_blitPass = 0;
    if (FAILED(dev->StretchRect(g_liveColorBackup, nullptr, liveEnd.targets[0], nullptr, D3DTEXF_NONE)))
        eye1Ok = false;
    if(!liveEnd.Apply(dev)) {
        eye1Ok=false;
        d2vr_replay::RecLockGuard lock;
        d2vr_replay::g_vsConstantHistory.Clear();
        d2vr_replay::g_psEyeHistory.Clear();
        warnEarly("live-end state restore failed");
    }
    d2vr_replay::InvalidateShadowAtlasPass();
    liveEnd.Release(g_reuseStateSnapshots);
    // Return the worker's context lease before XR/NIS or capture readback.
    // A deferred GPU reader remains retained until shared reuse/Reset fences it.
    if(rightTransfer.Active()) eye1Ok=FinishStereoTransfer(rightTransfer,rightJob) && eye1Ok;
    rightJob.reset();
    QueryPerformanceCounter(&perf[3]);

    DumpEyeFramesIfRequested();
    QueryPerformanceCounter(&perf[4]);
    // CPU wall times include driver waits; these are not GPU timestamp queries.
    static double phaseMs[4]{};
    static unsigned perfSamples = 0;
#ifdef D2VR_MATCHED_CAPTURE
    static long long uploadWindowQpc = 0;
    if (!perfSamples) uploadWindowQpc = perf[0].QuadPart;
#endif
    for (int i = 0; i < 4; ++i)
        phaseMs[i] += (perf[i+1].QuadPart - perf[i].QuadPart) * 1000.0 / frequency.QuadPart;
    if (++perfSamples == 120) {
        Log("[StereoPerf] mean CPU wall ms/segment: left=%.2f replay+history=%.2f right+restore=%.2f dump=%.2f source=%ux%u",
            phaseMs[0]/120, phaseMs[1]/120, phaseMs[2]/120, phaseMs[3]/120, g_srcW, g_srcH);
#ifdef D2VR_MATCHED_CAPTURE
        d2vr_replay::g_constantUploadWindow.Report(g_trueStereoFrames+1,uploadWindowQpc,perf[4].QuadPart,frequency.QuadPart);
#endif
        for (double& ms : phaseMs) ms = 0;
        perfSamples = 0;
    }

    d2vr_replay::g_recActive = false;
    if (g_stereoDebugMode == 2 && eye0Ok && g_dev11 && g_eyeCache[0] && g_eyeCache[1])
        g_ctx11->CopyResource(g_eyeCache[1], g_eyeCache[0]);
    else if (g_stereoDebugMode == 3 && eye1Ok && g_dev11 && g_eyeCache[0] && g_eyeCache[1])
        g_ctx11->CopyResource(g_eyeCache[0], g_eyeCache[1]);
    g_eyePairReady = eye0Ok && eye1Ok;
    g_eyePairW=targetW; g_eyePairH=targetH;
    ++g_trueStereoFrames;

    if (s_logged < 10 || (g_trueStereoFrames % 600) == 0) {
        ++s_logged;
        Log("[TrueStereo] frame %d: records=%zu data=%zuKB overflow=%d eye0=%d eye1=%d shifts=%ld vsKnown=%zu zeroCopy=%d testClear=%d",
            (int)g_trueStereoFrames, recCount, recBytes / 1024,
            (int)d2vr_replay::g_recOverflow, (int)eye0Ok, (int)eye1Ok,
            (long)InterlockedExchange(&d2vr_replay::g_shiftCount, 0),
            d2vr_replay::g_vsInfo.Size(), (int)zeroCopy, (int)g_replayTestClear);
        const uint64_t* c = d2vr_replay::g_recCounts;
        Log("[TrueStereo] ops: Begin=%llu End=%llu Clear=%llu RT=%llu DSS=%llu Draw=%llu DIP=%llu DPUP=%llu DIPUP=%llu VS=%llu VSCF=%llu Tex=%llu RS=%llu TSS=%llu Smp=%llu VP=%llu SB=%llu",
            c[3], c[4], c[5], c[1], c[2], c[13], c[14], c[15], c[16], c[19], c[20], c[9], c[8], c[10], c[11], c[7], c[36]);
        Log("[ReplayRefs] unique=%d requested=%zu retained=%zu; frame-only COM ownership",
            (int)d2vr_replay::g_recordRefs.unique(), d2vr_replay::g_recordRefs.requests(),
            d2vr_replay::g_recordRefs.size());
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
static SRWLOCK g_cachedXrPadLock=SRWLOCK_INIT;
static void PublishXrPad(const XINPUT_STATE* value) {
    AcquireSRWLockExclusive(&g_cachedXrPadLock);
    g_cachedXrPad=value ? *value:XINPUT_STATE{};
    InterlockedExchange(&g_cachedXrPadValid,value ? 1:0);
    ReleaseSRWLockExclusive(&g_cachedXrPadLock);
}
static bool CopyXrPad(XINPUT_STATE& value) {
    AcquireSRWLockShared(&g_cachedXrPadLock);
    const bool valid=g_cachedXrPadValid!=0;
    if(valid) value=g_cachedXrPad;
    ReleaseSRWLockShared(&g_cachedXrPadLock);
    return valid;
}

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
XrAction g_actGripPose[2]{}, g_actAimPose[2]{}, g_actSqueeze[2]{}, g_actHaptic[2]{};
bool g_gripHapticOn[2]{};
static d2vr::psvr2::HapticDelivery g_hapticDelivery[2];
static ULONGLONG g_hapticRetry[2]{};
static unsigned g_hapticEpoch=0;
XrSpace g_handGripSpace[2]{}, g_handAimSpace[2]{};
struct TrackedHand {
    XrPosef grip{}, aim{};
    XrSpaceLocationFlags flags=0;
    bool valid=false,gripValid=false,aimValid=false;
    float squeeze=0;
    d2vr_input::Motion motion;
};
TrackedHand g_hands[2];
static d2vr_settings::Panel g_vrSettings;
static d2vr_settings::ToggleAction g_vrSettingsToggle;
d2vr_input::RecenterChord g_recenterChord;
d2vr_input::ContextTriangle g_triangleRecenter;
struct GestureState {
    d2vr_input::DwellZone objective,reload[2];
    d2vr_input::Slash slash;
    ULONGLONG objectiveUntil=0,reloadUntil=0,slashUntil=0;
    ULONGLONG reloadUntilByHand[2]{};
    bool grabbed=false;
};
static GestureState g_gestures;
static d2vr_input::Tentacles g_tentacles;
static void CancelGripHaptic(int hand) {
    const auto now=GetTickCount64();
    if(!g_gripHapticOn[hand] || !g_xrSession || !g_actHaptic[hand] || now>=g_hapticDelivery[hand].until) {
        g_gripHapticOn[hand]=false;g_hapticDelivery[hand].stopped();g_hapticRetry[hand]=0;return;
    }
    if(now<g_hapticRetry[hand])return;
    XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};
    info.action=g_actHaptic[hand];info.subactionPath=hand?g_pathHandRight:g_pathHandLeft;
    if(xrStopHapticFeedback(g_xrSession,&info)==XR_SUCCESS) {
        g_gripHapticOn[hand]=false;g_hapticDelivery[hand].stopped();g_hapticRetry[hand]=0;
    } else g_hapticRetry[hand]=now+10; // retry cancellation only until finite expiry
}
static void StopGripHaptics() {
    ++g_hapticEpoch;
    d2vr::psvr2::SubmitBothOff(d2vr::psvr2::TriggerOffReason::Unfocused);
    for (int hand = 0; hand < 2; ++hand)CancelGripHaptic(hand);
}

static void ApplyGripHaptic(int hand, const d2vr::psvr2::GripHaptic& haptic,
                            std::uint64_t event,bool continuous=false) {
    if (!g_xrSession || !g_actHaptic[hand]) return;
    XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};
    info.action = g_actHaptic[hand];
    info.subactionPath = hand ? g_pathHandRight : g_pathHandLeft;
    if (!d2vr::psvr2::gripHapticActive(haptic)) {
        CancelGripHaptic(hand);
        return;
    }
    const auto now=GetTickCount64();
    if(now<g_hapticRetry[hand] || !g_hapticDelivery[hand].needsApply(haptic,event,now,continuous))return;
    XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
    vibration.amplitude = haptic.amplitude;
    vibration.frequency = haptic.frequency;
    vibration.duration = static_cast<XrDuration>(haptic.durationNanoseconds);
    const auto result=xrApplyHapticFeedback(g_xrSession,&info,
        reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
    // Bounded delivery receipts distinguish missing bindings/runtime refusal
    // from native event routing. Never emit per-frame success logs.
    static unsigned receipts[2]{};
    static XrResult previousResult[2]={XR_ERROR_RUNTIME_FAILURE,XR_ERROR_RUNTIME_FAILURE};
    if(receipts[hand]<3 || (result!=previousResult[hand] && receipts[hand]<16)) {
        ++receipts[hand];previousResult[hand]=result;
        Log("[GripHaptic] hand=%s result=%d amplitude=%.2f frequency=%.1f durationNs=%llu event=%llu; submission receipt, not physical feedback proof",
            hand?"right":"left",int(result),haptic.amplitude,haptic.frequency,
            haptic.durationNanoseconds,event);
    }
    if(result==XR_SUCCESS) {
        g_gripHapticOn[hand]=true;g_hapticDelivery[hand].applied(haptic,event,now);
    } else g_hapticRetry[hand]=now+100;
}

static void ResetPoseInput() {
    StopGripHaptics();
#ifdef D2VR_MATCHED_CAPTURE
    g_protectedGrips={};
    g_protectedRightTentacle={};
#endif
    g_gestures={}; g_recenterChord={}; g_triangleRecenter={}; g_tentacles={};
    g_tentacleAimVisible=false; g_aimMarkerValid[0]=g_aimMarkerValid[1]=false;
    ResetNativeAim();
    for(auto& hand:g_hands) { hand.valid=hand.gripValid=hand.aimValid=false; hand.flags=0; hand.squeeze=0; hand.motion={}; }
}
#include "native_aim.inl"
#include "protected_left_aim.inl"
#include "native_throw.inl"
#include "protected_left_throw.inl"
#include "native_weapon.inl"
#include "native_hands.inl"
#include "protected_hands.inl"
#include "protected_weapon.inl"
#include "native_recenter.inl"
#ifdef D2VR_MATCHED_CAPTURE
#include "native_hands_witness.inl"
#endif
static void LocateTrackedHands();
static WORD GestureButtons(const XINPUT_GAMEPAD&);
static void ApplyTentacleGestures(XINPUT_GAMEPAD&);
#ifdef D2VR_MATCHED_CAPTURE
static void ApplyProtectedGestureActions(XINPUT_GAMEPAD&);
#endif
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
    std::vector<XrActionSuggestedBinding> all(binds, binds+count);
    const bool simple = strstr(profile, "simple_controller") != nullptr;
    for (int h=0; h<2; ++h) {
        const char* hand = h ? "/user/hand/right" : "/user/hand/left";
        char path[128];
        sprintf_s(path, "%s/input/grip/pose", hand);
        all.push_back({g_actGripPose[h],XrPathFrom(path)});
        sprintf_s(path, "%s/input/aim/pose", hand);
        all.push_back({g_actAimPose[h],XrPathFrom(path)});
        if (!simple) {
            sprintf_s(path, "%s/input/squeeze/value", hand);
            all.push_back({g_actSqueeze[h],XrPathFrom(path)});
        }
        if (g_actHaptic[h]) {
            sprintf_s(path, "%s/output/haptic", hand);
            all.push_back({g_actHaptic[h], XrPathFrom(path)});
        }
    }
    XrInteractionProfileSuggestedBinding ipb{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
    ipb.interactionProfile = XrPathFrom(profile);
    ipb.countSuggestedBindings = (uint32_t)all.size();
    ipb.suggestedBindings = all.data();
    XrResult r = xrSuggestInteractionProfileBindings(g_xrInstance, &ipb);
    if (XR_FAILED(r)) {
        Log("SuggestBindings '%s' failed (%d)", profile, (int)r);
        return false;
    }
    Log("SuggestBindings '%s' OK (%u bindings)", profile, (uint32_t)all.size());
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
    for (int h=0; h<2; ++h) {
        const XrPath hand=h ? g_pathHandRight : g_pathHandLeft;
        char name[64];
        sprintf_s(name,"%s_grip_pose",h ? "right" : "left");
        if (!CreatePadAction(g_padActionSet,name,XR_ACTION_TYPE_POSE_INPUT,hand,&g_actGripPose[h])) return false;
        sprintf_s(name,"%s_aim_pose",h ? "right" : "left");
        if (!CreatePadAction(g_padActionSet,name,XR_ACTION_TYPE_POSE_INPUT,hand,&g_actAimPose[h])) return false;
        sprintf_s(name,"%s_squeeze",h ? "right" : "left");
        if (!CreatePadAction(g_padActionSet,name,XR_ACTION_TYPE_FLOAT_INPUT,hand,&g_actSqueeze[h])) return false;
        sprintf_s(name,"%s_haptic",h ? "right" : "left");
        if (!CreatePadAction(g_padActionSet,name,XR_ACTION_TYPE_VIBRATION_OUTPUT,hand,&g_actHaptic[h]))
            Log("grip haptic action unavailable on %s", h ? "right" : "left");
        XrActionSpaceCreateInfo space{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        space.subactionPath=hand; space.poseInActionSpace.orientation.w=1;
        space.action=g_actGripPose[h];
        if (XR_FAILED(xrCreateActionSpace(g_xrSession,&space,&g_handGripSpace[h]))) return false;
        space.action=g_actAimPose[h];
        if (XR_FAILED(xrCreateActionSpace(g_xrSession,&space,&g_handAimSpace[h]))) return false;
    }

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
    SuggestBindings("/interaction_profiles/sony/psvr2_sense_controller", touchB,
        static_cast<uint32_t>(sizeof(touchB) / sizeof(touchB[0])));

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

static bool ReadBool(XrAction act, XrPath sub, bool* active=nullptr) {
    XrActionStateGetInfo gi{ XR_TYPE_ACTION_STATE_GET_INFO };
    gi.action = act;
    gi.subactionPath = sub;
    XrActionStateBoolean st{ XR_TYPE_ACTION_STATE_BOOLEAN };
    const XrResult result=xrGetActionStateBoolean(g_xrSession, &gi, &st);
    if(active) *active=XR_SUCCEEDED(result) && st.isActive;
    return XR_SUCCEEDED(result) && st.isActive && st.currentState;
}

static void LocateTrackedHands() {
    const double time = g_trackedFrame.state.predictedDisplayTime * 1e-9;
    const XrSpaceLocationFlags required = XR_SPACE_LOCATION_POSITION_VALID_BIT |
        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT |
        XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
    for (int h=0; h<2; ++h) {
        TrackedHand& hand=g_hands[h];
        hand.valid=hand.gripValid=hand.aimValid=false; hand.flags=0; hand.squeeze=0;
        const XrPath path=h ? g_pathHandRight : g_pathHandLeft;
        ReadFloat(g_actSqueeze[h],path,&hand.squeeze);
        XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO}; gi.subactionPath=path;
        XrActionStatePose gripState{XR_TYPE_ACTION_STATE_POSE}, aimState{XR_TYPE_ACTION_STATE_POSE};
        gi.action=g_actGripPose[h];
        const bool gripActive=XR_SUCCEEDED(xrGetActionStatePose(g_xrSession,&gi,&gripState)) && gripState.isActive;
        gi.action=g_actAimPose[h];
        const bool aimActive=XR_SUCCEEDED(xrGetActionStatePose(g_xrSession,&gi,&aimState)) && aimState.isActive;
        XrSpaceLocation grip{XR_TYPE_SPACE_LOCATION}, aim{XR_TYPE_SPACE_LOCATION};
        XrSpaceVelocity gripVelocity{XR_TYPE_SPACE_VELOCITY};
        grip.next=&gripVelocity;
        // Each action owns its tracking result. A right slash needs grip
        // motion, not an aiming ray; native ray consumers still require both.
        hand.gripValid=gripActive && g_haveViews &&
            XR_SUCCEEDED(xrLocateSpace(g_handGripSpace[h],g_xrSpace,g_trackedFrame.state.predictedDisplayTime,&grip)) &&
            (grip.locationFlags & required)==required;
        hand.aimValid=aimActive && g_haveViews &&
            XR_SUCCEEDED(xrLocateSpace(g_handAimSpace[h],g_xrSpace,g_trackedFrame.state.predictedDisplayTime,&aim)) &&
            (aim.locationFlags & required)==required;
        hand.flags=grip.locationFlags & aim.locationFlags;
        hand.valid=hand.gripValid && hand.aimValid;
        if(hand.gripValid)hand.grip=grip.pose;
        if(hand.aimValid)hand.aim=aim.pose;
        const auto& p=hand.grip.position;
        d2vr_input::Vec3 velocity{};
        const bool haveVelocity=hand.gripValid &&
            (gripVelocity.velocityFlags&XR_SPACE_VELOCITY_LINEAR_VALID_BIT)!=0;
        if(haveVelocity)velocity={gripVelocity.linearVelocity.x,gripVelocity.linearVelocity.y,gripVelocity.linearVelocity.z};
        hand.motion.Update(hand.gripValid,{p.x,p.y,p.z},time,haveVelocity?&velocity:nullptr);
    }
    if ((g_trackedFrame.id%300)==0) {
        for (int h=0; h<2; ++h) {
            XrInteractionProfileState profile{XR_TYPE_INTERACTION_PROFILE_STATE};
            char name[XR_MAX_PATH_LENGTH]="unavailable"; uint32_t length=0;
            if (XR_SUCCEEDED(xrGetCurrentInteractionProfile(g_xrSession,h ? g_pathHandRight : g_pathHandLeft,&profile)) && profile.interactionProfile)
                xrPathToString(g_xrInstance,profile.interactionProfile,sizeof(name),&length,name);
            Log("[TrackedHand] %s valid=%d grip=%.2f profile=%s aim=(%.3f,%.3f,%.3f)",h ? "R":"L",
                (int)g_hands[h].valid,g_hands[h].squeeze,name,g_hands[h].aim.position.x,
                g_hands[h].aim.position.y,g_hands[h].aim.position.z);
        }
    }
}

static void PublishAdaptiveTriggers(const XINPUT_GAMEPAD* gp, bool focused) {
    if (!g_adaptiveTriggers) {
        StopGripHaptics();
        return;
    }
    if(!focused || !gp){StopGripHaptics();return;}
    const auto started = d2vr::psvr2::ClientStart();
    static int announced = -1;
    const int announcement = started == d2vr::psvr2::ClientStartResult::Disabled ? 0 : 1;
    if (announced != announcement) {
        announced = announcement;
        Log("[PSVR2] adaptive triggers %s", announcement
            ? "client started" : "inactive (helper unavailable or externally disabled)");
    }
    const bool triggerClient = d2vr::psvr2::ClientRunning();

    using namespace d2vr::psvr2;
    FrameGates gates{};
    gates.toolkitEnabled = true;
    gates.steamVrCompatibleRuntime = true;
    bool menu = g_vrSettings.Open() || g_tentacles.paused ||
        (gp && (gp->wButtons & XINPUT_GAMEPAD_START) != 0);
#ifdef D2VR_MATCHED_CAPTURE
    menu=menu || g_protectedRightTentacle.paused || g_protectedLeftTentacle.paused;
#endif
    gates.openXrFocused = focused && g_xrSessionState == XR_SESSION_STATE_FOCUSED;
    gates.gameplayActive = g_in3DGameplayMode && !menu;
    gates.inactiveReason = menu ? TriggerOffReason::Menu : TriggerOffReason::Loading;

    unsigned char observed[2]{};
    unsigned char demonHold = 0;
    unsigned char shotOwnedMask=0;
    if (gates.openXrFocused && gates.gameplayActive) {
        NativeGameplaySample sample{};
        if (CopyNativeGameplay(sample)) {
            observed[0] = sample.firearm[0];
            observed[1] = sample.firearm[1];
            demonHold = sample.demonHold;
            shotOwnedMask=sample.shotOwnedMask;
        }
    }
    const Firearm kinds[2] = {
        resolveFirearm(static_cast<Firearm>(observed[1]), static_cast<Firearm>(observed[0])),
        resolveFirearm(static_cast<Firearm>(observed[0]), static_cast<Firearm>(observed[1]))};

    static WeaponLatch latch[2]{};
    static ULONGLONG fireSince[2]{};
    static std::uint32_t seenShot[2]{};
    static ULONGLONG shotUntil[2]{};
    static unsigned epoch=0;
    if(epoch!=g_hapticEpoch) {
        epoch=g_hapticEpoch;
        for(unsigned hand=0;hand<2;++hand) {
            shotUntil[hand]=0;seenShot[hand]=shotSerial[hand].load(std::memory_order_acquire);
        }
    }
    static ULONGLONG reloadUntil = 0;
    static ULONGLONG xSince = 0;
    static ULONGLONG eatUntil = 0;
    static ULONGLONG impaleUntil = 0;
    static ULONGLONG throwUntil = 0;
    static bool wasThrowAck = false;
    static bool wasHolding = false;
    static unsigned char loggedKind[2]{0xff, 0xff};
    static unsigned char loggedCue[2]{0xff, 0xff};
    const ULONGLONG now = GetTickCount64();
    const bool xDown = gp && (gp->wButtons & XINPUT_GAMEPAD_X) != 0;
    const bool characterHeld = demonHold == 2;
    if (!xDown) xSince = 0;
    else if (!xSince) xSince = now ? now : 1;
    const bool xHeld = xSince && now - xSince >= 280;
    const bool hipReload = g_gestures.reloadUntil && now < g_gestures.reloadUntil;
    const bool individualReload=now<g_gestures.reloadUntilByHand[0] || now<g_gestures.reloadUntilByHand[1];
    if (hipReload || (xDown && !xHeld && !characterHeld && !individualReload)) reloadUntil = now + 400;
    if (xHeld || (characterHeld && xDown)) reloadUntil = 0;
    if (characterHeld && gp && (gp->wButtons & XINPUT_GAMEPAD_A) != 0)
        impaleUntil = now + 700;
    if (xHeld || (characterHeld && xDown)) eatUntil = now + 700;
    const bool holding = (g_tentacleBridge && g_tentacles.holdingIntent) || demonHold != 0
        || (!g_tentacleBridge && g_gestures.grabbed);
    const bool throwAck = g_tentacleBridge && g_tentacles.awaitingThrowAck;
    if ((throwAck && !wasThrowAck) || (!g_tentacleBridge && wasHolding && !holding
        && d2vr_input::Length(g_hands[0].motion.velocity) > .8f)) {
        throwUntil = now + 140;
        eatUntil = impaleUntil = 0;
    }
    wasThrowAck = throwAck;
    wasHolding = holding;
    if(!gates.openXrFocused || !gates.gameplayActive) {
        xSince=eatUntil=impaleUntil=throwUntil=reloadUntil=0;
        fireSince[0]=fireSince[1]=shotUntil[0]=shotUntil[1]=0;
    }
    d2vr::psvr2::TentacleCue cues[2]{};
    if (gates.openXrFocused && gates.gameplayActive) {
        if (now < eatUntil) cues[0] = d2vr::psvr2::TentacleCue::Eat;
        else if (now < impaleUntil) cues[0] = d2vr::psvr2::TentacleCue::Impale;
        else if (now < throwUntil) cues[0] = d2vr::psvr2::TentacleCue::Throw;
        else if (holding) cues[0] = d2vr::psvr2::TentacleCue::Grip;
        if (gp && (gp->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0)
            cues[1] = d2vr::psvr2::TentacleCue::Strike;
    }
    const bool reloading = now < reloadUntil;
    const BYTE analog[2] = {
        static_cast<BYTE>(gp ? gp->bLeftTrigger : 0),
        static_cast<BYTE>(gp ? gp->bRightTrigger : 0)};
    for (int hand = 0; hand < 2; ++hand) {
        const bool connected=g_hands[hand].aimValid || g_hands[hand].gripValid;
        const bool active=gates.openXrFocused && gates.gameplayActive && connected;
        const bool nativeShots=(shotOwnedMask&(1u<<(1-hand)))!=0;
        const auto serial=shotSerial[hand].load(std::memory_order_acquire);
        const bool newShot=serial!=seenShot[hand];seenShot[hand]=serial;
        if(!active)shotUntil[hand]=0;
        else if(nativeShots && newShot)shotUntil[hand]=now+60;
        if (loggedKind[hand] != static_cast<unsigned char>(kinds[hand])) {
            loggedKind[hand] = static_cast<unsigned char>(kinds[hand]);
            Log("[PSVR2] %s firearm=%s", hand ? "right" : "left", firearmName(kinds[hand]));
        }
        if (loggedCue[hand] != static_cast<unsigned char>(cues[hand])) {
            loggedCue[hand] = static_cast<unsigned char>(cues[hand]);
            if (cues[hand] != d2vr::psvr2::TentacleCue::None)
                Log("[PSVR2] %s tentacle=%s", hand ? "right" : "left",
                    tentacleCueName(cues[hand]));
        }
        const bool pulled = active && analog[hand] >= 64;
        if(!pulled)shotUntil[hand]=0;
        if (!pulled) fireSince[hand] = 0;
        else if (!fireSince[hand]) fireSince[hand] = now ? now : 1;
        const auto held = fireSince[hand] ? now - fireSince[hand] : 0;
        const bool handReloading=reloading || now<g_gestures.reloadUntilByHand[hand];
        const bool vibratingShot=nativeShots ? now<shotUntil[hand] : pulled;
        GripHaptic haptic = tentacleHaptic(cues[hand]);
        if (active && vibratingShot && !handReloading)
            haptic = combineGripHaptic(haptic, weaponHaptic(kinds[hand], true, nativeShots?0:held));
        const auto identity=(static_cast<std::uint64_t>(nativeShots?serial:fireSince[hand])<<8)
            | static_cast<unsigned char>(cues[hand]);
        const bool continuous=!nativeShots && pulled &&
            (kinds[hand]==Firearm::Automatic || kinds[hand]==Firearm::Smg || kinds[hand]==Firearm::Rifle);
        ApplyGripHaptic(hand, active ? haptic : GripHaptic{},identity,continuous);
        if (!triggerClient) continue;
        const auto which = hand ? TriggerHand::Right : TriggerHand::Left;
        if(!active){Submit(offCommand(which,menu?TriggerOffReason::Menu:TriggerOffReason::Unfocused));continue;}
        if (gates.openXrFocused && gates.gameplayActive
            && noteFirearmChange(latch[hand], kinds[hand])) {
            consumeFirearmReset(latch[hand]);
            Submit(weaponChangedReset(which));
            continue;
        }
        HandInput input{};
        input.firearm = kinds[hand];
        input.fireDown = nativeShots ? now<shotUntil[hand] : pulled;
        input.fireHeldMilliseconds = nativeShots ? 0 : held;
        input.reloading = handReloading;
        input.tentacle = cues[hand];
        Submit(selectHand(gates, input, which));
    }
}

bool SyncOpenXRGamepad(XINPUT_STATE* out) {
    if (!out || !g_padActionsReady || !g_xrRunning) {
        if (!g_xrRunning) PublishAdaptiveTriggers(nullptr, false);
        return false;
    }
    XrActiveActionSet active{ g_padActionSet, XR_NULL_PATH };
    XrActionsSyncInfo sync{ XR_TYPE_ACTIONS_SYNC_INFO };
    sync.countActiveActionSets = 1;
    sync.activeActionSets = &active;
    const XrResult syncResult=xrSyncActions(g_xrSession, &sync);
    static XrResult lastResult=XR_ERROR_RUNTIME_FAILURE;
    static ULONGLONG lastReport=0;
    const ULONGLONG now=GetTickCount64();
    if(syncResult!=lastResult || now-lastReport>=5000) {
        lastResult=syncResult; lastReport=now;
        Log("[XRInput] sync=%d sessionState=%d frame=%llu; input requires focused session",
            (int)syncResult,(int)g_xrSessionState,(unsigned long long)g_trackedFrame.id);
        for(int h=0;h<2;++h) {
            XrInteractionProfileState profile{XR_TYPE_INTERACTION_PROFILE_STATE};
            char name[XR_MAX_PATH_LENGTH]="none"; uint32_t length=0;
            if(XR_SUCCEEDED(xrGetCurrentInteractionProfile(g_xrSession,h ? g_pathHandRight:g_pathHandLeft,&profile)) && profile.interactionProfile)
                xrPathToString(g_xrInstance,profile.interactionProfile,sizeof(name),&length,name);
            Log("[XRInput] %s profile=%s",h ? "R":"L",name);
            static XrPath hapticProfile[2]={XR_NULL_PATH,XR_NULL_PATH};
            if(g_actHaptic[h] && profile.interactionProfile!=hapticProfile[h]) {
                hapticProfile[h]=profile.interactionProfile;
                XrBoundSourcesForActionEnumerateInfo bound{XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
                bound.action=g_actHaptic[h];uint32_t sources=0;
                const auto result=xrEnumerateBoundSourcesForAction(g_xrSession,&bound,0,&sources,nullptr);
                Log("[GripHapticBinding] hand=%s profile=%s result=%d sources=%u",h?"right":"left",name,int(result),sources);
            }
        }
    }
    // XR_SESSION_NOT_FOCUSED is positive but actions must not be forwarded.
    if (syncResult != XR_SUCCESS) {
        g_vrSettings.Close();g_vrSettingsToggle.Update(false,false);
        PublishAdaptiveTriggers(nullptr, false);
        ResetPoseInput();
        return false;
    }

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
    LocateTrackedHands(); // current grip sample also owns the triangle modifier
    if (ReadBool(g_actA, g_pathHandRight)) gp.wButtons |= XINPUT_GAMEPAD_A;
    if (ReadBool(g_actB, g_pathHandRight)) gp.wButtons |= XINPUT_GAMEPAD_B;
    if (ReadBool(g_actX, g_pathHandLeft))  gp.wButtons |= XINPUT_GAMEPAD_X;
    bool triangleActive=false;
    const bool triangleDown=ReadBool(g_actY,g_pathHandLeft,&triangleActive);
    const bool menuDown=ReadBool(g_actMenu,g_pathHandLeft);
    const bool viewDown=ReadBool(g_actView,g_pathHandLeft);
    bool leftActive=false,rightActive=false;
    const bool l3=ReadBool(g_actStickL,g_pathHandLeft,&leftActive);
    const bool r3=ReadBool(g_actStickR,g_pathHandRight,&rightActive);
    // Menu input ownership is decided before triangle/recenter/gesture actions.
    // In particular resetting gestures must not re-fire a held L3+R3 chord.
    const bool focused=g_xrSessionState==XR_SESSION_STATE_FOCUSED;
    const bool settingsDown=g_vrSettingsToggleKey &&
        (GetAsyncKeyState(g_vrSettingsToggleKey)&0x8000)!=0;
    if(g_vrSettingsToggle.Update(focused,settingsDown) && g_haveViews)
        g_vrSettings.Toggle(HeadCenterPose(g_eyePose[0],g_eyePose[1]),g_xrSpace);
    d2vr_menu::Aim settingsAim;
    settingsAim.frame=g_trackedFrame.id;settingsAim.space=g_xrSpace;
    settingsAim.pose=g_hands[1].aim;settingsAim.tracked=g_hands[1].aimValid;
    settingsAim.menu=focused;settingsAim.trigger=rt;
    settingsAim.back=(gp.wButtons&XINPUT_GAMEPAD_B)!=0;
    settingsAim.time=GetTickCount64()*.001;
    g_vrSettings.Input(settingsAim,g_adaptiveFPSMode);
    const bool rawButtons=gp.wButtons || triangleDown || menuDown || viewDown || l3 || r3;
    if(g_vrSettings.OwnInput(d2vr_settings::Neutral(g_hands[1].aimValid,
        rawButtons?1u:0u,lt,rt,lx,ly,rx,ry,g_hands[0].squeeze,g_hands[1].squeeze))) {
        ResetPoseInput();out->Gamepad={};return true;
    }
    // A native-confirmed character/retained-target hold already supplies the
    // action context. Do not require a second grip press (stock LB) just to Y.
    const bool triangleModifier=NativeHeldActionContext() ||
        (g_in3DGameplayMode && g_hands[0].gripValid && g_hands[0].squeeze>.65f);
    const auto triangle=g_triangleRecenter.Update(triangleActive,triangleDown,triangleModifier,
        GetTickCount64()*.001,g_triangleHoldRecenter);
    if(triangle.action)gp.wButtons|=XINPUT_GAMEPAD_Y;
    if(triangle.tap)gp.wButtons|=XINPUT_GAMEPAD_START;
    if(triangle.recenter)RequestRecenter("VR triangle hold");
    if (menuDown) gp.wButtons |= XINPUT_GAMEPAD_START;
    if (viewDown) gp.wButtons |= XINPUT_GAMEPAD_BACK;
    if(l3) gp.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
    if(r3) gp.wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;
    static int lastStickState=-1;
    const int stickState=int(l3) | (int(r3)<<1) | (int(leftActive)<<2) | (int(rightActive)<<3);
    if(stickState!=lastStickState) {
        Log("[RecenterInput] L3 active=%d down=%d R3 active=%d down=%d",(int)leftActive,(int)l3,(int)rightActive,(int)r3);
        lastStickState=stickState;
    }

    if (g_recenterChord.Update((gp.wButtons & XINPUT_GAMEPAD_LEFT_THUMB)!=0,
        (gp.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)!=0,GetTickCount64()*.001)) {
        if(g_objectDrawPadToggle || g_submissionPadToggle) InterlockedIncrement(&g_objectDrawToggleRequests);
        else RequestRecenter("VR PAD L3+R3 click");
    }
    if (g_recenterChord.suppress) gp.wButtons &= ~(XINPUT_GAMEPAD_LEFT_THUMB | XINPUT_GAMEPAD_RIGHT_THUMB);
#ifdef D2VR_MATCHED_CAPTURE
    if(g_imagePreservingInput) gp.wButtons |= g_protectedGrips.Update(g_hands[0].squeeze,g_hands[1].squeeze,
        g_hands[0].gripValid,g_hands[1].gripValid);
    if(g_protectedGestureActions) ApplyProtectedGestureActions(gp);
    ApplyProtectedLeftThrow(gp);
    PublishProtectedLeftAim(gp);
    PublishProtectedHandPoses();
#endif
    gp.wButtons |= GestureButtons(gp);
#ifdef D2VR_MATCHED_CAPTURE
    if(!g_protectedGestureActions)
#endif
        ApplyTentacleGestures(gp);

    (void)haveL; (void)haveR; // buttons remain usable with profiles lacking sticks

    out->Gamepad = gp;
    if (InterlockedCompareExchange(&g_padFromXrLogged, 1, 0) == 0)
        Log("feeding XInput port 0 from OpenXR controllers");

    static int s_diag = 0;
    if (++s_diag % 300 == 0) {
        Log("XR Pad live: LStick=(%d, %d) RStick=(%d, %d) Trig=(%u, %u) Btns=0x%04X",
            gp.sThumbLX, gp.sThumbLY, gp.sThumbRX, gp.sThumbRY, gp.bLeftTrigger, gp.bRightTrigger, gp.wButtons);
    }
    PublishAdaptiveTriggers(&gp, true);
    return true;
}

static WORD GestureButtons(const XINPUT_GAMEPAD& gp) {
    bool gestureBridge=g_gestureBridge;
    bool protectedActions=false,protectedPaused=false;
#ifdef D2VR_MATCHED_CAPTURE
    protectedActions=g_protectedGestureActions;
    protectedPaused=protectedActions && g_protectedRightTentacle.paused;
    gestureBridge=gestureBridge || protectedActions;
#endif
    auto& objective=g_gestures.objective; auto& reload=g_gestures.reload;
    auto& slash=g_gestures.slash;
    auto& objectiveUntil=g_gestures.objectiveUntil;
    auto& reloadUntil=g_gestures.reloadUntil;
    auto& slashUntil=g_gestures.slashUntil;
    const ULONGLONG now=GetTickCount64();
    const double time=g_trackedFrame.state.predictedDisplayTime*1e-9;
    const XrPosef head=HeadCenterPose(g_eyePose[0],g_eyePose[1]);
    const XrViewStateFlags trackedHead=XR_VIEW_STATE_POSITION_TRACKED_BIT | XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
    const bool enabled=g_haveViews && (g_trackedFrame.flags & trackedHead)==trackedHead &&
        g_in3DGameplayMode && !(gp.wButtons & XINPUT_GAMEPAD_START) && !protectedPaused;
    for(int h=0;h<2;++h) {
        const auto& hand=g_hands[h];
        const auto& p=hand.grip.position;
        const XrVector3f delta=RotVec({-head.orientation.x,-head.orientation.y,-head.orientation.z,head.orientation.w},
            p.x-head.position.x,p.y-head.position.y,p.z-head.position.z);
        const bool safe=enabled && (protectedActions ? hand.gripValid:hand.valid) && hand.squeeze<.25f &&
            (h ? gp.bRightTrigger : gp.bLeftTrigger)<30;
        if(protectedActions && !safe) {
            g_gestures.reloadUntilByHand[h]=0;
            if(!h)objectiveUntil=0;
        }
        if (!h) {
            const float distance=d2vr_input::Length({delta.x+.18f,delta.y+.02f,delta.z+.08f});
            if (objective.Update(safe,distance,time,.16f,.24f,.25)) {
                Log("[Gesture] ShowObjective detected; bridge=%d",(int)gestureBridge);
                if(gestureBridge) objectiveUntil=now+120;
            }
        }
        // Bring the weapon hand to the upper leg, below the former hip zone.
        // The existing dwell/rearm and tracking/trigger/pause gates still apply.
        const float yaw=ComputeHorizontalYaw(head.orientation);
        const XrVector3f hipDelta=RotVec({0,-std::sin(yaw*.5f),0,std::cos(yaw*.5f)},
            p.x-head.position.x,p.y-head.position.y,p.z-head.position.z);
        const float distance=d2vr_input::Length({hipDelta.x-(h ? .23f:-.23f),hipDelta.y+1.05f,hipDelta.z});
        if(reload[h].Update(safe,distance,time,.20f,.30f,.20)) {
            Log("[Gesture] Reload%s detected; bridge=%d (gamepad reload is shared)",h ? "Right":"Left",(int)gestureBridge);
            if(gestureBridge) {
                if(protectedActions)g_gestures.reloadUntilByHand[h]=now+120;
                else reloadUntil=now+120;
            }
        }
    }
    if(protectedActions) {
        // Only the stock objective/reload buttons are shared with this path.
        // Left pickup/release and right strokes have separate routing.
        slash={}; slashUntil=0; g_gestures.grabbed=false;
        reloadUntil=0; // protected pulses retain the emitting hand's identity
        WORD buttons=0;
        if(now<objectiveUntil) buttons|=XINPUT_GAMEPAD_BACK;
        if(now<g_gestures.reloadUntilByHand[0] || now<g_gestures.reloadUntilByHand[1]) buttons|=XINPUT_GAMEPAD_X;
        return buttons;
    }
    if(slash.Update(enabled && g_hands[1].valid,g_hands[1].squeeze>.65f,
        d2vr_input::Length(g_hands[1].motion.velocity),time) && !g_tentacleBridge) {
        Log("[Gesture] DemonSlash detected; bridge=%d",(int)g_gestureBridge);
        if(g_gestureBridge) slashUntil=now+120;
    }
    if(!enabled || !g_hands[0].valid || !g_hands[1].valid) objectiveUntil=reloadUntil=slashUntil=0;
    auto& grabbed=g_gestures.grabbed;
    const bool grabbing=enabled && g_hands[0].valid &&
        g_hands[0].squeeze>(grabbed ? .35f:.65f);
    if(!g_tentacleBridge && grabbing && !grabbed) Log("[Gesture] DemonGrab armed (native target acquisition not implemented)");
    if(!g_tentacleBridge && grabbed && !grabbing && enabled && g_hands[0].valid) {
        const auto v=g_hands[0].motion.velocity;
        Log("[Gesture] DemonRelease velocity=(%.2f,%.2f,%.2f)m/s throwCandidate=%d (native throw not implemented)",
            v.x,v.y,v.z,(int)(d2vr_input::Length(v)>.8f));
    }
    grabbed=grabbing;
    WORD buttons=0;
    if(g_gestureBridge) {
        if(now<objectiveUntil) buttons|=XINPUT_GAMEPAD_BACK;
        if(now<reloadUntil) buttons|=XINPUT_GAMEPAD_X;
        if(!g_tentacleBridge && now<slashUntil) buttons|=XINPUT_GAMEPAD_RIGHT_SHOULDER;
        if(!g_tentacleBridge && grabbing) buttons|=XINPUT_GAMEPAD_LEFT_SHOULDER;
    }
    return buttons;
}

#ifdef D2VR_MATCHED_CAPTURE
static void ApplyProtectedGestureActions(XINPUT_GAMEPAD& gp) {
    if(!g_protectedGestureActions) return;
    // Raw right squeeze must not attack before a stroke has been recognized.
    // Left grip/LB and unrelated movement/fire/menu inputs remain unchanged.
    gp.wButtons &= ~XINPUT_GAMEPAD_RIGHT_SHOULDER;
    const XrViewStateFlags tracked=XR_VIEW_STATE_POSITION_TRACKED_BIT | XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
    const bool context=g_haveViews && g_in3DGameplayMode && (g_trackedFrame.flags & tracked)==tracked;
    const auto head=HeadCenterPose(g_eyePose[0],g_eyePose[1]);
    const auto velocity=g_hands[1].motion.velocity;
    // Stock tentacle gestures are directions in the visible head-attached
    // plane. A fixed level/recenter yaw loses side strokes after a head turn
    // and misclassifies vertical strokes when the head is tilted.
    const auto& q=head.orientation;
    const auto local=RotVec({-q.x,-q.y,-q.z,q.w},velocity.x,velocity.y,velocity.z);
    const auto out=g_protectedRightTentacle.Update(context,(gp.wButtons & XINPUT_GAMEPAD_START)!=0,
        false,g_hands[1].gripValid,0,g_hands[1].squeeze,{},{},{local.x,local.y,local.z},
        g_trackedFrame.state.predictedDisplayTime*1e-9);
    if(out.rightButton) gp.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
    if(out.ownsRightStick) {
        gp.sThumbRX=StickShort(out.slashX); gp.sThumbRY=StickShort(out.slashY);
    }
    if(out.slash) Log("[ProtectedGesture] current-head planar right stroke -> stock RB + stick; native aim/throw/pose hooks OFF");
}
#endif

static void ApplyTentacleGestures(XINPUT_GAMEPAD& gp) {
    if(!g_tentacleBridge) { g_tentacles={}; g_tentacleAimVisible=false; ResetNativeAim(); return; }
    const XrViewStateFlags tracked=XR_VIEW_STATE_POSITION_TRACKED_BIT | XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
    const bool context=g_haveViews && g_in3DGameplayMode && (g_trackedFrame.flags & tracked)==tracked;
    const auto head=HeadCenterPose(g_eyePose[0],g_eyePose[1]);
    const float yaw=ComputeHorizontalYaw(g_headGeometryReferenceValid ? g_headGeometryReference.orientation:head.orientation);
    const auto velocity=g_hands[1].motion.velocity;
    const auto local=RotVec({0,-std::sin(yaw*.5f),0,std::cos(yaw*.5f)},velocity.x,velocity.y,velocity.z);
    const auto p=g_hands[0].grip.position;
    const auto aim=RotVec(g_hands[0].aim.orientation,0,0,-1);
    const bool wasPaused=g_tentacles.paused;
    const auto out=g_tentacles.Update(context,(gp.wButtons & XINPUT_GAMEPAD_START)!=0,
        g_hands[0].valid,g_hands[1].valid,g_hands[0].squeeze,g_hands[1].squeeze,
        {p.x,p.y,p.z},g_hands[0].motion.velocity,{local.x,local.y,local.z},
        g_trackedFrame.state.predictedDisplayTime*1e-9,ReadNativeGrabFeedback(),{aim.x,aim.y,aim.z});
    if(wasPaused!=g_tentacles.paused) Log("[Tentacle] controller pause latch=%d; pending gestures cancelled",(int)g_tentacles.paused);
    const bool aimActive=g_hands[0].squeeze>.35f || g_tentacles.holdingIntent || out.leftButton;
    PublishNativeAim(context && !g_tentacles.paused,gp,out.throwObject,aimActive,out.grab);
    PublishNativeThrow(context && !g_tentacles.paused,out.grab,out.throwObject,out.throwVelocity,out.throwAim);
    NativeAimSnapshot visibleAim;
    g_tentacleAimVisible=context && !g_tentacles.paused && g_hands[0].valid &&
        (aimActive || CopyNativeAim(visibleAim));
    PublishNativeWeaponPoses(context && !g_tentacles.paused);
    if(out.leftButton) gp.wButtons|=XINPUT_GAMEPAD_LEFT_SHOULDER;
    if(out.rightButton) gp.wButtons|=XINPUT_GAMEPAD_RIGHT_SHOULDER;
    if(out.ownsRightStick) {
        gp.sThumbRX=StickShort(out.slashX); gp.sThumbRY=StickShort(out.slashY);
    }
    if(out.grab) Log("[Tentacle] Grab -> LB pulse; awaiting native feedback");
    if(out.throwObject) Log("[Tentacle] Throw -> second LB pulse; swing=(%.3f,%.3f,%.3f), awaiting native physics",out.throwVelocity.x,out.throwVelocity.y,out.throwVelocity.z);
    if(out.slash) Log("[Tentacle] Slash -> RB + stick direction=(%.2f,%.2f); stock-game attack",g_tentacles.strokeX,g_tentacles.strokeY);
    static ULONGLONG lastDiag=0;
    const auto now=GetTickCount64();
    if(context && g_hands[1].squeeze>.35f && now-lastDiag>1000) {
        lastDiag=now;
        Log("[TentacleMotion] R tracked=%d grip=%.2f planar=%.2fm/s ready=%d travel=%.3fm cooldown=%.3fs",
            (int)g_hands[1].valid,g_hands[1].squeeze,std::sqrt(local.x*local.x+local.y*local.y),
            (int)g_tentacles.rightReady,d2vr_input::Length(g_tentacles.slashTravel),
            std::max(0.,.28-(g_trackedFrame.state.predictedDisplayTime*1e-9-g_tentacles.lastSlash)));
    }
}

static bool PadHasInput(const XINPUT_GAMEPAD& g) {
    const int deadzone = 7000;
    return g.wButtons != 0 || g.bLeftTrigger > 30 || g.bRightTrigger > 30 ||
           abs(g.sThumbLX) > deadzone || abs(g.sThumbLY) > deadzone ||
           abs(g.sThumbRX) > deadzone || abs(g.sThumbRY) > deadzone;
}

void UpdateCachedXrPad() {
#ifdef D2VR_MATCHED_CAPTURE
    if(InterlockedExchange(&g_protectedRecenterRequested,0)) RequestRecenter("PHYSICAL PAD L3+R3 click (render thread)");
#endif
    if (!g_padActionsReady || !g_xrRunning) return;
    XINPUT_STATE s{};
    if (SyncOpenXRGamepad(&s)) {
        if (memcmp(&s.Gamepad, &g_lastGamepadState, sizeof(XINPUT_GAMEPAD)) != 0) {
            g_lastGamepadState = s.Gamepad;
            ++g_padPacket;
        }
        s.dwPacketNumber = g_padPacket;
        PublishXrPad(&s);
    } else {
        g_lastGamepadState={}; PublishXrPad(nullptr);
        ResetPoseInput();
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
#ifdef D2VR_MATCHED_CAPTURE
    static LONG firstPoll = 0;
    if (InterlockedCompareExchange(&firstPoll, 1, 0) == 0)
        Log("[TrackedInput] engine XInput poll port=%lu restored=%d", userIndex, g_restoredTrackedGameplay);
#endif

    XINPUT_STATE real{};
    DWORD r = ERROR_DEVICE_NOT_CONNECTED;
    auto native=g_origXInputGetState;
#ifdef D2VR_MATCHED_CAPTURE
    if(g_imagePreservingInput) native=g_protectedXInput.Native();
#endif
    if (native) {
        r = native(userIndex, &real);
        if (g_remapPhysical && userIndex == 0 && r != ERROR_SUCCESS) {
            for (DWORD i = 1; i < 4; ++i) {
                if (native(i, &real) == ERROR_SUCCESS) {
                    r = ERROR_SUCCESS;
                    if (InterlockedCompareExchange(&g_xinputRemapLogged, 1, 0) == 0)
                        Log("XInput: remapping physical gamepad from port %lu -> port 0", i);
                    break;
                }
            }
        }
    }

    static d2vr_input::RecenterChord physicalChord;
    if(userIndex==0) {
        // The external export can also be polled by Steam Input/overlay threads.
        static SRWLOCK physicalChordLock=SRWLOCK_INIT;
        bool recenter=false;
        AcquireSRWLockExclusive(&physicalChordLock);
        if(r!=ERROR_SUCCESS) physicalChord={};
        else {
            recenter=physicalChord.Update((real.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB)!=0,
                (real.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)!=0,GetTickCount64()*.001);
            if(physicalChord.suppress) real.Gamepad.wButtons &= ~(XINPUT_GAMEPAD_LEFT_THUMB | XINPUT_GAMEPAD_RIGHT_THUMB);
        }
        ReleaseSRWLockExclusive(&physicalChordLock);
        if(recenter) {
            if(g_objectDrawPadToggle || g_submissionPadToggle) InterlockedIncrement(&g_objectDrawToggleRequests);
            else {
#ifdef D2VR_MATCHED_CAPTURE
            if(g_imagePreservingInput) InterlockedExchange(&g_protectedRecenterRequested,1);
            else
#endif
                RequestRecenter("PHYSICAL PAD L3+R3 click");
            }
        }
    }

    DWORD res = ERROR_DEVICE_NOT_CONNECTED;
    XINPUT_STATE xrPad{};
    if (g_enableVRGamepad && userIndex == 0 && CopyXrPad(xrPad)) {
        if (r == ERROR_SUCCESS && PadHasInput(real.Gamepad)) {
            ResetNativeAim();
            *state = real;
            res = ERROR_SUCCESS;
        } else {
            *state = xrPad;
            res = ERROR_SUCCESS;
#ifdef D2VR_MATCHED_CAPTURE
            static LONG firstActivePacket = 0;
            if (PadHasInput(state->Gamepad) && InterlockedCompareExchange(&firstActivePacket, 1, 0) == 0)
                Log("[TrackedInput] active XR packet forwarded port=0 sticks=(%d,%d)/(%d,%d) buttons=%04X",
                    state->Gamepad.sThumbLX, state->Gamepad.sThumbLY,
                    state->Gamepad.sThumbRX, state->Gamepad.sThumbRY, state->Gamepad.wButtons);
#endif
        }
    } else if (r == ERROR_SUCCESS) {
        *state = real;
        res = ERROR_SUCCESS;
    }

    if (res == ERROR_SUCCESS) {
        if (!g_headGeometry && (g_headLookMode == 2 || g_headLookMode == 3) && g_in3DGameplayMode && userIndex == 0) {
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

            if (!NativeImageWriteAllowed(reinterpret_cast<uintptr_t>(&iat->u1.Function),sizeof(iat->u1.Function))) return false;
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

            if (!NativeImageWriteAllowed(reinterpret_cast<uintptr_t>(&iat->u1.Function),sizeof(iat->u1.Function))) return false;
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
#ifdef D2VR_MATCHED_CAPTURE
    if(g_nativeImageGuard.enabled) {
        // Ready requires a real native Present, so installation cannot happen
        // under DllMain or before graphics admission. No native bundle needed.
        if(!d2vr_protected_input::Requested(g_imagePreservingPresent,true,g_imagePreservingInput,g_enableVRGamepad) ||
            !g_imagePresent.Ready()) return;
        if(InterlockedCompareExchange(&g_xinputAttempted,1,0)!=0) return;
        const bool installed=g_protectedXInput.Start(&Hook_XInputGetState,g_nativeImageGuard);
        InterlockedExchange(&g_xinputHooked,installed ? 1:0);
        Log("[ProtectedInput] external XInputGetState installed=%d; EXE/IAT writes forbidden; native=%p",
            installed,reinterpret_cast<void*>(g_protectedXInput.Native()));
        return; // never fall back to imports, including on refusal
    }
#endif
#ifdef D2VR_MATCHED_CAPTURE
    // The default capture build stays isolated. The playable opt-in has already
    // passed SHA/PE/site checks at CreateDevice before opening this gate. Do not
    // consume the one-shot attempt during DllMain/factory calls before that gate.
    if (!g_restoredTrackedGameplay) return;
#endif
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
#ifdef D2VR_MATCHED_CAPTURE
    if(g_imagePreservingPresent && (!g_nativeImageGuard.enabled || !g_imagePresent.Ready() || !g_imagePresent.InPresentation())) return false;
#endif
    const bool refreshAdvertised=g_displayRefresh.Discover(&xrEnumerateInstanceExtensionProperties);
    const char* exts[] = { XR_KHR_D3D11_ENABLE_EXTENSION_NAME,XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME };
    XrVersion vers[] = { XR_MAKE_VERSION(1, 0, 34), XR_CURRENT_API_VERSION };
    XrResult r = XR_ERROR_RUNTIME_FAILURE;
    // An optional capability must not prevent the existing D3D11 session from
    // starting. Retry the original extension list if its admission fails.
    for(unsigned extensionAttempt=0;extensionAttempt<(refreshAdvertised?2u:1u);++extensionAttempt) {
        const bool enableRefresh=refreshAdvertised && extensionAttempt==0;
        for (XrVersion v : vers) {
            XrInstanceCreateInfo ici{ XR_TYPE_INSTANCE_CREATE_INFO };
            strcpy_s(ici.applicationInfo.applicationName, "Darkness2VR");
            ici.applicationInfo.apiVersion = v;
            ici.enabledExtensionCount = enableRefresh?2:1;
            ici.enabledExtensionNames = exts;
            r = xrCreateInstance(&ici, &g_xrInstance);
            if (XR_SUCCEEDED(r)) {g_displayRefresh.Enable(enableRefresh);break;}
        }
        if(XR_SUCCEEDED(r))break;
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
            if (viewConfigCount>=2) {
                g_xrMaxEyeW=std::min(vcv[0].maxImageRectWidth,vcv[1].maxImageRectWidth);
                g_xrMaxEyeH=std::min(vcv[0].maxImageRectHeight,vcv[1].maxImageRectHeight);
            }
            Log("SteamVR / OpenXR recommended view resolution: %ux%u per eye (max: %ux%u)",
                g_steamVrRecW, g_steamVrRecH, vcv[0].maxImageRectWidth, vcv[0].maxImageRectHeight);
        }
    }

    Log("OpenXR session ready (LOCAL + VIEW spaces initialized)");
    LogStartupStage(14, "InitXR session ready");
    g_xrReady = true;
    return true;
}

static void ReleaseVrSettingsRenderer();
#include "native_menu_input_snapshot.inl"

void PumpXR() {
    XrEventDataBuffer ev{ XR_TYPE_EVENT_DATA_BUFFER };
    while (xrPollEvent(g_xrInstance, &ev) == XR_SUCCESS) {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto* s = reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);
            g_xrSessionState=s->state;
            if(s->state==XR_SESSION_STATE_LOSS_PENDING || s->state==XR_SESSION_STATE_EXITING) {
                g_displayRefresh.Stop();
                g_steamRefresh.Stop();
            }
            Log("[XRSession] state=%d",(int)s->state);
            if(s->state!=XR_SESSION_STATE_FOCUSED) {
                g_vrSettings.Close();
                g_vrSettingsToggle.Update(false,false);
                g_adaptiveFPS.DiscardFrames();
                d2vr::psvr2::SubmitBothOff(d2vr::psvr2::TriggerOffReason::Unfocused);
                ResetPoseInput(); PublishXrPad(nullptr);
            }
            if (s->state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo bi{ XR_TYPE_SESSION_BEGIN_INFO };
                bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                if (XR_SUCCEEDED(xrBeginSession(g_xrSession, &bi))) {
                    g_xrRunning = true;
                    g_steamRefresh.Stop();
                    if(!g_displayRefresh.Start(g_xrInstance,g_xrSession,&xrGetInstanceProcAddr)) {
                        XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
                        if(xrGetInstanceProperties(g_xrInstance,&properties)==XR_SUCCESS &&
                           g_steamRefresh.Start(properties.runtimeName)) {
                            g_displayRefresh.Fallback(g_steamRefresh.Read());
                            g_steamRefreshNext=GetTickCount64()+2000;
                        }
                    }
                    Log("[DisplayRefresh] physicalHz=%.3f extensionEnabled=%d; zero=unknown; application pacing is separate; half-rate control unimplemented",
                        double(g_displayRefresh.Hz()),int(g_displayRefresh.Enabled()));
                    if(g_displayRefresh.FallbackActive())
                        Log("[DisplayRefresh] source=OpenVR HMD DisplayFrequency; read-only background client, no pacing/settings control");
                    g_anchorValid = false;
                    g_vrRefValid = false;
                    Log("XR session running (TestMode=%d SubmitMode=%d TrackingSubMode=%d)",
                        g_testMode, g_submitMode, g_trackingSubMode);
                    if (g_hideGameWindow && g_gameHwnd && g_submitMode == 4) {
                        ShowWindow(g_gameHwnd, SW_HIDE);
                        Log("[Display] game window hidden (SubmitMode 4 VR-only; disable HideGameWindow=0 to debug on monitor)");
                    }
                }
            } else if (s->state == XR_SESSION_STATE_STOPPING) {
                ReleaseVrSettingsRenderer();
                d2vr::psvr2::SubmitBothOff(d2vr::psvr2::TriggerOffReason::SessionEnd);
                if (g_gameHwnd) ShowWindow(g_gameHwnd, SW_SHOWNA);
                if(xrEndSession(g_xrSession)==XR_SUCCESS)g_adaptiveFPS.SessionEnded();
                g_xrRunning = false;
                g_displayRefresh.Stop();
                g_steamRefresh.Stop();
                g_trackedFrame.open=false;
                PublishXrPad(nullptr);
                for(auto& hand:g_hands) { hand.valid=hand.gripValid=hand.aimValid=false; hand.motion.valid=false; }
            }
        } else if(ev.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
            g_vrSettings.Close();g_vrSettingsToggle.Update(false,false);
            g_adaptiveFPS.DiscardFrames();
            g_displayRefresh.Stop();
            g_steamRefresh.Stop();
        } else if(g_displayRefresh.Event(ev)) {
            Log("[DisplayRefresh] changed physicalHz=%.3f; zero=unknown",double(g_displayRefresh.Hz()));
        } else if(ev.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
            g_vrSettings.Close();
            g_vrSettingsToggle.Update(false,false);
            g_vrRefValid=false; g_anchorValid=false; g_lockedHeadValid=false;
            g_headGeometryReferenceValid=false; ResetPoseInput();
            g_lastWorldHeadContextValid=false;
            g_recenterChord={};g_triangleRecenter={};
            for(auto& hand:g_hands) hand.motion.valid=false;
            Log("[Tracking] reference space change: reset anchors and gesture velocities");
        }
        ev = { XR_TYPE_EVENT_DATA_BUFFER };
    }
    ObserveNativeMenuReceiver();
    if(g_xrRunning && g_displayRefresh.FallbackActive() && GetTickCount64()>=g_steamRefreshNext) {
        g_steamRefreshNext=GetTickCount64()+2000;
        const float previous=g_displayRefresh.Hz();
        g_displayRefresh.Fallback(g_steamRefresh.Read());
        if(previous!=g_displayRefresh.Hz())
            Log("[DisplayRefresh] OpenVR physicalHz=%.3f; zero=unknown",double(g_displayRefresh.Hz()));
    }
}

bool EnsureSwapchains(UINT w, UINT h) {
    UINT cacheW=w,cacheH=h;
    if(g_nisMode==3 && g_submitMode==4 && !g_nisScalerFailed) {
        if(NisInit(&g_nisEye,g_dev11,kNisScalerCS,sizeof(kNisScalerCS),
                   kNisSharpenCS,sizeof(kNisSharpenCS),nullptr,NisEyeLog)) {
            cacheW=std::min(w,(UINT)g_renderW);
            cacheH=std::min(h,(UINT)g_renderH);
        } else {
            g_nisScalerFailed=true;
            Log("[NIS] scaler init failed; preserving full-size eye blit");
        }
    }
    if (g_swapchain[0] && g_swapchain[1] && w == g_scW && h == g_scH &&
        cacheW==g_eyeCacheW && cacheH==g_eyeCacheH) return true;
    static ULONGLONG retryAfter = 0;
    static bool reportedNoFormats = false;
    const ULONGLONG now = GetTickCount64();
    if (now < retryAfter) return false;
    g_eyePairReady=false;
    NisEyeReleaseSources();
    g_eyeCacheW=g_eyeCacheH=0;
    for (int e = 0; e < 2; ++e) {
        if (g_swapchain[e]) {
            if(XR_FAILED(xrDestroySwapchain(g_swapchain[e])))return false;
            g_swapchain[e] = XR_NULL_HANDLE;
        }
        g_scWrite[e]={};
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
    XrResult formatResult = xrEnumerateSwapchainFormats(g_xrSession, 0, &nfmt, nullptr);
    if (XR_FAILED(formatResult) || !nfmt) {
        if (!reportedNoFormats)
            Log("OpenXR swapchain formats unavailable (result=%d count=%u); retry in 5 s",
                (int)formatResult, nfmt);
        reportedNoFormats = true;
        retryAfter = now + 5000;
        return false;
    }
    reportedNoFormats = false;
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
        td.Width = cacheW;
        td.Height = cacheH;
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
    g_eyeCacheW=cacheW;g_eyeCacheH=cacheH;
    Log("Stereo swapchains [0,1] %ux%u eyeCache=%ux%u count=(%u,%u)", w, h,cacheW,cacheH,g_scCount[0],g_scCount[1]);
    return true;
}

bool EnsureUploadTexture(UINT w, UINT h) {
    if (g_upload && g_uploadSRV) {
        D3D11_TEXTURE2D_DESC actual{}; g_upload->GetDesc(&actual);
        if (w == actual.Width && h == actual.Height) {
            g_srcW = w; g_srcH = h; return true;
        }
    }
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
#ifdef D2VR_MATCHED_CAPTURE
        ObserveProtectedSourceSize(sd.Width,sd.Height,unsigned(sd.Format));
#endif
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
#ifdef D2VR_MATCHED_CAPTURE
    MatchedCaptureInput(rt0,!capturedRt0);
#endif
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
#ifdef D2VR_MATCHED_CAPTURE
    d2vr_cpu_sync::Scope cpuSwapchain(d2vr_cpu_sync::XrSwapchainTotal);
#endif
    const unsigned eye=sc && sc==g_swapchain[0]?0u:sc && sc==g_swapchain[1]?1u:2u;
    if(eye>=2 || !scImages || scImages!=g_scImages[eye] || !g_scCount[eye] || !srcTex || !g_ctx11)return false;
    auto& state=g_scWrite[eye];
    if(!state.acquired) {
        XrSwapchainImageAcquireInfo ai{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
        const XrResult acquired=xrAcquireSwapchainImage(sc,&ai,&state.index);
        if(XR_FAILED(acquired))return false;
        state.acquired=true;state.waited=false;state.publishable=acquired==XR_SUCCESS;
    }
    XrSwapchainImageWaitInfo wi{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    wi.timeout = XR_INFINITE_DURATION;
    if(!state.waited) {
        XrResult waitResult;
        {
#ifdef D2VR_MATCHED_CAPTURE
        d2vr_cpu_sync::Scope cpuSwapWait(d2vr_cpu_sync::XrSwapWait);
#endif
            waitResult=xrWaitSwapchainImage(sc, &wi);
        }
        // TIMEOUT_EXPIRED is positive but grants no image access. Keep this
        // acquisition and retry its wait next frame, never acquire another or
        // release an image that has not become available (OpenXR rendering spec).
        if(XR_FAILED(waitResult) || waitResult==XR_TIMEOUT_EXPIRED)return false;
        state.waited=true;state.publishable=state.publishable && waitResult==XR_SUCCESS;
    }
    const bool ok=state.publishable && state.index<g_scCount[eye] && scImages[state.index];
    if(ok)g_ctx11->CopyResource(scImages[state.index],srcTex);
    XrSwapchainImageReleaseInfo ri{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    const XrResult releaseResult = xrReleaseSwapchainImage(sc, &ri);
    // Failed release retains the waited acquisition; retry release without a
    // second wait. Successful session-loss cleanup still cannot publish a pair.
    if(XR_SUCCEEDED(releaseResult))state={};
    // A copied image is not available to the compositor until release succeeds.
    return ok && releaseResult==XR_SUCCESS;
}

#include "vr_settings_renderer.inl"

void SubmitXR(IDirect3DDevice9* dev) {
    if (g_disableOpenXR) return;
    g_lastStage = 20;

    if (!g_xrReady) return;
    if (!g_xrRunning) return;
    if (!g_trackedFrame.open) { g_eyePairReady = false; return; }
    const XrFrameState fs = g_trackedFrame.state;

    const bool wasWorldActive=g_in3DGameplayMode;
    g_in3DGameplayMode=g_worldActivity.Update(g_had3DCameraThisFrame,GetTickCount64()*.001);
    if(g_in3DGameplayMode!=wasWorldActive) {
        g_vrRefValid=false; g_lockedHeadValid=false; g_headGeometryReferenceValid=false;
        ResetPoseInput();
        Log("[WorldActivity] active=%d (render heuristic; native gameplay/menu state unverified)",(int)g_in3DGameplayMode);
    }

    const XrCompositionLayerBaseHeader* layers[3] = { nullptr,nullptr,nullptr };
    XrCompositionLayerQuad settingsQuad{XR_TYPE_COMPOSITION_LAYER_QUAD};
    XrCompositionLayerQuad settingsLaser{XR_TYPE_COMPOSITION_LAYER_QUAD};
    uint32_t layerCount = 0;
    XrCompositionLayerQuad quad{ XR_TYPE_COMPOSITION_LAYER_QUAD };
    XrCompositionLayerProjection proj{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    XrCompositionLayerProjectionView projViews[2]{};

    bool havePixels = false;
    if (fs.shouldRender && g_haveViews && g_trackedFrame.renderTrackingValid) {
        UINT targetW = g_steamVrRecW ? g_steamVrRecW : (g_srcW ? g_srcW : 1920);
        UINT targetH = g_steamVrRecH ? g_steamVrRecH : (g_srcH ? g_srcH : 1080);
        if (g_submitMode == 4) {
            // TRUE same-frame stereo: both eyes captured during Present (live pass + replay pass)
            // Submit exactly the allocation in which this pair was rendered.
            // Recomputing a size from the newly updated source here could
            // destroy the completed images on a first frame or reset.
            havePixels = g_eyePairReady && g_eyePairFrameId==g_trackedFrame.id &&
                g_swapchain[0] && g_swapchain[1] && g_eyeCache[0] && g_eyeCache[1] &&
                g_eyePairW==g_scW && g_eyePairH==g_scH;
            g_eyePairReady = false;
        } else if (g_testMode > 0) {
            havePixels = FillTestPattern(targetW, targetH, g_testMode) && EnsureSwapchains(targetW, targetH);
        } else if (dev) {
            havePixels = CopyFrame(dev) && EnsureSwapchains(targetW, targetH);
        }
    }

    if (havePixels) {
        if(g_submitMode==4)
            ResolveHeadGeometryBackgroundPose(g_had3DCameraThisFrame,g_frameEyeParams[0].headGeometry);
        const float aspect = (g_srcW && g_srcH) ? ((float)g_srcW / (float)g_srcH) : (16.f / 9.f);
        const XrPosef head = HeadCenterPose(g_eyePose[0], g_eyePose[1]);
        const XrPosef layerHead = g_submitMode==4 ? g_trackedFrame.layerPose : LockedViewPose(head);

        // ---------------- Head-Look Camera Rotation (Turning Head Rotates In-Game Camera) ----------------
        // NOTE: active whenever HeadLookMode > 0 (the old 3D-mode gate silently disabled head
        // tracking outside the detector path - the user reported "no head tracking").
        if (!g_headGeometry && g_headLookMode > 0) {
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
            const int leftIdx = (g_stereoDebugMode == 4) ? 1 : 0;
            const int rightIdx = (g_stereoDebugMode == 4) ? 0 : 1;
            ID3D11Texture2D* submitted[2]={g_eyeCache[leftIdx],g_eyeCache[rightIdx]};
            ID3D11Texture2D* sharpened[2]{};
            const bool scaleRequired=g_nisMode==3 && !g_nisScalerFailed &&
                (g_eyeCacheW!=g_scW || g_eyeCacheH!=g_scH);
            if((g_nisMode==2 || scaleRequired) && NisEyeProcess(submitted[0],submitted[1],sharpened)) {
                submitted[0]=sharpened[0]; submitted[1]=sharpened[1];
            } else if(scaleRequired) {
                // A small cache cannot be copied into the full-size XR images.
                // Drop this one world layer and rebuild full-size eye caches on
                // the next frame. Never submit stale or mismatched eye images.
                g_nisScalerFailed=true;
                submitted[0]=submitted[1]=nullptr;
                Log("[NIS] scale failed; one layer dropped, full-size fallback next frame");
            }
#ifdef D2VR_MATCHED_CAPTURE
            if(submitted[0] && submitted[1]) {
                matched_capture::Final(0,submitted[0]);
                matched_capture::Final(1,submitted[1]);
            }
#endif
            if (submitted[0] && submitted[1] &&
                WriteTextureToSwapchain(g_swapchain[0], g_scImages[0], submitted[0]) &&
                WriteTextureToSwapchain(g_swapchain[1], g_scImages[1], submitted[1])) {
                // Clip-shift renders encode IPD in pixels; default layer pose is head center
                // (OpenVR-style). Per-eye poses are optional (StereoLayerPose=1).
                const XrPosef headSubmit = g_pairSubmitMetaValid ? g_pairSubmitHead
                    : HeadCenterPose(g_eyePose[0], g_eyePose[1]);
                static int s_poseLog = 0;
                if (s_poseLog++ < 8) {
                    Log("[StereoSubmit] layerPose=%s headClip=%d ipd=%.4f debug=%d presentBoundary=%ld",
                        g_stereoLayerPose ? "PerEye" : "HeadCenter",
                        (int)g_frameEyeParams[0].headGeometry, g_stereoIPD, g_stereoDebugMode,
                        (long)InterlockedCompareExchange(&g_stereoPresentBoundarySeen, 0, 0));
                }
                for (int e = 0; e < 2; ++e) {
                    projViews[e] = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
                    projViews[e].pose = g_stereoLayerPose ? (g_pairSubmitMetaValid ? g_pairSubmitEyePose[e]
                                                                                  : g_eyePose[e])
                                                          : headSubmit;
                    projViews[e].fov = g_pairSubmitMetaValid ? g_pairSubmitFov[e] : g_eyeFov[e];
                    projViews[e].fov.angleLeft += g_trackedFrame.offsetX;
                    projViews[e].fov.angleRight += g_trackedFrame.offsetX;
                    projViews[e].subImage.swapchain = g_swapchain[e];
                    projViews[e].subImage.imageRect.offset = { 0, 0 };
                    projViews[e].subImage.imageRect.extent = { (int32_t)g_scW, (int32_t)g_scH };
                }
#ifdef D2VR_MATCHED_CAPTURE
                matched_capture::Submission(projViews);
#endif
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
            if (g_trackingSubMode != 2 && !g_lockHeadPosition) {
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
                XrPosef viewPose = layerHead;
                if (g_trackingSubMode == 0 && g_headLookMode == 0) {
                    if (!g_vrWorldAnchorValid) {
                        g_vrWorldAnchor = layerHead;
                        const float initYaw = ComputeHorizontalYaw(head.orientation);
                        g_vrWorldAnchor.orientation.x = 0.0f;
                        g_vrWorldAnchor.orientation.y = std::sin(initYaw * 0.5f);
                        g_vrWorldAnchor.orientation.z = 0.0f;
                        g_vrWorldAnchor.orientation.w = std::cos(initYaw * 0.5f);
                        g_vrWorldAnchorValid = true;
                        Log("VR World Anchor latched at yaw=%.1f deg (frame=%d)", initYaw * 57.29578f, g_frames);
                    }

                    // Always anchor position to current head position for natural 6DoF parallax
                    g_vrWorldAnchor.position = layerHead.position;

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

    const uint32_t worldLayerCount=layerCount;
    bool settingsSubmitted=false;
    if(g_vrSettings.Open()) {
        const auto settingsResult=g_xrSessionState!=XR_SESSION_STATE_FOCUSED ? VrSettingsRender::Failed :
            !layerCount || !fs.shouldRender ? VrSettingsRender::Pending : RenderVrSettings(settingsQuad);
        if(settingsResult==VrSettingsRender::Ready) {
            if(g_haveViews && g_vrSettingsRenderer.Laser(settingsQuad,g_trackedFrame.id,
                HeadCenterPose(g_eyePose[0],g_eyePose[1]),settingsLaser))
                layers[layerCount++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&settingsLaser);
            layers[layerCount++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&settingsQuad);
            settingsSubmitted=true;
        } else if(settingsResult==VrSettingsRender::Pending)g_vrSettings.SuspendSurface();
        else g_vrSettings.Submitted(false);
    }
    XrFrameEndInfo fei{ XR_TYPE_FRAME_END_INFO };
    fei.displayTime = fs.predictedDisplayTime;
    fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    fei.layerCount = layerCount;
    fei.layers = layerCount ? layers : nullptr;
    XrResult endR;
    {
#ifdef D2VR_MATCHED_CAPTURE
        d2vr_cpu_sync::Scope cpuEnd(d2vr_cpu_sync::XrEnd);
#endif
        endR=xrEndFrame(g_xrSession, &fei);
    }
    if(settingsSubmitted)g_vrSettings.Submitted(endR==XR_SUCCESS);
    AdaptiveStereoSubmitted(endR,havePixels,worldLayerCount);
#ifdef D2VR_MATCHED_CAPTURE
    matched_capture::Finish(XR_SUCCEEDED(endR) && g_had3DCameraThisFrame && layerCount==1 && g_pairSubmitMetaValid && g_eyePairFrameId==g_trackedFrame.id);
#endif
    g_trackedFrame.open = false;
    const ULONGLONG frameMs = GetTickCount64() - g_trackedFrame.startedMs;
    if ((g_trackedFrame.id % 300) == 0)
        Log("[FrameContext] id=%llu time=%lld period=%.3fms CPU-window=%llums layers=%u tracking=0x%llX pair=%ld",
            g_trackedFrame.id, fs.predictedDisplayTime, fs.predictedDisplayPeriod / 1000000.0,
            frameMs, layerCount, (unsigned long long)g_trackedFrame.flags, g_trueStereoFrames);
    // Submission cadence is not unique simulation FPS: the game can emit
    // several EndScene segments per world frame. Report that distinction.
    static ULONGLONG cadenceStart = GetTickCount64();
    static unsigned cadenceSubmits = 0;
    static unsigned cadenceWorld=0,cadencePresents=0;
    static ULONGLONG lastBoundary=0;
    static std::vector<double> intervals;
    if (XR_SUCCEEDED(endR) && layerCount) {
        ++cadenceSubmits;
        InterlockedExchange(&g_xrSubmittedThisFrame, 1);
    }
    const ULONGLONG cadenceNow = GetTickCount64();
    ++cadencePresents; if(g_had3DCameraThisFrame) ++cadenceWorld;
    if(lastBoundary && intervals.size()<2048) intervals.push_back(double(cadenceNow-lastBoundary));
    lastBoundary=cadenceNow;
    if (cadenceNow - cadenceStart >= 5000) {
        std::sort(intervals.begin(),intervals.end());
        const double scale=1000.0/(cadenceNow-cadenceStart);
        const double p95=intervals.empty() ? 0:intervals[(intervals.size()-1)*95/100];
        Log("[XRCadence] layers=%.1f/s game-boundaries=%.1f/s world-bearing=%.1f/s boundary-p95=%.2fms runtimePeriod=%.3fms physicalHz=%.3f (zero=unknown; not simulation FPS)",
            cadenceSubmits*scale,cadencePresents*scale,cadenceWorld*scale,p95,fs.predictedDisplayPeriod/1000000.0,double(g_displayRefresh.Hz()));
        cadenceStart = cadenceNow; cadenceSubmits=cadenceWorld=cadencePresents=0; intervals.clear();
    }
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
    g_hadWorldDrawThisFrame = false;
    g_3dCamCallsThisFrame = 0;
    g_depthCapturedThisFrame = false;
    g_lastStage = 21;
}

static void BeginTrackedFrame() {
    if (!g_xrReady || g_trackedFrame.open) return;
    PumpXR();
    if (!g_xrRunning) { g_haveViews = false; return; }
    XrFrameState fs{ XR_TYPE_FRAME_STATE };
    XrResult r;
    {
#ifdef D2VR_MATCHED_CAPTURE
        d2vr_cpu_sync::Scope cpuFrameWait(d2vr_cpu_sync::XrFrameWait);
#endif
        r=xrWaitFrame(g_xrSession, nullptr, &fs);
    }
    if (XR_FAILED(r)) { g_haveViews = false; return; }
    r = xrBeginFrame(g_xrSession, nullptr);
    if (XR_FAILED(r)) { g_haveViews = false; Log("[FrameContext] begin failed %d", (int)r); return; }
    g_trackedFrame.state = fs;
    g_trackedFrame.open = true;
    ++g_trackedFrame.id;
#ifdef D2VR_MATCHED_CAPTURE
    g_nativeCullingAppliedFrame=0;
    g_nativeCullingReason=NativeCullingReason::NoBoundary;
    d2vr_cpu_sync::timing.MarkFrame(g_trackedFrame.id);
#endif
    g_trackedFrame.startedMs = GetTickCount64();
    if(g_recenterHeadLock) { g_lockedHeadValid=false; g_recenterHeadLock=false; }
    XrView views[2] = {{ XR_TYPE_VIEW }, { XR_TYPE_VIEW }};
    XrViewLocateInfo li{ XR_TYPE_VIEW_LOCATE_INFO };
    li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    li.displayTime = fs.predictedDisplayTime;
    li.space = g_xrSpace;
    XrViewState vs{ XR_TYPE_VIEW_STATE };
    uint32_t count = 0;
    const XrResult located = xrLocateViews(g_xrSession, &li, &vs, 2, &count, views);
    g_trackedFrame.flags = vs.viewStateFlags;
    const XrViewStateFlags required = XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT;
    g_haveViews = XR_SUCCEEDED(located) && count == 2 && (vs.viewStateFlags & required) == required;
    g_headDeltaValid=false;
    g_trackedFrame.renderTrackingValid=true;
    if (g_haveViews) {
        for (int e = 0; e < 2; ++e) { g_eyePose[e] = views[e].pose; g_eyeFov[e] = views[e].fov; }
        SyncStereoIpdFromOpenXr();
        if (g_submitMode == 4) UpdateSubmissionFovFromMatrices();
        const auto head=HeadCenterPose(g_eyePose[0],g_eyePose[1]);
        UpdateHeadGeometry(head);
        g_trackedFrame.layerPose=g_headDeltaValid ? head:LockedViewPose(head);
        // On positional tracking loss, let the compositor reproject the last
        // valid image instead of presenting a sudden neutral diagnostic camera.
        const bool headClipActive = g_nativeStereoHeadGeometry ||
                                    (g_submitMode != 4 && g_headGeometry);
        if (headClipActive && g_in3DGameplayMode && !g_headDeltaValid)
            g_trackedFrame.renderTrackingValid=false;
    }
    g_trackedFrame.halfFovX=g_gameHalfFovX; g_trackedFrame.halfFovY=g_gameHalfFovY;
    g_trackedFrame.offsetX=g_viewOffsetX;
    g_trackedFrame.cropX=g_fovWidenX>0 ? std::min(1.f,g_fovCoverX/g_fovWidenX):1;
    g_trackedFrame.cropY=g_fovWidenY>0 ? std::min(1.f,g_fovCoverY/g_fovWidenY):1;
    for (int e = 0; e < 2; ++e) {
        g_perEyeCropX[e] = g_fovWidenEyeX[e] > 0.0f
            ? std::min(1.0f, g_fovCoverEyeX[e] / g_fovWidenEyeX[e]) : 1.0f;
        g_perEyeCropY[e] = g_fovWidenEyeY[e] > 0.0f
            ? std::min(1.0f, g_fovCoverEyeY[e] / g_fovWidenEyeY[e]) : 1.0f;
#ifdef D2VR_MATCHED_CAPTURE
        if(g_nativeCameraCoverInstalled && g_nativeCameraCoverConfirmed)
            g_perEyeCropX[e]=g_perEyeCropY[e]=1/(1+g_edgeCrop);
#endif
    }
    g_frameEyeParamsValid=false;
    for(int e=0;e<2;++e) g_frameEyeParams[e]=BuildEyeParams(e);
    g_frameEyeParamsValid=true;
    if(g_headGeometry && g_trackedFrame.id%300==0)
        Log("[HeadGeometry] applied=%d tracked=0x%llX world=%d projection=%d; GPU camera diagnostic",
            (int)g_frameEyeParams[0].headGeometry,(unsigned long long)g_trackedFrame.flags,
            (int)g_in3DGameplayMode,(int)d2vr_replay::g_cameraProjectionKnown);
#ifdef D2VR_MATCHED_CAPTURE
    matched_capture::iniPath=g_matchedIniPath;
    matched_capture::Begin();
#endif
    UpdateCachedXrPad();
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
static thread_local bool g_insidePresentation=false;
struct PresentationScope {
    bool previous = g_insidePresentation;
    PresentationScope() { g_insidePresentation=true; }
    ~PresentationScope() { g_insidePresentation=previous; }
};

static void TweakPresentParams(D3DPRESENT_PARAMETERS* pp);

void* PatchVTable(void* obj, int index, void* repl) {
    void** vt = *reinterpret_cast<void***>(obj);
    const bool allowed=NativeImageWriteAllowed(reinterpret_cast<uintptr_t>(&vt[index]),sizeof(void*));
    DWORD old = 0;
    if (!allowed || !VirtualProtect(&vt[index], sizeof(void*), PAGE_READWRITE, &old)) {
#ifdef D2VR_MATCHED_CAPTURE
        if(g_imagePreservingPresent) g_imagePresent.Fail("required graphics interface hook refused");
#endif
        return nullptr;
    }
    void* orig = vt[index];
    vt[index] = repl;
    const bool restored=VirtualProtect(&vt[index], sizeof(void*), old, &old)!=FALSE;
#ifdef D2VR_MATCHED_CAPTURE
    if(!restored && g_imagePreservingPresent) g_imagePresent.Fail("graphics interface protection restore failed");
#endif
    return orig;
}

static void VrFrame(IDirect3DDevice9* dev, const char* caller);
static void TryHookSwapChainPresent(IDirect3DDevice9* dev);

static d2vr_frame::Event MakePipelineEvent(d2vr_frame::EventType type) {
    d2vr_frame::Event e{};
    e.type = type;
    e.timestampMs = GetTickCount64();
    e.threadId = GetCurrentThreadId();
    e.recordCount = (uint32_t)d2vr_replay::g_rec.size();
    return e;
}

static bool ShouldEmitSceneTrace(d2vr_frame::EventType type) {
    if (!d2vr_trace::g_enabled || !d2vr_trace::g_traceScenes) return false;
    if (d2vr_trace::g_traceAllScenes) return true;
    const int depth = g_frameBoundary.OpenBeginScenes();
    if (type == d2vr_frame::EventType::BeginScene) return depth == 1;
    if (type == d2vr_frame::EventType::EndScene) return depth == 0;
    return true;
}

static void EmitPipelineTrace(const d2vr_frame::Event& e, void* dev, const char* note = "") {
    if (!d2vr_trace::g_enabled) return;
    if (e.type == d2vr_frame::EventType::BeginScene || e.type == d2vr_frame::EventType::EndScene) {
        if (!ShouldEmitSceneTrace(e.type)) return;
    }
    const int depth = (e.type == d2vr_frame::EventType::BeginScene ||
                       e.type == d2vr_frame::EventType::EndScene)
                          ? g_frameBoundary.OpenBeginScenes()
                          : 0;
    d2vr_trace::EmitSimple(e.timestampMs, e.threadId, d2vr_frame::EventTypeName(e.type), depth,
                           e.recordCount, dev, note);
}

static void TracePresentLike(IDirect3DDevice9* dev, const char* ev, void* swapchain, void* backbuffer,
                             uint32_t flags, const char* note) {
    if (!d2vr_trace::g_enabled) return;
    d2vr_trace::Emit(GetTickCount64(), GetCurrentThreadId(), ev, 0,
                     (uint32_t)d2vr_replay::g_rec.size(), dev, swapchain, backbuffer, flags,
                     _ReturnAddress(), flags, note);
}

HRESULT STDMETHODCALLTYPE Hook_Reset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp) {
    d2vr_replay::ReleaseLightReadbackTargets();
    d2vr_shader_preload::BeginReset(dev);
    QueryPreloadBeginReset(dev);
    d2vr_sampling::InvalidateSamplerCaps();
#ifdef D2VR_MATCHED_CAPTURE
    if(g_imagePreservingPresent) g_imagePresent.ResetBegin();
    if(pp && pp->hDeviceWindow) g_protectedSourceSize.Window(pp->hDeviceWindow);
    g_capturePresent.Reset();
    InterlockedIncrement(&g_captureResetGeneration); InterlockedExchange(&g_captureResetting,1);
#endif
    {
        const auto ev = MakePipelineEvent(d2vr_frame::EventType::Reset);
        g_frameBoundary.OnEvent(ev);
        EmitPipelineTrace(ev, dev);
    }
    g_gpuTiming.Release();
    NisEyeShutdown();
    d2vr_replay::InvalidateShadowAtlasPass();
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
    d2vr_replay::ResetBufferReplicas();
    d2vr_replay::ResetProjectionHistories();
    if(g_liveEndSnapshot)g_liveEndSnapshot->Release();
    g_rightColorHistory.Release();
    if(!ReleaseSharedCapture()) return D3DERR_DEVICELOST;
    g_zeroCopyFailures = 0;
    if (g_liveColorBackup) { g_liveColorBackup->Release(); g_liveColorBackup = nullptr; }
    ReleaseDepthResources9();
    g_anchorValid = false;
    g_vrWorldAnchorValid = false;
    g_vrRefValid = false;
    HRESULT hr = g_origReset(dev, pp);
    d2vr_shader_preload::EndReset(dev,hr);
    QueryPreloadEndReset(dev,hr);
#ifdef D2VR_MATCHED_CAPTURE
    if(g_imagePreservingPresent) g_imagePresent.ResetEnd(SUCCEEDED(hr),dev);
    g_capturePresent.ResetResult(SUCCEEDED(hr));
    InterlockedExchange(&g_captureResetting,FAILED(hr));
#endif
    Log("Device Reset (%ux%u fmt=%d windowed=%d refresh=%u) -> hr=0x%08lX",
        pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0,
        pp ? (int)pp->BackBufferFormat : 0, pp ? (int)pp->Windowed : 0,
        pp ? pp->FullScreen_RefreshRateInHz : 0, hr);
    // Install from the first completed EndScene, after overlay device setup.
    // Reset can run while another interceptor is still collecting entry points.
    if (SUCCEEDED(hr)) g_frameBoundary.OnEvent(MakePipelineEvent(d2vr_frame::EventType::ResetSucceeded));
    return hr;
}

HRESULT STDMETHODCALLTYPE Hook_ResetEx(IDirect3DDevice9Ex* dev, D3DPRESENT_PARAMETERS* pp, D3DDISPLAYMODEEX* dm) {
    d2vr_replay::ReleaseLightReadbackTargets();
    d2vr_shader_preload::BeginReset(dev);
    QueryPreloadBeginReset(dev);
    d2vr_sampling::InvalidateSamplerCaps();
#ifdef D2VR_MATCHED_CAPTURE
    if(g_imagePreservingPresent) g_imagePresent.ResetBegin();
    if(pp && pp->hDeviceWindow) g_protectedSourceSize.Window(pp->hDeviceWindow);
    g_capturePresent.Reset();
    InterlockedIncrement(&g_captureResetGeneration); InterlockedExchange(&g_captureResetting,1);
#endif
    g_frameBoundary.OnEvent(MakePipelineEvent(d2vr_frame::EventType::ResetEx));
    g_gpuTiming.Release();
    NisEyeShutdown();
    d2vr_replay::InvalidateShadowAtlasPass();
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
    // ResetEx preserves default-pool buffers; their replay replicas survive too.
    d2vr_replay::ReleaseRecording();
    d2vr_replay::ResetProjectionHistories();
    if(g_liveEndSnapshot)g_liveEndSnapshot->Release();
    g_rightColorHistory.Release();
    if(!ReleaseSharedCapture()) return D3DERR_DEVICELOST;
    g_zeroCopyFailures = 0;
    if (g_liveColorBackup) { g_liveColorBackup->Release(); g_liveColorBackup = nullptr; }
    ReleaseDepthResources9();
    g_anchorValid = false;
    g_vrWorldAnchorValid = false;
    g_vrRefValid = false;
    HRESULT hr = g_origResetEx ? g_origResetEx(dev, pp, dm) : D3D_OK;
    d2vr_shader_preload::EndReset(dev,g_origResetEx?hr:E_FAIL);
    QueryPreloadEndReset(dev,g_origResetEx?hr:E_FAIL);
#ifdef D2VR_MATCHED_CAPTURE
    g_capturePresent.ResetResult(g_origResetEx && SUCCEEDED(hr));
    if(g_imagePreservingPresent) g_imagePresent.ResetEnd(g_origResetEx && SUCCEEDED(hr),dev);
    InterlockedExchange(&g_captureResetting,!g_origResetEx || FAILED(hr));
#endif
    if (SUCCEEDED(hr)) g_frameBoundary.OnEvent(MakePipelineEvent(d2vr_frame::EventType::ResetSucceeded));
    Log("Device ResetEx (%ux%u fmt=%d windowed=%d refresh=%u) -> hr=0x%08lX",
        pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0,
        pp ? (int)pp->BackBufferFormat : 0, pp ? (int)pp->Windowed : 0,
        pp ? pp->FullScreen_RefreshRateInHz : 0, hr);
    return hr;
}

static void VrFrame(IDirect3DDevice9* dev, const char* caller) {
    if((g_objectDrawPadToggle || g_submissionPadToggle) && (InterlockedExchange(&g_objectDrawToggleRequests,0)&1)) {
        if(g_submissionPadToggle) {
            const bool enabled=!(g_replayConstantBatch && g_replayConstantWindow);
            g_replayConstantBatch=enabled;g_replayConstantWindow=enabled;
            if(g_submissionPackage) {
                g_wideRecordCopy=enabled;g_uniqueReplayRefs=enabled;g_replayConstantBindWindow=enabled;
                d2vr_native_constants::enabled=enabled && g_nativeOwnedConstantCopyRequested;
                d2vr_native_constants::materialEnabled=enabled && g_nativeOwnedMaterialCopyRequested;
            }
            Log("[SubmissionAB] L3+R3 frame-boundary toggle: grouped=%d; all geometry retained",int(enabled));
            if(g_submissionPackage) Log("[SubmissionPackage] enabled=%d wideCopy=%d uniqueRefs=%d (ref ledger changes next recording)",int(enabled),int(g_wideRecordCopy.load()),int(g_uniqueReplayRefs));
        } else {
            const bool hidden=!g_skipObjectDraws.load();
            g_skipObjectDraws.store(hidden);
            Log("[ObjectDrawTest] L3+R3 frame-boundary toggle: geometry=%s",hidden?"HIDDEN":"VISIBLE");
        }
        MessageBeep(MB_OK);
    }
    d2vr_shader_preload::Tick();
    QueryPreloadTick();
    g_renderThreadId = GetCurrentThreadId();
    PollHotkeys();
    TryInstallXInputHook();

    LONG id = g_frameId;
    if (id == 0) {
        id = InterlockedIncrement(&g_backupFrameCounter);
    }

    if (InterlockedCompareExchange(&g_submittedFrameId, id, id) == id) return;
    InterlockedExchange(&g_submittedFrameId, id);
    InterlockedExchange(&g_xrSubmittedThisFrame, 0);

    static int frameLogs = 0;
    if (frameLogs++ < 15) {
        Log("VrFrame called by %s (frameId=%ld tid=%lu)", caller, id, g_renderThreadId);
    }

    if (!g_disableOpenXR && InterlockedCompareExchange(&g_xrInitTried, 1, 0) == 0) InitXR();

    // True stereo: build the eye pair from the recorded frame stream BEFORE submitting.
    // Runs once per frame thanks to the g_submittedFrameId dedupe above.
    if (g_submitMode == 4 && g_stereoMode >= 1 && d2vr_replay::g_recActive) {
        g_eyePairReady=false;
        const size_t recCount = d2vr_replay::g_rec.size();
        // A real Present seals the recording. Replay actual draw content, not a
        // guessed command-count threshold (a small UI frame is still a frame).
        const bool hasDraw = std::any_of(d2vr_replay::g_rec.begin(), d2vr_replay::g_rec.end(),
            [](const d2vr_replay::Rec& r) {
                return r.op == d2vr_replay::OP_DrawPrimitive || r.op == d2vr_replay::OP_DrawIndexedPrimitive ||
                    r.op == d2vr_replay::OP_DrawPrimitiveUP || r.op == d2vr_replay::OP_DrawIndexedPrimitiveUP;
            });
        if (hasDraw && !d2vr_replay::g_recOverflow) {
            RenderEyePairFromReplay(dev);
            g_eyePairFrameId=g_trackedFrame.id;
        } else {
            static int s_skipLog = 0;
            if (s_skipLog++ < 6)
                Log("[StereoPair] no replayable draw stream (rec=%zu overflow=%d)", recCount,
                    (int)d2vr_replay::g_recOverflow);
        }
    }

    // Native engine FOV override: the avatar's GetFOV is patched so the projection AND
    // the CPU culling frustum widen together (a GPU-only widen leaves holes).
    {
        static LONG s_fovTick = 0;
        if ((g_autoFovWiden || g_fovOverrideDeg > 0.0f) &&
            (InterlockedIncrement(&s_fovTick) % 20) == 1) {
            TryApplyEngineFovOverride();
        }
    }
    g_gpuTiming.End();
    SubmitXR(dev);
    // Apply only after the current live/replay pair is complete: never compare
    // an old-preset live eye with a new-preset replay eye.
    if (g_pendingLightingPreset >= 0) {
        g_lightingPreset = g_pendingLightingPreset;
        g_pendingLightingPreset = -1;
        g_projectionFix = 3;
        g_noInvProj = g_lightPosShift = g_skipLightVsShift = 0;
        g_diagnosticSameEye = -1;
        g_diagnosticSkipVs = 0;
        g_stereoMode = 1;
        const int key = g_lightingPreset == 9 ? '0' : '1' + g_lightingPreset;
        Log("[LightingPreset] Shift+%c: %s (next pair; F12 captures evidence)",
            key, kLightingPresetNames[g_lightingPreset]);
        SYSTEMTIME selectedAt{}; GetLocalTime(&selectedAt);
        char savedChoice[512];
        sprintf_s(savedChoice,
            "[Lighting]\r\nLastKey=%c\r\nName=%s\r\nSelectedAt=%04u-%02u-%02u %02u:%02u:%02u\r\n",
            key, kLightingPresetNames[g_lightingPreset], selectedAt.wYear, selectedAt.wMonth,
            selectedAt.wDay, selectedAt.wHour, selectedAt.wMinute, selectedAt.wSecond);
        // Replace atomically and flush now; do not depend on clean game shutdown.
        char temporary[MAX_PATH]; sprintf_s(temporary, "%s.tmp", g_lightingChoicePath);
        HANDLE choiceFile = CreateFileA(temporary, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        bool saved = false;
        if (choiceFile != INVALID_HANDLE_VALUE) {
            DWORD written = 0; const DWORD length = (DWORD)strlen(savedChoice);
            saved = WriteFile(choiceFile, savedChoice, length, &written, nullptr) && written == length;
            saved = FlushFileBuffers(choiceFile) && saved;
            CloseHandle(choiceFile);
            if (saved) saved = MoveFileExA(temporary, g_lightingChoicePath,
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        }
        Log("[LightingPreset] last choice %s: %s", saved ? "saved" : "SAVE FAILED", g_lightingChoicePath);
    }
    d2vr_replay::CloseBufferFrame();
    BeginTrackedFrame(); // latch before either eye starts drawing the next game frame
    if (g_submitMode == 4 && g_stereoMode >= 1) {
        d2vr_replay::LightEndOfWindowSnapshot(dev);   // diagnostic only (LightReadback)
        InterlockedExchange(&g_needRecBegin, 1);
        g_preSceneWriteMissed=false;
    } else {
        d2vr_replay::ReleaseRecording();
    }

}

static void ApplyBoundaryDecision(IDirect3DDevice9* dev, const d2vr_frame::Decision& d) {
#ifdef D2VR_MATCHED_CAPTURE
    // The protected candidate has NO EndScene or implicit-device fallback.
    // Only a witnessed native additional-swapchain implementation call opens XR.
    if(g_imagePreservingPresent && (!g_nativeImageGuard.enabled || !g_imagePresent.Ready() ||
        !g_imagePresent.InPresentation() || g_submitMode!=4 || g_stereoMode!=1 || g_disableOpenXR)) return;
#endif
    if (!d.requestVrFrame || !d.reason) return;
    if (d.incrementGameFrameId && g_submitMode == 4) InterlockedIncrement(&g_frameId);
    if (strstr(d.reason, "Present") || strstr(d.reason, "SwapChain"))
        InterlockedExchange(&g_stereoPresentBoundarySeen, 1);
#ifdef D2VR_MATCHED_CAPTURE
    const bool witnessed = g_presentReadiness && g_insidePresentation && !g_captureResetting &&
        g_capturePresent.Enter(d.reason,reinterpret_cast<uintptr_t>(dev),g_presentCaller,
            GetCurrentThreadId(),g_frameId);
    if (witnessed && (g_capturePresent.serial==1 || g_capturePresent.serial%1000==0))
        Log("[PresentReadiness] entry kind=%s serial=%llu epoch=%llu frame=%llu device=%p caller=%p tid=%lu engineOwnership=UNKNOWN",
            d.reason,g_capturePresent.serial,g_capturePresent.epoch,g_capturePresent.frame,dev,
            reinterpret_cast<void*>(g_presentCaller),GetCurrentThreadId());
#endif
    VrFrame(dev, d.reason);
    // Leave at the hook's return, after the original Present HRESULT is known.
}

static void RecordPreSceneWrite(IDirect3DDevice9* dev) {
    // Clear, ColorFill and StretchRect can precede BeginScene. Open one
    // packet before their native writes; BeginScene must retain that packet.
    if(g_insidePresentation || g_submitMode!=4 || g_stereoMode<1 ||
       GetCurrentThreadId()!=g_renderThreadId ||
       !InterlockedCompareExchange(&g_needRecBegin,0,0))return;
    InterlockedExchange(&g_needRecBegin,0);
    d2vr_replay::RecBeginFrame(dev);
    if(d2vr_replay::g_recActive)g_gpuTiming.Begin(dev);
    else g_preSceneWriteMissed=true;
}

HRESULT STDMETHODCALLTYPE Hook_BeginScene(IDirect3DDevice9* dev) {
    if (g_insidePresentation) return g_origBeginScene(dev);
    g_renderThreadId = GetCurrentThreadId();
    PollHotkeys();
    if (g_submitMode != 4 && !d2vr_trace::g_enabled) {
        InterlockedIncrement(&g_frameId);
    }
    if (g_submitMode == 4 && g_stereoMode >= 1) {
        const auto ev = MakePipelineEvent(d2vr_frame::EventType::BeginScene);
        g_frameBoundary.OnEvent(ev);
        EmitPipelineTrace(ev, dev);
    } else if (d2vr_trace::g_enabled && d2vr_trace::g_traceScenes) {
        const auto ev = MakePipelineEvent(d2vr_frame::EventType::BeginScene);
        g_frameBoundary.OnEvent(ev);
        EmitPipelineTrace(ev, dev);
    }
    if (g_submitMode == 4 && g_stereoMode >= 1) {
        if (InterlockedCompareExchange(&g_needRecBegin, 0, 0)) {
            InterlockedExchange(&g_needRecBegin, 0);
            d2vr_replay::RecBeginFrame(dev);
            g_gpuTiming.Begin(dev);
        } else if (!d2vr_replay::g_recActive) {
            d2vr_replay::RecBeginFrame(dev);
            g_gpuTiming.Begin(dev);
        }
        d2vr_replay::RecEmit(d2vr_replay::OP_BeginScene, 0);
        if(g_preSceneWriteMissed)d2vr_replay::g_recOverflow=true;
    }
    HRESULT hrBS = g_origBeginScene(dev);
    if (FAILED(hrBS)) Log("!!! BeginScene FAILED hr=0x%08lX (game may stop rendering)", hrBS);
    return hrBS;
}

HRESULT STDMETHODCALLTYPE Hook_EndScene(IDirect3DDevice9* dev) {
    if (g_insidePresentation) return g_origEndScene(dev);
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
#ifdef D2VR_MATCHED_CAPTURE
    if (g_presentReadiness) {
        static unsigned scenes=0;
        if (++scenes==1 || scenes%600==0) {
            const auto& stats=g_frameBoundary.Stats();
            Log("[PresentReadiness] scenes=%u device=%llu ex=%llu swapchain=%llu resetGeneration=%ld slot17=%p slot121=%p observer=%s",
                scenes,stats.devicePresent,stats.presentEx,stats.swapChainPresent,g_captureResetGeneration,
                (*reinterpret_cast<void***>(dev))[17],(*reinterpret_cast<void***>(dev))[121],
                g_imagePreservingPresent ? (g_imagePresent.Failed()?"EXTERNAL_FAULT":
                    g_imagePresent.Ready()?"EXTERNAL_NATIVE_PRESENT_READY":"EXTERNAL_PENDING") :
                    g_enginePresentInstalled ? "EXPLICIT_ENGINE_CALLSITE" : "ABSENT");
        }
    }
#endif
    if (FAILED(hr)) Log("!!! EndScene FAILED hr=0x%08lX", hr);
    if ((g_hookMask & 2) && g_swapChainPresentCount == 0 && g_devicePresentCount == 0) {
        static int s_reprobe = 0;
        if ((++s_reprobe % 120) == 1) TryHookSwapChainPresent(dev);
    }
    {
        const auto ev = MakePipelineEvent(d2vr_frame::EventType::EndScene);
        d2vr_frame::Decision decision{};
        if (g_submitMode == 4 && g_stereoMode >= 1) {
            decision = g_frameBoundary.OnEvent(ev);
        } else if (d2vr_trace::g_enabled && d2vr_trace::g_traceScenes) {
            decision = g_frameBoundary.OnEvent(ev);
            EmitPipelineTrace(ev, dev);
        } else if (!d2vr_trace::g_enabled &&
                   InterlockedCompareExchange(&g_presentSeen, 0, 0) == 0) {
            decision = g_frameBoundary.OnEvent(ev);
        }
        if (g_submitMode == 4 && g_stereoMode >= 1)
            ApplyBoundaryDecision(dev, decision);
        else if (!d2vr_trace::g_enabled &&
                 InterlockedCompareExchange(&g_presentSeen, 0, 0) == 0)
            ApplyBoundaryDecision(dev, decision);
    }
    return hr;
}

static bool ShouldRouteDisplayToHeadsetOnly() {
    return g_submitMode == 4 && g_xrRunning &&
           InterlockedCompareExchange(&g_xrSubmittedThisFrame, 0, 0) != 0;
}

HRESULT STDMETHODCALLTYPE Hook_Present(IDirect3DDevice9* dev, const RECT* a, const RECT* b, HWND w,
                                       const RGNDATA* r) {
    if(g_insidePresentation) return g_origPresent(dev,a,b,w,r);
    PresentationScope presenting;
    AdaptivePresentationScope adaptive;
#ifdef D2VR_MATCHED_CAPTURE
    g_presentCaller=reinterpret_cast<uintptr_t>(_ReturnAddress());
#endif
    PollHotkeys();
    const LONG dp = InterlockedIncrement(&g_devicePresentCount);
    if (dp == 1 || (dp % 600) == 0)
        Log("[PresentHook] DevicePresent count=%ld dev=%p", dp, dev);
    if (InterlockedCompareExchange(&g_presentSeen, 1, 0) == 0) {
        Log("Hook_Present@17 detected — SubmitMode 4 stereo submits at Present@17 (not EndScene@42)");
    }
    IDirect3DSurface9* bb = nullptr;
    dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb);
    TracePresentLike(dev, "DevicePresent", nullptr, bb, 0, "");
    if (bb) bb->Release();
    {
        const auto ev = MakePipelineEvent(d2vr_frame::EventType::DevicePresent);
        const auto decision = g_frameBoundary.OnEvent(ev);
        if (g_submitMode == 4)
            ApplyBoundaryDecision(dev, decision);
        else if (!d2vr_trace::g_enabled)
            VrFrame(dev, "Present@17");
    }
    if (g_suppressDesktopPresent && ShouldRouteDisplayToHeadsetOnly()) {
        static int s_skipLog = 0;
        if (s_skipLog++ < 5)
            Log("[Display] skipping desktop Present (frame submitted to OpenXR)");
#ifdef D2VR_MATCHED_CAPTURE
        matched_capture::PresentReturned(D3D_OK,false);
        g_capturePresent.Leave();
#endif
        return D3D_OK;
    }
#ifdef D2VR_MATCHED_CAPTURE
    const HRESULT result=g_origPresent(dev, a, b, w, r);
    adaptive.Returned(result);
    matched_capture::PresentReturned(result,true);
    g_capturePresent.Leave();
    return result;
#else
    const HRESULT result=g_origPresent(dev, a, b, w, r);
    adaptive.Returned(result);
    return result;
#endif
}

HRESULT STDMETHODCALLTYPE Hook_PresentEx(IDirect3DDevice9Ex* dev, const RECT* a, const RECT* b, HWND w,
                                         const RGNDATA* r, DWORD flags) {
    if(g_insidePresentation) return g_origPresentEx ? g_origPresentEx(dev,a,b,w,r,flags):D3D_OK;
    PresentationScope presenting;
    AdaptivePresentationScope adaptive;
#ifdef D2VR_MATCHED_CAPTURE
    g_presentCaller=reinterpret_cast<uintptr_t>(_ReturnAddress());
#endif
    PollHotkeys();
    if (InterlockedCompareExchange(&g_presentSeen, 1, 0) == 0) {
        Log("Hook_PresentEx@121 detected — SubmitMode 4 stereo submits at PresentEx@121 (not EndScene@42)");
    }
    IDirect3DDevice9* base = dev;
    IDirect3DSurface9* bb = nullptr;
    base->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb);
    TracePresentLike(base, "PresentEx", nullptr, bb, flags, "");
    if (bb) bb->Release();
    {
        const auto ev = MakePipelineEvent(d2vr_frame::EventType::PresentEx);
        const auto decision = g_frameBoundary.OnEvent(ev);
        if (g_submitMode == 4)
            ApplyBoundaryDecision(dev, decision);
        else if (!d2vr_trace::g_enabled)
            VrFrame(dev, "PresentEx@121");
    }
#ifdef D2VR_MATCHED_CAPTURE
    if (g_suppressDesktopPresent && ShouldRouteDisplayToHeadsetOnly()) {
        matched_capture::PresentReturned(D3D_OK,false); g_capturePresent.Leave();
        return D3D_OK;
    }
    const HRESULT result=g_origPresentEx ? g_origPresentEx(dev, a, b, w, r, flags) : D3D_OK;
    adaptive.Returned(result,g_origPresentEx!=nullptr);
    matched_capture::PresentReturned(result,g_origPresentEx!=nullptr);
    g_capturePresent.Leave();
    return result;
#else
    if (g_suppressDesktopPresent && ShouldRouteDisplayToHeadsetOnly()) return D3D_OK;
    const HRESULT result=g_origPresentEx ? g_origPresentEx(dev, a, b, w, r, flags) : D3D_OK;
    adaptive.Returned(result,g_origPresentEx!=nullptr);
    return result;
#endif
}

// Use the actual presentation boundary, not each of the world's/UI's EndScene segments.
typedef HRESULT(STDMETHODCALLTYPE* PFN_SwapChainPresent)(IDirect3DSwapChain9*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);
// Diagnostic pass-through at the engine's actual COM dispatch. The vtable and
// implementation entry remain owned by D3D/other interceptors. Resolve the live
// slot for each call, so we never retain an entry subsequently detoured back to us.
static HRESULT STDMETHODCALLTYPE EngineSwapChainPresent(IDirect3DSwapChain9* sc,
    const RECT* a, const RECT* b, HWND w, const RGNDATA* r, DWORD flags) {
#ifdef D2VR_MATCHED_CAPTURE
    static thread_local d2vr_present_patch::Dispatch dispatch;
    if(!dispatch.Enter()) {
        Log("[DiagnosticEnginePresent] FAIL recursion entries=%llu forwarded=%llu returned=%llu",dispatch.entries,dispatch.forwarded,dispatch.returned);
        matched_capture::PresentReturned(D3DERR_INVALIDCALL,false);
        return D3DERR_INVALIDCALL; // Never forward an already recursive cycle.
    }
    const auto next=sc ? reinterpret_cast<PFN_SwapChainPresent>((*reinterpret_cast<void***>(sc))[3]) : nullptr;
    if(!next || next==&EngineSwapChainPresent) {
        dispatch.fault=true;dispatch.Leave();
        Log("[DiagnosticEnginePresent] FAIL invalid/self target sc=%p",sc);
        matched_capture::PresentReturned(D3DERR_INVALIDCALL,false);
        return D3DERR_INVALIDCALL;
    }
    const bool outer=!g_insidePresentation;
    PresentationScope presenting;
    AdaptivePresentationScope adaptive;
    void* caller=_ReturnAddress();
    g_presentCaller=reinterpret_cast<uintptr_t>(caller);
    IDirect3DDevice9* owner=nullptr;
    const HRESULT ownerHr=sc->GetDevice(&owner);
    const bool exactCaller=caller==reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr))+0x694057;
    if(outer && exactCaller && SUCCEEDED(ownerHr) && owner && !dispatch.fault && !g_captureResetting) {
        const auto decision=g_frameBoundary.OnEvent(MakePipelineEvent(d2vr_frame::EventType::SwapChainPresent));
        if(g_submitMode==4) {
            d2vr_cpu_sync::Scope cpuBoundary(d2vr_cpu_sync::BoundaryWork);
            ApplyBoundaryDecision(owner,decision);
        }
    }
    if(dispatch.entries==1) {
        HMODULE module=nullptr;char path[MAX_PATH]={};
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(next),&module);
        if(module) GetModuleFileNameA(module,path,MAX_PATH);
        Log("[DiagnosticEnginePresent] identity sc=%p vt=%p device=%p getHr=%08lX next=%p module=%s caller=%p exactCaller=%d tid=%lu",
            sc,*reinterpret_cast<void***>(sc),owner,ownerHr,reinterpret_cast<void*>(next),path,caller,exactCaller,GetCurrentThreadId());
    }
    ++dispatch.forwarded;
    HRESULT hr;
    {
        d2vr_cpu_sync::Scope cpuPresent(d2vr_cpu_sync::PresentForward);
        hr=next(sc,a,b,w,r,flags); // Exactly one call; no live slot re-read.
    }
    adaptive.Returned(hr,outer && exactCaller && !dispatch.fault);
    const bool reportReturn=dispatch.Returned(static_cast<int32_t>(hr));
#ifdef D2VR_MATCHED_CAPTURE
    d2vr_cpu_sync::timing.Report(g_srcW,g_srcH,g_trackedFrame.id);
#endif
    InterlockedIncrement(&g_swapChainPresentCount);InterlockedExchange(&g_presentSeen,1);
    if(outer) {
        if(dispatch.fault || !exactCaller) matched_capture::Reject("Present dispatch fault or unexpected caller");
        matched_capture::PresentReturned(hr,true); // The original really was forwarded.
        g_capturePresent.Leave();
    }
    if(owner) owner->Release();
    if(reportReturn)
        Log("[DiagnosticEnginePresent] entries=%llu forwarded=%llu returned=%llu recursive=%llu hr=%08lX sc=%p tid=%lu fault=%d ok=%llu statuses=%llu errors=%llu",
            dispatch.entries,dispatch.forwarded,dispatch.returned,dispatch.recursive,hr,sc,GetCurrentThreadId(),dispatch.fault,
            dispatch.ok,dispatch.statuses,dispatch.errors);
    dispatch.Leave();
    return hr;
#else
    const auto next = reinterpret_cast<PFN_SwapChainPresent>((*reinterpret_cast<void***>(sc))[3]);
    PresentationScope presenting;
    AdaptivePresentationScope adaptive;
    // Complete the recorded game frame while its backbuffer is still valid,
    // before a DISCARD Present. Overlay scenes inside next() are not game frames.
    if (g_presentHookLevel >= 1) {
        const auto decision = g_frameBoundary.OnEvent(MakePipelineEvent(d2vr_frame::EventType::SwapChainPresent));
        if (g_submitMode == 4 && decision.requestVrFrame) {
            IDirect3DDevice9* owner = nullptr;
            if (SUCCEEDED(sc->GetDevice(&owner)) && owner) {
                ApplyBoundaryDecision(owner, decision);
                owner->Release();
            }
        }
    }
    const HRESULT hr = next(sc, a, b, w, r, flags);
    adaptive.Returned(hr);
    const LONG n = InterlockedIncrement(&g_swapChainPresentCount);
    InterlockedExchange(&g_presentSeen, 1);
    if (n == 1) {
        IDirect3DDevice9* owner = nullptr;
        IDirect3DSwapChain9* implicit = nullptr;
        if (SUCCEEDED(sc->GetDevice(&owner)) && owner) {
            owner->GetSwapChain(0, &implicit);
            Log("[EnginePresent] identity actual=%p implicit=%p device=%p chains=%u",
                sc, implicit, owner, owner->GetNumberOfSwapChains());
            IDirect3DSurface9 *actualBack=nullptr, *implicitBack=nullptr, *rt=nullptr;
            sc->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&actualBack);
            owner->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&implicitBack);
            owner->GetRenderTarget(0,&rt);
            D3DSURFACE_DESC desc{};
            if (actualBack) actualBack->GetDesc(&desc);
            Log("[EnginePresent] surfaces actual=%p implicit=%p rt0=%p actualSize=%ux%u",
                actualBack,implicitBack,rt,desc.Width,desc.Height);
            if(actualBack) actualBack->Release();
            if(implicitBack) implicitBack->Release();
            if(rt) rt->Release();
            if (implicit) implicit->Release();
            owner->Release();
        }
    }
    if (n == 1 || n % 1000 == 0)
        Log("[EnginePresent] completed=%ld sc=%p next=%p hr=0x%08lX caller=%p ms=%llu",
            n, sc, reinterpret_cast<void*>(next), hr, _ReturnAddress(), GetTickCount64());
    if (g_presentHookLevel >= 2)
        d2vr_trace::Emit(GetTickCount64(), GetCurrentThreadId(), "SwapChainPresent", 0,
            static_cast<uint32_t>(d2vr_replay::g_rec.size()), nullptr, sc, nullptr,
            static_cast<uint32_t>(hr), _ReturnAddress(), flags, "engine-dispatch-completed");
    return hr;
#endif
}

#ifdef D2VR_MATCHED_CAPTURE
#include "image_present_runtime.inl"
#include "protected_source_size_runtime.inl"
#include "native_view_boundary_runtime.inl"
// Explicit exception: changes five bytes in the LOADED EXE, never its file.
// Called only from the first CreateDevice callback before returning the device
// to the engine. Never install/reinstall asynchronously from EndScene/Reset.
static void InstallDiagnosticEnginePresent() {
    if(!GetPrivateProfileIntA("Diagnostics","EnginePresentCallsite",0,".\\Darkness2VR-diagnostic.ini")) return;
    if (!NativeImageWriteAllowed(0xA94052,5)) return;
    auto image=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    char path[MAX_PATH]={};if(!GetModuleFileNameA(nullptr,path,MAX_PATH)) return;
    HANDLE locked=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(locked==INVALID_HANDLE_VALUE) {Log("[DiagnosticEnginePresent] REFUSED executable read lock");return;}
    const std::string sha=matched_capture::HashFile(path);
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    auto pe=reinterpret_cast<const IMAGE_NT_HEADERS32*>(image+dos->e_lfanew);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || pe->Signature!=IMAGE_NT_SIGNATURE ||
        !d2vr_present_patch::Identity(sha.c_str(),pe->FileHeader.Machine,pe->FileHeader.TimeDateStamp,
            pe->OptionalHeader.SizeOfImage,pe->OptionalHeader.ImageBase,pe->OptionalHeader.Magic)) {
        Log("[DiagnosticEnginePresent] REFUSED SHA/PE sha=%s",sha.c_str());CloseHandle(locked);return;
    }
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCSTR>(&EngineSwapChainPresent),&pinned)) {CloseHandle(locked);return;}
    struct Memory {
        unsigned char* site;DWORD old=0;
        unsigned char* bytes(){return site;}
        bool writable(){return VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old)!=FALSE;}
        bool flush(){return FlushInstructionCache(GetCurrentProcess(),site,5)!=FALSE;}
        bool restore(){DWORD ignored=0;return VirtualProtect(site,5,old,&ignored)!=FALSE;}
    } memory{image+0x694052};
    static d2vr_present_patch::Patch patch;
    const bool installed=patch.Install(memory,static_cast<uint32_t>(reinterpret_cast<uintptr_t>(memory.site)),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&EngineSwapChainPresent)));
    CloseHandle(locked);
    g_enginePresentInstalled=installed;
    if(installed) g_presentReadiness=true;
    Log("[DiagnosticEnginePresent] installed=%d rollbackUncertain=%d site=%p sha=%s IN_MEMORY_EXE_PATCH=5_BYTES",
        installed,patch.rollbackUncertain,memory.site,sha.c_str());
    // A failed rollback is not a runnable experiment. Do not return a possibly
    // half-installed entry point to the engine or silently claim recovery.
    if(patch.rollbackUncertain) RaiseFailFastException(nullptr,nullptr,0);
}

// Explicit playable-candidate opt-in. Install once before the first CreateDevice
// returns, never from DllMain, a frame callback or an asynchronous INI reload.
// Retain the diagnostic build's suppression of ALL historical native FOV probes.
static void InstallDiagnosticTrackedGameplay() {
    const bool requested=GetPrivateProfileIntA("Diagnostics","RestoreTrackedGameplay",0,
        ".\\Darkness2VR-diagnostic.ini")!=0;
    if(!requested) return;
    if (!NativeImageWriteAllowed(0xC63009,6)) return; // refuse the bundle before its first write
    char path[MAX_PATH]{};
    if(!GetModuleFileNameA(nullptr,path,MAX_PATH)) return;
    HANDLE locked=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(locked==INVALID_HANDLE_VALUE) {Log("[TrackedGameplay] REFUSED executable read lock");return;}
    const std::string sha=matched_capture::HashFile(path);
    const auto image=reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS32*>(image+dos->e_lfanew);
    const bool identity=reinterpret_cast<uintptr_t>(image)==0x400000 &&
        dos->e_magic==IMAGE_DOS_SIGNATURE && pe->Signature==IMAGE_NT_SIGNATURE &&
        d2vr_present_patch::Identity(sha.c_str(),pe->FileHeader.Machine,pe->FileHeader.TimeDateStamp,
            pe->OptionalHeader.SizeOfImage,pe->OptionalHeader.ImageBase,pe->OptionalHeader.Magic);
    const bool ready=d2vr_tracked_gameplay::Ready(requested,identity,g_enginePresentInstalled,
        !g_disableOpenXR,g_submitMode,g_stereoMode,g_headGeometry,g_nativeStereoHeadGeometry!=0,
        g_nativeLeftAim,g_nativeWeaponPose,g_nativeHandsEnabled,g_tentacleBridge);
    uint32_t failed=0;
    const auto read=[](uint32_t address,void* out,size_t size) {
        __try {memcpy(out,reinterpret_cast<const void*>(address),size);return true;}
        __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
    };
    if(!ready || !d2vr_tracked_gameplay::Validate(read,failed)) {
        Log("[TrackedGameplay] REFUSED identity/config/sites ready=%d failed=%08X sha=%s",ready,failed,sha.c_str());
        CloseHandle(locked); return;
    }
    const bool handPackRequested=GetPrivateProfileIntA("Diagnostics","NativeHandsPackHook",0,
        ".\\Darkness2VR-diagnostic.ini")!=0;
    if(handPackRequested && !d2vr_tracked_gameplay::ValidateHandPack(read,failed)) {
        Log("[NativeHandsPack] REFUSED original bytes at %08X",failed);
        CloseHandle(locked); return;
    }
    const bool installed=InstallNativeAimHook() && InstallNativeThrowHook() &&
        InstallNativeWeaponHook() && InstallNativePistolHook() && InstallNativeHandsHook() &&
        (!handPackRequested || InstallNativeHandsPackHook());
    if(!installed) {
        const bool packRemoved=RemoveNativeHandsPackHook();
        RemoveNativeHandsHook(); RemoveNativePistolHook(); RemoveNativeWeaponHook();
        RemoveNativeThrowHook(); RemoveNativeAimHook();
        if(!packRemoved || !d2vr_tracked_gameplay::Validate(read,failed) ||
           (handPackRequested && !d2vr_tracked_gameplay::ValidateHandPack(read,failed))) {
            Log("[TrackedGameplay] FATAL rollback mismatch at %08X",failed);
            CloseHandle(locked); RaiseFailFastException(nullptr,nullptr,0); return;
        }
    }
    CloseHandle(locked);
    g_restoredTrackedGameplay=installed;
    // The native pose hooks do not deliver locomotion, turning or gesture button
    // pulses. Install the existing XInput bridge here, outside the loader lock.
    if(installed) TryInstallXInputHook();
    Log("[TrackedGameplay] restored=%d sha=%s head6DoF=%d hands=%d handPack=%d gestures=%d xinput=%ld; legacy native FOV hooks remain OFF; IN_MEMORY_EXE_PATCH=21_BYTES%s",
        installed,sha.c_str(),g_headGeometry && g_nativeStereoHeadGeometry,g_nativeHandsEnabled,
        g_nativeHandsPackInstalled,g_tentacleBridge,(long)InterlockedCompareExchange(&g_xinputHooked,0,0),
        g_nativeHandsPackInstalled?"+4_BYTE_VTABLE":"");
}

static void InstallDiagnosticCameraCover() {
    if(!GetPrivateProfileIntA("Diagnostics","NativeCameraCover",0,".\\Darkness2VR-diagnostic.ini")) return;
    if (!NativeImageWriteAllowed(d2vr_camera_cover::site,5)) return;
    if(!g_enginePresentInstalled || g_disableOpenXR || g_submitMode!=4 || g_stereoMode!=1 ||
       !g_autoFovWiden || g_engineFovFactor>1 || g_diagnosticFovWiden!=0) {
        Log("[NativeCameraCover] REFUSED incompatible config/Present");return;
    }
    char path[MAX_PATH]{};if(!GetModuleFileNameA(nullptr,path,MAX_PATH)) return;
    HANDLE locked=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(locked==INVALID_HANDLE_VALUE) return;
    const auto image=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    const auto pe=reinterpret_cast<const IMAGE_NT_HEADERS32*>(image+dos->e_lfanew);
    const auto sha=matched_capture::HashFile(path);
    const bool identity=reinterpret_cast<uintptr_t>(image)==0x400000 && dos->e_magic==IMAGE_DOS_SIGNATURE &&
        pe->Signature==IMAGE_NT_SIGNATURE && d2vr_present_patch::Identity(sha.c_str(),pe->FileHeader.Machine,
        pe->FileHeader.TimeDateStamp,pe->OptionalHeader.SizeOfImage,pe->OptionalHeader.ImageBase,pe->OptionalHeader.Magic);
    uint32_t failed=0;
    const auto read=[](uint32_t a,void* out,size_t n) {
        __try {std::memcpy(out,reinterpret_cast<const void*>(a),n);return true;}
        __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
    };
    if(!identity || !d2vr_camera_cover::Validate(read,failed)) {
        Log("[NativeCameraCover] REFUSED identity/sites sha=%s failed=%08X",sha.c_str(),failed);
        CloseHandle(locked);return;
    }
    struct Memory {
        DWORD old=0;unsigned char* bytes(){return reinterpret_cast<unsigned char*>(d2vr_camera_cover::site);}
        bool writable(){return VirtualProtect(bytes(),5,PAGE_EXECUTE_READWRITE,&old)!=FALSE;}
        bool flush(){return FlushInstructionCache(GetCurrentProcess(),bytes(),5)!=FALSE;}
        bool restore(){DWORD ignored;return VirtualProtect(bytes(),5,old,&ignored)!=FALSE;}
    } memory;
    static d2vr_camera_cover::Patch patch;
    g_nativeCameraCoverInstalled=patch.Install(memory,static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&SelectedRenderCameraNear)));
    CloseHandle(locked);
    Log("[NativeCameraCover] installed=%d rollbackUncertain=%d sha=%s site=00986139 IN_MEMORY_EXE_PATCH=5_BYTES; experimental OFF by default",
        g_nativeCameraCoverInstalled,patch.rollbackUncertain,sha.c_str());
    if(patch.rollbackUncertain) RaiseFailFastException(nullptr,nullptr,0);
}
#endif

static void TryHookSwapChainPresent(IDirect3DDevice9* dev) {
#ifdef D2VR_MATCHED_CAPTURE
    return; // explicit diagnostic opt-in installs only before CreateDevice returns
#endif
    if (!(g_hookMask & 2) || !dev) return;
    static bool attempted = false;
    if (attempted) return;
    attempted = true;
    // Verified x86 engine dispatch: mov eax,[ecx+0Ch]; call eax. All six
    // stdcall arguments are already on the stack. No relocated instructions.
    auto image = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    const auto pe = reinterpret_cast<const IMAGE_NT_HEADERS32*>(image + dos->e_lfanew);
    if (pe->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        pe->FileHeader.TimeDateStamp != 0x4F68A875 || pe->OptionalHeader.SizeOfImage != 0xDEC000) {
        Log("[EnginePresent] REFUSED: unsupported executable identity");
        return;
    }
    auto site = image + 0x694052;
    if (!NativeImageWriteAllowed(reinterpret_cast<uintptr_t>(site),5)) return;
    const unsigned char expected[] = {0x8B,0x41,0x0C,0xFF,0xD0};
    if (memcmp(site, expected, sizeof(expected)) != 0) {
        Log("[EnginePresent] REFUSED: call-site bytes differ");
        return;
    }
    DWORD old = 0;
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old)) return;
    const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&EngineSwapChainPresent) -
        reinterpret_cast<uintptr_t>(site + 5));
    site[0] = 0xE8;
    memcpy(site + 1, &rel, sizeof(rel));
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    VirtualProtect(site, 5, old, &old);
    g_enginePresentInstalled = true;
    Log("[EnginePresent] installed verified call-site=%p (no COM slot patch)", site);
    // The old fixed address reads 0xC9, not a proven COM device. Never call
    // through it: SEH is not object validation. Only use CreateDevice results.
}

#include "startup_shader_warmup.inl"

static void InstallQueryPreload(IDirect3DDevice9* dev) {
    if(!g_queryPreloadCount || g_queryPreload.load())return;
#ifdef D2VR_MATCHED_CAPTURE
    if(!g_imagePreservingPresent || !g_nativeImageGuard.enabled)return;
    const auto* image=reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
    char path[MAX_PATH]{};
    if(reinterpret_cast<uintptr_t>(image)!=0x400000 || !GetModuleFileNameA(nullptr,path,MAX_PATH))return;
    const auto sha=matched_capture::HashFile(path);
    static constexpr BYTE site[]={0x8b,0x44,0x24,0x04,0x8b,0x10,0x83,0xc1,0x04,0x51,0x6a,0x09,
        0x50,0x8b,0x82,0xd8,0x01,0x00,0x00,0xff,0xd0,0xc2,0x04,0x00};
    if(sha!="d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1" ||
       memcmp(image+0x5fbdd0,site,sizeof(site))) {
        Log("[QueryPreload] refused: native SHA/site mismatch");return;
    }
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&Hook_CreateQuery),&pinned))return;
    auto* table=*reinterpret_cast<void***>(dev);
    g_queryCreateOriginal=reinterpret_cast<d2vr_query_preload::CreateFn>(table[118]);
    try {
    auto pool=std::make_unique<d2vr_query_preload::Pool>(dev,g_renderSizeHwnd,0x9fbde5,g_queryCreateOriginal,g_queryPreloadCount);
    pool->EndReset(dev,D3D_OK);
    // Publish before installing the API gate. Native code and query objects
    // remain untouched; PatchVTable uses the existing device-hook architecture.
    g_queryPreload.store(pool.get(),std::memory_order_release);
    auto original=PatchVTable(dev,118,reinterpret_cast<void*>(&Hook_CreateQuery));
    if(!original){g_queryPreload.store(nullptr);return;}
    pool.release();
    Log("[QueryPreload] installed count=%u pending=%u caller=009FBDE5; startup/Reset only, no visibility result changes",
        g_queryPreloadCount,g_queryPreload.load()->Pending());
    } catch(...) {
        g_queryPreload.store(nullptr);
        Log("[QueryPreload] initialization failed; original CreateQuery retained");
    }
#else
    Log("[QueryPreload] refused: protected diagnostic architecture required");
#endif
}

void HookDevice(IDirect3DDevice9* dev) {
    d2vr_sampling::InvalidateSamplerCaps();
    StartAsyncLogging();
    g_renderThreadId = GetCurrentThreadId();
    if (InterlockedCompareExchange(&g_devHooked, 1, 0) != 0) return;
    StartupShaderWarmup(dev); // before our CreateShader hooks; native owner not yet supplied this device
    d2vr_shader_preload::Initialize(dev);
    LogStartupStage(11, "HookDevice (D3D9 vtable + replay hooks)");
    g_smallRecordCopyAdmitted = g_smallRecordCopyAdmission.Admit(g_smallRecordCopyRequested);
    if (g_smallRecordCopyRequested) Log("[SmallRecordCopy] requested=1 admitted=%d guardProbe=%d limit=%zu (failed reads retain legacy copy)",
        int(g_smallRecordCopyAdmitted), int(g_smallRecordCopyAdmission.Tested()), d2vr_record_copy::kSmallLimit);
    MaybeInstallFovProbeAfterDevice();
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
    InstallQueryPreload(dev);

    // SwapChain present hooks are installed after the first successful Device::Reset only.

    void** devVt = *reinterpret_cast<void***>(dev);
#ifdef D2VR_MATCHED_CAPTURE
    g_presentReadiness=GetPrivateProfileIntA("Diagnostics","PresentReadiness",0,".\\Darkness2VR-diagnostic.ini") ||
        GetPrivateProfileIntA("Diagnostics","MatchedFrameCapture",0,".\\Darkness2VR-diagnostic.ini");
    if (g_presentReadiness) {
        IDirect3DSwapChain9* implicit=nullptr;
        const HRESULT obtained=dev->GetSwapChain(0,&implicit);
        void* slot=implicit ? (*reinterpret_cast<void***>(implicit))[3] : nullptr;
        Log("[PresentReadiness] coverage device=%p vt=%p slot17=%p slot121=%p implicit=%p slot3=%p getHr=0x%08lX swapchainObserver=%s engineDispatchPatch=DISABLED",
            dev,devVt,devVt[17],devVt[121],implicit,slot,obtained,
            g_imagePreservingPresent?"EXTERNAL_PENDING":"ABSENT");
        Log("[PresentReadiness] implicit identity does not establish actual engine swapchain coverage; EndScene cannot certify capture");
        if (implicit) implicit->Release();
    }
#endif
#ifdef D2VR_MATCHED_CAPTURE
    // Read once before either Present installer latches optional timing callbacks.
    d2vr_cpu_sync::timing.Enable(GetPrivateProfileIntA("Diagnostics","CpuSyncTiming",0,
        ".\\Darkness2VR-diagnostic.ini")!=0);
    InstallImagePresent(dev);
    InstallProtectedSourceQuality();
    InstallNativeViewBoundary();
    InstallDiagnosticEnginePresent();
    InstallDiagnosticTrackedGameplay();
    InstallDiagnosticCameraCover();
#endif
    Log("[EffectiveConfig] SwapChainPresentHook=%s OpenXR=%s TraceCSV=%s",
#ifdef D2VR_MATCHED_CAPTURE
        g_imagePreservingPresent ? (g_imagePresent.Failed()?"external-fault-XR-blocked":"external-pending-native-Present") :
            g_enginePresentInstalled ? "diagnostic-explicit-engine-callsite" : "disabled-native-patch-no-swapchain-observer",
#else
        g_enginePresentInstalled ? "engine-callsite" : "pending-first-EndScene",
#endif
        g_disableOpenXR ? "disabled" : "enabled",
        d2vr_trace::g_enabled ? "enabled" : "disabled");
    Log("Method 2 Hooks active on device %p vtable=%p Present@17=%p EndScene@42=%p swapchainsHooked=%zu",
        dev, devVt, devVt[17], devVt[42], size_t(g_enginePresentInstalled));

    // Evidence check of the RE map (ENGINE_REVERSE_ENGINEERING.md): engine D3D9 device global
    __try {
        uintptr_t devGlobal = *(uintptr_t*)(0x010dee84 + 0x0c);
        Log("[FovRE] engine device global @0x010DEE90 = %p (hooked device = %p) -> %s",
            (void*)devGlobal, dev, (devGlobal == (uintptr_t)dev) ? "MATCH" : "mismatch");
    } __except (1) {
        Log("[FovRE] engine device global not readable");
    }

    // Existing production resolution path. Diagnostic captures can explicitly
    // match it without enabling the unrelated native gameplay/FOV patches.
#ifdef D2VR_MATCHED_CAPTURE
    g_diagnosticProductionResolution = g_enginePresentInstalled &&
        GetPrivateProfileIntA("Diagnostics", "ProductionResolution", 0, ".\\Darkness2VR-diagnostic.ini") != 0;
    const bool resolutionAllowed = g_diagnosticProductionResolution;
#else
    const bool resolutionAllowed = true;
#endif
    if (resolutionAllowed && !g_getClientRectHooked && g_fakeClientRect) {
        __try {
            void** slot = (void**)0x00EEB478;
            void* cur = *slot;
            // Verified USER32!GetClientRect import and engine call at C9646E.
            // An arbitrary high address or another interceptor is insufficient.
            void* expected = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetClientRect"));
            const unsigned char call[] = {0xff,0x15,0x78,0xb4,0xee,0x00};
            if (cur && cur == expected && !memcmp(reinterpret_cast<void*>(0x00C9646E), call, sizeof(call))) {
                g_origGetClientRect = reinterpret_cast<PFN_GetClientRect>(cur);
                DWORD old = 0;
                if (NativeImageWriteAllowed(reinterpret_cast<uintptr_t>(slot),sizeof(void*)) &&
                    VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) {
                    *slot = reinterpret_cast<void*>(&Hook_GetClientRect);
                    VirtualProtect(slot, sizeof(void*), old, &old);
                    g_getClientRectHooked = true;
                    Log("[VrRes] GetClientRect IAT@0x00EEB478 hooked (orig=%p hwnd=%p) request=%dx%d; actual RT0 must be measured",
                        cur, g_renderSizeHwnd, g_renderW, g_renderH);
                }
            } else {
                Log("[VrRes] GetClientRect import/call fingerprint mismatch (%p expected=%p) - skipped", cur, expected);
            }
        } __except (1) {
            Log("[VrRes] GetClientRect IAT hook failed");
        }
    }
    Log("[VrRes] diagnosticOptIn=%d hooked=%d request=%dx%d", (int)g_diagnosticProductionResolution,
        (int)g_getClientRectHooked, g_renderW, g_renderH);
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
    g_renderSizeHwnd = pp && pp->hDeviceWindow ? pp->hDeviceWindow : hwnd;
    if(g_stereoTransferMode==1 || g_stereoTransferMode==2) flags|=D3DCREATE_MULTITHREADED;
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
    g_renderSizeHwnd = pp && pp->hDeviceWindow ? pp->hDeviceWindow : hwnd;
    if(g_stereoTransferMode==1 || g_stereoTransferMode==2) flags|=D3DCREATE_MULTITHREADED;
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
#ifdef D2VR_MATCHED_CAPTURE
    // Native C27160 calls the factory before A0B540 creates/sizes the window.
    // CreateDevice is too late to intercept C96440's initial size request.
    InstallProtectedSourceQuality();
#endif
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
#ifdef D2VR_MATCHED_CAPTURE
    InstallProtectedSourceQuality();
#endif
    LogStartupStage(7, "Direct3DCreate9Ex");
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
        LogStartupStage(1, "DllMain PROCESS_ATTACH");
#ifdef D2VR_MATCHED_CAPTURE
        Log("[BuildFlavor] Darkness2VR-build:protected-native-v1");
#else
        Log("[BuildFlavor] Darkness2VR-build:standard-v1");
#endif
        g_vehHandle = AddVectoredExceptionHandler(1, &CrashVehHandler);
        LogStartupStage(2, "VEH crash handler installed");
        LoadConfig();
        LogStartupStage(3, "LoadConfig");
        bool imageProfile=false;
#ifdef D2VR_MATCHED_CAPTURE
        imageProfile=g_imagePreservingPresent;
#endif
        const bool protectedConfig=g_nativeImageGuard.ConfigAllowed(g_disableOpenXR!=0,g_stereoMode,
            GetPrivateProfileIntA("Diagnostics","MatchedFrameCapture",0,".\\Darkness2VR-diagnostic.ini")!=0,imageProfile);
#ifdef D2VR_MATCHED_CAPTURE
        const bool protectedInputAllowed=!g_imagePreservingInput ||
            d2vr_protected_input::Requested(imageProfile,g_nativeImageGuard.enabled,g_imagePreservingInput,g_enableVRGamepad);
        const bool protectedGestureAllowed=!g_protectedGestureActions ||
            d2vr_protected_input::Requested(imageProfile,g_nativeImageGuard.enabled,g_imagePreservingInput,g_enableVRGamepad);
        const bool protectedLeftAimAllowed=!g_protectedLeftAim ||
            (g_protectedGestureActions && g_nativeViewBoundaryMode==3 && g_headGeometry &&
             d2vr_protected_input::Requested(imageProfile,g_nativeImageGuard.enabled,g_imagePreservingInput,g_enableVRGamepad));
        const bool protectedGestureRequested=g_protectedGestureActions;
        const bool protectedLeftAimRequested=g_protectedLeftAim;
        const bool protectedLeftThrowAllowed=!g_protectedLeftThrow || (g_protectedLeftAim && protectedLeftAimAllowed);
        const bool protectedLeftThrowRequested=g_protectedLeftThrow;
        const bool protectedHandsAllowed=!g_protectedTrackedHands || (g_protectedLeftAim && protectedLeftAimAllowed &&
            g_nativeViewBoundaryMode==3 && g_protectedHeadTracking);
        const bool inputFeatureAllowed=!g_enableVRGamepad || g_imagePreservingInput;
#else
        const bool protectedInputAllowed=true,inputFeatureAllowed=!g_enableVRGamepad;
        const bool protectedGestureAllowed=true,protectedGestureRequested=false;
        const bool protectedLeftAimAllowed=true;
        const bool protectedLeftAimRequested=false;
        const bool protectedLeftThrowAllowed=true,protectedLeftThrowRequested=false;
        const bool protectedHandsAllowed=true;
#endif
        const bool protectedFeatures=protectedInputAllowed && protectedGestureAllowed && protectedLeftAimAllowed && protectedLeftThrowAllowed && protectedHandsAllowed && (!imageProfile || (g_submitMode==4 && g_frameBoundaryPolicy==0 &&
            !g_nativeLeftAim && !g_nativeWeaponPose && !g_nativeHandsEnabled && !g_gestureBridge && !g_tentacleBridge &&
            inputFeatureAllowed && g_headLookMode==0 &&
            (!g_protectedHeadTracking || ProtectedHeadConfigurationAllowed()) &&
            (!(g_headGeometry || g_nativeStereoHeadGeometry) || g_protectedHeadTracking) &&
            !GetPrivateProfileIntA("Diagnostics","EnginePresentCallsite",0,".\\Darkness2VR-diagnostic.ini") &&
            !GetPrivateProfileIntA("Diagnostics","RestoreTrackedGameplay",0,".\\Darkness2VR-diagnostic.ini") &&
            !GetPrivateProfileIntA("Diagnostics","NativeHandsPackHook",0,".\\Darkness2VR-diagnostic.ini") &&
            !GetPrivateProfileIntA("Diagnostics","NativeCameraCover",0,".\\Darkness2VR-diagnostic.ini") &&
            !GetPrivateProfileIntA("Diagnostics","ProductionResolution",0,".\\Darkness2VR-diagnostic.ini")));
        if ((g_nativeImageGuard.enabled || protectedGestureRequested || protectedLeftAimRequested || protectedLeftThrowRequested) && (!protectedConfig || !protectedFeatures)) {
            Log("[NativeImageGuard] REFUSED configuration: mono isolation OR protected SubmitMode4/Stereo1/PresentPrimary; input requires ImagePreservingInput; native hands/aim/FOV/EXE Present/sizing must stay disabled");
            if (g_vehHandle) {RemoveVectoredExceptionHandler(g_vehHandle);g_vehHandle=nullptr;}
            EnterCriticalSection(&g_logLock);
            g_logReady=false;g_logFile.Close();
            LeaveCriticalSection(&g_logLock);
            return FALSE;
        }
#ifdef D2VR_MATCHED_CAPTURE
        Log("[MatchedCapture] diagnostic build: legacy FOV probes suppressed; RestoreTrackedGameplay and NativeCameraCover OFF unless explicitly opted in and verified at CreateDevice");
        if (false) {
#else
        if (g_startupMode < 1) {
#endif
            InstallNativeAimHook();
            InstallNativeThrowHook();
            InstallNativeWeaponHook();
            InstallNativePistolHook();
            InstallNativeHandsHook();
            LogStartupStage(4, "native gameplay hooks (no exe patch in hands RC7b+)");
        } else {
            LogStartupStage(4, "StartupMode>=1: skipped native gameplay hooks");
        }
        Log("Darkness2VR v0.4.0 attached (pid=%lu) candidate=rc20b-load-fix-20261021 startupMode=%d",
            GetCurrentProcessId(), g_startupMode);
        Log("[HeadGeometry] startup=%d; FovFrustumHook=%d HideGameWindow=%d SuppressDesktopPresent=%d",
            (int)g_headGeometry, g_fovFrustumHook, g_hideGameWindow, g_suppressDesktopPresent);
        if (!LoadReal()) {
            Log("FATAL: could not load system d3d9.dll");
            return FALSE;
        }
        LogStartupStage(5, "LoadReal system d3d9.dll OK");
        TryInstallXInputHook();
        LogStartupStage(6, "TryInstallXInputHook done (FOV probes deferred until CreateDevice)");
    } else if (reason == DLL_PROCESS_DETACH) {
        Log("[PresentHook] detach presentCount=%ld maxTlsDepth=%ld hookedSwapChains=%zu",
            (long)InterlockedCompareExchange(&g_swapChainPresentCount, 0, 0),
            (long)InterlockedCompareExchange(&g_swapChainPresentMaxDepth, 0, 0), size_t(g_enginePresentInstalled));
        d2vr_trace::Shutdown();
        Log("[RenderPipelineTrace] attempted=%ld dropped=%ld (last worker flush may be lost at process exit)",
            (long)InterlockedCompareExchange(&d2vr_trace::g_count,0,0),
            (long)InterlockedCompareExchange(&d2vr_trace::g_dropped,0,0));
        d2vr::psvr2::ClientRequestStop();
#ifdef D2VR_MATCHED_CAPTURE
        if(!RemoveNativeHandsPackHook()) RaiseFailFastException(nullptr,nullptr,0);
#endif
        RemoveNativeHandsHook();
        RemoveNativePistolHook();
        RemoveNativeWeaponHook();
        RemoveNativeThrowHook();
        RemoveNativeAimHook();
        Log("Darkness2VR detaching (pid=%lu)", GetCurrentProcessId());
        if (g_vehHandle) {
            RemoveVectoredExceptionHandler(g_vehHandle);
            g_vehHandle = nullptr;
        }
        if (g_asyncLog.Started()) {
            // Process-lifetime pinned worker: no join, FILE close or CRT file
            // lock under loader lock. Process exit can lose queued tail rows.
            g_logReady = false;
            g_asyncLog.SignalStop();
        } else {
            EnterCriticalSection(&g_logLock);
            g_logReady = false;
            g_logFile.Close();
            // Emergency handle is process-lifetime; no handler-close race here.
            LeaveCriticalSection(&g_logLock);
        }
    }
    return TRUE;
}

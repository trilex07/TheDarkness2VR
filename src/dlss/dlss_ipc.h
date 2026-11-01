// dlss/dlss_ipc.h - the wire contract between the 32-bit proxy (dlss/dlss_client.cpp)
// and the 64-bit NGX/FSR helper (src/tools/dlss_host). Plain fixed-size structs, packed, no
// pointers: both sides compile this header, one as x86 and one as x64, and the static
// asserts below hold on both.
//
// Design adapted from Dishonored-VR and the BioShock VR DLSS/DLAA community fork (MIT),
// itself derived from DLSS5-Feeder (MIT); see src/tools/dlss_host/NOTICE.md.
#pragma once
#include <stdint.h>

namespace d2vr::dlss_ipc {

const uint32_t kMagic   = 0x52563244u;   // 'D2VR'
const uint32_t kVersion = 3u;

enum Backend : uint32_t { BackendDlss = 0, BackendFsr = 1 };

#define D2VR_DLSS_PIPE_FMT L"\\\\.\\pipe\\d2vr-dlss.%lu"

enum Slot : uint32_t { Color = 0, Output, Depth, Motion, Bias, SlotCount };
enum Tag : uint8_t { TagBuild = 'B', TagFrame = 'F', TagQuit = 'Q' };

#pragma pack(push, 1)
struct Hello {            // game -> helper, once
    uint32_t magic, version, pid;
    uint32_t luidLow; int32_t luidHigh;   // the game's D3D11 adapter; the helper must match it
    uint32_t backend;        // Backend
    uint32_t fsrVersion;     // FSR: 0 = the newest the runtime offers, else a 1-based index into its list
};
struct HelloAck {         // helper -> game
    uint32_t magic, version;
    int32_t  ok;             // 1 = D3D12 device on that adapter and NGX/FSR both up
    uint32_t ngxResult;      // NVSDK_NGX_Result of init / the capability query
    int32_t  dlssAvailable;  // NVSDK_NGX_Parameter_SuperSampling_Available
    uint32_t driverMin[2];   // minimum driver NGX reported (major, minor), 0 when none needed
    char     adapter[128];   // the helper's adapter description, UTF-8
    char     runtime[64];    // DLSS: nvngx_dlss.dll version; FSR: provider version name
    char     offered[128];   // DLSS: presets supported; FSR: every version the runtime offers
};
struct Build {            // game -> helper, per eye, on every size/format change
    uint32_t eye;            // 0 left, 1 right
    uint32_t width, height;           // render (input) size
    uint32_t outWidth, outHeight;     // output size; equal = DLAA
    uint32_t colorFormat, outputFormat;   // DXGI_FORMAT
    int32_t  depthInverted;  // 1: larger depth is nearer (the guides pass writes reversed Z)
    int32_t  autoExposure;   // 1: NGX computes exposure (LDR input, no engine exposure value)
    int32_t  preset;         // NVSDK_NGX_DLSS_Hint_Render_Preset_*, 0 = K, 16 = per-mode K/M/L
    uint64_t tex[SlotCount]; // shared NT handles, already duplicated INTO the helper
    uint64_t fenceIn, fenceOut;
};
struct BuildAck {         // helper -> game
    uint32_t eye;
    int32_t  ok;
    uint32_t ngxResult;
    char     detail[128];    // why a build failed, UTF-8
};
struct Frame {            // game -> helper, per eye image
    uint32_t eye;
    uint64_t value;          // fence value of this image (in and out)
    uint32_t reset;          // 1: this eye's history does not belong to this image
    float    jitterX, jitterY;    // render pixels; 0 while the projection is not jittered
    float    mvScaleX, mvScaleY;  // the motion texture is in UV: scale = render size
    float    sharpness;      // 0 = none (the DLSS 2.x sharpening parameter, deprecated in 3.x+)
    uint32_t useBias;        // 1: the Bias slot holds this image's mask (R8, 1 = take the current colour)
    float    frameTimeMs;    // since this eye's previous image
    float    fovY;           // vertical field of view of the render, radians
    float    metersPerUnit;  // one depth unit in metres
};
struct FrameAck {         // helper -> game, after Signal(out, value)
    uint32_t eye;
    uint64_t value;
    int32_t  ok;             // 0: evaluate refused; out was still signalled, the output is stale
    uint32_t ngxResult;
    float    gpuMs;          // the helper's evaluate, from its own timestamps (-1 = not sampled)
};
#pragma pack(pop)

static_assert(sizeof(Hello) == 28, "Hello layout");
static_assert(sizeof(HelloAck) == 348, "HelloAck layout");
static_assert(sizeof(Build) == 96, "Build layout");
static_assert(sizeof(BuildAck) == 140, "BuildAck layout");
static_assert(sizeof(Frame) == 52, "Frame layout");
static_assert(sizeof(FrameAck) == 24, "FrameAck layout");

} // namespace d2vr::dlss_ipc

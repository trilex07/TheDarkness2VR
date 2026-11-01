// dlss/dlss_client.h - the 32-bit side of the DLSS 4.5 / FSR transport: starts the x64 NGX helper
// (src/tools/dlss_host), shares one set of textures and a fence pair per eye with it, and
// runs one eye image through its DLSS feature.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <dxgiformat.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

namespace d2vr::dlss {

// 0 info, 1 warn, 2 error
using LogFn = void (*)(int level, const char* line);

struct StartParams {
    const wchar_t* hostExe = nullptr;   // d2vr_dlss_host64.exe; nvngx_dlss.dll beside it
    const wchar_t* dataDir = nullptr;   // the helper's log and NGX's cache/logs
    LogFn log = nullptr;
    uint32_t timeoutMs = 20000;         // launch + NGX init
    uint32_t backend = 0;               // dlss_ipc::Backend: 0 DLSS (NGX), 1 FSR (FidelityFX API)
    uint32_t fsrVersion = 0;            // FSR: 0 = the runtime's default, else 1-based in its list
};

struct EyeInputs {
    ID3D11Texture2D* color = nullptr;   // render size, B8G8R8A8/R8G8B8A8 family (gamma LDR)
    ID3D11Texture2D* depth = nullptr;   // render size, R32_FLOAT, reversed (1 near, 0 far/sky)
    ID3D11Texture2D* motion = nullptr;  // render size, R16G16_FLOAT, previous UV minus current UV
    ID3D11Texture2D* bias = nullptr;    // optional, render size, R8_UNORM: 1 = take the current colour
    bool reset = false;
    float jitterX = 0, jitterY = 0;     // render pixels, already in the backend's sign (dlss_jitter.h)
    float mvSign = 1;                   // the motion texture's sign as the backend reads it
    float sharpness = 0;
    float frameTimeMs = 0, fovY = 0, metersPerUnit = 0;
    void (*afterCopy)(ID3D11DeviceContext* ctx) = nullptr;
};

struct Stats {
    uint64_t frames[2] = {}, refused[2] = {};
    double cpuMsSum[2] = {};            // submit to output-ready, present thread
    double gpuMsSum[2] = {}; uint64_t gpuN[2] = {};   // the helper's evaluate timestamps
    double cpuMsMax[2] = {};
};

class Client {
public:
    ~Client() { stop(); }
    bool start(ID3D11Device* dev, const StartParams& sp, char* why, size_t cap);
    bool build(int eye, uint32_t w, uint32_t h, uint32_t ow, uint32_t oh, DXGI_FORMAT colorFormat,
               int preset, char* why, size_t cap);
    bool evaluate(ID3D11DeviceContext* ctx, int eye, const EyeInputs& in, char* why, size_t cap);
    ID3D11ShaderResourceView* output(int eye) const;
    ID3D11Texture2D* output_texture(int eye) const;
    bool built(int eye, uint32_t w, uint32_t h, uint32_t ow, uint32_t oh, DXGI_FORMAT colorFormat) const;
    bool running() const;
    void stop();
    const char* adapter() const { return adapter_; }
    const char* runtime() const { return runtime_; }
    const char* offered() const { return offered_; }
    uint32_t backend() const { return backend_; }
    uint64_t bytes() const { return bytes_; }
    Stats stats;

private:
    struct Eye;
    bool send(const void* p, uint32_t n, uint32_t ms);
    bool recv(void* p, uint32_t n, uint32_t ms);
    void release_eye(int eye);
    void say(int level, const char* fmt, ...);
    void fail(const char* fmt, ...);

    ID3D11Device* dev_ = nullptr;
    void* dev5_ = nullptr;     // ID3D11Device5
    void* process_ = nullptr;  // HANDLE
    void* job_ = nullptr;
    void* pipe_ = nullptr;
    void* event_ = nullptr;
    Eye* eyes_[2] = {};
    LogFn log_ = nullptr;
    char adapter_[128] = "";
    char runtime_[64] = "";
    char offered_[128] = "";
    uint32_t backend_ = 0;
    uint64_t bytes_ = 0;
    bool running_ = false;
};

} // namespace d2vr::dlss

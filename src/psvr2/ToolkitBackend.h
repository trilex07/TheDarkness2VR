#pragma once

// x64 PSVR2 Toolkit loader. Adapted from KHARVOX Psvr2ToolkitBackend (MIT).
// The Toolkit DLL is loaded from the bridge directory and is not required to
// play: any failure leaves triggers off and the game client keeps running.

#include "GazeAbi.h"
#include "TriggerEffectAbi.h"
#include "TriggerPolicy.h"

#include <filesystem>
#include <windows.h>

namespace d2vr::psvr2 {

constexpr int toolkitResultOk = 0;
constexpr int toolkitResultDriverInactive = -1;
constexpr int toolkitResultNoSlot = -2;
constexpr int toolkitResultTimeout = -3;
constexpr int toolkitResultInvalidParameter = -4;

enum class BackendLoadResult {
    Ready,
    LoaderMissing,
    LoaderLoadFailed,
    LoaderExportMissing,
    CapiUnavailable,
    CapiExportMissing,
};

const char* backendLoadResultName(BackendLoadResult result);
const char* toolkitResultName(int result);

class ToolkitBackend {
public:
    ToolkitBackend() = default;
    ~ToolkitBackend();
    ToolkitBackend(const ToolkitBackend&) = delete;
    ToolkitBackend& operator=(const ToolkitBackend&) = delete;

    BackendLoadResult loadFromDirectory(const std::filesystem::path& directory);
    int initialize();
    bool driverActive(bool& callSucceeded);
    int apply(const TriggerCommand& command);
    int applyOff(Controller controller);
    // Optional export: false when the Toolkit has no gaze API or no new sample.
    bool gazeStatus(gaze_abi::Status& out, std::uint32_t timeoutMs);
    bool hasGaze() const { return gazeStatus_ != nullptr; }
    void shutdown();
    void unload();
    bool loaded() const { return capiModule_ != nullptr; }
    bool initialized() const { return initialized_; }

private:
    using GetModuleHandleFn = void* (*)();
    using InitFn = int (*)();
    using DeinitFn = void (*)();
    using GetDriverActiveFn = bool (*)();
    using SetTriggerEffectFn = int (*)(std::uint8_t, const EffectCommand&);
    using GazeStatusFn = bool (*)(void*, std::uint32_t);

    HMODULE loaderModule_{};
    HMODULE capiModule_{};
    GetModuleHandleFn getModuleHandle_{};
    InitFn init_{};
    DeinitFn deinit_{};
    GetDriverActiveFn getDriverActive_{};
    SetTriggerEffectFn setTriggerEffect_{};
    GazeStatusFn gazeStatus_{};
    bool initialized_{};
};

} // namespace d2vr::psvr2

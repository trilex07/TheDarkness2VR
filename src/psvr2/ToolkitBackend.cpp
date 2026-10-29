#include "ToolkitBackend.h"

#include <cstring>

namespace d2vr::psvr2 {

namespace {

template <typename Function>
Function loadExport(HMODULE module, const char* name) {
    return reinterpret_cast<Function>(GetProcAddress(module, name));
}

template <typename Function>
bool safeGetDriverActive(Function function, bool& succeeded) {
    __try {
        const bool active = function();
        succeeded = true;
        return active;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        succeeded = false;
        return false;
    }
}

EffectCommand toEffect(const TriggerCommand& requested) {
    EffectCommand command{};
    std::memset(&command, 0, sizeof(command));
    switch (requested.effect) {
    case TriggerEffect::Weapon:
        command.mode = EffectMode::Weapon;
        command.weapon.startPosition = requested.startPosition;
        command.weapon.endPosition = requested.endPosition;
        command.weapon.strength = requested.strength;
        break;
    case TriggerEffect::Vibration:
        command.mode = EffectMode::Vibration;
        command.vibration.position = requested.position;
        command.vibration.amplitude = requested.amplitude;
        command.vibration.frequency = requested.frequency;
        break;
    case TriggerEffect::Feedback:
        command.mode = EffectMode::Feedback;
        command.feedback.position = requested.position;
        command.feedback.strength = requested.strength;
        break;
    case TriggerEffect::SlopeFeedback:
        command.mode = EffectMode::SlopeFeedback;
        command.slope.startPosition = requested.startPosition;
        command.slope.endPosition = requested.endPosition;
        command.slope.startStrength = requested.startStrength;
        command.slope.endStrength = requested.endStrength;
        break;
    case TriggerEffect::MultiplePositionFeedback:
    case TriggerEffect::MultiplePositionVibration:
    case TriggerEffect::Off:
        command.mode = EffectMode::Off;
        break;
    }
    return command;
}

} // namespace

const char* backendLoadResultName(BackendLoadResult result) {
    switch (result) {
    case BackendLoadResult::Ready: return "ready";
    case BackendLoadResult::LoaderMissing: return "loader-missing";
    case BackendLoadResult::LoaderLoadFailed: return "loader-load-failed";
    case BackendLoadResult::LoaderExportMissing: return "loader-export-missing";
    case BackendLoadResult::CapiUnavailable: return "capi-unavailable";
    case BackendLoadResult::CapiExportMissing: return "capi-export-missing";
    }
    return "unknown";
}

const char* toolkitResultName(int result) {
    switch (result) {
    case toolkitResultOk: return "PSVR2TK_RESULT_OK";
    case toolkitResultDriverInactive: return "PSVR2TK_RESULT_DRIVER_INACTIVE";
    case toolkitResultNoSlot: return "PSVR2TK_RESULT_NO_SLOT";
    case toolkitResultTimeout: return "PSVR2TK_RESULT_TIMEOUT";
    case toolkitResultInvalidParameter: return "PSVR2TK_RESULT_INVALID_PARAMETER";
    default: return "PSVR2TK_RESULT_UNKNOWN";
    }
}

ToolkitBackend::~ToolkitBackend() {
    shutdown();
    unload();
}

BackendLoadResult ToolkitBackend::loadFromDirectory(
    const std::filesystem::path& directory) {
    unload();
    std::error_code error;
    const auto absoluteDirectory = std::filesystem::absolute(directory, error);
    if (error) return BackendLoadResult::LoaderMissing;
    const auto path = absoluteDirectory / L"psvr2_toolkit_capi_loader.dll";
    if (!std::filesystem::is_regular_file(path))
        return BackendLoadResult::LoaderMissing;

    loaderModule_ = LoadLibraryExW(path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!loaderModule_) return BackendLoadResult::LoaderLoadFailed;
    getModuleHandle_ = loadExport<GetModuleHandleFn>(
        loaderModule_, "psvr2_toolkit_loader_get_module_handle");
    if (!getModuleHandle_) {
        unload();
        return BackendLoadResult::LoaderExportMissing;
    }
    capiModule_ = static_cast<HMODULE>(getModuleHandle_());
    if (!capiModule_) {
        unload();
        return BackendLoadResult::CapiUnavailable;
    }
    init_ = loadExport<InitFn>(capiModule_, "psvr2_toolkit_init");
    deinit_ = loadExport<DeinitFn>(capiModule_, "psvr2_toolkit_deinit");
    getDriverActive_ = loadExport<GetDriverActiveFn>(
        capiModule_, "psvr2_toolkit_get_driver_active");
    setTriggerEffect_ = loadExport<SetTriggerEffectFn>(
        capiModule_, "psvr2_toolkit_set_trigger_effect");
    if (!init_ || !deinit_ || !getDriverActive_ || !setTriggerEffect_) {
        unload();
        return BackendLoadResult::CapiExportMissing;
    }
    return BackendLoadResult::Ready;
}

int ToolkitBackend::initialize() {
    if (!init_) return toolkitResultInvalidParameter;
    int result = toolkitResultInvalidParameter;
    __try { result = init_(); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        result = toolkitResultInvalidParameter;
    }
    initialized_ = result == toolkitResultOk;
    return result;
}

bool ToolkitBackend::driverActive(bool& callSucceeded) {
    callSucceeded = false;
    if (!initialized_ || !getDriverActive_) return false;
    return safeGetDriverActive(getDriverActive_, callSucceeded);
}

int ToolkitBackend::applyOff(Controller controller) {
    if (!initialized_ || !setTriggerEffect_)
        return toolkitResultInvalidParameter;
    EffectCommand command{};
    std::memset(&command, 0, sizeof(command));
    command.mode = EffectMode::Off;
    int result = toolkitResultInvalidParameter;
    __try {
        result = setTriggerEffect_(static_cast<std::uint8_t>(controller), command);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = toolkitResultInvalidParameter;
    }
    return result;
}

int ToolkitBackend::apply(const TriggerCommand& requested) {
    if (!initialized_ || !setTriggerEffect_ || !validTriggerCommand(requested))
        return toolkitResultInvalidParameter;
    if (requested.effect == TriggerEffect::Off)
        return applyOff(requested.hand == TriggerHand::Left
            ? Controller::Left : Controller::Right);
    const EffectCommand command = toEffect(requested);
    const auto controller = static_cast<std::uint8_t>(
        requested.hand == TriggerHand::Left ? Controller::Left : Controller::Right);
    int result = toolkitResultInvalidParameter;
    __try { result = setTriggerEffect_(controller, command); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        result = toolkitResultInvalidParameter;
    }
    return result;
}

void ToolkitBackend::shutdown() {
    if (!initialized_) return;
    if (setTriggerEffect_) {
        applyOff(Controller::Left);
        applyOff(Controller::Right);
    }
    if (deinit_) {
        __try { deinit_(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    initialized_ = false;
}

void ToolkitBackend::unload() {
    shutdown();
    init_ = nullptr;
    deinit_ = nullptr;
    getDriverActive_ = nullptr;
    setTriggerEffect_ = nullptr;
    getModuleHandle_ = nullptr;
    if (capiModule_ && capiModule_ != loaderModule_)
        FreeLibrary(capiModule_);
    capiModule_ = nullptr;
    if (loaderModule_) FreeLibrary(loaderModule_);
    loaderModule_ = nullptr;
}

} // namespace d2vr::psvr2

#include "../../src/psvr2/ToolkitBackend.h"

#include <iostream>

namespace {
int failures = 0;
void require(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "FAILED: " << name << '\n';
        ++failures;
    }
}
void setEnvironment(const wchar_t* name, const wchar_t* value) {
    SetEnvironmentVariableW(name, value);
}
}

int wmain(int argc, wchar_t** argv) {
    using namespace d2vr::psvr2;
    if (argc != 4) return 2;
    const std::filesystem::path complete = argv[1];
    const std::filesystem::path incomplete = argv[2];
    const std::filesystem::path missing = argv[3];

    {
        ToolkitBackend backend;
        require(backend.loadFromDirectory(missing) == BackendLoadResult::LoaderMissing,
            "loader missing");
    }
    setEnvironment(L"D2VR_FAKE_PSVR2_MODULE", L"0");
    {
        ToolkitBackend backend;
        require(backend.loadFromDirectory(complete) == BackendLoadResult::CapiUnavailable,
            "loader returns no module");
    }
    setEnvironment(L"D2VR_FAKE_PSVR2_MODULE", L"1");
    {
        ToolkitBackend backend;
        require(backend.loadFromDirectory(incomplete) == BackendLoadResult::CapiExportMissing,
            "required export missing");
    }

    HMODULE fake = LoadLibraryW((complete / L"psvr2_toolkit_capi_loader.dll").c_str());
    if (!fake) return 3;
    const auto reset = reinterpret_cast<void(*)()>(GetProcAddress(fake, "fake_psvr2_reset"));
    const auto get = reinterpret_cast<int(*)(int)>(GetProcAddress(fake, "fake_psvr2_get"));
    if (!reset || !get) return 4;

    setEnvironment(L"D2VR_FAKE_PSVR2_INIT_RESULT", L"0");
    setEnvironment(L"D2VR_FAKE_PSVR2_DRIVER_ACTIVE", L"1");
    setEnvironment(L"D2VR_FAKE_PSVR2_SET_RESULT", L"0");
    reset();
    {
        ToolkitBackend backend;
        require(backend.loadFromDirectory(complete) == BackendLoadResult::Ready
            && backend.initialize() == toolkitResultOk, "ready");
        const auto shotgun = weaponCommand(TriggerHand::Right, 2, 4, 7);
        require(backend.apply(shotgun) == toolkitResultOk, "weapon apply");
        require(get(4) == static_cast<int>(Controller::Right)
            && get(5) == static_cast<int>(EffectMode::Weapon)
            && get(6) == 2 && get(7) == 4 && get(8) == 7, "weapon mode is resistance");
        require(get(5) != static_cast<int>(EffectMode::Vibration), "weapon is not vibration");
        const auto vibration = vibrationCommand(TriggerHand::Left, 3, 7, 55);
        require(backend.apply(vibration) == toolkitResultOk, "vibration apply");
        require(get(4) == static_cast<int>(Controller::Left)
            && get(5) == static_cast<int>(EffectMode::Vibration)
            && get(6) == 3 && get(7) == 7 && get(8) == 55, "fire vibration params");
        require(get(24) == static_cast<int>(EffectMode::Weapon)
            && get(25) == 2 && get(26) == 4 && get(27) == 7,
            "right weapon preserved while left fires");
        const auto slope = slopeFeedbackCommand(TriggerHand::Left, 2, 8, 3, 8);
        require(backend.apply(slope) == toolkitResultOk, "slope apply");
        require(get(5) == static_cast<int>(EffectMode::SlopeFeedback)
            && get(6) == 2 && get(7) == 8 && get(8) == 8, "slope end strength");
        backend.shutdown();
        require(get(3) == 2, "shutdown releases both triggers");
        require(get(1) == 1, "deinit");
    }

    setEnvironment(L"D2VR_FAKE_PSVR2_DRIVER_ACTIVE", L"0");
    {
        ToolkitBackend backend;
        require(backend.loadFromDirectory(complete) == BackendLoadResult::Ready
            && backend.initialize() == 0, "inactive setup");
        bool succeeded = false;
        require(!backend.driverActive(succeeded) && succeeded, "driver inactive is not a crash");
    }
    FreeLibrary(fake);
    return failures == 0 ? 0 : 1;
}

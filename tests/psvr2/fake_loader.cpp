#include "../../src/psvr2/TriggerEffectAbi.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {
std::atomic<int> initCalls{};
std::atomic<int> deinitCalls{};
std::atomic<int> setCalls{};
std::atomic<int> offCalls{};
std::atomic<int> lastController{-1};
std::atomic<int> lastMode{-1};
std::atomic<int> lastA{};
std::atomic<int> lastB{};
std::atomic<int> lastC{};
std::atomic<int> handMode[2]{};
std::atomic<int> handA[2]{};
std::atomic<int> handB[2]{};
std::atomic<int> handC[2]{};
std::atomic<int> handSets[2]{};

int environmentInt(const wchar_t* name, int fallback) {
    wchar_t buffer[32]{};
    if (!GetEnvironmentVariableW(name, buffer, 32)) return fallback;
    wchar_t* end = nullptr;
    const long value = wcstol(buffer, &end, 10);
    return end && *end == L'\0' ? static_cast<int>(value) : fallback;
}

void writeStatus() {
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"D2VR_FAKE_PSVR2_STATUS", path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return;
    int record[12]{};
    for (int hand = 0; hand < 2; ++hand) {
        record[hand * 4 + 0] = handMode[hand].load();
        record[hand * 4 + 1] = handA[hand].load();
        record[hand * 4 + 2] = handB[hand].load();
        record[hand * 4 + 3] = handC[hand].load();
    }
    record[8] = handSets[0].load();
    record[9] = handSets[1].load();
    record[10] = setCalls.load();
    record[11] = offCalls.load();
    FILE* file = nullptr;
    if (_wfopen_s(&file, path, L"wb") != 0 || !file) return;
    fwrite(record, sizeof(record), 1, file);
    fflush(file);
    fclose(file);
}
}

extern "C" __declspec(dllexport) void fake_psvr2_reset() {
    initCalls = 0;
    deinitCalls = 0;
    setCalls = 0;
    offCalls = 0;
    lastController = -1;
    lastMode = -1;
    lastA = 0;
    lastB = 0;
    lastC = 0;
    for (int hand = 0; hand < 2; ++hand) {
        handMode[hand].store(0);
        handA[hand].store(0);
        handB[hand].store(0);
        handC[hand].store(0);
        handSets[hand].store(0);
    }
}

extern "C" __declspec(dllexport) int fake_psvr2_get(int key) {
    switch (key) {
    case 0: return initCalls;
    case 1: return deinitCalls;
    case 2: return setCalls;
    case 3: return offCalls;
    case 4: return lastController;
    case 5: return lastMode;
    case 6: return lastA;
    case 7: return lastB;
    case 8: return lastC;
    default: break;
    }
    if (key >= 20 && key < 28) {
        const int hand = key >= 24 ? 1 : 0;
        const int field = (key - 20) % 4;
        if (field == 0) return handMode[hand];
        if (field == 1) return handA[hand];
        if (field == 2) return handB[hand];
        return handC[hand];
    }
    return -1;
}

extern "C" __declspec(dllexport) void* psvr2_toolkit_loader_get_module_handle() {
    if (environmentInt(L"D2VR_FAKE_PSVR2_MODULE", 1) == 0) return nullptr;
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
        | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&psvr2_toolkit_loader_get_module_handle), &module);
    return module;
}

extern "C" __declspec(dllexport) int psvr2_toolkit_init() {
    ++initCalls;
    return environmentInt(L"D2VR_FAKE_PSVR2_INIT_RESULT", 0);
}

extern "C" __declspec(dllexport) void psvr2_toolkit_deinit() { ++deinitCalls; }

extern "C" __declspec(dllexport) bool psvr2_toolkit_get_driver_active() {
    return environmentInt(L"D2VR_FAKE_PSVR2_DRIVER_ACTIVE", 1) != 0;
}

extern "C" __declspec(dllexport) int psvr2_toolkit_set_trigger_effect(
    std::uint8_t controller, const d2vr::psvr2::EffectCommand& command) {
    using namespace d2vr::psvr2;
    ++setCalls;
    const int hand = controller == static_cast<std::uint8_t>(Controller::Left) ? 0 : 1;
    lastController = controller;
    lastMode = static_cast<int>(command.mode);
    int a = 0, b = 0, c = 0;
    if (command.mode == EffectMode::Weapon) {
        a = command.weapon.startPosition;
        b = command.weapon.endPosition;
        c = command.weapon.strength;
    } else if (command.mode == EffectMode::Vibration) {
        a = command.vibration.position;
        b = command.vibration.amplitude;
        c = command.vibration.frequency;
    } else if (command.mode == EffectMode::Feedback) {
        a = command.feedback.position;
        b = command.feedback.strength;
    } else if (command.mode == EffectMode::SlopeFeedback) {
        a = command.slope.startPosition;
        b = command.slope.endPosition;
        c = command.slope.startStrength;
        lastC = command.slope.endStrength;
    }
    if (command.mode != EffectMode::SlopeFeedback) lastC = c;
    lastA = a;
    lastB = b;
    handMode[hand].store(static_cast<int>(command.mode));
    handA[hand] = a;
    handB[hand] = b;
    handC[hand] = command.mode == EffectMode::SlopeFeedback
        ? command.slope.endStrength : c;
    ++handSets[hand];
    if (command.mode == EffectMode::Off) ++offCalls;
    writeStatus();
    return environmentInt(L"D2VR_FAKE_PSVR2_SET_RESULT", 0);
}

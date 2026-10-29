#include "../../src/psvr2/IpcClient.h"
#include "../../src/psvr2/TriggerEffectAbi.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {
bool loadStatus(const wchar_t* path, int record[12]) {
    FILE* file = nullptr;
    if (_wfopen_s(&file, path, L"rb") != 0 || !file) return false;
    const auto read = fread(record, sizeof(int), 12, file);
    fclose(file);
    return read == 12;
}
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    const wchar_t* bridgeExe = argv[1];
    const wchar_t* statusPath = argv[2];
    DeleteFileW(statusPath);
    const DWORD pid = GetCurrentProcessId();
    wchar_t pidText[16]{};
    swprintf_s(pidText, L"%lu", pid);
    SetEnvironmentVariableW(L"D2VR_USE_PSVR2_TOOLKIT", L"1");
    SetEnvironmentVariableW(L"D2VR_PSVR2_PIPE_NAME", L"D2VRPsvr2Loopback");
    SetEnvironmentVariableW(L"D2VR_PSVR2_SESSION_TOKEN", L"a1b2c3d4");
    SetEnvironmentVariableW(L"D2VR_PSVR2_PARENT_PID", pidText);
    SetEnvironmentVariableW(L"D2VR_FAKE_PSVR2_STATUS", statusPath);
    SetEnvironmentVariableW(L"D2VR_FAKE_PSVR2_MODULE", L"1");
    SetEnvironmentVariableW(L"D2VR_FAKE_PSVR2_INIT_RESULT", L"0");
    SetEnvironmentVariableW(L"D2VR_FAKE_PSVR2_DRIVER_ACTIVE", L"1");
    SetEnvironmentVariableW(L"D2VR_FAKE_PSVR2_SET_RESULT", L"0");

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::wstring command = L"\"";
    command += bridgeExe;
    command += L"\"";
    if (!CreateProcessW(bridgeExe, command.data(), nullptr, nullptr, FALSE,
            0, nullptr, nullptr, &startup, &process)) {
        std::fwprintf(stderr, L"bridge launch failed %lu\n", GetLastError());
        return 3;
    }

    using namespace d2vr::psvr2;
    if (ClientStart() == ClientStartResult::Disabled) {
        TerminateProcess(process.hProcess, 1);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        std::fputs("client disabled\n", stderr);
        return 4;
    }
    Submit(weaponCommand(TriggerHand::Left, 4, 6, 3));
    Submit(weaponCommand(TriggerHand::Right, 2, 4, 7));

    int record[12]{};
    bool weapons = false;
    for (int attempt = 0; attempt < 80 && !weapons; ++attempt) {
        Sleep(100);
        if (!loadStatus(statusPath, record)) continue;
        const bool left = record[0] == static_cast<int>(d2vr::psvr2::EffectMode::Weapon)
            && record[1] == 4 && record[2] == 6 && record[3] == 3;
        const bool right = record[4] == static_cast<int>(d2vr::psvr2::EffectMode::Weapon)
            && record[5] == 2 && record[6] == 4 && record[7] == 7;
        weapons = left && right;
    }
    Submit(vibrationCommand(TriggerHand::Left, 3, 7, 55));
    bool independent = false;
    if (weapons) {
        for (int attempt = 0; attempt < 40 && !independent; ++attempt) {
            Sleep(100);
            if (!loadStatus(statusPath, record)) continue;
            const bool leftFire = record[0] == static_cast<int>(d2vr::psvr2::EffectMode::Vibration)
                && record[1] == 3 && record[2] == 7 && record[3] == 55;
            const bool rightHeld = record[4] == static_cast<int>(d2vr::psvr2::EffectMode::Weapon)
                && record[5] == 2 && record[6] == 4 && record[7] == 7;
            independent = leftFire && rightHeld;
        }
    }
    SubmitBothOff(TriggerOffReason::SessionEnd);
    ClientStop();
    WaitForSingleObject(process.hProcess, 5000);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (!weapons || !independent) {
        std::fprintf(stderr, "loopback weapons=%d independent=%d status=%d %d %d %d | %d %d %d %d\n",
            weapons ? 1 : 0, independent ? 1 : 0,
            record[0], record[1], record[2], record[3],
            record[4], record[5], record[6], record[7]);
        return 5;
    }
    std::puts("PASS");
    return 0;
}

#pragma once

// x86 game-side client. Submit() only exchanges two packed commands.
// The worker thread owns the pipe. Adapted from the KHARVOX IPC client and
// the SingularityVR non-blocking submit shape (both MIT).

#include "IpcProtocol.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib,"bcrypt.lib")

#include <atomic>
#include <cctype>
#include <cstdint>
#include <cwchar>

namespace d2vr::psvr2 {

inline unsigned BehaviorVtable(const void* pointer) {
    __try {return pointer?*static_cast<const unsigned*>(pointer):0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}

inline unsigned char ObserveFirearm(const void* gun) {
    __try {
        if (!gun) return 0;
        const auto* bytes = static_cast<const unsigned char*>(gun);
        if (*reinterpret_cast<const unsigned*>(bytes) != 0x1010E30u) return 0;
        const unsigned mode = *reinterpret_cast<const unsigned*>(bytes + 0xA78);
        if (mode >= 4) return 0;
        const auto parts = *reinterpret_cast<const unsigned char* const*>(bytes + 0xA64);
        if (!parts) return 0;
        const unsigned char* record = parts + static_cast<unsigned>(48 * mode);
        const auto action=**reinterpret_cast<const void* const* const*>(record+0xC);
        bool pistolFire = BehaviorVtable(action)==0x1010AE0u;
        for (int slot = 0; slot < 12; ++slot) {
            const auto object = *reinterpret_cast<const unsigned char* const*>(
                record + slot * 4);
            if (!object) continue;
            const unsigned vtable = BehaviorVtable(object);
            const Firearm kind = firearmFromBehaviorVtable(vtable);
            if (kind != Firearm::None)
                return static_cast<unsigned char>(kind);
            if (vtable == 0x1010AE0u) pistolFire = true;
        }
        if (pistolFire) return static_cast<unsigned char>(Firearm::Pistol);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return 0;
}

enum class ClientStartResult {
    Running,
    Started,
    Disabled,
};

namespace client_detail {

struct Slot {
    std::atomic<std::uint64_t> packed{0};
    std::atomic<std::uint64_t> generation{0};
};

inline Slot g_slots[2]{};
inline std::atomic<int> g_state{0}; // 0 idle, 1 running, 2 disabled, 3 starting
inline std::atomic<bool> g_stop{false};
inline HANDLE g_stopEvent = nullptr;
inline HANDLE g_thread = nullptr;
inline wchar_t g_pipePath[160]{};
inline char g_token[ipcMaximumTokenBytes + 1]{};

inline bool hexToken(const char* token) {
    if (!token || !token[0]) return false;
    std::size_t length = 0;
    for (const char* cursor = token; *cursor; ++cursor, ++length) {
        if (length >= ipcMaximumTokenBytes) return false;
        if (!std::isxdigit(static_cast<unsigned char>(*cursor))) return false;
    }
    return length > 0;
}

inline bool pipeSuffix(const wchar_t* name) {
    if (!name || !name[0]) return false;
    std::size_t length = 0;
    for (const wchar_t* cursor = name; *cursor; ++cursor, ++length) {
        if (length > 128) return false;
        const wchar_t character = *cursor;
        const bool ok = (character >= L'a' && character <= L'z')
            || (character >= L'A' && character <= L'Z')
            || (character >= L'0' && character <= L'9')
            || character == L'_' || character == L'-';
        if (!ok) return false;
    }
    return true;
}

inline bool writeTimeout(HANDLE pipe, const void* data, DWORD bytes) {
    OVERLAPPED overlapped{};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent) return false;
    DWORD written = 0;
    bool success = WriteFile(pipe, data, bytes, &written, &overlapped) != FALSE;
    if (!success && GetLastError() == ERROR_IO_PENDING) {
        const HANDLE waits[]{overlapped.hEvent, g_stopEvent};
        const DWORD wait = WaitForMultipleObjects(2, waits, FALSE, 250);
        if (wait == WAIT_OBJECT_0)
            success = GetOverlappedResult(pipe, &overlapped, &written, FALSE) != FALSE;
        else {
            CancelIoEx(pipe, &overlapped);
            // OVERLAPPED and its buffer must outlive cancellation completion.
            GetOverlappedResult(pipe, &overlapped, &written, TRUE);
            success = false;
        }
    }
    CloseHandle(overlapped.hEvent);
    return success && written == bytes;
}

inline bool writeMessage(HANDLE pipe, const EncodedIpcMessage& message) {
    return message.size != 0
        && writeTimeout(pipe, message.bytes.data(), static_cast<DWORD>(message.size));
}

inline HANDLE connectPipe() {
    while (!g_stop.load(std::memory_order_acquire)) {
        if (WaitNamedPipeW(g_pipePath, 250)) {
            HANDLE pipe = CreateFileW(g_pipePath, GENERIC_WRITE, 0, nullptr,
                OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
            if (pipe != INVALID_HANDLE_VALUE) return pipe;
        }
        if (g_stopEvent && WaitForSingleObject(g_stopEvent, 250) == WAIT_OBJECT_0)
            break;
    }
    return nullptr;
}

inline void storeOff(TriggerHand hand, TriggerOffReason reason) {
    const int index = hand == TriggerHand::Left ? 0 : 1;
    const auto packed = packTriggerCommand(offCommand(hand, reason));
    if (g_slots[index].packed.exchange(packed, std::memory_order_acq_rel) != packed)
        g_slots[index].generation.fetch_add(1, std::memory_order_release);
}

inline DWORD WINAPI workerMain(void*) {
    std::uint32_t sequence = 0;
    HANDLE pipe = nullptr;
    bool hello = false;
    std::uint64_t sent[2]{};
    while (!g_stop.load(std::memory_order_acquire)) {
        if (!pipe) {
            pipe = connectPipe();
            hello = false;
            sent[0] = sent[1] = 0;
            sequence = 0;
            if (!pipe) break;
        }
        if (!hello) {
            const auto message = encodeHello(++sequence, GetTickCount64(), g_token);
            if (!writeMessage(pipe, message)) {
                CloseHandle(pipe);
                pipe = nullptr;
                continue;
            }
            hello = true;
        }
        bool wrote = false;
        for (int hand = 0; hand < 2; ++hand) {
            const auto generation = g_slots[hand].generation.load(std::memory_order_acquire);
            if (generation == sent[hand]) continue;
            const auto command = unpackTriggerCommand(
                g_slots[hand].packed.load(std::memory_order_acquire));
            if (!writeMessage(pipe, encodeTriggerState(++sequence, GetTickCount64(), command))) {
                CloseHandle(pipe);
                pipe = nullptr;
                wrote = false;
                break;
            }
            sent[hand] = generation;
            wrote = true;
        }
        if (!pipe) continue;
        if (g_stop.load(std::memory_order_acquire)) break;
        if (!wrote && g_stopEvent)
            WaitForSingleObject(g_stopEvent, 50);
    }
    if (pipe && hello) {
        for (int hand = 0; hand < 2; ++hand) {
            const auto command = unpackTriggerCommand(
                g_slots[hand].packed.load(std::memory_order_acquire));
            writeMessage(pipe, encodeTriggerState(++sequence, GetTickCount64(), command));
        }
        writeMessage(pipe, encodeShutdown(++sequence, GetTickCount64()));
    }
    if (pipe) CloseHandle(pipe);
    return 0;
}

} // namespace client_detail

inline bool ClientRunning() {
    return client_detail::g_state.load(std::memory_order_acquire) == 1;
}

// Called from input sync, never DllMain. The authenticated x64 helper owns the
// Toolkit ABI and exits with this process. No persistent Steam launch options.
inline bool StartOwnedBridge() {
    wchar_t directory[MAX_PATH]{},path[MAX_PATH]{};
    const DWORD size=GetEnvironmentVariableW(L"LOCALAPPDATA",directory,MAX_PATH);
    if(!size || size>=MAX_PATH-70)return false;
    lstrcatW(directory,L"\\Darkness2VR\\runtime\\psvr2");
    lstrcpyW(path,directory);lstrcatW(path,L"\\Darkness2Psvr2Bridge.exe");
    if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES)return false;
    unsigned char random[16]{};
    if(BCryptGenRandom(nullptr,random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)return false;
    char token[33]{};const char hex[]="0123456789abcdef";
    for(unsigned i=0;i<16;++i){token[2*i]=hex[random[i]>>4];token[2*i+1]=hex[random[i]&15];}
    wchar_t suffix[80]{},parent[16]{},command[MAX_PATH+4]{};
    swprintf_s(parent,L"%lu",GetCurrentProcessId());
    swprintf_s(suffix,L"D2VR_PSVR2_%lu_%02x%02x%02x%02x",GetCurrentProcessId(),random[0],random[1],random[2],random[3]);
    if(!SetEnvironmentVariableW(L"D2VR_PSVR2_PIPE_NAME",suffix) ||
       !SetEnvironmentVariableA("D2VR_PSVR2_SESSION_TOKEN",token) ||
       !SetEnvironmentVariableW(L"D2VR_PSVR2_PARENT_PID",parent))return false;
    swprintf_s(command,L"\"%s\"",path);
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(path,command,nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,directory,&startup,&process))return false;
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
    return SetEnvironmentVariableA("D2VR_USE_PSVR2_TOOLKIT","1")!=FALSE;
}

inline ClientStartResult ClientStart() {
    using namespace client_detail;
    int state = g_state.load(std::memory_order_acquire);
    if (state == 1) return ClientStartResult::Running;
    if (state == 2) return ClientStartResult::Disabled;
    int expected = 0;
    if (!g_state.compare_exchange_strong(expected, 3, std::memory_order_acq_rel))
        return g_state.load(std::memory_order_acquire) == 1
            ? ClientStartResult::Running : ClientStartResult::Disabled;

    char external[8]{};
    if(!GetEnvironmentVariableA("D2VR_USE_PSVR2_TOOLKIT",external,sizeof(external)))StartOwnedBridge();

    char enabled[8]{};
    wchar_t suffix[129]{};
    char token[ipcMaximumTokenBytes + 1]{};
    const DWORD enabledBytes = GetEnvironmentVariableA(
        "D2VR_USE_PSVR2_TOOLKIT", enabled, static_cast<DWORD>(sizeof(enabled)));
    const DWORD suffixBytes = GetEnvironmentVariableW(
        L"D2VR_PSVR2_PIPE_NAME", suffix, static_cast<DWORD>(sizeof(suffix) / sizeof(suffix[0])));
    const DWORD tokenBytes = GetEnvironmentVariableA(
        "D2VR_PSVR2_SESSION_TOKEN", token, static_cast<DWORD>(sizeof(token)));
    const bool configured = enabledBytes == 1 && enabled[0] == '1'
        && suffixBytes > 0 && suffixBytes < 129 && pipeSuffix(suffix)
        && tokenBytes > 0 && tokenBytes < sizeof(token) && hexToken(token);
    if (!configured) {
        g_state.store(2, std::memory_order_release);
        return ClientStartResult::Disabled;
    }

    lstrcpyW(g_pipePath, L"\\\\.\\pipe\\");
    lstrcatW(g_pipePath, suffix);
    lstrcpyA(g_token, token);
    g_stop.store(false, std::memory_order_release);
    g_slots[0].packed.store(packTriggerCommand(
        offCommand(TriggerHand::Left, TriggerOffReason::SessionEnd)),
        std::memory_order_relaxed);
    g_slots[1].packed.store(packTriggerCommand(
        offCommand(TriggerHand::Right, TriggerOffReason::SessionEnd)),
        std::memory_order_relaxed);
    g_slots[0].generation.store(1, std::memory_order_relaxed);
    g_slots[1].generation.store(1, std::memory_order_relaxed);
    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) {
        g_state.store(2, std::memory_order_release);
        return ClientStartResult::Disabled;
    }
    g_thread = CreateThread(nullptr, 0, workerMain, nullptr, 0, nullptr);
    if (!g_thread) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
        g_state.store(2, std::memory_order_release);
        return ClientStartResult::Disabled;
    }
    g_state.store(1, std::memory_order_release);
    return ClientStartResult::Started;
}

inline void Submit(const TriggerCommand& command) {
    using namespace client_detail;
    if (g_state.load(std::memory_order_relaxed) != 1) return;
    if (!validTriggerCommand(command)) return;
    const int index = command.hand == TriggerHand::Left ? 0 : 1;
    const auto packed = packTriggerCommand(command);
    if (g_slots[index].packed.exchange(packed, std::memory_order_acq_rel) != packed)
        g_slots[index].generation.fetch_add(1, std::memory_order_release);
}

inline void SubmitBothOff(TriggerOffReason reason) {
    Submit(offCommand(TriggerHand::Left, reason));
    Submit(offCommand(TriggerHand::Right, reason));
}

inline void ClientRequestStop() {
    using namespace client_detail;
    if (g_state.load(std::memory_order_acquire) != 1) return;
    storeOff(TriggerHand::Left, TriggerOffReason::SessionEnd);
    storeOff(TriggerHand::Right, TriggerOffReason::SessionEnd);
    g_stop.store(true, std::memory_order_release);
    if (g_stopEvent) SetEvent(g_stopEvent);
}

inline void ClientStop() {
    ClientRequestStop();
    if (client_detail::g_thread)
        WaitForSingleObject(client_detail::g_thread, 3000);
}

} // namespace d2vr::psvr2

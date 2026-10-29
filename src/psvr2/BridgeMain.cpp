// x64 bridge: named pipe in, PSVR2 Toolkit adaptive triggers out.
// Dual-hand delivery is independent. A left command never clears the right
// trigger. Adapted from the KHARVOX bridge (MIT, CactusVRStudios).

#include "IpcProtocol.h"
#include "ToolkitBackend.h"

#include <windows.h>
#include <aclapi.h>

#include <atomic>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::atomic<bool> shutdownRequested{};
std::atomic<std::uint64_t> desiredPacked[2]{};
HANDLE shutdownEvent = nullptr;

std::filesystem::path bridgeDirectory() {
    wchar_t path[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, 32768);
    if (length == 0 || length >= 32768) return {};
    return std::filesystem::path(path).parent_path();
}

std::filesystem::path logPath() {
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetTempPathW(MAX_PATH, path);
    if (length == 0 || length >= MAX_PATH) return {};
    return std::filesystem::path(path) / L"Darkness2VR-psvr2.log";
}

void writeLog(std::string_view message) noexcept {
    try {
        const auto path = logPath();
        if (path.empty()) return;
        if (std::filesystem::exists(path) && std::filesystem::file_size(path) >= 256 * 1024)
            std::ofstream(path, std::ios::trunc).close();
        std::ofstream stream(path, std::ios::app);
        stream << '[' << GetTickCount64() << "] " << message << '\n';
    } catch (...) {
    }
}

std::wstring readWideEnvironment(const wchar_t* name, std::size_t maximum) {
    const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
    if (required <= 1 || required > maximum + 1) return {};
    std::wstring value(required, L'\0');
    const DWORD copied = GetEnvironmentVariableW(name, value.data(), required);
    if (copied != required - 1) return {};
    value.resize(copied);
    return value;
}

std::string readAsciiEnvironment(const wchar_t* name, std::size_t maximum) {
    const auto wide = readWideEnvironment(name, maximum);
    if (wide.empty()) return {};
    std::string result;
    result.reserve(wide.size());
    for (const auto character : wide) {
        if (character < 0x20 || character > 0x7e) return {};
        result.push_back(static_cast<char>(character));
    }
    return result;
}

bool validPipeName(const std::wstring& name) {
    if (name.empty() || name.size() > 128) return false;
    for (const auto character : name) {
        const bool ok = (character >= L'a' && character <= L'z')
            || (character >= L'A' && character <= L'Z')
            || (character >= L'0' && character <= L'9')
            || character == L'_' || character == L'-';
        if (!ok) return false;
    }
    return true;
}

DWORD readParentProcessId() {
    const auto text = readAsciiEnvironment(L"D2VR_PSVR2_PARENT_PID", 16);
    DWORD value = 0;
    const auto conversion = std::from_chars(text.data(), text.data() + text.size(), value);
    return conversion.ec == std::errc{} && conversion.ptr == text.data() + text.size()
        ? value : 0;
}

struct CurrentUserSecurity {
    std::vector<std::uint8_t> tokenUserBuffer;
    PACL acl{};
    SECURITY_DESCRIPTOR descriptor{};
    SECURITY_ATTRIBUTES attributes{};

    ~CurrentUserSecurity() { if (acl) LocalFree(acl); }

    bool initialize() {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
        DWORD bytes = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &bytes);
        tokenUserBuffer.resize(bytes);
        const bool ready = bytes != 0 && GetTokenInformation(
            token, TokenUser, tokenUserBuffer.data(), bytes, &bytes) != FALSE;
        CloseHandle(token);
        if (!ready) return false;
        const auto* tokenUser = reinterpret_cast<const TOKEN_USER*>(tokenUserBuffer.data());
        EXPLICIT_ACCESSW access{};
        access.grfAccessPermissions = GENERIC_READ | GENERIC_WRITE;
        access.grfAccessMode = SET_ACCESS;
        access.grfInheritance = NO_INHERITANCE;
        access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        access.Trustee.TrusteeType = TRUSTEE_IS_USER;
        access.Trustee.ptstrName = static_cast<LPWSTR>(tokenUser->User.Sid);
        if (SetEntriesInAclW(1, &access, nullptr, &acl) != ERROR_SUCCESS) return false;
        if (!InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION)
            || !SetSecurityDescriptorOwner(&descriptor, tokenUser->User.Sid, FALSE)
            || !SetSecurityDescriptorDacl(&descriptor, TRUE, acl, FALSE))
            return false;
        attributes.nLength = sizeof(attributes);
        attributes.lpSecurityDescriptor = &descriptor;
        attributes.bInheritHandle = FALSE;
        return true;
    }
};

bool parentStillAlive(HANDLE parent) {
    return parent && WaitForSingleObject(parent, 0) == WAIT_TIMEOUT;
}

void publishOffBoth() {
    using namespace d2vr::psvr2;
    desiredPacked[0].store(packTriggerCommand(
        offCommand(TriggerHand::Left, TriggerOffReason::SessionEnd)),
        std::memory_order_release);
    desiredPacked[1].store(packTriggerCommand(
        offCommand(TriggerHand::Right, TriggerOffReason::SessionEnd)),
        std::memory_order_release);
}

HANDLE createAndConnectPipe(const std::wstring& path, SECURITY_ATTRIBUTES* security, HANDLE parent) {
    HANDLE pipe = CreateNamedPipeW(path.c_str(),
        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, 0, 4096, 1000, security);
    if (pipe == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;
    OVERLAPPED overlapped{};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent) {
        CloseHandle(pipe);
        return INVALID_HANDLE_VALUE;
    }
    const BOOL connected = ConnectNamedPipe(pipe, &overlapped);
    if (connected || GetLastError() == ERROR_PIPE_CONNECTED)
        SetEvent(overlapped.hEvent);
    else if (GetLastError() != ERROR_IO_PENDING) {
        CloseHandle(overlapped.hEvent);
        CloseHandle(pipe);
        return INVALID_HANDLE_VALUE;
    }
    while (parentStillAlive(parent) && !shutdownRequested.load(std::memory_order_acquire)) {
        const HANDLE waits[]{overlapped.hEvent, parent, shutdownEvent};
        const DWORD wait = WaitForMultipleObjects(3, waits, FALSE, 250);
        if (wait == WAIT_OBJECT_0) {
            DWORD ignored = 0;
            const bool ready = GetOverlappedResult(pipe, &overlapped, &ignored, FALSE) != FALSE
                || GetLastError() == ERROR_PIPE_CONNECTED;
            CloseHandle(overlapped.hEvent);
            if (ready) return pipe;
            CloseHandle(pipe);
            return INVALID_HANDLE_VALUE;
        }
        if (wait == WAIT_OBJECT_0 + 1 || wait == WAIT_OBJECT_0 + 2) break;
    }
    CancelIoEx(pipe, &overlapped);
    DWORD cancelledBytes=0;
    GetOverlappedResult(pipe, &overlapped, &cancelledBytes, TRUE);
    CloseHandle(overlapped.hEvent);
    CloseHandle(pipe);
    return INVALID_HANDLE_VALUE;
}

bool readExact(HANDLE pipe, HANDLE parent, void* destination, DWORD bytes) {
    auto* output = static_cast<std::uint8_t*>(destination);
    DWORD total = 0;
    while (total < bytes && parentStillAlive(parent)
        && !shutdownRequested.load(std::memory_order_acquire)) {
        OVERLAPPED overlapped{};
        overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!overlapped.hEvent) return false;
        DWORD read = 0;
        bool success = ReadFile(pipe, output + total, bytes - total, &read, &overlapped) != FALSE;
        if (!success && GetLastError() == ERROR_IO_PENDING) {
            const HANDLE waits[]{overlapped.hEvent, parent, shutdownEvent};
            const DWORD wait = WaitForMultipleObjects(3, waits, FALSE, 1000);
            if (wait == WAIT_OBJECT_0)
                success = GetOverlappedResult(pipe, &overlapped, &read, FALSE) != FALSE;
            else {
                CancelIoEx(pipe, &overlapped);
                GetOverlappedResult(pipe, &overlapped, &read, TRUE);
                success = false;
            }
        }
        CloseHandle(overlapped.hEvent);
        if (!success || read == 0) return false;
        total += read;
    }
    return total == bytes;
}

bool readMessage(HANDLE pipe, HANDLE parent,
    d2vr::psvr2::EncodedIpcMessage& encoded,
    d2vr::psvr2::DecodedIpcMessage& decoded) {
    using namespace d2vr::psvr2;
    IpcMessageHeader header{};
    if (!readExact(pipe, parent, &header, sizeof(header))) return false;
    if (header.payloadBytes > ipcMaximumPayloadBytes) return false;
    encoded.size = sizeof(header) + header.payloadBytes;
    std::memcpy(encoded.bytes.data(), &header, sizeof(header));
    if (header.payloadBytes && !readExact(pipe, parent,
            encoded.bytes.data() + sizeof(header), header.payloadBytes))
        return false;
    return validateIpcMessage(encoded.bytes.data(), encoded.size, &decoded)
        == IpcValidationResult::Valid;
}

struct PipeThreadContext {
    std::wstring pipePath;
    std::string token;
    SECURITY_ATTRIBUTES* security{};
    HANDLE parent{};
};

DWORD WINAPI pipeThreadMain(void* rawContext) {
    using namespace d2vr::psvr2;
    const auto& context = *static_cast<PipeThreadContext*>(rawContext);
    while (parentStillAlive(context.parent)
        && !shutdownRequested.load(std::memory_order_acquire)) {
        HANDLE pipe = createAndConnectPipe(context.pipePath, context.security, context.parent);
        if (pipe == INVALID_HANDLE_VALUE) continue;
        ServerSessionState session{};
        bool connected = true;
        while (connected && parentStillAlive(context.parent)
            && !shutdownRequested.load(std::memory_order_acquire)) {
            EncodedIpcMessage encoded{};
            DecodedIpcMessage decoded{};
            if (!readMessage(pipe, context.parent, encoded, decoded)) break;
            switch (handleServerMessage(session, decoded, context.token)) {
            case ServerSessionAction::Authorized:
                writeLog("[PSVR2TK] client authorized");
                break;
            case ServerSessionAction::TriggerState: {
                const auto command = decodeTriggerCommand(decoded);
                const int index = command.hand == TriggerHand::Left ? 0 : 1;
                desiredPacked[index].store(packTriggerCommand(command),
                    std::memory_order_release);
                break;
            }
            case ServerSessionAction::Shutdown:
                shutdownRequested.store(true, std::memory_order_release);
                if (shutdownEvent) SetEvent(shutdownEvent);
                connected = false;
                break;
            case ServerSessionAction::Reject:
                writeLog("[PSVR2TK] client rejected");
                connected = false;
                break;
            case ServerSessionAction::None:
                break;
            }
        }
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        publishOffBoth();
        writeLog("[PSVR2TK] pipe closed; both triggers released");
    }
    return 0;
}

void logApplied(const d2vr::psvr2::TriggerCommand& command) {
    using namespace d2vr::psvr2;
    if (command.effect == TriggerEffect::Off) {
        writeLog(std::string("[PSVR2TK] ") + triggerHandName(command.hand)
            + " trigger effect off reason=" + triggerOffReasonName(command.offReason));
        return;
    }
    if (command.effect == TriggerEffect::Weapon) {
        writeLog(std::string("[PSVR2TK] ") + triggerHandName(command.hand)
            + " weapon effect start=" + std::to_string(command.startPosition)
            + " end=" + std::to_string(command.endPosition)
            + " strength=" + std::to_string(command.strength));
        return;
    }
    if (command.effect == TriggerEffect::Vibration) {
        writeLog(std::string("[PSVR2TK] ") + triggerHandName(command.hand)
            + " trigger vibration position=" + std::to_string(command.position)
            + " amplitude=" + std::to_string(command.amplitude)
            + " frequency=" + std::to_string(command.frequency));
        return;
    }
    if (command.effect == TriggerEffect::Feedback) {
        writeLog(std::string("[PSVR2TK] ") + triggerHandName(command.hand)
            + " trigger feedback position=" + std::to_string(command.position)
            + " strength=" + std::to_string(command.strength));
        return;
    }
    if (command.effect == TriggerEffect::SlopeFeedback) {
        writeLog(std::string("[PSVR2TK] ") + triggerHandName(command.hand)
            + " trigger slope start=" + std::to_string(command.startPosition)
            + " end=" + std::to_string(command.endPosition)
            + " strength=" + std::to_string(command.startStrength)
            + "->" + std::to_string(command.endStrength));
    }
}

} // namespace

int wmain() {
    using namespace d2vr::psvr2;
    try {
        writeLog("[PSVR2TK] bridge started");
        const auto pipeName = readWideEnvironment(L"D2VR_PSVR2_PIPE_NAME", 128);
        const auto token = readAsciiEnvironment(L"D2VR_PSVR2_SESSION_TOKEN", ipcMaximumTokenBytes);
        const DWORD parentId = readParentProcessId();
        if (!validPipeName(pipeName) || token.empty() || parentId == 0) {
            writeLog("[PSVR2TK] bridge configuration rejected");
            return 2;
        }
        HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parentId);
        if (!parent) {
            writeLog("[PSVR2TK] parent unavailable");
            return 3;
        }
        CurrentUserSecurity security;
        if (!security.initialize()) {
            writeLog("[PSVR2TK] local IPC security unavailable");
            CloseHandle(parent);
            return 4;
        }
        shutdownEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!shutdownEvent) {
            CloseHandle(parent);
            return 5;
        }
        publishOffBoth();
        PipeThreadContext pipeContext{
            L"\\\\.\\pipe\\" + pipeName, token, &security.attributes, parent};
        HANDLE pipeThread = CreateThread(nullptr, 0, pipeThreadMain, &pipeContext, 0, nullptr);
        if (!pipeThread) {
            CloseHandle(shutdownEvent);
            CloseHandle(parent);
            return 6;
        }

        ToolkitBackend backend;
        TriggerDeliveryState delivery[2]{};
        bool driverWasActive = false;
        std::uint64_t connectionGeneration = 0;
        std::uint64_t retryAt = 0;
        std::uint64_t retryDelay = 500;
        std::uint64_t lastFailureLog = 0;

        while (!shutdownRequested.load(std::memory_order_acquire) && parentStillAlive(parent)) {
            const auto now = GetTickCount64();
            if (!backend.loaded() && now >= retryAt) {
                const auto loadResult = backend.loadFromDirectory(bridgeDirectory());
                if (loadResult != BackendLoadResult::Ready) {
                    if (now - lastFailureLog > 5000) {
                        writeLog(std::string("[PSVR2TK] loader ")
                            + backendLoadResultName(loadResult));
                        lastFailureLog = now;
                    }
                    backend.unload();
                    retryAt = now + retryDelay;
                    if (retryDelay < 8000) retryDelay *= 2;
                } else {
                    const int initResult = backend.initialize();
                    if (initResult != toolkitResultOk) {
                        writeLog(std::string("[PSVR2TK] init ")
                            + toolkitResultName(initResult));
                        backend.unload();
                        retryAt = now + retryDelay;
                        if (retryDelay < 8000) retryDelay *= 2;
                    } else {
                        writeLog("[PSVR2TK] initialized");
                        retryDelay = 500;
                    }
                }
            }
            if (backend.initialized()) {
                bool callSucceeded = false;
                const bool active = backend.driverActive(callSucceeded);
                if (!callSucceeded) {
                    writeLog("[PSVR2TK] driver query failed");
                    backend.unload();
                    driverWasActive = false;
                    retryAt = now + retryDelay;
                } else if (!active) {
                    if (driverWasActive) writeLog("[PSVR2TK] driver inactive");
                    driverWasActive = false;
                } else {
                    if (!driverWasActive) {
                        writeLog("[PSVR2TK] driver active");
                        driverWasActive = true;
                        ++connectionGeneration;
                    }
                    for (int hand = 0; hand < 2; ++hand) {
                        const auto command = unpackTriggerCommand(
                            desiredPacked[hand].load(std::memory_order_acquire));
                        if (!validTriggerCommand(command)) continue;
                        if (!shouldApplyTriggerCommand(delivery[hand], command, connectionGeneration))
                            continue;
                        const int result = backend.apply(command);
                        if (result == toolkitResultOk) {
                            markTriggerCommandApplied(delivery[hand], command, connectionGeneration);
                            logApplied(command);
                        } else if (now - lastFailureLog > 1000) {
                            writeLog(std::string("[PSVR2TK] apply ")
                                + toolkitResultName(result));
                            lastFailureLog = now;
                            if (result == toolkitResultDriverInactive) {
                                driverWasActive = false;
                                writeLog("[PSVR2TK] driver inactive");
                            }
                        }
                    }
                }
            }
            WaitForSingleObject(shutdownEvent, 50);
        }

        shutdownRequested.store(true, std::memory_order_release);
        if (shutdownEvent) SetEvent(shutdownEvent);
        backend.shutdown();
        backend.unload();
        WaitForSingleObject(pipeThread, 2000);
        CloseHandle(pipeThread);
        CloseHandle(shutdownEvent);
        CloseHandle(parent);
        writeLog("[PSVR2TK] bridge stopped");
        return 0;
    } catch (...) {
        writeLog("[PSVR2TK] bridge stopped after internal error");
        return 1;
    }
}

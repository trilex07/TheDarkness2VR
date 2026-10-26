#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define D2VR_MATCHED_CAPTURE 1
#include <windows.h>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include "../../src/persistent_log.h"
#include "../../src/async_log.h"
#include "../../src/exception_record_witness.h"

CRITICAL_SECTION g_logLock;
d2vr_log::File g_logFile;
d2vr_log::Async g_asyncLog;
d2vr_log::Emergency g_emergencyLog;
std::atomic<bool> g_logReady{false};
bool g_asyncLogRequested = false;
bool g_asyncLogAdmissionFailed = false;
char g_logPath[MAX_PATH]{};
volatile DWORD g_renderThreadId = 0;
volatile LONG g_lastStage = 7, g_lastHookId = 11;
#include "async_log_under_test.inl"

static unsigned checks;
static void Check(bool ok, const char* label) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", label); std::exit(1); }
}
static std::string Read(const char* path) {
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    std::string result; char bytes[4096]; DWORD size;
    while (ReadFile(file, bytes, sizeof(bytes), &size, nullptr) && size) result.append(bytes, size);
    CloseHandle(file); return result;
}
static bool WaitText(const char* path, const char* text) {
    const auto until = GetTickCount64() + 3000;
    do {
        if (Read(path).find(text) != std::string::npos) return true;
        Sleep(5);
    } while (GetTickCount64() < until);
    return false;
}
struct SinkState {
    HANDLE entered = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    HANDLE release = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    std::atomic<bool> block{true}, fail{false};
    std::vector<std::string> rows; // worker-only until StopDrain establishes join
};
static bool Sink(void* context, const char* text, size_t size) {
    auto& state = *static_cast<SinkState*>(context);
    if (state.block.exchange(false)) {
        SetEvent(state.entered);
        WaitForSingleObject(state.release, INFINITE);
    }
    if (state.fail.exchange(false)) return false;
    state.rows.emplace_back(text, size);
    return g_logFile.WriteLine(text, size);
}
static void ResetState(SinkState& state, bool blocked) {
    ResetEvent(state.entered); ResetEvent(state.release);
    state.block = blocked; state.fail = false; state.rows.clear();
}
int main(int argc, char**) {
    char temp[MAX_PATH], directory[MAX_PATH], emergency[MAX_PATH];
    Check(GetTempPathA(MAX_PATH, temp) != 0, "temp directory");
    Check(GetTempFileNameA(temp, "dal", 0, directory) != 0, "unique temp identity");
    Check(DeleteFileA(directory) && CreateDirectoryA(directory, nullptr), "private test directory");
    sprintf_s(g_logPath, "%s\\d3d9.log", directory);
    sprintf_s(emergency, "%s\\d3d9-emergency.log", directory);
    InitializeCriticalSection(&g_logLock);
    Check(g_logFile.Open(g_logPath), "persistent ordinary sink");
    std::atomic<bool> publishedWrite{false};
    std::thread emergencyObserver([&] {
        const auto until = GetTickCount64() + 3000;
        while (!g_emergencyLog.Ready() && GetTickCount64() < until) SwitchToThread();
        publishedWrite = g_emergencyLog.Write("emergency handle publication");
    });
    Check(g_emergencyLog.Open(emergency), "preopened independent emergency sink");
    emergencyObserver.join();
    Check(publishedWrite && Read(emergency).find("emergency handle publication") != std::string::npos,
          "acquired emergency handle is visible to another thread after publication");
    if (argc > 1) {
        // Unexpected already-open emergency sink forces actual admission to fail.
        g_logReady = true; g_asyncLogRequested = true;
        StartAsyncLogging();
        Check(g_asyncLogAdmissionFailed && !g_asyncLog.Started() && !g_logReady,
              "actual failed activation is explicitly NOT_ADMITTED, no synchronous fallback");
        const auto receipt = Read(g_logPath);
        Check(receipt.find("NOT_ADMITTED") != std::string::npos, "failed activation leaves ordinary receipt");
        Check(Read(emergency).find("NOT_ADMITTED") != std::string::npos, "failed activation leaves independent receipt when available");
        Log("must not fall back to render-thread I/O");
        Check(Read(g_logPath) == receipt, "ordinary logging stays disabled after admission failure");
        g_emergencyLog.Close(); g_logFile.Close(); DeleteCriticalSection(&g_logLock);
        Check(DeleteFileA(g_logPath) && DeleteFileA(emergency) && RemoveDirectoryA(directory), "failure test cleanup");
        std::printf("PASS: %u checks; actual activation failure admission and no I/O fallback\n", checks);
        return 0;
    }
    SinkState state;
    Check(!g_asyncLog.Start(nullptr, nullptr), "invalid writer rejected");
    Check(g_asyncLog.Start(Sink, &state), "worker started and published");
    Check(!g_asyncLog.Start(Sink, &state), "duplicate start rejected");
    g_logReady = true;
    Check(g_asyncLog.Write("seq=0"), "initial row accepted");
    Check(WaitForSingleObject(state.entered, 3000) == WAIT_OBJECT_0, "injected writer is blocked");

    // The actual production Log body must not take its old global CS or wait
    // for blocked FILE I/O. The finite queue must account for every overflow.
    EnterCriticalSection(&g_logLock);
    HANDLE finished = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    std::thread producer([&] {
        for (unsigned i = 1; i <= 10000; ++i) Log("seq=%u", i);
        SetEvent(finished);
    });
    const DWORD result = WaitForSingleObject(finished, 3000);
    LeaveCriticalSection(&g_logLock);
    Check(result == WAIT_OBJECT_0, "10000 producer calls finish while writer and old log CS are blocked");
    producer.join(); CloseHandle(finished);
    const auto filled = g_asyncLog.ReadStats();
    Check(filled.enqueued == 1 + d2vr_log::Async::Capacity, "queue is fixed and exactly bounded");
    Check(filled.dropped == 10000 - d2vr_log::Async::Capacity, "every full-queue loss counted");
    Check(filled.written == 0, "blocked writer has not reached file I/O");

    // Exercise the ACTUAL full VEH body while the ordinary writer is blocked.
    uintptr_t stack[32]{}; stack[0] = reinterpret_cast<uintptr_t>(&Log);
    CONTEXT context{}; context.Esp = reinterpret_cast<DWORD>(stack); context.Ebp = 0;
    context.Eip = reinterpret_cast<DWORD>(&Log); context.Eax = 0x12345678;
    EXCEPTION_RECORD record{}; record.ExceptionAddress = reinterpret_cast<void*>(&Log);
    record.ExceptionCode = EXCEPTION_ACCESS_VIOLATION; record.NumberParameters = 2;
    record.ExceptionInformation[0] = 0; record.ExceptionInformation[1] = 0x471AA1A2;
    EXCEPTION_POINTERS pointers{&record, &context};
    Check(CrashVehHandler(&pointers) == EXCEPTION_CONTINUE_SEARCH, "native VEH preserves exception disposition");
    const auto fatal = Read(emergency);
    Check(fatal.find("C0000005") != std::string::npos && fatal.find("471AA1A2") != std::string::npos,
          "native fault metadata visible before ordinary writer release");
    for (const char* text : {"EAX=12345678", "Call Stack", "information[1]", "targetPage", "[00]"})
        Check(fatal.find(text) != std::string::npos, "full existing native register/parameter/page/stack detail is synchronous");
    record.ExceptionCode = 0xE06D7363u;
    for (unsigned i = 0; i < 4; ++i)
        Check(CrashVehHandler(&pointers) == EXCEPTION_CONTINUE_SEARCH, "C++ VEH preserves exception disposition");
    Check(Read(emergency) == fatal, "handled C++ never enters emergency sink");
    Check(g_asyncLog.ReadStats().dropped > filled.dropped, "handled C++ details attempt ordinary bounded queue");
    record.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
    Check(CrashVehHandler(&pointers) == EXCEPTION_CONTINUE_SEARCH, "native detail budget survives C++ notifications");
    const auto repeatedFatal = Read(emergency);
    const auto firstStack = repeatedFatal.find("Call Stack");
    Check(firstStack != std::string::npos && repeatedFatal.find("Call Stack", firstStack + 1) != std::string::npos,
          "C++ notifications cannot consume admitted async native full-detail budget");

    SetEvent(state.release);
    Check(WaitText(g_logPath, "[AsyncLog] counters"), "overflow counters visible while worker remains alive");
    Check(g_asyncLog.Started(), "live counters do not require stop/join");
    Check(g_asyncLog.StopDrain(3000), "asynchronous queue drains outside loader lock");
    unsigned sequence = 0;
    for (const auto& row : state.rows) {
        if (row.find("seq=") != 0) continue;
        Check(row == "seq=" + std::to_string(sequence++), "accepted queue order preserved");
    }
    Check(sequence == filled.enqueued, "all accepted rows delivered once");
    Check(g_asyncLog.ReadStats().written == filled.enqueued, "write count excludes diagnostic counter/footer rows");

    // Report truncation and writer errors during normal operation, not detach.
    ResetState(state, false); state.fail = true;
    Check(g_asyncLog.Start(Sink, &state), "worker restarts after controlled stop");
    Check(g_asyncLog.Write("deliberate write failure"), "failure row queued");
    std::string longLine(d2vr_log::Async::LineBytes * 2, 'x');
    Check(g_asyncLog.Write("%s", longLine.c_str()), "oversize row accepted with bounded truncation");
    Check(WaitText(g_logPath, "truncated=1 errors=1"), "truncation/error counters visible before stopping");
    Check(WaitText(g_logPath, "[truncated]"), "written truncation is explicitly marked");
    Check(g_asyncLog.StopDrain(3000), "error/truncation session drains");
    const auto errors = g_asyncLog.ReadStats();
    Check(errors.enqueued == 2 && errors.written == 1 && errors.errors == 1 && errors.truncated == 1,
          "actual helper maintains independent error/truncation counts");

    // Stop timeout never destroys the worker's handles, object or file.
    ResetState(state, true);
    Check(g_asyncLog.Start(Sink, &state) && g_asyncLog.Write("timeout case"), "timeout session starts");
    Check(WaitForSingleObject(state.entered, 3000) == WAIT_OBJECT_0, "timeout writer blocked");
    Check(!g_asyncLog.StopDrain(1) && g_asyncLog.Started(), "timed out stop leaves worker lifetime intact");
    SetEvent(state.release);
    Check(g_asyncLog.StopDrain(3000), "later controlled stop safely joins");
    for (unsigned cycle = 0; cycle < 20; ++cycle) {
        ResetState(state, false);
        Check(g_asyncLog.Start(Sink, &state), "repeated worker start");
        uint64_t accepted = 0;
        for (unsigned i = 0; i < 50; ++i) if (g_asyncLog.Write("cycle=%u row=%u", cycle, i)) ++accepted;
        Check(g_asyncLog.StopDrain(3000), "repeated worker drain");
        const auto stats = g_asyncLog.ReadStats();
        Check(stats.enqueued == accepted && stats.written == accepted && stats.dropped + accepted == 50,
              "repeated lifecycle resets accounting and drains accepted rows");
    }
    Check(g_asyncLog.StopDrain(1), "idempotent stopped drain");

    // Actual HookDevice activation body: default synchronous path is unchanged.
    g_emergencyLog.Close();
    StartAsyncLogging();
    Check(!g_asyncLog.Started(), "OFF default creates no worker");
    Log("startup synchronous visible");
    Check(Read(g_logPath).find("startup synchronous visible") != std::string::npos, "default Log retains immediate persistent visibility");
    g_asyncLogRequested = true;
    StartAsyncLogging();
    Check(g_asyncLog.Started() && g_emergencyLog.Ready() && !g_asyncLogAdmissionFailed,
          "actual activation preopens emergency and starts admitted worker");
    Log("activation row");
    Check(g_asyncLog.StopDrain(3000), "actual activation drains outside DllMain");
    Check(Read(g_logPath).find("activation row") != std::string::npos, "actual ordinary Log reaches async sink");
    g_logReady = false; g_emergencyLog.Close(); g_logFile.Close();
    CloseHandle(state.entered); CloseHandle(state.release); DeleteCriticalSection(&g_logLock);
    Check(DeleteFileA(g_logPath) && DeleteFileA(emergency) && RemoveDirectoryA(directory), "test artifacts cleaned");
    std::printf("PASS: %u checks; actual async logger/Log/activation/VEH, blocked writer, bounded queue, live losses, emergency and lifecycle\n", checks);
}

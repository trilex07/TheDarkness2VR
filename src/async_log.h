#pragma once
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>

namespace d2vr_log {
// One process-lifetime worker, started outside DllMain after pinning its code.
// Start/StopDrain require externally serialized, quiescent lifecycle callers.
// Producers never acquire the writer's FILE lock, wait for it, or perform I/O.
class Async {
public:
    static constexpr unsigned Capacity = 256, LineBytes = 2048;
    using Sink = bool (*)(void*, const char*, size_t);
    struct Stats { uint64_t enqueued, written, dropped, truncated, errors; };
private:
    struct Line { size_t size; char text[LineBytes]; };
    Line queue_[Capacity]{};
    SRWLOCK lock_ = SRWLOCK_INIT;
    unsigned read_ = 0, write_ = 0, count_ = 0;
    HANDLE wake_ = nullptr, thread_ = nullptr;
    Sink sink_ = nullptr;
    void* context_ = nullptr;
    std::atomic<bool> started_{false}, stopping_{false};
    std::atomic<uint64_t> enqueued_{0}, written_{0}, dropped_{0}, truncated_{0}, errors_{0};
    static DWORD WINAPI Worker(void* self) { return static_cast<Async*>(self)->Run(); }
    DWORD Run() {
        Stats reported{};
        for (;;) {
            Line batch[16]; unsigned count = 0;
            AcquireSRWLockExclusive(&lock_);
            while (count_ && count < 16) {
                batch[count++] = queue_[read_];
                read_ = (read_ + 1) % Capacity; --count_;
            }
            ReleaseSRWLockExclusive(&lock_);
            // No queue lock is held across a writer/antivirus/filesystem stall.
            for (unsigned i = 0; i < count; ++i) {
                if (sink_(context_, batch[i].text, batch[i].size)) ++written_;
                else ++errors_;
            }
            const auto stats = ReadStats();
            if (stats.dropped != reported.dropped || stats.truncated != reported.truncated || stats.errors != reported.errors) {
                char status[256];
                const int n = snprintf(status, sizeof(status),
                    "[AsyncLog] counters enqueued=%llu written=%llu dropped=%llu truncated=%llu errors=%llu",
                    stats.enqueued, stats.written, stats.dropped, stats.truncated, stats.errors);
                reported = stats;
                if (n > 0 && !sink_(context_, status, static_cast<size_t>(n))) ++errors_;
            }
            if (!count) {
                if (stopping_.load(std::memory_order_acquire)) break;
                // Report losses even if the last producer could not enqueue.
                WaitForSingleObject(wake_, 500);
            }
        }
        const auto stats = ReadStats();
        char footer[256];
        const int n = snprintf(footer, sizeof(footer),
            "[AsyncLog] drained enqueued=%llu written=%llu dropped=%llu truncated=%llu errors=%llu",
            stats.enqueued, stats.written, stats.dropped, stats.truncated, stats.errors);
        if (n > 0 && !sink_(context_, footer, static_cast<size_t>(n))) ++errors_;
        return 0;
    }
public:
    Async() = default;
    Async(const Async&) = delete;
    Async& operator=(const Async&) = delete;
    // No automatic join/CloseHandle/FILE close: destruction may run under loader lock.
    bool Started() const { return started_.load(std::memory_order_acquire); }
    bool Start(Sink sink, void* context) {
        if (Started() || thread_ || !sink) return false;
        HMODULE pinned = nullptr;
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCSTR>(&Worker), &pinned)) return false;
        wake_ = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        if (!wake_) return false;
        read_ = write_ = count_ = 0;
        enqueued_ = 0; written_ = 0; dropped_ = 0; truncated_ = 0; errors_ = 0;
        sink_ = sink; context_ = context;
        stopping_.store(false, std::memory_order_release);
        thread_ = CreateThread(nullptr, 0, &Worker, this, 0, nullptr);
        if (!thread_) { CloseHandle(wake_); wake_ = nullptr; return false; }
        started_.store(true, std::memory_order_release);
        return true;
    }
    bool WriteV(const char* format, va_list args) {
        if (!Started() || stopping_.load(std::memory_order_acquire)) { ++dropped_; return false; }
        Line line{};
        const int n = vsnprintf(line.text, sizeof(line.text), format, args);
        if (n < 0) {
            strcpy_s(line.text, "[format-error]"); ++errors_;
        } else if (static_cast<size_t>(n) >= sizeof(line.text)) {
            ++truncated_;
            static constexpr char marker[] = " [truncated]";
            memcpy(line.text + sizeof(line.text) - sizeof(marker), marker, sizeof(marker));
        }
        line.size = strlen(line.text);
        if (!TryAcquireSRWLockExclusive(&lock_)) { ++dropped_; return false; }
        if (stopping_.load(std::memory_order_acquire) || count_ == Capacity) {
            ReleaseSRWLockExclusive(&lock_); ++dropped_; return false;
        }
        queue_[write_] = line; write_ = (write_ + 1) % Capacity; ++count_;
        ++enqueued_;
        ReleaseSRWLockExclusive(&lock_);
        SetEvent(wake_);
        return true;
    }
    bool Write(const char* format, ...) {
        va_list args; va_start(args, format);
        const bool result = WriteV(format, args); va_end(args); return result;
    }
    Stats ReadStats() const {
        return {enqueued_.load(), written_.load(), dropped_.load(), truncated_.load(), errors_.load()};
    }
    void SignalStop() {
        stopping_.store(true, std::memory_order_release);
        if (wake_) SetEvent(wake_);
    }
    // ONLY outside DllMain/loader lock, after producers have quiesced. Timeout
    // leaves the pinned worker and handles intact; caller must not destroy us.
    bool StopDrain(DWORD timeoutMs = INFINITE) {
        if (!thread_) return true;
        SignalStop();
        if (WaitForSingleObject(thread_, timeoutMs) != WAIT_OBJECT_0) return false;
        CloseHandle(thread_); CloseHandle(wake_);
        thread_ = wake_ = nullptr;
        started_.store(false, std::memory_order_release);
        return true;
    }
};

inline bool EmergencyException(DWORD code) {
    return code >= 0xC0000000u && code != 0xE06D7363u && code != 0xC0000420u;
}
// Independent preopened OS handle: no queue or CRT FILE lock shared with Worker.
// A native first-chance exception is labelled as such, never declared unhandled.
class Emergency {
    std::atomic<HANDLE> handle_{INVALID_HANDLE_VALUE};
public:
    bool Open(const char* path) {
        if (Ready()) return false;
        HANDLE opened = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (opened == INVALID_HANDLE_VALUE) return false;
        handle_.store(opened, std::memory_order_release); // lifecycle callers serialized
        return true;
    }
    bool Ready() const { return handle_.load(std::memory_order_acquire) != INVALID_HANDLE_VALUE; }
    bool WriteV(const char* format, va_list args) {
        const HANDLE handle = handle_.load(std::memory_order_acquire);
        if (handle == INVALID_HANDLE_VALUE) return false;
        char text[2048];
        const int n = vsnprintf(text, sizeof(text) - 1, format, args);
        size_t size;
        if (n < 0) {
            strcpy_s(text, "[format-error]"); size = strlen(text);
        } else if (static_cast<size_t>(n) >= sizeof(text) - 1) {
            static constexpr char marker[] = " [truncated]";
            memcpy(text + sizeof(text) - 1 - sizeof(marker), marker, sizeof(marker));
            size = sizeof(text) - 2;
        } else size = static_cast<size_t>(n);
        text[size++] = '\n'; DWORD written = 0;
        return WriteFile(handle, text, static_cast<DWORD>(size), &written, nullptr) && written == size;
    }
    bool Write(const char* format, ...) {
        va_list args; va_start(args, format);
        const bool result = WriteV(format, args); va_end(args); return result;
    }
    bool Record(const EXCEPTION_RECORD& record, const CONTEXT& context,
                DWORD renderThread, LONG stage, LONG lastHook) {
        const HANDLE handle = handle_.load(std::memory_order_acquire);
        if (handle == INVALID_HANDLE_VALUE || !EmergencyException(record.ExceptionCode)) return false;
        char text[2048];
        int n = snprintf(text, sizeof(text),
            "!!! NATIVE FIRST-CHANCE code=%08lX flags=%08lX address=%p tid=%lu renderTid=%lu stage=%ld lastHook=%ld "
            "EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX ESI=%08lX EDI=%08lX EBP=%08lX ESP=%08lX EIP=%08lX parameters=%lu",
            record.ExceptionCode, record.ExceptionFlags, record.ExceptionAddress, GetCurrentThreadId(),
            renderThread, stage, lastHook, context.Eax, context.Ebx, context.Ecx, context.Edx,
            context.Esi, context.Edi, context.Ebp, context.Esp, context.Eip, record.NumberParameters);
        if (n < 0 || static_cast<size_t>(n) >= sizeof(text) - 1) return false;
        size_t used = static_cast<size_t>(n);
        const DWORD count = record.NumberParameters < EXCEPTION_MAXIMUM_PARAMETERS
            ? record.NumberParameters : EXCEPTION_MAXIMUM_PARAMETERS;
        for (DWORD i = 0; i < count; ++i) {
            n = snprintf(text + used, sizeof(text) - used, " info[%lu]=%p", i,
                reinterpret_cast<void*>(record.ExceptionInformation[i]));
            if (n < 0 || static_cast<size_t>(n) >= sizeof(text) - used - 1) return false;
            used += static_cast<size_t>(n);
        }
        text[used++] = '\n'; DWORD written = 0;
        return WriteFile(handle, text, static_cast<DWORD>(used), &written, nullptr) && written == used;
    }
    // ONLY after all handlers/writers quiesce, outside loader lock. Runtime
    // keeps this preopened sink for process lifetime, including failed admission.
    void Close() {
        const HANDLE handle = handle_.exchange(INVALID_HANDLE_VALUE, std::memory_order_acq_rel);
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    }
};
} // namespace d2vr_log

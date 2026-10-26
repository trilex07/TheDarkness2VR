#pragma once
#include <cstdio>
#include <cstdarg>
#include <share.h>

namespace d2vr_log {
// Caller serializes Open/Write/Close. Keep one file open, but flush every line
// so a crash does not strand diagnostics in a userspace buffer. This helper
// owns no worker; the opt-in Async wrapper calls WriteLine on its writer only.
class File {
    FILE* stream_ = nullptr;
public:
    File() = default;
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    bool Open(const char* path) {
        if (stream_) return false;
        // Capturers must be able to read/copy the live log while it stays open.
        stream_ = _fsopen(path, "w", _SH_DENYNO);
        return stream_ != nullptr;
    }
    bool WriteV(const char* format, va_list args) {
        if (!stream_) return false;
        const int result = vfprintf(stream_, format, args);
        const int newline = fputc('\n', stream_);
        const int flushed = fflush(stream_);
        return result >= 0 && newline != EOF && flushed == 0;
    }
    // Async worker sink; callers still own serialization and lifetime.
    bool WriteLine(const char* text, size_t size) {
        if (!stream_) return false;
        const bool result = fwrite(text, 1, size, stream_) == size;
        const int newline = fputc('\n', stream_);
        const int flushed = fflush(stream_);
        return result && newline != EOF && flushed == 0;
    }
    bool Write(const char* format, ...) {
        va_list args; va_start(args, format);
        const bool result = WriteV(format, args);
        va_end(args); return result;
    }
    void Close() {
        if (stream_) { fclose(stream_); stream_ = nullptr; }
    }
};
}

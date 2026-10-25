#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "../../src/exception_record_witness.h"

static unsigned checks = 0;
static unsigned observed = 0;
static std::vector<std::string> lines;
static void Check(bool ok, const char* message) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static void Write(const char* fmt, ...) {
    char row[512]; va_list args; va_start(args, fmt);
    vsnprintf(row, sizeof(row), fmt, args); va_end(args);
    lines.emplace_back(row);
}
static bool Contains(const char* text) {
    for (const auto& row : lines) if (row.find(text) != std::string::npos) return true;
    return false;
}
static LONG WINAPI Observe(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode == 0xE06D7363) {
        ++observed;
        d2vr_exception_witness::LogRecord(*ep->ExceptionRecord, Write);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
// No C++ local destructors in this SEH scope. The exception is confined to
// this test process and consumed by its own handler, never by the mod/game.
static bool RaiseAndCatch() {
    ULONG_PTR info[3] = {0x91930520, 0x00EEA1D7, 1};
    __try { RaiseException(0xE06D7363, 0, 3, info); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return true; }
    return false;
}
int main() {
    lines.reserve(64);
    EXCEPTION_RECORD record{};
    record.ExceptionCode = 0xE06D7363;
    record.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
    record.ExceptionRecord = reinterpret_cast<EXCEPTION_RECORD*>(1);
    record.NumberParameters = 0;
    d2vr_exception_witness::LogRecord(record, Write);
    Check(lines.size() == 1, "zero parameters must not read the information array");
    Check(Contains("flags=00000001 parameters=0 logged=0"), "flags and reported count survive");
    Check(Contains("nested=00000001"), "nested pointer is logged without being dereferenced");
    const DWORD counts[] = {1, 3, EXCEPTION_MAXIMUM_PARAMETERS, MAXDWORD};
    for (DWORD count : counts) {
        lines.clear(); record.NumberParameters = count;
        for (DWORD i = 0; i < EXCEPTION_MAXIMUM_PARAMETERS; ++i)
            record.ExceptionInformation[i] = 0xBAD00000u + i;
        d2vr_exception_witness::LogRecord(record, Write);
        const DWORD wanted = count > EXCEPTION_MAXIMUM_PARAMETERS ? EXCEPTION_MAXIMUM_PARAMETERS : count;
        Check(lines.size() == 1 + wanted, "parameter reporting is bounded to the OS array");
        Check(Contains("information[0]=BAD00000"), "information values remain uninterpreted");
        Check(!Contains("information[15]"), "oversized count cannot overrun the array");
    }
    lines.clear();
    auto handler = AddVectoredExceptionHandler(1, Observe);
    Check(handler != nullptr, "test VEH installs");
    Check(RaiseAndCatch(), "logging must preserve the original SEH continuation");
    Check(observed == 1, "real first-chance record observed exactly once");
    Check(lines.size() == 4, "real three-parameter record is complete");
    Check(Contains("code=E06D7363 flags=00000000 parameters=3 logged=3"), "real OS metadata survives");
    Check(Contains("information[0]=91930520") && Contains("information[1]=00EEA1D7") &&
          Contains("information[2]=00000001"), "real parameters copied without following invalid pointers");
    Check(RemoveVectoredExceptionHandler(handler) != 0, "test VEH removed");
    std::printf("exception record witness: %u checks PASS\n", checks);
}

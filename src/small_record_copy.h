#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <atomic>

namespace d2vr_record_copy {
// Diagnostic control changes the selected copy algorithm, never admission,
// arena ownership or payload lifetime. Configure only before device hooks.
class RuntimeControl {
    bool armed_ = false;
    std::atomic<bool> enabled_{true};
public:
    void Configure(bool armed) { armed_ = armed; }
    bool Armed() const { return armed_; }
    bool Request(int value) {
        if (!armed_ || (value != 0 && value != 1)) return false;
        return enabled_.exchange(value != 0, std::memory_order_relaxed) != (value != 0);
    }
    bool Effective(bool admitted) const {
        return admitted && (!armed_ || enabled_.load(std::memory_order_relaxed));
    }
};

constexpr size_t kSmallLimit = 192;
constexpr size_t kWideLimit = 64u * 1024;
using ReadMemory = BOOL (WINAPI*)(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*);

// Legacy recorder validation/copy, intentionally unchanged. Permissions are
// queried afresh; neither this path nor the optional path caches source pages.
inline size_t ClampReadableBytes(const void* p, size_t bytes) {
    const uint8_t* b = (const uint8_t*)p;
    size_t done = 0;
    while (done < bytes) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(b + done, &mbi, sizeof(mbi))) break;
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) break;
        const uint8_t* regionEnd = (const uint8_t*)mbi.BaseAddress + mbi.RegionSize;
        size_t chunk = (size_t)(regionEnd - (b + done));
        if (chunk == 0) break;
        done += chunk;
    }
    return done;
}
inline bool SafeMemcpy(void* dst, const void* src, size_t bytes) {
    __try {
        memcpy(dst, src, bytes);
        return true;
    } __except (1) {
        memset(dst, 0, bytes);
        return false;
    }
}
inline bool LegacyCopy(void* dst, const void* data, size_t bytes) {
    const size_t readable = ClampReadableBytes(data, bytes);
    if (readable < bytes) {
        memset(dst, 0, bytes);
        if (readable) SafeMemcpy(dst, data, readable);
        return false;
    } else {
        return SafeMemcpy(dst, data, bytes);
    }
}

// Copy without a permission syscall. Consuming PAGE_GUARD is the only fault
// side effect: re-arm that page so the caller's legacy fallback observes, and
// refuses, the protection the source had before this read.
inline LONG DirectCopyFilter(const EXCEPTION_POINTERS* info) {
    const EXCEPTION_RECORD* record = info->ExceptionRecord;
    if (record->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION && record->NumberParameters >= 2) {
        void* page = reinterpret_cast<void*>(record->ExceptionInformation[1]);
        MEMORY_BASIC_INFORMATION m{};
        DWORD previous = 0;
        if (VirtualQuery(page, &m, sizeof(m)) == sizeof(m) && m.State == MEM_COMMIT &&
            !(m.Protect & PAGE_GUARD))
            VirtualProtect(page, 1, m.Protect | PAGE_GUARD, &previous);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
inline bool DirectCopy(void* dst, const void* src, size_t bytes) {
    __try {
        memcpy(dst, src, bytes);
        return true;
    } __except (DirectCopyFilter(GetExceptionInformation())) {
        return false;
    }
}

// Constant setters forward this exact source, or a full memcpy of it made by
// the stereo substitution, to the native runtime on this thread before
// returning. An in-range upload is therefore read in full with ordinary loads
// later in the same call anyway; a DirectCopy fault only precedes that read and
// DirectCopyFilter leaves PAGE_GUARD armed for it. Out-of-range uploads may be
// rejected unread by the runtime and keep the legacy copy.
inline bool NativeConsumedRange(unsigned start, unsigned count, unsigned registers) {
    return count && start < registers && count <= registers - start;
}

// Output remains the legacy readable-prefix/zero-tail representation on error.
// Only a full successful copy certifies an owned replay payload.
inline bool Copy(void* dst, const void* data, size_t bytes, bool admitted,
                 ReadMemory readMemory = &ReadProcessMemory,size_t limit=kSmallLimit) {
    if (!bytes) return true;
    if (admitted && bytes <= (limit>kWideLimit?kWideLimit:limit)) {
        SIZE_T copied = 0;
        if (readMemory(GetCurrentProcess(), data, dst, bytes, &copied) && copied == bytes) return true;
        // RPM does not preserve the readable prefix on every failed/partial
        // read. Discard any output before using the original prefix/zero policy.
        memset(dst, 0, bytes);
    }
    return LegacyCopy(dst, data, bytes);
}

// Probe only our allocation. A different OS policy, allocation failure or any
// inconclusive result rejects the fast path for this process. Never in DllMain.
inline bool GuardSemanticsSelfTest() {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    if (info.dwPageSize < 64) return false;
    auto memory = static_cast<uint8_t*>(VirtualAlloc(nullptr, size_t(info.dwPageSize) * 2,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!memory) return false;
    auto guard = memory + info.dwPageSize;
    bool passed = true;
    uint8_t output[64]{};
    for (unsigned trial = 0; trial < 2 && passed; ++trial) {
        DWORD previous = 0;
        MEMORY_BASIC_INFORMATION before{}, after{};
        passed = VirtualProtect(guard, info.dwPageSize, PAGE_READWRITE | PAGE_GUARD, &previous) &&
            VirtualQuery(guard, &before, sizeof(before)) == sizeof(before) &&
            before.State == MEM_COMMIT && (before.Protect & PAGE_GUARD) != 0;
        if (!passed) break;
        SIZE_T copied = 0;
        const bool success = ReadProcessMemory(GetCurrentProcess(),
            trial ? guard - 16 : guard, output, sizeof(output), &copied) != FALSE;
        passed = !success && copied == 0 &&
            VirtualQuery(guard, &after, sizeof(after)) == sizeof(after) &&
            after.State == MEM_COMMIT && after.Protect == before.Protect;
    }
    VirtualFree(memory, 0, MEM_RELEASE);
    return passed;
}

class Admission {
    bool tested_ = false, passed_ = false;
public:
    bool Admit(bool requested, bool (*probe)() = &GuardSemanticsSelfTest) {
        if (!requested) return false; // default OFF: no RPM calls or probe allocation
        if (!tested_) { tested_ = true; passed_ = probe(); }
        return passed_;
    }
    bool Tested() const { return tested_; }
};
} // namespace d2vr_record_copy

#pragma once
#include <windows.h>

// Diagnostic only. All values belong to the OS-supplied exception record.
// ExceptionInformation values may be arbitrary integers or invalid addresses:
// logging them must never chase a throw object, ThrowInfo or nested record.
namespace d2vr_exception_witness {
inline void LogRecord(const EXCEPTION_RECORD& record, void (*write)(const char*, ...)) {
    const DWORD count = record.NumberParameters > EXCEPTION_MAXIMUM_PARAMETERS
        ? EXCEPTION_MAXIMUM_PARAMETERS : record.NumberParameters;
    write("[ExceptionWitness] code=%08lX flags=%08lX parameters=%lu logged=%lu nested=%p",
        record.ExceptionCode, record.ExceptionFlags, record.NumberParameters, count,
        static_cast<void*>(record.ExceptionRecord));
    for (DWORD i = 0; i < count; ++i)
        write("[ExceptionWitness] information[%lu]=%p", i,
            reinterpret_cast<void*>(record.ExceptionInformation[i]));
}
}

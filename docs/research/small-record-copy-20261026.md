# Bounded small recorder copy candidate

Status: IMPLEMENTED / OFFLINE VERIFIED. Steam: NOT RUN. PSVR2: NOT RUN.
No measured game FPS or outdoor stutter improvement is claimed.

Hypothesis: replace a fresh VirtualQuery plus memcpy for valid, small recorder
payloads with a checked self-process ReadProcessMemory call. No permission cache.

`[Render] SmallRecordCopy=0` remains the default. With explicit opt-in, HookDevice
runs one guard-semantics probe on our own VirtualAlloc pages, before installing
the replay hooks. It never runs in DllMain. Allocation, protection, query or
read-result uncertainty leaves the legacy copy path active for this process.
Both direct and cross-page failed reads must preserve the guard protection.
An authorized opt-in test must confirm `[SmallRecordCopy] requested=1 admitted=1`
in the startup log; editing a file is not proof that the effective INI was read.

Only 1..192-byte copies can enter the fast path. Success requires both TRUE and
the exact requested byte count. Every failed/short read discards any output and
uses the original, freshly queried VQ + SafeMemcpy path, preserving its readable
prefix and zero tail. Larger copies and the default-OFF configuration use the
legacy path. Record locks, offsets, arena sizing, commands and resource ownership
are unchanged. No EXE-image mutation, FOV, shader, hand or image-quality change.

## Private microbenchmark receipt

2026-10-26, MSVC x86 `/O2`, five alternating rounds of 200,000 valid copies per
method and size; 128 committed source pages, warm-up before timing. Medians:

- 16 bytes: legacy 827.857 ns, self-RPM 606.897 ns (26.7% less).
- 64 bytes: legacy 821.557 ns, self-RPM 598.772 ns (27.1% less).
- 192 bytes: legacy 850.543 ns, self-RPM 613.551 ns (27.9% less).
- 1088 bytes: legacy 884.222 ns, self-RPM 948.402 ns (7.3% more).

These are microbenchmark results, not the full candidate or gameplay timing.
All five rounds favored RPM at each small size; all five disfavored 1088 bytes.
The candidate deliberately excludes that larger size. A universal RPM replacement
was rejected. Raw partial RPM reads returned FALSE/error299/zero bytes, omitting
the readable prefix: the legacy failure fallback is mandatory.

Direct and cross-page guard reads left protection `0x104` unchanged. READONLY,
EXECUTE-only and EXECUTE_READ copied identical bytes to the legacy path on this
test host. Other OS behavior is not assumed; the startup guard probe fails closed.
Private benchmark sources/results remain outside Git; no binaries or traces are
included here.

## Regression and acceptance

Run `tools/frame-boundary-simulator/build-small-record-copy.ps1`. The optimized
x86 test includes the actual production helper: default OFF/no RPM calls,
one-time admission and rejection, null/zero, exact/short/failed reads, changed
payload bytes, READONLY/EXECUTE, NOACCESS, guard preservation, cross-page prefix,
193/1088-byte legacy fallback, and untouched neighboring arena bytes.

Build production and diagnostic DLL profiles with `build.ps1` and
`build.ps1 -DiagnosticCapture`. Do not deploy automatically.

Offline receipt: both DLL profiles built successfully; the optimized x86 test
passed 141 byte/range checks plus admission/API-count/protection assertions;
44 existing Python Present/capture/stereo-transfer tests and the native image
guard test also passed. No Steam/HMD execution was performed for this candidate.

Rollback: leave/remove `SmallRecordCopy=0` and restart, or use the saved baseline
DLL. Any eventual authorized A/B must hold configuration and scene constant and
measure the complete helper/FPS; this small-copy experiment does not address the
separately measured 47.833 ms logging flush stall or shader first-use stalls.

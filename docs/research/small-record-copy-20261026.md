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

## MAIN integration receipt

Original implementation d15f40e integrated as804d0e7 on
codex/outdoor-frame-bottleneck; tests-only b35c4d6 integrated asaecf54a.
The existing script now compiles extracted actual RecPush AND RecLockGuard.
239 byte/range checks PASS, including all141 former helper checks plus98 real
recorder-body checks for both flags: prior arena/append/offset/payload identity,
1..1088-byte sizes, null source, zero, exact8MiB, >8MiB and SIZE_MAX sanity clamp,
full/cross-page NOACCESS and guard fallback with protection retained. The source
checks effective admission passed to Copy, requested/admitted/config defaultOFF,
and guard-probe order before any device PatchVTable/InstallReplayHooks.

Integrated production/diagnostic x86 builds PASS. Protected head117021,
controller20032 and gesture227 checks PASS. Caps3705 checks still PASS after
integration. Read-only independent review found no material blocker; existing
source mutation races remain unchanged. This is OFFLINE VERIFIED, not an
in-game or HMD-accepted performance correction.

Frozen artifact source804d0e7: production DLL SHA-256
9B37F6342E70C8199A722DD86C5AF2A2E5C74539E88FF1A99233980ED30E8BE2;
diagnostic4F8A9A455F4413B9BB5ADEE6022D0CA89D90DB3228895D7F38D6A518A923BC2E.
PDB matches each RSDS: GUID26b151b8-b2a9-4a6f-b1fc-8cc227629edc, ages168/169.
Later integration changes only tests/evidence. Binary/map/PDB artifacts stay
private. The package's FIRST proposed A/B is still AsyncLog0/1 ONLY: the same
DLL in both arms, byte-identical prior protected INIs, SmallRecordCopy,
SamplerCapsCache and LastConsumerReaderFence OFF. Win32 effective-INI enumeration
confirms the sole difference is Render/AsyncLog; diagnostic INIs byte-identical.
No deployment or Steam launch occurred. Preserve the returned Steam baseline.

This targets RecPush->ClampReadableBytes->VirtualQuery, whose retained outdoor
subtree is2.486ms INCLUSIVE. The eligible1..192-byte payload distribution and
complete outdoor32/64-bit stack intersections are unavailable, so neither that
number nor the microbenchmark's27% translates into a claimed FPS improvement.
Do not add it to overlapping WOW64/query samples. The outdoor average20.429ms
still exceeds11.11ms by9.318ms; a controlled game comparison is required before
any measured budget reduction or FIXED/90Hz claim.

## Incomplete payload admission, 2026-10-29

Actual recorder regression reproduced a separate correctness defect: copying a
64-byte VS matrix across an unreadable page produced a readable prefix/zero
tail, but RecEmitData certified all four rows from arena length alone. The
pre-change test fails `g_recOverflow && g_rec.empty() && !reference.valid`
(small_record_copy_test.cpp, line74). This is not evidence of its frequency in
the user's run or proof that it explains observed right-eye flicker.

Copy/SafeMemcpy/LegacyCopy now return full-copy receipts without changing their
byte output, fallback algorithm, guard-page handling or startup admission.
Successful legacy fallback after a failed/short RPM remains valid. RecPushLocked
marks the existing recording failure flag for an incomplete or sanity-clamped
packet; RecEmitData refuses to mark its raw VS rows valid. Failed packets cannot
publish records or certify a replay pair. A fresh valid recording recovers.
Original live driver calls/HRESULTs and lifetime rules are unchanged. This adds
no query, allocation, permission cache, readback, diagnostic timer or INI key.

PASS: actual x86 recorder/copy6133 byte/range checks; native source/ABI9239;
actual UP hooks/replay4377 checks, including unreadable vertex/index pages,
both index formats, preserved guard flags, original call exactly once and
next-stream recovery. Both production and DiagnosticCapture x86 builds PASS.
The change is a payload-integrity correction; FPS saving and HMD effect remain
unmeasured. Dynamic VB/IB versioning and draw-inherited constants remain open.

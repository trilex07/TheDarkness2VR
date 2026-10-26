# Async ordinary logger candidate — offline receipt

Base `010a84e`; isolated branch `codex/async-log`. No caps/RPM, renderer,
resolution, input, FOV, native hooks, installed files or Steam changes.

The existing outdoor analysis found a 72.7119 ms frame containing 47.833 ms
blocked through Ntfs/WriteFile/ucrt fflush/d2vr_log::File::WriteV. Persistent
logging removed repeated file close; this candidate moves remaining ordinary
file writes/flushes off the calling/render thread. FPS improvement is unmeasured.

## Operation and admission

`[Render] AsyncLog=0` remains the default. Startup/default writes retain the
persistent synchronous logger. Explicit `AsyncLog=1` activates once at
HookDevice, outside DllMain. The emergency file is opened first, code is pinned
before worker publication, and the sync transition excludes in-flight startup
writers. Failure records `[AsyncLog] NOT_ADMITTED` and disables ordinary logging
instead of silently resuming render-thread I/O. Do not admit that run as a test.
If the emergency CreateFile itself fails, only native VEH details may use the
still-open persistent File under its old lock, guarded by failed admission,
missing emergency and no worker. Ordinary producers and handled C++ cannot use
that fallback. Its admission flag is atomically published to the VEH.

After admission, ordinary Log formats into fixed 2,048-byte entries and uses a
non-waiting try-lock queue (256 entries). The worker releases the queue lock
before fwrite/fflush; no producer takes the old FILE/critical-section lock or
waits for writer I/O. Accepted entries retain enqueue order. Full/contended
queues drop rows, oversized rows carry `[truncated]`, and independent loss,
truncation and write-error counters are available through ReadStats. The worker
reports changed counters while alive, outside the queue lock, when its sink can
write; a blocked filesystem cannot publish a new file receipt until it resumes.

The preopened `%LOCALAPPDATA%/Darkness2VR/d3d9-emergency.log` has an atomically
published OS handle and no queue/CRT FILE lock shared with the worker. Native
exception metadata is recorded before module lookup, and existing full native
VEH diagnostics (parameters, registers, target page, raw stack and diagnostic
EBP chain) use this independent synchronous sink. E06D7363 handled C++ remains
ordinary queued work. Exception disposition stays EXCEPTION_CONTINUE_SEARCH.
The original combined three-record budget remains when async is inactive;
separate native/C++ budgets apply only after async/emergency admission.

DllMain only signals an active worker: no join, CRT close or emergency-handle
close under loader lock. Both handles are process-lifetime. Ordinary queued tail
rows can be lost at process exit. StopDrain and emergency Close are controlled
outside-loader APIs requiring quiescent callers/handlers; a drain timeout keeps
the worker/handles alive. A separate emergency sink avoids the blocked ordinary
writer, but cannot guarantee disk latency if the filesystem itself is stalled.

## Offline results

- `./tools/frame-boundary-simulator/build-async-log.ps1`: PASS, 370 actual-helper
  and extracted production Log/activation/VEH checks; 11 checks of the actual
  failed activation in a separate process, plus 24 checks forcing a real
  emergency CreateFile failure with a directory at its pathname. Those checks
  prove full native register/parameter/page/stack details survive after four
  handled C++ notifications, without re-enabling ordinary/CPP synchronous I/O.
  Includes blocked writer + old global
  CS, 10,000 producer calls, exact bounded overflow, enqueue order, asynchronous
  drain, live loss/truncation/error reports, native emergency visibility while
  blocked, handled C++ routing, cross-thread handle publication, timeout and
  20 repeated start/drain cycles. Source gates cover module pin, default OFF,
  unconditional HookDevice activation and loader-safe lifecycle.
- The same command runs unchanged `persistent_log_test.cpp`: PASS, persistent
  handle, immediate synchronous visibility, 10,000 serialized records, reopen.
- `./build.ps1 -DiagnosticCapture`: PASS.
- `./build.ps1`: PASS.

## Package/runtime receipt requirements

MAIN must preserve **both d3d9.log and d3d9-emergency.log** before another run
(the next activation recreates the emergency file). Keep original INI/resolution
and the previous DLL backup; change only explicit AsyncLog opt-in for A/B.
Require an active admission receipt and inspect live drop/error counters.
Measure render-thread blocking, frame percentiles and stability on comparable
unpaused gameplay before claiming improvement. Steam/PSVR2 testing is NOT RUN.
Rollback: restore previous DLL/INI or revert this isolated commit; AsyncLog=0
retains the synchronous path. No automatic deployment or merge was performed.

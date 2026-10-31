# Handoff: NVIDIA producer-fence hitch (SIDE)

**Date**: 2026-10-31 · **Branch**: side/nvidia-producer-fence-hitch · **Base**: eab97b35 (codex/main-single-owner-d3d9-validation, contains f24ced92)
**Status**: ROOT CAUSE OF 42.656 ms UNPROVEN FOR THIS RUN; no synchronization change justified. Diagnostic-only trace added.
Previous `side/zero-latency-render-pipeline` (a7f1df69) marked SUPERSEDED (4cdb7d28); not merged/reimplemented.

## Findings
- Mode-4 producer path (`CaptureBackbufferToShared`, deferProducer=true) already is minimal: one `Issue(END)` + one
  `GetData(FLUSH)`, no spin. Polling (`PumpStereoProducer` / `DrainOwnerProducer`) uses flags 0 and is outside the
  D3D9Fence scope. Nothing redundant to remove.
- The D3D9Fence CPUSync scope covers Issue + (history Scope ctor) + GetData(FLUSH) only. The SharedCapture max
  (42.666) exceeds producer max (42.656) by 0.010 ms, consistent with the same call (reader wait + StretchRect ~0),
  but window maxima do not prove identity.
- PresentMon spike frame: CPUBusy worst 58.68, CPUWait worst 1.46, GPUBusy worst 10.5 → CPU/driver-side stall, GPU idle.
  58.98 − 42.66 ≈ 16.3 ms ≈ one normal frame: consistent with "normal frame + one producer stall", NOT proven.
- Three earlier same-site stalls (15.7 / 21.3 / 90.9 ms) were proven by ETW ready-thread wake to be the render thread
  blocked inside GetData(FLUSH) on the NVIDIA threaded-driver worker running nvgpucomp32 (shader/pipeline compile of
  previously queued work). The FLUSH is the first sync point, not the cost; removing/deferring it only relocates the
  wait (D3D11 blit and xrEndFrame need the event). Prior decision (compiler-dependency-20261030.md) forbids removing
  FLUSH, moving it across driver lock, or speculative prewarming. Not repeated.

## Change (protected/DiagnosticCapture build only; release build behaviour unchanged)
Exact boundaries in `CaptureBackbufferToShared` (src/shared_capture.inl):
- issue = QPC immediately before/after `slot.producer->Issue(D3DISSUE_END)` only.
- flush = QPC immediately before/after the first `GetData(nullptr,0,D3DGETDATA_FLUSH)` only.
- `command_history::Scope` construction (between the two) and `Finish()` (after flush) are outside both.
- totalMs = issueMs + flushMs (inter-call gap excluded). Mode 0/3 later flags-0 polls are not timed.
Log (only when totalMs >= 11.111, max 256 per process; also on Issue failure if >= threshold):
`[CPUSync] producer outlier mode= capture= frame=<g_trackedFrame.id> issueQpc=a..b issueMs= issueHr= flushQpc=c..d flushMs= flushHr= totalMs= tid=`
flushQpc=0..0 / flushHr=0x8000000A (E_PENDING) means Issue failed and flush never ran.
Fences, worker scheduling, call order and HRESULT handling are unchanged; the release build uses the identical
original calls via a macro. Overhead when CPUSync on: 4 QPC reads + one compare per producer call (~2/frame).

## Validation (offline)
- build.ps1 (release x86) PASS; build.ps1 -DiagnosticCapture (protected x86) PASS.
- test/build_producer_outlier_trace.ps1 PASS: threshold, inter-call gap excluded, exact fields/QPC/HRESULT,
  issue-only, disabled, cap 256, non-outlier overhead 92.1 ns/call measured (~0.2 us/frame at 2 calls/frame).
- build_shared_capture_slots PASS; build_stereo_transfer_worker PASS (real D3D9Ex/D3D11, pixel equality,
  query retirement, Reset/recreate).
- NOT RUN (environmental): build_query_preload_pool and build_light_readback_lifetime require
  CreateDevice(D3DDEVTYPE_REF); the D3D9 REF rasterizer (d3d9ref.dll, SDK) is absent on this host, so they fail at
  device creation. They do not compile the changed code. Run them only on a host with the REF rasterizer.

## MAIN: minimal capture procedure (one launch)
1. Build `build.ps1 -DiagnosticCapture` from this branch; install via the normal protected procedure with the
   current mode-4 INIs unchanged except `[Diagnostics] CpuSyncTiming=1`; keep rollback backup.
2. Run the existing PresentMon-only collector: 5 s stationary + 25 s combat. No WPR.
3. From d3d9.log take every `producer outlier` line; from PresentMon take every interval > 22.22 ms.
4. Classify each spike: contains a flush range with flushMs ~ spike excess -> GetData(FLUSH) driver wait;
   issueMs dominant -> Issue blocking; no producer outlier inside -> producer fence not the cause
   (next: engine Present boundary). Report table: spike QPC, frame, issueMs, flushMs, spike ms.

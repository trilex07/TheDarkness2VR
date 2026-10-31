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
`[CPUSync] producer outlier capture= qpcBegin= qpcIssued= qpcEnd= issueMs= flushMs= totalMs= hr= tid=`
logged only when one mode-4 producer call ≥ 11.111 ms (cap 256). Splits Issue vs GetData(FLUSH) and gives
QPC on the PresentMon clock. Files: src/cpu_sync_timing.inl, src/shared_capture.inl.

## Validation (offline)
- build.ps1 (release x86) PASS; build.ps1 -DiagnosticCapture (protected x86) PASS.
- build_shared_capture_slots PASS (23182 checks); build_stereo_transfer_worker PASS (2660 real D3D9Ex/D3D11 transfers,
  pixel equality, query retirement, Reset/recreate).
- build_query_preload_pool / build_light_readback_lifetime: FAIL at CreateDevice(D3DDEVTYPE_REF) — REF rasterizer not
  available on this host; they do not compile the changed code. Environmental, not a regression.

## MAIN: minimum next trace (one session, only when convenient)
Deploy this branch's protected build, same INIs, CPUSync on, PresentMon-only (no WPR needed). Then:
1. Match each `producer outlier` QPC range to the PresentMon interval containing it.
2. If flushMs ≈ totalMs and lies inside the 50+ ms interval → spike = GetData(FLUSH) driver wait (same class as proven
   compiler stalls). If issueMs dominates → Issue blocking (new finding). If no outlier inside the spike → producer
   fence is not the cause; inspect engine Present boundary max (46.4 ms) next.
3. Only if (2) attributes to FLUSH and a WPR capture is anyway authorized: enable existing command history to name
   preceding binds. No fix is proposed before this.

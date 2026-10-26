# Protected Present CPU timing correction — 2026-10-26

Base: `4281f29`. This is an opt-in attribution correction for the protected
Present path, not an FPS optimization. No Steam launch or deployment.

## Defect and code correction

The latest accepted combined log contains no `[CPUSync]` or `[PerfClock]` records.
Independently, source inspection proved that the protected observer bypassed the
legacy path's `BoundaryWork`, `PresentForward` and periodic `timing.Report` calls.
Enabling the old accumulator alone would not publish its stage windows here.

- `HookDevice` now reads the existing `CpuSyncTiming` option once before either
  Present installer. It previously lived in the later legacy installer; retaining
  that order would latch null timing callbacks in the protected observer.
- `ImagePresentBefore` brackets the existing `ApplyBoundaryDecision` with
  `BoundaryWork`.
- Optional observer callbacks bracket **the actual typed trampoline call** to
  `next(sc,a,b,w,r,flags)`. They run after `before` and before `after`, only for
  the qualified outer call. Nested overlay forwarding cannot double-count or
  replace the outer timestamp.
- `ImagePresentAfter` publishes the existing five-second stage accumulator.
- With `CpuSyncTiming=0` (unchanged default), forwarding callbacks remain null.
  No extra QPC measurement, timer lock or timing log is admitted. Existing
  boundary-scope and Report calls return immediately when disabled.

No arguments, HRESULTs, reference ownership, admission rules, recursion policy,
image-write guard, synchronization or rendering commands changed. All INIs and
profiles are unchanged. Reverting this commit restores the previous behavior.

## What current timings actually cover

`StereoPerf` starts at `RenderEyePairFromReplay`, after native game/update/live
rendering and live command recording have already happened. Its left segment
includes left shared transfer and D3D11 eye draw. Replay+history includes native
state snapshots, history copies and right-eye replay. Right+restore includes
right history/transfer, D3D11 eye draw, color and state restoration. It excludes
OpenXR swapchain acquisition/copy/release and `xrEndFrame`, the next `xrWaitFrame`,
and actual native Present forwarding.

The shared path still requires its D3D11 reader and D3D9 producer fences. The
accepted log records `zeroCopy=1`: CPU pixel readback and upload-texture Map are
not active. D3D11 **constant-buffer** `Map(WRITE_DISCARD)` still runs per eye.
These distinct Maps must not be conflated.

`FrameContext CPU-window` starts after `xrWaitFrame`/`xrBeginFrame`, and ends after
submission. It spans the prior original Present, native game/live rendering,
recording, the eye pair and XR submission. It is coarse wall time, not thread
execution time. `BoundaryWork` also includes the next `xrWaitFrame`; its nested
stage spans are not additive.

D3D9 GPU timestamp elapsed time spans command/submission gaps and required waits;
it is not PresentMon GPU Busy and must not be added to CPU segments as though
both were serial CPU work. It ends before XR swapchain copies/compositor work.

## Existing accepted-session evidence and limit

Private input: `protected-combined-20261026T140248Z/d3d9-runtime.log`.
Only focus-active sections were considered; the long headset-inactive tail is
not gameplay performance evidence. No scene/position marker identifies outdoors.

A focus-active cadence window records 53.8 submitted layers/s, boundary p95
32 ms and reported runtime period 22.222 ms. Nearby, separate coarse batches
record GPU elapsed means 15.349, 13.106 and 13.621 ms. CPU segment means near those
batches total 2.52, 2.11 and 2.29 ms respectively. These are adjacent aggregate
windows, not a matched per-frame CPU/GPU join or proof of one specific wait.

Current evidence cannot divide the missing native/update/recording/Present/XR
wall time into CPU execution, driver blocking, ready delay or frame pacing.
Earlier ETL shader-creation waits establish historical behavior, not attribution
of this session's reported 20-30 ms. No second speculative optimization is added.
The ref-ledger A/B must use this same timing DLL and INI in both arms; only
`UniqueReplayResourceRefs` differs. New logs can separate original Present, XR
waits and boundary work without changing required synchronization.

## Offline verification

- Production x86 build: PASS.
- Diagnostic x86 build: PASS.
- Real x86 fixture DLL observer: PASS with timing on and off; 8,002 native calls
  per complete run, 1,000 release/recreate cycles, original args/HRESULTs, Reset,
  unknown/private interface forwarding and EXE text unchanged.
- Callback ordering proves the native leaf runs exactly once between timing
  callbacks, while frame callbacks remain outside the span. Unqualified calls
  forward but cannot publish native timing or advance the frame boundary.
- Separate existing fixture runs: protected EXE target refusal, unexpected
  HRESULT admission closure and thread-change admission closure: PASS.
- Frame-boundary suite: 2,327,551 assertions, zero failures; 50,000 synthetic
  frames and 25,000 timing cases: PASS.
- Protected profile regression: 3/3 PASS, including startup order/default-off.
- Integration profile regressions: 6/6 PASS.
- Steam / HMD timing validation: NOT RUN by SIDE. No FPS improvement claimed.

# Handoff: overlap native mirror Present with the right-eye GPU wait

**Date**: 2026-10-31
**Task ID**: present-overlap
**Role**: SIDE (offline only)
**Agent**: Claude (Opus 5.5), session 01FyWRDwj9Pjwkcwz7bewmvq
**Branch**: side/present-overlap (parent 75432ae3, codex/main-single-owner-d3d9-validation)
**Worktree**: none (committed via object/ref plumbing while MAIN repairs worktrees)
**Baseline**: provisional mode-4 candidate f24ced92; accepted rollback 4a26aabd
**Integration commit**: pending (MAIN). Independent of the deferred shared-capture candidate.

## Evidence: CPU execution vs CPU wall waiting (existing logs only)
`%LOCALAPPDATA%/Darkness2VR/d3d9.log`, PID 26264 mode 4 (flags 0x50, eyeSlots=2),
combat PerfClock 54600-55320:
- Boundary work 3.8 ms/frame = left 0.07 + replay+history 1.35-1.39 + right+restore 1.55-1.72 + XR/other ~0.8.
- mainJoinMean 0.68-0.77 ms/job (2 jobs/frame), workerMean 0.08: ~1.4 ms/frame of the
  render thread SPINNING on D3D9 producer events = wall time, not CPU work.
- GPUFrameSplit replay span 3.3-3.5 ms vs CPU replay 1.35 ms: the GPU is busy for the
  whole right eye (a starved GPU would give a ~1.6 ms span). Extra flushes cannot help.
- Original SwapChain Present 0.36-0.37 ms mean (max 0.85-1.49), runs AFTER the wait.
- Oct-30 command-history (`command-history-26912.bin`): each frame is boundary ->
  ~10 ms with no D3D9 rendering (game update) -> ~5.5 ms live submission. The right
  eye GPU work cannot start before live submission ends.

## Ranked opportunities
1. Twin-resource interleaved right-eye replay (replay passes into per-eye RT/depth
   twins during the live submission phase, so the ~3 ms right-eye GPU work overlaps
   live CPU). Hides most of the ~1.4 ms wait and removes all colour copies.
   Same-frame, no latency change, no OpenXR change. LARGE: RT/texture/depth/INTZ/cube
   twin lifetime, read-before-write seeding, StretchRect/ColorFill remap, per-segment
   state save/restore. Not a minimal change; needs MAIN runtime iterations.
2. Cross-frame pipelining of right-eye completion (~1.4 ms). Ruled out: needs
   xrWaitFrame(N+1) before xrEndFrame(N) (OpenXR order change), D3D9 event polling
   from a non-owner thread (device is not MULTITHREADED in mode 4) or from game
   update code that issues no D3D9 calls for ~5-10 ms (adds latency).
3. Fill the GPU wait with right-eye-independent CPU work: the native mirror Present
   (0.36 ms). IMPLEMENTED here.

## Change
- `src/present_overlap.h` (new, pure): admission policy (EMA of measured wait vs
  measured Present, 0.25 ms margin, 32-sample warm-up, 90-frame back-off whenever the
  GPU finished before the early Present returned) and a forward-once slot.
- `src/image_present_observer.h`: the qualified, ready OUTER Present arms a
  thread-local forward-once slot during before(). `Observer::ForwardEarly()` runs the
  identical forward (counters, CpuSync brackets, HRESULT); after before() the slot is
  disarmed and Finish forwards only if not already done. All other paths unchanged.
- `src/d3d9.cpp` (protected build only, `[Render] OverlapNativePresent`, default 0):
  just before the right-eye `FinishStereoTransfer`, i.e. after the right capture,
  live-colour restore and live-end state apply are queued, forward the mirror Present
  if the policy admits and the right producer is still pending. Matched capture keeps
  original order. `[PresentOverlap]` summary every 600 pairs.
- Unchanged: replay, recording, fences, worker, XR call order (RenderEyePair ->
  SubmitXR/xrEndFrame -> BeginTrackedFrame), hands, tentacles, input, UI.

## Correctness
- Present needs nothing the GPU is still computing; every D3D9 command of the pair
  precedes it in queue order, so GPU order is identical. The game's Present args,
  flags and HRESULT are passed through unchanged, exactly once.
- Latency: xrEndFrame still waits for the same producer event. Overlap only when the
  measured wait exceeds Present+0.25 ms; possible overruns are detected (event already
  complete at the first poll after Present) and force 90 plain frames.
- Overlays running inside Present see g_insidePresentation=true and recording sealed,
  as before. Diagnostic shift: CpuSync "engine Present boundary work" now contains the
  hidden Present, and GPUFrame End is issued after Present.

## Expected saving
~0.33-0.37 ms/frame (mean Present CPU wall time hidden inside a ~1.4 ms wait),
i.e. 13.73 -> ~13.4 ms. Estimate from existing logs, not a measurement.

## Offline tests
- `test/present_overlap_test.cpp`: 435,138 checks PASS (g++ -Wall -Wextra -Werror, x64 VM).
  Windows runner `test/build_present_overlap.ps1` (/W4 /WX x86): NOT RUN.
- `tools/test_present_overlap_contract.py` 4/4 PASS; existing tools/test_*.py PASS.
- NEW Windows mode `early-forward` in tools/frame-boundary-simulator observer test
  (real x86 fixture DLL + MinHook): NOT RUN here (no MSVC/Windows on SIDE VM).
- NOT RUN: build.ps1 / -DiagnosticCapture, Steam/PSVR2.

## MAIN next steps
1. Build both flavours; run build-image-present-observer.ps1 (now incl. early-forward)
   and test/build_present_overlap.ps1.
2. One mode-4 run with current profile + `OverlapNativePresent=1`. Expect
   `[PresentOverlap] allowed=1 overlapped>>overruns`; compare PresentMon combat
   FrameTime mean/p95 and xrEndFrame timing vs the f24ced92 pair (13.73 ms).
3. Roll back with `OverlapNativePresent=0` (no other effect).

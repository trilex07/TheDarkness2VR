# Handoff: right-eye GPU-path copy reduction (ReuseSharedCaptureColor)

**Date**: 2026-10-31
**Task ID**: shared-capture-color-reuse
**Role**: SIDE (offline only; no Steam, no deployment)
**Agent**: Claude (Opus 5.5), session 01FyWRDwj9Pjwkcwz7bewmvq
**Branch**: side/cpu-hotpath-opt
**Worktree**: C:/Users/Trilex/Darkness2VR/.side-cpu-hotpath
**Baseline**: provisional mode-4 candidate f24ced92 (installed protected DLL 34641A89...); accepted rollback 4a26aabd
**Integration commit**: pending (MAIN)

## Goal
Remove the largest avoidable cost on the measured mode-4 critical path without
touching latency, tracking, hands, tentacles, input or the XR frame protocol.

## Status
Ready for Review. Code default OFF (`[Render] ReuseSharedCaptureColor=0`).

## Root cause (existing logs only, no new capture)
Source: `%LOCALAPPDATA%/Darkness2VR/d3d9.log`, mode-4 session PID 26264, combat
PerfClock frames 54600-55320 (`StereoTransferWorker` admitted mode=4 flags=0x50,
`SharedCapture eyeSlots=2`, deferReader=1, ReuseStateSnapshots=1, RT0 2676x2728 fmt=22).

- `[CPUSync] engine Present boundary work` mean 3.81-3.85 ms of the 13.73 ms frame.
- `[StereoPerf]` left 0.07, replay+history 1.35-1.39, right+restore 1.54-1.72 ms.
- `[StereoTransferWorker]` mainJoinMean 0.68-0.77 ms/job, workerMean 0.08 ms.
  The left job's event is long complete when joined, so the right join is
  ~1.4 ms/frame: the render thread SPINS in DrainOwnerProducer waiting for the
  GPU to finish the right eye. `[GPUFrameSplit] replay` 3.3-3.5 ms vs CPU replay
  ~1.35 ms confirms the boundary is GPU-bound from replay start to the right event.
- XR itself is small: xrEndFrame 0.31, swapchain 0.055/eye, xrWaitFrame 0.02 ms,
  reader fence 0.005, producer fence 0.08-0.09 ms.

Therefore every GPU operation queued between the boundary start and the right
producer event is paid 1:1 in frame time. Besides the right-eye replay itself
that chain contained FIVE full-resolution StretchRects (left capture, live
backup, history seed, history save, right capture). Two are redundant whenever
the eye slots are split: the left capture already is an exact private copy of
live RT0 (slot 0 is written only by capture 0), and the right capture already is
an exact copy of the right eye (slot 1 is written only by capture 1).

## Change
- `src/shared_color_reuse.h` (new, pure policy): exact-mirror check by RT0
  identity + capture serial + size + lossless format (X8R8G8B8/A8R8G8B8, no MSAA).
- `src/shared_capture.inl`: each slot records the RT0 identity/serial of its copy,
  cleared on entry and certified only after a successful StretchRect.
- `src/d3d9.cpp`:
  - live backup: when slot 0 mirrors RT0 this pair, skip `BackupLiveColor` and
    restore from slot 0 (re-validated unchanged before the restore);
  - right history: publication only validates; after the right capture the
    history ADOPTS slot 1 (`EyeColorHistory::Adopt`, read-only alias, ref held);
    capture failure / non-mirror falls back to the original Save copy while RT0
    still holds the right eye. `Save` never writes through an adopted alias.
  - startup INI switch + one-line state log `[SharedCaptureColor]`.
- Unchanged: replay, recording, fences, worker ordering, XR calls, Present order,
  head/hands/tentacles/input, Reset ordering (history released before slots).

## Expected saving
Two 2676x2728x4 StretchRects = 2 x (29.2 MB read + 29.2 MB write) = 116.8 MB
removed from the GPU path that the render thread waits on, plus two blit
submissions: ~0.12-0.26 ms/frame at 450-1000 GB/s effective copy bandwidth
(GPU-dependent; estimate, not a measurement). Also frees 2 x 29.2 MB VRAM.
Visible as lower `right+restore`/`mainJoinMean` and boundary work at equal scenes.

## Tests (offline, this VM: g++ 11 x64, Python 3)
- `test/shared_color_reuse_test.cpp`: 402,242 checks PASS (format sweep, identity,
  pair-serial ordering, restore re-validation). Windows runner:
  `test/build_shared_color_reuse.ps1` (/W4 /WX x86) - NOT RUN here.
- `test/eye_color_history_test.cpp` (actual-source extraction, same generator as
  `build_eye_color_history.ps1`): baseline fixture on baseline source PASS;
  baseline fixture on new source PASS; new fixture (1000 adopted frames, zero
  history copies, read-only slot, rejection, fallback, alias-safe Save, target
  switch, Reset refs) PASS; new fixture on old source fails to compile (proves coverage).
- `tools/test_shared_capture_color_reuse.py` 5/5 PASS (pair ordering, certify-after-
  copy, alias guard, Reset order, default OFF); existing `tools/test_*.py` PASS.
- NOT RUN: MSVC `build.ps1` / `-DiagnosticCapture` (no MSVC on the SIDE VM; a
  downloaded clang cross toolchain was refused), HAL D3D9 fixtures, Steam/PSVR2.

## Next steps (MAIN)
1. `build.ps1` and `build.ps1 -DiagnosticCapture`; run `build_eye_color_history.ps1`,
   `build_eye_color_history_d3d9.ps1 -Hardware`, `build_shared_color_reuse.ps1`,
   `build_shared_capture_slots.ps1`, `build_stereo_transfer_worker.ps1`.
2. One run with the current mode-4 profile + `ReuseSharedCaptureColor=1` (needs
   eyeSlots=2). Confirm `[SharedCaptureColor] active=1 ... format=22`, compare
   StereoPerf right+restore, mainJoinMean, boundary work, PresentMon combat mean.
3. Visual check: right eye in UI-only/menu segments (seeded history), no left-eye
   bleed after restore, mirror window intact.

## Larger remaining item (not implemented, needs MAIN runtime evidence)
The ~1.4 ms right-eye GPU wait itself can only be hidden by pipelining the right
eye publication (+xrEndFrame N) into the next frame. That moves xrBeginFrame(N+1)
and depends on SteamVR accepting xrWaitFrame(N+1) before xrEndFrame(N); it is a
protocol/latency change and was deliberately not attempted offline.

## Notes
NativeSubmission still reports `enabled=0 denied=0x12` (OpaqueStateBlock |
UncontrolledMutation) in this session, so the native float/binding filters are
inactive; the game builds state blocks (Begin/EndStateBlock) during gameplay.

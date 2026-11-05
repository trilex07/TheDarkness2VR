# Handoff: per-frame menu-state cache (30 FPS render-thread fix)

**Date**: 2026-11-05
**Task ID**: menu-state-cache-20261105
**Role**: MAIN (user-assigned)
**Agent**: Claude
**Branch**: claude/menu-state-cache-20261105
**Worktree**: C:/Users/Trilex/Darkness2VR/.main-menu-state-cache-20261105
**Base**: 9515f875 (accepted crane baseline, source e641c9a5, Steam DLL 622D75E5)
**Status**: simulator A/B PASS; NOT deployed to normal Steam; headset NOT RUN; not merged

## Cause (evidence)

WPR CPU capture (see `.main-fps-diagnostic-20261105` handoff, commit f1e3b2b3):
render thread spent ~12% of samples in `ZwReadVirtualMemory` <- `ReadProcessMemory`
<- folded reader lambda RVA 0x18C60, whose only caller is
`d2vr_menu_native::ObservePause`. Source chain: `HideMenuFirstPerson`
(stereo_replay.inl, called from all four draw wrappers) and `NativeCameraUiScale`
-> `NativeMenuPresentationActive()` -> `MenuConfirmActive()` ->
`PollBoolSnapshot::Read`, which caches only inside an input-poll scope. On the
render thread there is none, so every draw (live + replay eyes) in gameplay
re-walked the native pause graph twice (up to 18 syscalls per draw).

## Change

- `src/input_poll_snapshot.h`: `PollBoolSnapshot::Active()`; new
  `FrameBoolSnapshot` (thread_local; refresh on key change, frame change, age
  >maxAgeMs, or clock regression; inside a poll scope, poll semantics win).
- `src/eye_gaze.inl`: `MenuConfirmActiveThisFrame()` keyed by the same
  (RegionMgr, in3DGameplay) key, frame = (g_frameId, g_backupFrameCounter), 50 ms cap.
- `src/native_action_movies.inl`: `NativeMenuPresentationActive()` uses the
  frame-cached read. All other `MenuConfirmActive()` callers are unchanged.
- `src/d3d9.cpp`: `extern g_backupFrameCounter`.
- Test: `tools/frame-boundary-simulator/xinput_bridge_test.cpp` FrameBoolSnapshot
  cases. `build-xinput-bridge.ps1` PASS: production 725, diagnostic 779 checks.
- `tools/measure-sim-arm.ps1`: 20 s stationary sample = simulator live frame
  summary + per-thread CPU deltas + StereoPerf/XRCadence log lines.

Build: `build.ps1 -DiagnosticCapture` -> protected-native, SHA256
A9D487B1C5D3B73930A9514C28C9B2CC643D35C1E6A997BBFA65C077E462CE72
(assert-candidate-profile PASS).

## Simulator A/B (OpenXR-Simulator x86, process-only override)

Lab: `test-output/menu-state-cache-lab-20261105` (git-excluded, private).
Same save (four user files restored between arms), title -> Enter -> Continue,
stationary at crane checkpoint, identical view (A-view.png / B-view.png).

| Arm | DLL | FPS | p50 / p99 ms | render thread | replay+history |
|---|---|---|---|---|---|
| A1 baseline | 622D75E5 | 32.21 | 30.81 / 35.30 | 97.4% | 11.6-12.6 ms |
| A2 baseline | 622D75E5 | 32.23 | 30.84 / 34.97 | 95.8% | 11.6-12.0 ms |
| B1 fix | A9D487B1 | 89.15 | 10.81 / 13.74 | 92.2% | 1.1-1.25 ms |
| B2 fix | A9D487B1 | 89.43 | 10.65 / 14.34 | 92.2% | 1.1-1.23 ms |

Baseline reproduces the user's slow-state signature (≈30 world frames/s,
replay+history ≈12 ms, app_busy 100%). Fix: 88-90 world frames/s; 20-27% of
frames still over the 11.1 ms budget (app_busy) -> render thread still near
saturation; further headroom work remains.

Functional: Escape pause menu in fix arm -> first-person hidden both eyes
(`[MenuFirstPerson] hidden eye=0/1`), menu enlarged, VR settings entry shown
(B-pause.png). Resume -> hands, guns, tentacles back (B-resume.png).
Not covered: R2/A confirm routing in menus, double-tap/objective gates (they
call uncached `MenuConfirmActive()` and are unchanged), metro/combat scenes,
physical headset.

## Final installed state / recovery

Own PIDs 39148 (A) and 36084 (B) closed gracefully after path/start-tick
identity check. `simulator-lab.ps1 -Action Restore` PASS. Steam:
d3d9.dll 622D75E5…, Darkness2VR-diagnostic.ini 117230AC…, EXE D46BB6CB…,
openxr_loader D4DD992E…, Darkness2VR.ini 22862511… (unchanged during lab).
Four user files (EE.cfg, Editor.cfg, CONTINUE.SAV, settings) restored and
hash-verified against lab/user-backup/receipt.json. Simulator OFF.

## Next steps

1. User headset test of the A9D487B1 candidate in the previously slow scene
   (needs a frozen install package with full rollback, MAIN deployment only on request).
2. Remaining over-budget frames: re-profile fix build with WPR during gameplay.

# Handoff: Offline audit of the whole mod

**Date**: 2026-11-07
**Task ID**: mod-audit-20261107
**Role**: SIDE (offline only — nothing deployed, no Steam launch, no headset test)
**Branch**: side/mod-audit-20261107 (from origin/main 9515f875)

## Method
- `build.ps1` (W3) clean; full main TU at /W4 + MSVC `/analyze`.
- INI keys vs `GetPrivateProfile*` reads (99 INI keys, 240 reads).
- Ran every `test/build_*.ps1` and `tools/frame-boundary-simulator/build*.ps1` (109 scripts).

## Findings
| Area | Result |
|---|---|
| Proxy build / static analysis | Clean. Analyzer hits were false positives (guarded `__try`, success-checked locks) except two dead INI fallbacks (fixed). |
| INI | Every key in `Darkness2VR.ini` is read by code; no duplicates. |
| Test suite | 23/109 failed. 15 only because test scripts call `vswhere.exe` by bare name (not on PATH). After PATH fix, 21 remained: **stale fixtures** — production code moved on (new gates/deps), tests did not. |
| Shadows (`build_shadow_draw_test`) | Fixture rot (missing `HideMenuFirstPerson` stub) → fixed, PASS. |
| FPS (`build_adaptive_fps`) | Policy 3253 checks PASS. Integration exe is blocked by Windows ("Access denied" on any copy) — environment/AV, not code. |
| Resolution / supersampling | `RenderWidth/Height` force backbuffer size; VR settings menu can override with Supersample mode. `refresh`, `nis_eye_host_warp`, `xr_display_refresh` tests PASS. Working as designed. |
| Interactions / gestures | `build-protected-gestures` asserted old START-toggle pause; f8ea5c2a intentionally moved pause to native `MenuConfirmActive`. Test updated → 913 checks PASS. |
| Hands | 3 stale deps + 2 intentional behaviour changes (2e9b887a keeps hands during demon holds; cinema gate via `CinemaSceneThisFrame`). → 2234 checks PASS. |
| Left tentacle aim | New deps (gaze grab, action movies, `aimValid`) + 83ebbd9d intent (ray before LB edge, consumption uses fresh ray). → 444 checks PASS. |
| Tentacle throw | Stale deps (`demonHold`, gaze throw, stabilizer). → 316 checks + 21-instruction native contract vs real EXE PASS. |
| Demon grab/throw in legacy `GestureBridge` path | Still logs "native target acquisition / throw not implemented" (d3d9.cpp `[Gesture]`). The protected path is the real implementation; legacy path is unfinished. |

## Changes (this branch)
- `src/d3d9.cpp`: removed `GetPrivateProfileIntA(nullptr, ...)` fallbacks for `RenderPipelineTrace`/`DisableOpenXR` (NULL section is invalid → always default; no behaviour change).
- Untracked committed binaries `stereo_harness.exe`, `frame_boundary_suite.exe`; `.gitignore` for tools/test exes.
- Test fixtures updated: protected_gestures, protected_left_throw, shadow_draw, protected_hands, protected_left_aim.

## Still failing (stale fixtures, follow-up)
camera_native_source, gaze_fxaa, live_record_status, native_player_camera_owner, outline_coverage & specular_footprint (need mandatory params), pipeline_warmup, projection_history, shader_preload_pool, shared_capture_slots (d3d11 header redefinition), tentacle_head_scope, build_test (stereo), command-history, live-pass-probe, projection-estimate, small-record-copy, xr-event-recovery.
Also: test scripts should resolve `vswhere` by full path.

## MAIN verification needed
Nothing here changes runtime behaviour except dead-code removal; in-game verification of shadows/FPS/resolution/interactions was NOT done (SIDE).

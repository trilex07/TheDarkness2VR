# Handoff: Adaptive application half-rate pacing

**Date**: 2026-10-31
**Agent**: Codex
**Task ID**: adaptive-half-rate
**Role**: MAIN (explicit user assignment and deployment authorization)
**Branch**: codex/adaptive-half-rate
**Worktree**: C:/Users/Trilex/.codex/worktrees/adaptive-half-rate/Darkness2VR
**Baseline**: source 7adcacbd; installed candidate 71597871, DLL BFF8B0A9F5BE83C511762E66C78C5903BD9FB3A48FD3D2F81BF15C035FBD1CFA. Accepted rollback remains 4a26aabd per BASELINE.md.

## Goal
User explicitly requested implementation: sustained FPS below headset refresh should select half-refresh application rendering, allowing runtime reprojection.

## Status
Deployment preflight PASS; authorized MAIN deployment in progress. Runtime/PSVR2 acceptance pending.

## Context Loaded
BASELINE.md, WORKFLOW.md, FAILED-APPROACHES.md, superseded zero-latency handoff, existing adaptive policy/refresh and VR settings implementation.

## Changes
- Enable AdaptiveFPS=AUTO in repository INI/default config. OFF and HALF remain selectable in the existing VR panel.
- Existing accepted fresh-stereo/native-Present receipts feed the policy. Entry below 98% physical Hz requires two complete slow windows and >=3s residence.
- Bind application limiter, not a compositor API. Pace before xrWaitFrame and pose location; preserve paired eyes, fresh prediction and one wait/begin/end cycle. No game binary patches or SteamVR setting writes.
- Physical refresh comes from existing XR_FB_display_refresh_rate/OpenVR display-property fallback, never predictedDisplayPeriod. Unknown refresh disables limiting.
- HALF targets 2/Hz seconds per app frame; late frames add no delay. High-resolution waitable timer with older-Windows fallback; no busy spin.
- AUTO probes uncapped after 8s in half-rate, measures fresh throughput again, and re-enters half after sustained underperformance. Manual HALF does not probe. This replaces the otherwise unavailable full CPU/GPU recovery input; probe transitions may be perceptible.
- VR panel explicitly labels application limiting; no claim that motion smoothing is active.

## Evidence
Official OpenXR frame timing: https://github.com/KhronosGroup/OpenXR-Guide/blob/main/chapters/frame_submission.md
Core OpenXR does not offer a portable switch forcing compositor synthesis. Runtime reprojection/motion smoothing must be available/configured independently. 90Hz ->45 app FPS;120Hz ->60. Rates below half capacity remain below target.

## Offline validation
- Production and diagnostic x86 DLL builds passed after the final deadline fix.
- Policy: 3253 checks passed (including 85 FPS at 90Hz entry and AUTO probe/manual HALF behavior).
- Application deadline test passed: 72/90/120/144Hz, runtime already late, no extra delay, disabled/reset/unknown rates. Caught and fixed floating-point cancellation at exact frame start.
- VR settings: 4518 input/laser, 863 renderer, 50 actual host-routing and 7215 actual submission checks passed.
- Adaptive actual-host integration fixture compiled but Windows refused execution with sharing error 32 after the script's bounded retries; not counted as PASS. This matches a previously documented environment limitation.
- git diff --check passed. No Steam launch/deployment/runtime/HMD tests.

## Next Steps
MAIN review/cherry-pick only the task commit onto current integration, build and deploy using backups. Test AUTO at 90/120Hz, transition/probe behavior, OFF/HALF menu, focus loss/session restart, changed/unknown refresh, full stereo, tracking latency, gameplay timing and runtime frame timing. Confirm actual motion smoothing separately. No Steam/game files were touched by SIDE.

## Limitations
This is an application frame limiter, not motion estimation/frame synthesis. No hardware pacing precision, FPS improvement or reprojection acceptance is claimed. Existing unrelated dirty root checkout was left untouched.

Remote push was rejected by automatic approval review: repository destination/payload not explicitly authorized. Local commit is complete; push requires user approval.


## MAIN deployment preparation
Current installed DLL, EXE, Steam/local INIs match the frozen deployment gate. No game is running. Current quarantined image switches remain OFF. The protected-native DLL is required by the preserved profile; verified candidate SHA256 4B50EAF295135D8BBB06B039D91FA739D114266A189C3572D95D0890DF1693C3. Source: 346fac9327493ac6fffc2ebd2bdade07ad52e824. StateBlock lifetime model and actual x86 HAL/Ex regression PASS. Deployment script saves all nine current DLL/INI/cache files with SHA256 verification and Restore.ps1, changes only DLL and Render/AdaptiveFPS, and verifies all preserved contracts. No game launch is part of this deployment.

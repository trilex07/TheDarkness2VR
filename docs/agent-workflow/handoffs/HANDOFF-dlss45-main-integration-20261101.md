# Handoff: DLSS 4.5 MAIN integration

**Date**: 2026-11-01
**Task ID**: dlss45-main-integration-20261101
**Role**: MAIN
**Agent**: Codex
**Branch**: codex/gaze-nis-main-integration
**Worktree**: C:/Users/Trilex/.codex/worktrees/gaze-nis-main-integration/Darkness2VR
**Baseline**: installed DLL `CC7DD6C7B048158255D607649841BDE6305D284414CAD790A4CE99B14A941248` from source `b9b1d190`; stock EXE `D46BB6CB...`; protected profile LOD 1.25/NIS 2. Accepted older rollback is separate (see BASELINE.md).
**Integration commit**: SIDE `8f67a69d` cherry-picked as `15d90d51`; MAIN hardening source `c9bb4057`.

## Goal

Integrate an opt-in DLSS/DLAA candidate from SIDE, validate offline, and prepare a protected MAIN headset test.

## Status

Headset test failed: user reported heavy blur and FPS near 50. Full protected rollback completed and independently verified; DLAA is no longer installed or enabled. Keep this trial off main.

## Context Loaded

- [x] BASELINE.md
- [x] FAILED-APPROACHES.md checked (no direct DLSS entry)
- [x] SIDE handoff and DLSS integration plan
- [x] Installed EXE/DLL/profile hashes and nine-target rollback manifest

## Changes

- Cherry-picked SIDE source `8f67a69d` into the current MAIN source without changing the SIDE worktree.
- Fixed undersized eye-cache fallback after DLSS evaluate failure. Failure now disables repeat attempts until reset; NIS scales the current small pair and the next swapchain pass returns to full size.
- Changed no-depth guide to a defined constant plane (`0.5`) rather than treating every pixel as sky. Native per-eye depth is still absent.
- Defaulted experimental block-matched object motion OFF because the local standalone test dispatched it but measured zero searched/overridden tiles.
- Fixed host-loss test to terminate this client's exact helper PID, rather than every process with the same image name.
- Preferred the locally consistent VS Build Tools install over a mismatched Community linker/PDB pair; corrected host license filenames in the legacy installer.
- Added `tools/deploy-dlss45-dlaa.ps1` and `tools/restore-dlss45-dlaa.ps1` for nine protected targets plus seven host files. Normal `build.ps1 -Install` is insufficient rollback protection for this trial.

## Evidence

- Standalone x86/x64, D3D11/D3D12, NGX v310.9.1.0 and FSR suite: **30 passed, 0 failed** on local RTX 5070 after the exact-PID test fix. This is synthetic GPU/IPC evidence, not in-game stereo acceptance.
- Protected-native x86 proxy build PASS and profile gate PASS. Installed DLL SHA256 `398B262A36582E136BBCF52C061681D3A7263FC111BFE1A51C47913A29ED1F42`.
- x64 host built from current source with local NGX/FidelityFX SDKs. Test DLAA 2676x2728: approximately 2.7-2.8 ms GPU per eye; present-thread CPU approximately 3.5 ms per eye. No FPS or engine CPU saving claim.
- Previous installed DLL and stock EXE matched BASELINE.md before deployment. The game host directory was absent. Protected deploy preflight PASS with seven staged host files and game closed.
- Actual game run: host initialized NGX on RTX 5070 and created both 2676x2728 DLAA eye features. Proxy log recorded 4500/4500 frames per eye with zero refusals; GPU evaluate averages were 3.67 ms left and 2.95 ms right. User reported a very blurry image and FPS dropping to approximately 50. The log does not independently prove exact FPS or isolate the whole frame-time regression.
- After the game closed, `Restore.ps1` restored all nine protected targets. Independent post-restore hash check passed 9/9. Installed DLL returned to `CC7DD6C7B048158255D607649841BDE6305D284414CAD790A4CE99B14A941248`, stock EXE stayed `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`, and the previously absent DLSS host directory is absent. The game INI again has LOD 1.25 and NIS 2 with sharpness 0.80 and no DLSS override.

## Next Steps

1. Diagnose native per-eye depth, projection jitter and motion-vector provenance before another visual candidate. Constant depth and rotation-only motion are plausible causes of blur, not yet proven.
2. Reduce or decouple the synchronous host acknowledgement cost and measure a controlled same-scene frame-time comparison before another performance claim. Any new candidate needs a separate protected package and headset gate.

## Blockers

Native full-resolution per-eye depth and reliable object motion/jitter are not connected. The current candidate uses constant depth and rotation-only camera vectors; quality under head translation, animation, tentacles and hard cuts is unknown. The x86 render path synchronously waits for the host's frame acknowledgement, adding present-thread CPU time. Host synthetic tests cannot prove headset comfort or FPS benefit.

## Installed MAIN receipt

- Historical rejected trial package: `C:/Users/Trilex/AppData/Local/Darkness2VR/packages/dlss45-dlaa-20261101T131236586`; restored as recorded above.
- Installed DLL SHA256: `398B262A36582E136BBCF52C061681D3A7263FC111BFE1A51C47913A29ED1F42`; stock EXE `D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1`.
- Backups 9/9 and host files 7/7 hashes independently verified after install. `DlssMode=1`, `DlssPreset=11`, `DlssBackend=0`, `DlssObjectMotion=0`, `DlssBiasMask=0`, `NisMode=0` confirmed in game INI. `EngineLODScale=1.25` and `NativeViewBoundary=3` retained. Game closed; no agent launch.

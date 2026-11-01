# Handoff: DLSS 4.5 MAIN integration

**Date**: 2026-11-01
**Task ID**: dlss45-main-integration-20261101
**Role**: MAIN
**Agent**: Codex
**Branch**: codex/gaze-nis-main-integration
**Worktree**: C:/Users/Trilex/.codex/worktrees/gaze-nis-main-integration/Darkness2VR
**Baseline**: installed DLL `CC7DD6C7B048158255D607649841BDE6305D284414CAD790A4CE99B14A941248` from source `b9b1d190`; stock EXE `D46BB6CB...`; protected profile LOD 1.25/NIS 2. Accepted older rollback is separate (see BASELINE.md).
**Integration commit**: SIDE `8f67a69d` cherry-picked as `15d90d51`; MAIN hardening commit pending.

## Goal

Integrate an opt-in DLSS/DLAA candidate from SIDE, validate offline, and prepare a protected MAIN headset test.

## Status

Offline candidate and protected installer preflight PASS. No headset result yet.

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
- Protected-native x86 proxy build PASS and profile gate PASS. Candidate DLL SHA256 `398B262A36582E136BBCF52C061681D3A7263FC111BFE1A51C47913A29ED1F42` before any later source changes. Rehash before install.
- x64 host built from current source with local NGX/FidelityFX SDKs. Test DLAA 2676x2728: approximately 2.7-2.8 ms GPU per eye; present-thread CPU approximately 3.5 ms per eye. No FPS or engine CPU saving claim.
- Exact installed DLL and stock EXE matched BASELINE.md. Current game host directory does not exist. Protected prior manifest lists nine files. Protected deploy preflight PASS with seven staged host files and game closed.

## Next Steps

1. Commit final source and deployment scripts; rerun profile/hash preflight.
2. Install the protected DLAA candidate only with the game closed. Verify the new DLL, stock EXE, all host files, six active INI keys, and all rollback backup hashes.
3. User tests both eyes, motion/ghosting, weapons and frame pacing in headset. Read log. Keep DLAA off main until accepted.

## Blockers

Native full-resolution per-eye depth and reliable object motion/jitter are not connected. The current candidate uses constant depth and rotation-only camera vectors; quality under head translation, animation, tentacles and hard cuts is unknown. The x86 render path synchronously waits for the host's frame acknowledgement, adding present-thread CPU time. Host synthetic tests cannot prove headset comfort or FPS benefit.

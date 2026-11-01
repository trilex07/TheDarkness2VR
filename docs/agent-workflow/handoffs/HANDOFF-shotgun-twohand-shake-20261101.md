# Handoff: shotgun support grip, alignment, and camera shake

**Date**: 2026-11-01
**Task ID**: shotgun-twohand-shake-20261101
**Role**: MAIN
**Agent**: Codex
**Branch**: codex/gaze-nis-main-integration
**Worktree**: C:/Users/Trilex/.codex/worktrees/gaze-nis-main-integration/Darkness2VR
**Baseline**: installed held-finisher candidate DLL `DDDEF1CD8FB135A7D89E6C515786D809351926914FC1FF1C195ADF1F27B6F5BF`, source `c9b86d0c`; its finisher headset result is still pending. Accepted rollback baseline remains `4a26aabd` per BASELINE.md.
**Integration commit**: pending; diagnostic source commit below

## Goal

The next gameplay candidate should let the left hand support an equipped shotgun, align its visible model and firing direction with the physical aim, and suppress camera shake without leaving hands or weapon shaking against a steady scene.

## Status

In Progress. No behavior change, installation, or headset acceptance yet. A bounded pose observer is built to establish the shotgun's live model and controller frames.

## Context Loaded

- [x] BASELINE.md and WORKFLOW.md
- [x] FAILED-APPROACHES.md (no shotgun or shake entry)
- [x] Current protected weapon/hands source; extracted `CameraShake.lua`, `DarknessVoice.lua`, `DrunkWalk.lua`, `Stoned.lua`; `trilex_vr_mod.md` camera/two-hand routes

## Findings

- `src/protected_hands.inl` publishes independent left grip and right gun aim poses. `src/native_hands.inl` explicitly leaves `support=false` for the protected packer. The legacy support calibration exists but is not a proven shotgun fore-end grip.
- `src/protected_weapon.inl` replaces the owned gun model and final shot matrix using the right controller aim pose. No shotgun-specific model-axis calibration exists. The latest installed log has no shotgun pose measurement, so a fixed rotation is not justified.
- The game's scripts feed `postProcess.viewShake` through several independent effects. `CameraShake.lua` writes `mShakeAmbient`; `DarknessVoice.lua`, `DrunkWalk.lua`, and `Stoned.lua` also write shake fields. Disabling only one script would miss others. A late view-only cancellation risks relative weapon/hand motion; identify and suppress the shared native shake producer before tracked hand and gun transforms consume the camera frame.
- The stock EXE contains the `mShakeAmbient` property name at VA `00F6A514`; static references at `010A391C` and `010B9BCC` appear in registration tables. The registered setter `00DC8F50` writes float field `+0x2C` of a script-resolved view-shake object at `00DC8FFC`, and the getter `00DC9030` reads it at `00DC90AA`. These are evidence for one scripting property, not the native camera-shake consumer or a safe patch site.

## Changes

- `src/protected_weapon.inl`: diagnostic `[WeaponPose]` at most once per second for the owned primary model, recording native and controller-driven world axes, weapon class, and left grip state. No model/shot/hand output changed.
- This handoff.

## Offline checks

- `build.ps1 -DiagnosticCapture` protected x86 build PASS outside the sandbox (sandbox linker alone refused its MSPDB140.DLL version).
- `tools/assert-candidate-profile.ps1` PASS for current repo, game and local profiles; DLL SHA256 `867E3DE9E96920836F9B3CE761AF53875029F6A89A26EB43AFC6E39118C3EE0C`.
- `git diff --check` PASS. No Steam launch or game file change.

## Next Steps

1. Finish protected x86 build and profile gate. Keep the current installed finisher candidate until its user test or a clearly authorized replacement window.
2. Capture a live shotgun pose with the user in the headset; identify the exact variant, whether the controlled model is rotated versus the firing ray, and the fore-end support point. Use the log and visual result to derive calibration, not a guessed offset.
3. Implement one coherent two-hand grip state for shotgun only: require fresh right/left tracking and intentional left grip near the proven fore-end; release on tracking loss, unequip, grab conflict, or cinematic. Keep model, shot ray and human IK in the same frame.
4. Trace the native `viewShake` consumer and camera/human/gun transform order before adding a shake-disable switch. Verify no independent arm or weapon oscillation during a known shake event.
5. Build, protect nine rollback targets, install only when the game is closed, and obtain user headset feedback before MAIN promotion.

## Blockers

No captured shotgun transform or shared native shake-consumer address yet. Thus the three requested behavior changes are not ready for installation.
